/*
* vr_update_mp2869.cpp
*
* Created on: Sep 18, 2024
* Author: Abinaya Dhandapani
*/

#include <thread>
#include <chrono>
#include "vr_update.hpp"
#include "vr_update_mp2869.hpp"

vr_update_mp2869::vr_update_mp2869(std::string Processor,uint32_t Crc,
        std::string Model,uint16_t SlaveAddress,std::string ConfigFilePath,std::string Revision,uint16_t PmbusAddress):
        vr_update(Processor,Crc,Model,SlaveAddress,ConfigFilePath,Revision,PmbusAddress)
{
        DriverPath = MPS2856_DRIVER_PATH;
}

bool vr_update_mp2869::crcCheckSum()
{
    uint16_t DeviceCrc;
    int ret = FAILURE;

    /*Set page number to 0*/
    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_0);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
        return false;
    }

    DeviceCrc = i2c_smbus_read_word_data(fd,USER_CRC_REG);

    if (DeviceCrc < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read 0xED register\n");
        return false;
    }

    if(DeviceCrc == Crc)
    {
       sd_journal_print(LOG_ERR, "Device CRC matches with file CRC. Skipping the update\n");
       CrcMatched = true;
       return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "CRC not matched with the previous image. Continuing the update\n");
        CrcMatched = false;
        return true;
    }

    return true;
}


bool vr_update_mp2869::isUpdatable()
{
    uint8_t rdata[MAXIMUM_SIZE] = {0};
    bool rc = FAILURE;
    uint32_t VrVendorId = 0;
    uint16_t FileVendorId = 0;
    uint16_t VrDeviceId = 0;
    uint16_t FileDeviceId = 0;
    uint16_t VrConfigId = 0;
    uint16_t FileConfigId = 0;
    int ret = FAILURE;
    std::string line;

    /*Vendor ID from 99h@page0*/
    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_0);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
        return false;
    }

    ret = i2c_smbus_read_i2c_block_data(fd, VENDOR_ID_REG, BYTE_COUNT_4, rdata);

    if(ret >= SUCCESS)
    {
        VrVendorId = (rdata[INDEX_3] << SHIFT_16) | (rdata[INDEX_2] << SHIFT_8) | rdata[INDEX_1];
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read Vendor ID from the VR device\n");
        return false;
    }

    if(VrVendorId == VENDOR_ID)
    {
        sd_journal_print(LOG_INFO, "Vendor ID 0x4D5053 matched\n");
    }
    else
    {
        sd_journal_print(LOG_ERR, "Vendor ID 0x%x mismatched. Aborting the update\n",VrVendorId);
        return false;
    }

    /*Device ID from ADh@page0*/
    ret = i2c_smbus_read_i2c_block_data(fd, DEVICE_ID_REG, BYTE_COUNT_4, rdata);

    if(ret >= SUCCESS)
    {
        VrDeviceId = ((uint16_t)rdata[INDEX_2] << SHIFT_8) | rdata[INDEX_1];
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read Device ID from the VR device\n");
        return false;
    }

    if((VrDeviceId == PRODUCT_ID) || (VrDeviceId == PRODUCT_ID_1))
    {
        sd_journal_print(LOG_INFO, "Device Id matched\n");
    }
    else
    {
        sd_journal_print(LOG_ERR, "Device ID = 0x%x mismatched. Aborting the update\n",VrDeviceId);
        return false;
    }

    /*Config ID from 9Eh@page0*/
    ret = i2c_smbus_read_i2c_block_data(fd, CONFIG_ID_REG, BYTE_COUNT_4, rdata);

    if(ret >= SUCCESS)
    {
        VrConfigId = ((uint16_t)rdata[INDEX_1] << SHIFT_8) | rdata[INDEX_0];
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read Config ID from the VR device\n");
        return false;
    }

    //Read config file for same values - convert string to hex here

    /*Read device id from the config file*/
    std::ifstream cFile(ConfigFilePath);

    if (cFile.is_open())
    {
        while(getline(cFile, line))
        {
             if(line.find(FILE_CONFIG_ID) != std::string::npos){
                  std::stringstream ss;
                  size_t pos = line.find(FILE_CONFIG_ID);
                  std::string Id = line.substr(pos + sizeof(FILE_CONFIG_ID), INDEX_4);
                  ss<<Id;
                  ss>>std::hex>>FileConfigId;
                  break;
           }
        }
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to open config file\n");
        return false;
    }

     if((VrConfigId == FileConfigId))
    {
        sd_journal_print(LOG_INFO, "Config ID matched\n");
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Config ID mismatch. Update failed 0x%x 0x%x\n",VrConfigId,FileConfigId);
        cFile.close();
        return false;
    }

    return true;
}

bool vr_update_mp2869::UpdateFirmware()
{
    int ret = FAILURE;
    bool rc = FAILURE;
    std::string line;
    uint16_t Page = 0;
    uint16_t Register = 0;
    uint32_t Data = 0;
    uint16_t Mode = 0;
    std::string text_data[TOTAL_COLUMNS] ;
    uint16_t current_page=-1;
    uint8_t rdata[BYTE_COUNT_4] = { 0 };
    uint8_t ByteData = 0;
    uint16_t word_data = 0;

    /*Password unlock*/

    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_1);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
        return false;
    }

    word_data = i2c_smbus_read_word_data(fd,MFR_PWD_USER);

    if (word_data < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read password register\n");
        return false;
    }

    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_0);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
        return false;
    }

    ret = i2c_smbus_write_word_data(fd, CMD_F7, word_data);

    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Writing password to address 0xF7 failed\n");
        return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "Written password %d to page0@F7h\n",word_data);
    }

    /*Set MFR_WRITE_PROTECT in page0@10h = 00h*/
    ret = i2c_smbus_write_byte_data(fd, UNLOCK_PROTECT, DATA_0);

    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Unlocking memory protection failed\n");
        return false;
    }

    sleep(1);

    std::ifstream cFile(ConfigFilePath);

    if (!cFile.is_open())
    {
        sd_journal_print(LOG_ERR, "Error: Failed to open config file\n");
        return FAILURE;
    }

    while(getline(cFile,line))
    {
        std::istringstream iss(line);
        std::stringstream ss;
        if (line.substr(INDEX_0,INDEX_3) == END_OF_REG){
            break;
        }

        for(int i=0; i<TOTAL_COLUMNS;i++)
        {
            iss >> text_data[i];

            if(i == INDEX_1)
            {
                ss << std::hex << text_data[i];
                ss >> Page;
                ss.clear();

                if(Page >= INT_16 && Page < INT_32)
                {
                    Page = Page & INT_15;
                }
                else if(Page >= INT_32)
                {
                    break;
                }
                if(current_page != Page)
                {
                    current_page = Page;

					sd_journal_print(LOG_DEBUG,"Setting current page number to %x\n",current_page);

                    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, current_page);

                    if (ret < SUCCESS)
                    {
                        sd_journal_print(LOG_ERR, "Error: Writing byte data to the device failed\n");
                        cFile.close();
                        return false;
                    }
                }
            }
            else if(i == INDEX_2)
            {
                ss << std::hex << text_data[i];
                ss >> Register;
                ss.clear();
            }

            else if(i == INDEX_5)
            {
               ss << text_data[i];
               ss >> Data;
               ss.clear();
            }

            else if(i == INDEX_7)
            {
                ss << text_data[i];
                ss >> Mode;
                ss.clear();
            }
        }

        if(Page >= INT_32)
            continue;

        if(Mode == BLOCK_1)
        {
            uint8_t rxdata = (uint8_t)Data;
            ret = i2c_smbus_write_byte_data(fd, Register, rxdata);
            if (ret < SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Writing byte data to the device failed\n");
                cFile.close();
                return false;
            }
            else
            {
                sd_journal_print(LOG_DEBUG, "Byte write : Page = 0x%x Register = 0x%x , data = 0x%x\n",current_page,Register,rxdata);
            }
        }

        else if(Mode == BLOCK_2)
        {
            uint16_t rxdata = (uint16_t)Data;
            ret = i2c_smbus_write_word_data(fd, Register, rxdata);
            if (ret < SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Writing word data to the device failed\n");
                cFile.close();
                return false;
            }
            else
            {
                sd_journal_print(LOG_DEBUG, "Byte write : Page = 0x%x Register = 0x%x , data = 0x%x\n",current_page,Register,rxdata);
            }
        }
        else
        {
            sd_journal_print(LOG_ERR, "Error: Invalid Mode: %d for Register: %x \n", Mode, Register);
            cFile.close();
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(INDEX_2));
    }

    /*Write Page1@CCh Bit[0]=1,Bit[5]=1,Bit[7]=0,Bit[12]=0 keep other Bits*/
    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_1);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
        return false;
    }

    word_data = i2c_smbus_read_word_data(fd,CMD_CC);

    if (word_data < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read 0xCC register\n");
        return false;
    }

    uint16_t mask = (INDEX_1 << INDEX_0) | (INDEX_1 << INDEX_5);
    word_data = (word_data & ~mask) | mask;
    
    mask = (INDEX_1 << INDEX_7) | (INDEX_1 << INDEX_12);
    word_data = word_data & ~mask;

    ret = i2c_smbus_write_word_data(fd, CMD_CC, word_data);

    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Writing to page1@CCh failed\n");
        return false;
    }
    else
    {
        sd_journal_print(LOG_INFO,"written 0x%x to page1@cch register \n",word_data);
    }

    /*Send no byte command 17h to store data into MTP*/
    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_0);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
        return false;
    }

    ret = i2c_smbus_write_byte(fd, CMD_17);

    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Unlocking memory protection failed\n");
        return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "Successfully sent no byte command to page0@17h \n");
    }

    sleep(1);
    return true;
}

bool vr_update_mp2869::ValidateFirmware()
{
    uint16_t DeviceCrc;
    int ret = FAILURE;

    std::string line;
    uint16_t UserCrc = 0;

    std::ifstream cFile(ConfigFilePath);

    if (cFile.is_open())
    {
        while(getline(cFile, line))
        {
             if(line.find(CRC_USER) != std::string::npos)
             {
                  std::stringstream ss;
                  size_t pos = line.find(CRC_USER);
                  std::string Id = line.substr(pos + sizeof(CRC_USER), INDEX_4);
                  ss<<Id;
                  ss>>std::hex>>UserCrc;
                  break;
             }
        }
    }

    /*Set page number to 0*/
    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_0);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
        return false;
    }

    DeviceCrc = i2c_smbus_read_word_data(fd,USER_CRC_REG);

    if (DeviceCrc < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read 0xED register\n");
        return false;
    }

    sd_journal_print(LOG_INFO, "CRC at page0@EDh = 0x%x\n",DeviceCrc);

    if(DeviceCrc == UserCrc)
    {
       sd_journal_print(LOG_INFO, "CRC at page0@EDh matched with the Config file CRC\n"); 
    }
    else
    {
       sd_journal_print(LOG_ERR, "CRC at page0@EDh did not match with the Config file CRC. Aborting the update\n");
       return false;
    }

    ret = i2c_smbus_write_byte(fd, CMD_18);

    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Sending no byte command to page0@18h failed\n");
        return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "Successfully sent no byte command to page0@18h \n");
    }

    return true; 
}
