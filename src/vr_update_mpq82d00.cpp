/*
 * vr_update_mpq82d00.cpp
 *
 * MPQ82D00GQT configuration update per AN243.
 */

#include "vr_update_mpq82d00.hpp"

#include "vr_update.hpp"

static void stripCarriageReturn(std::string& line)
{
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }
}

vr_update_mpq82d00::vr_update_mpq82d00(
    std::string Processor, uint32_t Crc, std::string Model,
    uint16_t SlaveAddress, std::string ConfigFilePath, std::string Revision,
    uint16_t PmbusAddress) :
    vr_update(Processor, Crc, Model, SlaveAddress, ConfigFilePath, Revision,
              PmbusAddress)
{
    DriverPath = MPQ82D00_DRIVER_PATH;
}

bool vr_update_mpq82d00::selectPage(uint8_t page)
{
    int ret = i2c_smbus_write_byte_data(fd, MPQ82D00_PAGE_REG, page);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Setting page 0x%x failed\n", page);
        return false;
    }
    return true;
}

bool vr_update_mpq82d00::readUserCrc(uint16_t& crc)
{
    /* User CRC is a 2-byte read of C8h on page 0 */
    if (!selectPage(MPQ82D00_PAGE0))
    {
        return false;
    }

    int value = i2c_smbus_read_word_data(fd, MPQ82D00_USER_CRC_REG);
    if (value < SUCCESS)
    {
        sd_journal_print(LOG_ERR,
                         "Error: Failed to read user CRC at page0 C8h\n");
        return false;
    }

    crc = static_cast<uint16_t>(value);
    return true;
}

bool vr_update_mpq82d00::readFileConfigId(uint16_t& configId)
{
    std::ifstream cFile(ConfigFilePath);
    if (!cFile.is_open())
    {
        sd_journal_print(LOG_ERR, "Error: Failed to open config file\n");
        return false;
    }

    std::string line;
    while (std::getline(cFile, line))
    {
        stripCarriageReturn(line);
        if (line.empty() || line.compare(0, INDEX_3, "END") == SUCCESS)
        {
            continue;
        }

        std::istringstream iss(line);
        std::string idText;
        if (!(iss >> idText))
        {
            continue;
        }

        try
        {
            unsigned long id = std::stoul(idText, nullptr, BASE_16);
            if (id > 0xFFFF)
            {
                sd_journal_print(LOG_ERR,
                                 "Error: Configuration ID 0x%lx is not a "
                                 "2-byte value\n",
                                 id);
                return false;
            }
            configId = static_cast<uint16_t>(id);
            return true;
        }
        catch (const std::exception&)
        {
            sd_journal_print(LOG_ERR,
                             "Error: Failed to parse configuration ID\n");
            return false;
        }
    }

    sd_journal_print(LOG_ERR,
                     "Error: Configuration ID missing from config file\n");
    return false;
}

bool vr_update_mpq82d00::readFileUserCrc(uint16_t& userCrc)
{
    std::ifstream cFile(ConfigFilePath);
    if (!cFile.is_open())
    {
        sd_journal_print(LOG_ERR, "Error: Failed to open config file\n");
        return false;
    }

    std::string line;
    while (std::getline(cFile, line))
    {
        stripCarriageReturn(line);
        if (line.find("CRC_USER") == std::string::npos)
        {
            continue;
        }

        std::istringstream iss(line);
        std::string col[MPQ82D00_ATE_COLUMNS];
        for (int i = 0; i < MPQ82D00_ATE_COLUMNS; i++)
        {
            if (!(iss >> col[i]))
            {
                sd_journal_print(LOG_ERR,
                                 "Error: CRC_USER line is not ATE format\n");
                return false;
            }
        }

        try
        {
            unsigned long crc = std::stoul(col[INDEX_5], nullptr, BASE_16);
            if (crc > 0xFFFF)
            {
                sd_journal_print(LOG_ERR,
                                 "Error: CRC_USER 0x%lx is not a 2-byte "
                                 "value\n",
                                 crc);
                return false;
            }
            userCrc = static_cast<uint16_t>(crc);
            return true;
        }
        catch (const std::exception&)
        {
            sd_journal_print(LOG_ERR, "Error: Failed to parse CRC_USER\n");
            return false;
        }
    }

    sd_journal_print(LOG_ERR, "Error: CRC_USER missing from config file\n");
    return false;
}

bool vr_update_mpq82d00::crcCheckSum()
{
    uint16_t deviceCrc = 0;

    if (!readUserCrc(deviceCrc))
    {
        CrcMatched = false;
        return false;
    }

    sd_journal_print(LOG_INFO, "CRC from the device = 0x%x\n", deviceCrc);
    sd_journal_print(LOG_INFO, "CRC from the manifest file = 0x%x\n", Crc);

    if (static_cast<uint32_t>(deviceCrc) == Crc)
    {
        sd_journal_print(
            LOG_ERR, "Device CRC matches with file CRC. Skipping the update\n");
        CrcMatched = true;
        return false;
    }

    sd_journal_print(
        LOG_INFO,
        "CRC not matched with the previous image. Continuing the update\n");
    CrcMatched = false;
    return true;
}

bool vr_update_mpq82d00::isUpdatable()
{
    uint8_t rdata[I2C_SMBUS_BLOCK_MAX] = {0};
    int ret = FAILURE;

    /* PMBus address ACK, then select page 0 for the ID registers */
    if (!selectPage(MPQ82D00_PAGE0))
    {
        sd_journal_print(LOG_ERR, "Fault 1: PMBus address not acknowledged\n");
        return false;
    }

    /* Vendor ID: page 0, 99h block, 3 bytes, 0x53504D ("MPS") */
    ret = i2c_smbus_read_block_data(fd, MPQ82D00_VENDOR_ID_REG, rdata);
    if (ret != MPQ82D00_VENDOR_ID_LEN)
    {
        sd_journal_print(LOG_ERR,
                         "Fault 1: Failed to read vendor ID from page0 99h\n");
        return false;
    }

    uint32_t vendorId = rdata[INDEX_0] | (rdata[INDEX_1] << SHIFT_8) |
                        (rdata[INDEX_2] << SHIFT_16);
    if (vendorId != MPQ82D00_VENDOR_ID)
    {
        sd_journal_print(LOG_ERR,
                         "Fault 1: Vendor ID 0x%x mismatched. Expected "
                         "0x53504D\n",
                         vendorId);
        return false;
    }
    sd_journal_print(LOG_INFO, "Vendor ID 0x53504D matched\n");

    /* Device ID: page 0, 9Ah block, 8 bytes, ASCII "MPQ82D00" */
    ret = i2c_smbus_read_block_data(fd, MPQ82D00_DEVICE_ID_REG, rdata);
    if (ret != MPQ82D00_DEVICE_ID_LEN)
    {
        sd_journal_print(LOG_ERR,
                         "Fault 1: Failed to read device ID from page0 9Ah\n");
        return false;
    }

    if (std::memcmp(rdata, MPQ82D00_DEVICE_ID, MPQ82D00_DEVICE_ID_LEN) !=
        SUCCESS)
    {
        sd_journal_print(LOG_ERR,
                         "Fault 1: Device ID mismatched. Expected MPQ82D00\n");
        return false;
    }
    sd_journal_print(LOG_INFO, "Device ID MPQ82D00 matched\n");

    /* Configuration ID: page 0, B5h, 2-byte read */
    int deviceConfig = i2c_smbus_read_word_data(fd, MPQ82D00_CONFIG_ID_REG);
    if (deviceConfig < SUCCESS)
    {
        sd_journal_print(
            LOG_ERR, "Fault 1: Failed to read configuration ID at page0 B5h\n");
        return false;
    }

    uint16_t fileConfigId = 0;
    if (!readFileConfigId(fileConfigId))
    {
        return false;
    }

    if (static_cast<uint16_t>(deviceConfig) != fileConfigId)
    {
        sd_journal_print(LOG_ERR,
                         "Fault 1: Configuration ID mismatch. Device 0x%x "
                         "file 0x%x\n",
                         deviceConfig, fileConfigId);
        return false;
    }
    sd_journal_print(LOG_INFO, "Configuration ID 0x%x matched\n", fileConfigId);

    return true;
}

bool vr_update_mpq82d00::writeConfigRegisters()
{
    std::ifstream cFile(ConfigFilePath);
    if (!cFile.is_open())
    {
        sd_journal_print(LOG_ERR, "Error: Failed to open config file\n");
        return false;
    }

    std::string line;
    int currentPage = MPQ82D00_PAGE0;

    while (std::getline(cFile, line))
    {
        stripCarriageReturn(line);
        if (line.empty())
        {
            continue;
        }
        if (line.compare(0, INDEX_3, "END") == SUCCESS)
        {
            break;
        }

        std::istringstream iss(line);
        std::string col[MPQ82D00_ATE_COLUMNS];
        bool complete = true;
        for (int i = 0; i < MPQ82D00_ATE_COLUMNS; i++)
        {
            if (!(iss >> col[i]))
            {
                complete = false;
                break;
            }
        }
        if (!complete)
        {
            sd_journal_print(LOG_ERR,
                             "Error: Invalid configuration file line\n");
            return false;
        }

        uint16_t page = 0;
        uint16_t reg = 0;
        uint32_t data = 0;
        uint16_t nbytes = 0;
        try
        {
            page = static_cast<uint16_t>(
                std::stoul(col[INDEX_1], nullptr, BASE_16));
            reg = static_cast<uint16_t>(
                std::stoul(col[INDEX_2], nullptr, BASE_16));
            data = static_cast<uint32_t>(
                std::stoul(col[INDEX_5], nullptr, BASE_16));
            nbytes =
                static_cast<uint16_t>(std::stoul(col[INDEX_7], nullptr, 10));
        }
        catch (const std::exception&)
        {
            sd_journal_print(LOG_ERR,
                             "Error: Failed to parse configuration file "
                             "line\n");
            return false;
        }

        if (page != currentPage)
        {
            if (!selectPage(static_cast<uint8_t>(page)))
            {
                return false;
            }
            currentPage = page;
        }

        int ret = FAILURE;
        if (nbytes == MPQ82D00_BYTE_COUNT)
        {
            ret = i2c_smbus_write_byte_data(fd, static_cast<uint8_t>(reg),
                                            static_cast<uint8_t>(data));
        }
        else if (nbytes == MPQ82D00_WORD_COUNT)
        {
            ret = i2c_smbus_write_word_data(fd, static_cast<uint8_t>(reg),
                                            static_cast<uint16_t>(data));
        }
        else
        {
            sd_journal_print(LOG_ERR,
                             "Error: Unsupported byte count %u for register "
                             "0x%x\n",
                             nbytes, reg);
            return false;
        }

        if (ret < SUCCESS)
        {
            sd_journal_print(LOG_ERR,
                             "Error: Writing register 0x%x on page 0x%x "
                             "failed\n",
                             reg, page);
            return false;
        }
    }

    return true;
}

int vr_update_mpq82d00::configureOnce(uint16_t expectedCrc)
{
    /* Turn to page 0, then write every register in the ATE file */
    if (!selectPage(MPQ82D00_PAGE0))
    {
        return MPQ82D00_CFG_FAILED;
    }

    if (!writeConfigRegisters())
    {
        return MPQ82D00_CFG_FAILED;
    }

    /* STORE_ALL is a send-byte command (15h) */
    int ret = i2c_smbus_write_byte(fd, MPQ82D00_STORE_ALL);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: STORE_ALL command 15h failed\n");
        return MPQ82D00_CFG_FAILED;
    }
    sd_journal_print(LOG_INFO, "Sent STORE_ALL command 15h\n");

    usleep(MPQ82D00_STORE_WAIT_US);

    uint16_t deviceCrc = 0;
    if (!readUserCrc(deviceCrc))
    {
        return MPQ82D00_CFG_FAILED;
    }

    sd_journal_print(LOG_INFO, "User CRC at page0 C8h = 0x%x, expected 0x%x\n",
                     deviceCrc, expectedCrc);

    if (deviceCrc == expectedCrc)
    {
        return MPQ82D00_CFG_OK;
    }
    return MPQ82D00_CFG_CRC_MISMATCH;
}

bool vr_update_mpq82d00::UpdateFirmware()
{
    uint16_t expectedCrc = 0;
    if (!readFileUserCrc(expectedCrc))
    {
        return false;
    }

    int result = configureOnce(expectedCrc);
    if (result == MPQ82D00_CFG_OK)
    {
        return true;
    }
    if (result == MPQ82D00_CFG_FAILED)
    {
        return false;
    }

    /* CRC mismatch on the first attempt: repeat from writing the registers */
    sd_journal_print(LOG_ERR,
                     "User CRC mismatch. First time failing, repeating "
                     "configuration\n");

    result = configureOnce(expectedCrc);
    if (result == MPQ82D00_CFG_OK)
    {
        return true;
    }
    if (result == MPQ82D00_CFG_CRC_MISMATCH)
    {
        sd_journal_print(
            LOG_ERR,
            "Fault 2: device failed to store the correct configuration to "
            "the MTP\n");
    }
    return false;
}

bool vr_update_mpq82d00::ValidateFirmware()
{
    uint16_t expectedCrc = 0;
    uint16_t deviceCrc = 0;

    if (!readFileUserCrc(expectedCrc))
    {
        return false;
    }
    if (!readUserCrc(deviceCrc))
    {
        return false;
    }

    if (deviceCrc != expectedCrc)
    {
        sd_journal_print(LOG_ERR,
                         "User CRC 0x%x did not match expected checksum "
                         "0x%x\n",
                         deviceCrc, expectedCrc);
        return false;
    }

    sd_journal_print(LOG_INFO,
                     "User CRC 0x%x matched the config file checksum\n",
                     deviceCrc);
    return true;
}
