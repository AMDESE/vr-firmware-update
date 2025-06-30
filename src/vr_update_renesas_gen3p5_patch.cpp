/*
 * vr_update_renesas_gen3p5_patch.cpp
 *
 * Created on: Apr 5, 2025
 * Author: Abinaya Dhandapani
 */

#include "vr_update.hpp"
#include "vr_update_renesas_gen3p5_patch.hpp"

struct PMBusPayload
{
    char flag;
    u_int8_t command;
    u_int32_t data;
};

vr_update_renesas_gen3p5_patch::vr_update_renesas_gen3p5_patch(
    std::string Processor, uint32_t Crc, std::string Model,
    uint16_t SlaveAddress, std::string ConfigFilePath, std::string Revision,
    uint16_t PmbusAddress, std::vector<std::string>& configFilePathArr) :
    vr_update(Processor, Crc, Model, SlaveAddress, ConfigFilePath, Revision,
              PmbusAddress),
    configFilePathArr(configFilePathArr)
{
    DriverPath = RAA_DRIVER_PATH;
}

bool vr_update_renesas_gen3p5_patch::crcCheckSum()
{
    CrcMatched = false;
    return true;
}

bool vr_update_renesas_gen3p5_patch::isUpdatable()
{
    u_int8_t rdata[MAXIMUM_SIZE] = {0};
    int length;
    int ret = 0;
    u_int32_t DeviceFw = 0;

    /*Read Device ID*/
    std::fill_n(rdata, MAXIMUM_SIZE, 0);
    u_int32_t VrDeviceId = 0;

    ret = i2c_smbus_read_i2c_block_data(fd, DEV_ID_CMD, BYTE_COUNT_5, rdata);

    if (ret >= SUCCESS)
    {
        VrDeviceId = (rdata[INDEX_4] << SHIFT_24) |
                     (rdata[INDEX_3] << SHIFT_16) |
                     (rdata[INDEX_2] << SHIFT_8) | rdata[INDEX_1];

        sd_journal_print(LOG_INFO, "Device ID from the VR = 0x%x\n",
                         VrDeviceId);
    }
    else
    {
        sd_journal_print(LOG_ERR,
                         "Failed to read device ID from the VR device\n");
        return false;
    }

    if ((VrDeviceId == DEV_ID_1) || (VrDeviceId == DEV_ID_2))
    {
        sd_journal_print(LOG_INFO,
                         "The device is compatible for Gen3.5 patch update\n");
    }
    else
    {
        sd_journal_print(
            LOG_ERR, "The device is not compatible for Gen3.5 patch update\n");
        return false;
    }

    bool fileFound = false;

    /*Read Device firmware version*/
    ret = i2c_smbus_write_word_data(fd, DMA_WRITE, DEVICE_FW_VERSION);
    if (ret == SUCCESS)
    {
        ret = i2c_smbus_read_i2c_block_data(fd, DMA_READ, BYTE_COUNT_4, rdata);

        if (ret >= SUCCESS)
        {
            DeviceFw = (rdata[INDEX_3] << SHIFT_24) |
                       (rdata[INDEX_2] << SHIFT_16) |
                       (rdata[INDEX_1] << SHIFT_8) | rdata[INDEX_0];

            std::ofstream patchFile;
            patchFile.open(PATCH_VERSION_FILE, std::ios::app);
            sd_journal_print(LOG_INFO,
                             "Device firmware version before update = 0x%x\n",
                             DeviceFw);

            patchFile << BusNumber << ",0x" << std::hex << SlaveAddress << ",0x"
                      << std::hex << DeviceFw;

            devVersion = DeviceFw;

            if ((DeviceFw == PATCH_FW_1) || (DeviceFw == PATCH_FW_2))
            {
                for (const auto& filePath : configFilePathArr)
                {
                    if (filePath.find("patch_2_0_1_2.txt") != std::string::npos)
                    {
                        ConfigFilePath =
                            filePath; // Assign the matching file path
                        fileFound = true;
                        break;
                    }
                }
            }
            else if (DeviceFw == PATCH_FW_3)
            {
                for (const auto& filePath : configFilePathArr)
                {
                    if (filePath.find("patch_2_0_3_0.txt") != std::string::npos)
                    {
                        ConfigFilePath =
                            filePath; // Assign the matching file path
                        fileFound = true;
                        break;
                    }
                }
            }
            else if (DeviceFw == PATCH_FW_4)
            {
                for (const auto& filePath : configFilePathArr)
                {
                    if (filePath.find("patch_2_0_2_0.txt") != std::string::npos)
                    {
                        ConfigFilePath =
                            filePath; // Assign the matching file path
                        fileFound = true;
                        break;
                    }
                }
            }
            else
            {
                sd_journal_print(
                    LOG_ERR,
                    "The FW version is not supported for patch update\n");
                patchFile << ",NotUpdated" << std::endl;
                patchFile.close();
                return false;
            }
            patchFile.close();
        }
        else
        {
            sd_journal_print(LOG_ERR, "Read to DMA Address Register failed\n");
            return false;
        }
    }
    else
    {
        sd_journal_print(LOG_ERR, "Write to DMA Address Register failed\n");
        return false;
    }

    if (fileFound == false)
    {
        sd_journal_print(
            LOG_ERR,
            " Valid patch file is not found in the tar file. Aborting the update\n");
        return false;
    }

    sd_journal_print(LOG_INFO, "Updating the patch file %s\n",
                     ConfigFilePath.c_str());
    return true;
}

bool vr_update_renesas_gen3p5_patch::UpdateFirmware()
{
    int ret = 0;

    /*Halt device firmware*/
    ret = i2c_smbus_write_word_data(fd, FW_WRITE, HALT_FW);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR,
                         "halt device fw:Setting DMA register failed\n");
        return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "Firmware halted successfully\n");
    }

    usleep(SLEEP_1000);

    /*Write patch data to the device*/
    std::string command;
    std::fstream newfile;

    newfile.open(ConfigFilePath, std::ios::in);

    if (newfile.is_open())
    {
        std::string HexfileData;
        // Read hex file line by line
        while (getline(newfile, HexfileData))
        {
            struct PMBusPayload pmbus_payload;
            std::string data;
            int len = HexfileData.size();
            pmbus_payload.flag = HexfileData[INDEX_0];

            /* "w" in the hex file indicates that
            the line should be written to the hw*/
            // if(pmbus_payload.flag != 'w')
            //     continue;

            /*Extract command code */
            command = HexfileData.substr(INDEX_2, INDEX_2);
            pmbus_payload.command = std::stoul(command, nullptr, BASE_16);

            /*Extract pmbus data*/
            data = HexfileData.substr(INDEX_5, len - INDEX_5);
            pmbus_payload.data = std::stoul(data, nullptr, BASE_16);

            if (pmbus_payload.command == CMD_CODE_C6)
            {
                u_int8_t write_pmbus_data[INDEX_4] = {0};
                write_pmbus_data[INDEX_3] =
                    ((pmbus_payload.data & MASK_BYTE_4) >> SHIFT_24);
                write_pmbus_data[INDEX_2] =
                    ((pmbus_payload.data & MASK_BYTE_3) >> SHIFT_16);
                write_pmbus_data[INDEX_1] =
                    ((pmbus_payload.data & MASK_BYTE_2) >> SHIFT_8);
                write_pmbus_data[INDEX_0] = (pmbus_payload.data & INT_255);

                ret = i2c_smbus_write_i2c_block_data(
                    fd, pmbus_payload.command, BYTE_COUNT_4, write_pmbus_data);
                if (ret < SUCCESS)
                {
                    sd_journal_print(
                        LOG_ERR, "Writing block data to the device failed\n");
                    return false;
                }
                else
                {
                    sd_journal_print(
                        LOG_INFO,
                        "Block write successful : Command = 0x%x data0 = 0x%x"
                        "data1 = 0x%x data2 = 0x%x data3 = 0x%x\n",
                        pmbus_payload.command, write_pmbus_data[INDEX_0],
                        write_pmbus_data[INDEX_1], write_pmbus_data[INDEX_2],
                        write_pmbus_data[INDEX_3]);
                }
            }
            else
            {
                u_int16_t write_pmbus_data;

                if (pmbus_payload.command == CMD_CODE_C7)
                {
                    write_pmbus_data = pmbus_payload.data & MASK_TWO_BYTES;
                }
                else if (pmbus_payload.command == CMD_CODE_E6)
                {
                    write_pmbus_data = (u_int16_t)pmbus_payload.data;
                }

                ret = i2c_smbus_write_word_data(fd, pmbus_payload.command,
                                                write_pmbus_data);
                if (ret < SUCCESS)
                {
                    sd_journal_print(
                        LOG_ERR,
                        "Writing word data to the device failed. Command = 0x%x Word = 0x%x\n",
                        pmbus_payload.command, write_pmbus_data);
                    ;
                    return false;
                }
                else
                {
                    sd_journal_print(
                        LOG_INFO,
                        "Write word successful: Command = 0x%x Word = 0x%x\n",
                        pmbus_payload.command, write_pmbus_data);
                }
            }
        }
        newfile.close();
    }
    else
    {
        sd_journal_print(LOG_ERR, "Opening config file failed\n");
        return false;
    }

    /*Commit patch to NVM*/

    if (i2c_smbus_write_word_data(fd, CMD_CODE_E6, COMMIT_DATA) == SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Commit patch to NVM successful\n");
        return true;
    }
    else
    {
        sd_journal_print(LOG_INFO,
                         "Commit patch to NVM : Setting DMA register failed\n");
        return false;
    }
}

bool vr_update_renesas_gen3p5_patch::ValidateFirmware()
{
    u_int8_t rdata[MAXIMUM_SIZE] = {0};
    int length, timeout = 0, ret = 0, status = FAILURE;

    usleep(SLEEP_1);

    // Poll PROGRAMMER_STATUS Register
    while (timeout < MAX_RETRY)
    {
        timeout++;
        usleep(SLEEP_1000);

        ret = i2c_smbus_write_word_data(fd, DMA_WRITE, POLL_REG);

        if (ret == SUCCESS)
        {
            ret = i2c_smbus_read_i2c_block_data(fd, DMA_READ, BYTE_COUNT_4,
                                                rdata);
            if (ret >= SUCCESS)
            {
                if ((rdata[INDEX_3] & COMPLETE_BIT))
                {
                    if ((rdata[INDEX_3] & PASS_BIT))
                    {
                        status = SUCCESS;
                        sd_journal_print(LOG_INFO, "Patch update Succeeded\n");
                        break;
                    }
                    else
                    {
                        status = FAILURE;
                        break;
                    }
                }
                else
                {
                    status = FAILURE;
                }
            }
            else
            {
                sd_journal_print(
                    LOG_ERR,
                    "Poll programmer status register: Read DMA register failed\n");
                return false;
            }
        }
        else
        {
            sd_journal_print(
                LOG_ERR,
                "Poll programmer status register: Setting DMA register address failed\n");
            return false;
        }
    }

    if (status == FAILURE)
    {
        sd_journal_print(LOG_ERR, "Patch update failed\n");
        return false;
    }
    else
    {
        uint8_t read_data[INDEX_4] = {0};
        sleep(1);

        /*Retrieve Device Data*/
        ret = i2c_smbus_write_word_data(fd, DMA_WRITE, RETRIEVE_DEV_DATA);

        if (ret == SUCCESS)
        {
            ret = i2c_smbus_read_i2c_block_data(fd, DMA_READ, BYTE_COUNT_4,
                                                read_data);

            if (ret >= SUCCESS)
            {
                // Clear bits [4:1] and set them to 0b0100

                read_data[0] = (read_data[0] & 0b11100001) | 0b00001000;
                ret = i2c_smbus_write_i2c_block_data(fd, DMA_READ, BYTE_COUNT_4,
                                                     read_data);

                if (ret < SUCCESS)
                {
                    sd_journal_print(
                        LOG_ERR, "Block write: Retrieve device data failed\n");
                    return false;
                }
                else
                {
                    sd_journal_print(
                        LOG_INFO,
                        "Block write : Retrive device data success\n");
                }
            }
            else
            {
                sd_journal_print(LOG_ERR, "DMA read of 0xECFO failed\n");
                return false;
            }
        }

        sleep(1);
    }

    /*Reload patch*/
    ret = i2c_smbus_write_word_data(fd, CMD_CODE_E6, RELOAD_PATCH);
    if (ret == SUCCESS)
    {
        sd_journal_print(LOG_INFO, "Successfully reloaded patch\n");
    }
    else
    {
        sd_journal_print(LOG_ERR, "Failed to reload patch\n");
        return false;
    }

    /*Read Patch CRC check*/
    ret = i2c_smbus_write_word_data(fd, DMA_WRITE, PATCH_CRC_CHECK);

    uint8_t read_data[INDEX_4] = {0};

    if (ret == SUCCESS)
    {
        ret = i2c_smbus_read_i2c_block_data(fd, DMA_READ, BYTE_COUNT_4,
                                            read_data);

        if (ret >= SUCCESS)
        {
            if (!(read_data[INDEX_0] & 0b10000000))
            {
                sd_journal_print(LOG_INFO,
                                 "Bit 7 of patch CRC check register is 0\n");
            }
            else
            {
                sd_journal_print(
                    LOG_ERR,
                    "Bit 7 of patch CRC check register is 1. Patch update failed. Part is unusable\n");
                return false;
            }
        }
        else
        {
            sd_journal_print(LOG_ERR, "DMA read of 0xEC02 failed\n");
            return false;
        }
    }

    /*Read Patch Status check*/
    ret = i2c_smbus_write_word_data(fd, DMA_WRITE, PATCH_CRC_STATUS);

    if (ret == SUCCESS)
    {
        ret = i2c_smbus_read_i2c_block_data(fd, DMA_READ, BYTE_COUNT_4,
                                            read_data);

        if (ret >= SUCCESS)
        {
            if (read_data[INDEX_0] & 0b00011000)
            {
                sd_journal_print(
                    LOG_ERR,
                    "Bits[3,4] of patch CRC status register is 1. Patch update failed. Part is unusable\n");
                return false;
            }
        }
        else
        {
            sd_journal_print(LOG_ERR, "DMA read of 0x00C2 failed\n");
            return false;
        }
    }

    u_int32_t DeviceFw = 0;

    /*Read Device firmware version*/
    ret = i2c_smbus_write_word_data(fd, DMA_WRITE, DEVICE_FW_VERSION);
    if (ret == SUCCESS)
    {
        ret = i2c_smbus_read_i2c_block_data(fd, DMA_READ, BYTE_COUNT_4, rdata);

        if (ret >= SUCCESS)
        {
            DeviceFw = (rdata[INDEX_3] << SHIFT_24) |
                       (rdata[INDEX_2] << SHIFT_16) |
                       (rdata[INDEX_1] << SHIFT_8) | rdata[INDEX_0];

            sd_journal_print(LOG_INFO,
                             "Device firmware version after update = 0x%x\n",
                             DeviceFw);

            std::ofstream patchFile;
            patchFile.open(PATCH_VERSION_FILE, std::ios::app);
            patchFile << ",0x" << std::hex << DeviceFw << std::endl;
            patchFile.close();

            devVersion = DeviceFw;
        }
    }
    return true;
}
