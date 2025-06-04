#include "vr_update_fan2510xx.hpp"

#include "vr_update.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <thread>

struct RegisterData
{
    int bits;
    int bytes;
    int lsb_start;
    int data;
    std::string name;
};

vr_update_fan2510xx::vr_update_fan2510xx(
    std::string Processor, uint32_t Crc, std::string Model,
    uint16_t SlaveAddress, std::string ConfigFilePath, std::string Revision,
    uint16_t PmbusAddress) :
    vr_update(Processor, Crc, Model, SlaveAddress, ConfigFilePath, Revision,
              PmbusAddress)
{
    DriverPath = FAN251030_DRIVER_PATH;
}

bool vr_update_fan2510xx::crcCheckSum()
{
    uint16_t DeviceCrc;
    int ret = FAILURE;

    DeviceCrc = i2c_smbus_read_word_data(fd, USER_CRC_REG);

    if (DeviceCrc < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read 0xCA register\n");
        return false;
    }

    if (DeviceCrc == Crc)
    {
        sd_journal_print(
            LOG_ERR, "Device CRC matches with file CRC. Skipping the update\n");
        CrcMatched = true;
        return false;
    }
    else
    {
        sd_journal_print(
            LOG_INFO,
            "CRC not matched with the previous image. Continuing the update\n");
        CrcMatched = false;
        return true;
    }

    return true;
}

bool vr_update_fan2510xx::isUpdatable()
{
    return true;
}

bool vr_update_fan2510xx::UpdateFirmware()
{
    std::ifstream cFile(ConfigFilePath);
    int ret = FAILURE;

    if (!cFile.is_open())
    {
        sd_journal_print(LOG_ERR, "Error: Failed to open config file\n");
        return false;
    }

    std::string line;
    std::map<int, std::vector<RegisterData>> registersData;

    bool isFirstLine = true; // Flag to track the first line

    while (std::getline(cFile, line))
    {
        if (isFirstLine)
        {
            isFirstLine = false; // Skip the first line
            continue;            // Skip this iteration and go to the next line
        }
        // Split the line into a vector of integers based on commas
        std::istringstream ss(line);
        std::string temp;
        std::vector<uint32_t> values;

        // Parse the Register name string
        std::getline(ss, temp, ',');
        RegisterData data;
        data.name = temp;

        while (std::getline(ss, temp, ','))
        {
            temp.erase(INDEX_0, temp.find_first_not_of(
                                    " \t\r\n")); // Remove leading whitespace
            temp.erase(temp.find_last_not_of(" \t\r\n") +
                       INDEX_1);                 // Remove trailing whitespace
            if (temp.empty())
            {
                break;
            }
            values.push_back(std::stoi(temp, nullptr, BASE_16));
        }

        // Create a RegisterData structure and add it to the map based on
        // register number
        data.bits = values[INDEX_1];
        data.bytes = values[INDEX_2];
        data.lsb_start = values[INDEX_3];
        data.data = values[INDEX_4];

        registersData[values[INDEX_0]].push_back(data);
    }

    // process each register
    for (auto& entry : registersData)
    {
        int register_num = entry.first;
        uint16_t registerValue = 0;
        int bitOffset = 0;

        if (entry.second[INDEX_0].name == "clear_faults" ||
            entry.second[INDEX_0].name == "restore_user_all")
        {
            ret = i2c_smbus_write_byte(fd, register_num);

            if (ret != SUCCESS)
            {
                sd_journal_print(
                    LOG_ERR,
                    "Error: Pmbus Send Byte command failed for register: %s\n",
                    entry.second[INDEX_0].name.c_str());
                return false;
            }
            else
            {
                sd_journal_print(
                    LOG_INFO, "Pmbus Send Byte successful for register: %s\n",
                    entry.second[INDEX_0].name.c_str());
                continue;
            }
        }
        if (entry.second[INDEX_0].name == "store_user_all")
        {
            continue;
        }

        // Iterate through the lines for the current register
        for (const auto& registerData : entry.second)
        {
            int bitCount = registerData.bits;
            int dataValue = registerData.data;

            // Place the bits at the appropriate offset
            for (int j = 0; j < bitCount; ++j)
            {
                if (dataValue & (INDEX_1 << j))
                {
                    registerValue |= (INDEX_1 << (bitOffset + j));
                }
            }

            bitOffset += bitCount;
        }

        // Depending on the byte size
        if (entry.second[INDEX_0].bytes == INDEX_1)
        {
            ret = i2c_smbus_write_byte_data(fd, register_num, registerValue);
            if (ret != SUCCESS)
            {
                sd_journal_print(
                    LOG_ERR,
                    "Error: Writing byte data to the device failed. Register = 0x%x data = 0x%x\n",
                    register_num, registerValue);
                return false;
            }
            else
            {
                sd_journal_print(
                    LOG_INFO,
                    "Byte write success. Register = 0x%x data = 0x%x\n",
                    register_num, registerValue);
            }
        }
        else if (entry.second[INDEX_0].bytes == INDEX_2)
        {
            ret = i2c_smbus_write_word_data(fd, register_num, registerValue);
            if (ret != SUCCESS)
            {
                sd_journal_print(
                    LOG_ERR,
                    "Error: Writing word data to the device failed. Register = 0x%x data = 0x%x\n",
                    register_num, registerValue);
                return false;
            }
            else
            {
                sd_journal_print(
                    LOG_INFO,
                    "Word write success. Register = 0x%x data = 0x%x\n",
                    register_num, registerValue);
            }
        }
    }
    usleep(SLEEP_1);
    ret = i2c_smbus_write_byte(fd, STORE_USER_ALL_CMD);

    if (ret != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Failed to send STORE_USER_ALL command");
        return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "Successfully sent STORE_USER_ALL command");
    }

    return true;
}

bool vr_update_fan2510xx::ValidateFirmware()
{
    return true;
}
