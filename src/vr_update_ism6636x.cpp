#include "vr_update_ism6636x.hpp"

#include "vr_update.hpp"
#include <chrono>
#include <thread>

vr_update_ism6636x::vr_update_ism6636x(
    std::string Processor, uint32_t Crc, std::string Model,
    uint16_t SlaveAddress, std::string ConfigFilePath, std::string Revision,
    uint16_t PmbusAddress) :
    vr_update(Processor, Crc, Model, SlaveAddress, ConfigFilePath, Revision,
              PmbusAddress)
{
    DriverPath = ISM6636X_DRIVER_PATH;
}

bool vr_update_ism6636x::crcCheckSum()
{
    return true;
}

bool vr_update_ism6636x::isUpdatable()
{
    uint8_t product_id;

    /*Read Product_ID from device*/

    product_id = i2c_smbus_read_byte_data(fd, PRODUCT_ID_REG);

    if (product_id < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read product ID\n");
        return false;
    }

    if (product_id != PRODUCT_ID)
    {
        sd_journal_print(
            LOG_ERR,
            "Error: Product ID 0x%d does not match with the device table\n",
            product_id);
        return false;
    }

    uint8_t ic_version;

    /*Read IC_Version from device*/

    ic_version = i2c_smbus_read_byte_data(fd, IC_VERSION_REG);

    if (ic_version < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to IC version\n");
        return false;
    }

    if (ic_version != IC_VERSION)
    {
        sd_journal_print(
            LOG_ERR,
            "Error: IC version 0x%d does not match with the device table\n",
            ic_version);
        return false;
    }

    uint8_t usr_ptr;

    /*User_Pointer from device*/
    usr_ptr = i2c_smbus_read_byte_data(fd, USR_PTR_CMD);

    if (usr_ptr < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read user ptr\n");
        return false;
    }

    sd_journal_print(LOG_INFO, "User pointer before update = 0x%x\n", usr_ptr);

    if (usr_ptr > USR_PTR_MAX)
    {
        sd_journal_print(
            LOG_ERR,
            "Usr Ptr greater than 0x3F. OTP cannot be burned successfully\n");
        return false;
    }

    return true;
}

bool vr_update_ism6636x::UpdateFirmware()
{
    std::ifstream cFile(ConfigFilePath);
    int ret = FAILURE;

    if (!cFile.is_open())
    {
        sd_journal_print(LOG_ERR, "Error: Failed to open config file\n");
        return false;
    }

    std::string line;

    bool isFirstLine = true; // Flag to track the first line

    while (std::getline(cFile, line))
    {
        if (isFirstLine)
        {
            isFirstLine = false; // Skip the first line
            continue;            // Skip this iteration and go to the next line
        }
        std::istringstream iss(line);

        if (line.substr(INDEX_0, INDEX_3) == "END")
        {
            break;
        }

        std::vector<std::string> values;
        std::string value;

        // Split the line by tabs or spaces
        while (iss >> value)
        {
            if (value.find("bit[") != std::string::npos)
            {
                continue; // Skip this token (bit pattern)
            }

            values.push_back(value);
        }

        uint8_t reg = static_cast<uint8_t>(std::stoi(values[3]));
        uint8_t data = static_cast<uint8_t>(std::stoi(values[6]));
        sd_journal_print(LOG_INFO, " Reg = %d value = %d\n", reg, data);

        ret = i2c_smbus_write_byte_data(fd, reg, data);

        if (ret < SUCCESS)
        {
            sd_journal_print(
                LOG_ERR, "Error: Writing data %d to the register %d failed\n",
                data, reg);
            cFile.close();
            return false;
        }
    }

    cFile.close();

    /*Enable OTP clock*/
    ret = i2c_smbus_write_byte_data(fd, USER_OTP_ON, USER_OTP_DATA);

    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Enabling OTP clock failed\n");
    }

    /*OTP program command*/
    ret = i2c_smbus_write_byte_data(fd, OTP_PROG_CMD, OTP_PROG_DATA);

    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Enabling OTP clock failed\n");
    }
    sleep(1);

    return true;
}

bool vr_update_ism6636x::ValidateFirmware()
{
    return true;
}
