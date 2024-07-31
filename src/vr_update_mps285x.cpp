#include <thread>
#include <chrono>
#include "vr_update.hpp"
#include "vr_update_mps285x.hpp"

vr_update_mps285x::vr_update_mps285x(std::string Processor,uint32_t Crc,
        std::string Model,uint16_t SlaveAddress,std::string ConfigFilePath,std::string Revision,uint16_t PmbusAddress):
        vr_update(Processor,Crc,Model,SlaveAddress,ConfigFilePath,Revision,PmbusAddress)
{
        DriverPath = MPS2857_DRIVER_PATH;

}

bool vr_update_mps285x::crcCheckSum()
{
    return true;
}


bool vr_update_mps285x::isUpdatable()
{
    uint8_t rdata[MAXIMUM_SIZE] = {0};
    bool rc = FAILURE;
    uint16_t VrVendorId = 0;
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
        VrVendorId = ((uint16_t)rdata[INDEX_2] << SHIFT_8) | rdata[INDEX_1] ;
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read Vendor ID from the VR device\n");
        return false;
    }

    /*Device ID ID from 9Ah@page0*/
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

    /*Config ID from 9Dh@page0*/
    ret = i2c_smbus_read_i2c_block_data(fd, CONFIG_ID_REG, BYTE_COUNT_4, rdata);

    if(ret >= SUCCESS)
    {
        VrConfigId = ((uint16_t)rdata[INDEX_2] << SHIFT_8) | rdata[INDEX_1];
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
             if(line.find(FILE_VENDOR_ID) != std::string::npos)
             {
                  std::stringstream ss;
                  size_t pos = line.find(FILE_VENDOR_ID);
                  std::string Id = line.substr(pos + sizeof(FILE_VENDOR_ID), INDEX_4);
                  ss<<Id;
                  ss>>std::hex>>FileVendorId;
             }
             else if(line.find(FILE_DEVICE_ID) != std::string::npos){
                  std::stringstream ss;
                  size_t pos = line.find(FILE_DEVICE_ID);
                  std::string Id = line.substr(pos + sizeof(FILE_DEVICE_ID), INDEX_4);
                  ss<<Id;
                  ss>>std::hex>>FileDeviceId;
           }
             else if(line.find(FILE_CONFIG_ID) != std::string::npos){
                  std::stringstream ss;
                  size_t pos = line.find(FILE_CONFIG_ID);
                  std::string Id = line.substr(pos + sizeof(FILE_CONFIG_ID), INDEX_4);
                  ss<<Id;
                  ss>>std::hex>>FileConfigId;
           }
        }
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to open config file\n");
        return false;
    }

     if((VrVendorId == FileVendorId) && (VrDeviceId == FileDeviceId) && (VrConfigId == FileConfigId))
    {
        sd_journal_print(LOG_INFO, "VR ids and file ids matched\n");
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: VR IDs and File devie IDs did not match. Update failed\n");
        cFile.close();
        return false;

    }

    return true;
}


bool vr_update_mps285x::UpdateFirmware()
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
    std::array<uint16_t,5> RegisterSet = {0x1F,0x3F,0x5F,0x7F,0x9F};
    uint8_t ByteData = 0;
    uint16_t word_data = 0;

    /*Password unlock*/

    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_0);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
        return false;
    }

    word_data = i2c_smbus_read_word_data(fd,PWD_UNLOCK_REG);

    if (word_data < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read password unlock register data\n");
        return false;
    }

    sd_journal_print(LOG_INFO,"Password unlock register - Page0 0x%x = 0x%x",PWD_UNLOCK_REG,word_data);

    if(word_data & (INDEX_1 << INDEX_3) == 0)
    {
        sd_journal_print(LOG_INFO, "The device is password locked. Reading pwd from Page 0 7Eh\n");

        word_data = i2c_smbus_read_word_data(fd,MFR_PWD_USER);

        if (word_data < SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Error: Failed to read password register\n");
            return false;
        }

        sd_journal_print(LOG_INFO, "Writing password 0x%x to unlock the device \n",word_data);

        ret = i2c_smbus_write_word_data(fd, CMD_F8, word_data);

        if (ret < SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Error: Writing password to address 0xF8 failed\n");
            return false;
        }

        ByteData = i2c_smbus_read_word_data(fd,PWD_UNLOCK_REG);

        if (ByteData < SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Error: Failed to read password unlock register data\n");
            return false;
        }

        sd_journal_print(LOG_INFO,"After writing password : Password unlock register Page0 7Eh = 0x%x",ByteData);

        if(ByteData & (INDEX_1 << INDEX_3) == INDEX_1)
        {
            sd_journal_print(LOG_INFO, "Password unlocked for VR update\n");
        }
        else
        {
            sd_journal_print(LOG_INFO, "Password unlocking failed for the device. Aborting the update\n");
            return false;
        }
    }
    /*Check write protect from 35h@page1*/
    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_1);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
        return false;
    }

    ret = i2c_smbus_read_i2c_block_data(fd, WRITE_PROTECT, BYTE_COUNT_2, rdata);

    if(ret >= SUCCESS)
    {
        ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_0);

        if (ret < SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
            return false;
        }

		sd_journal_print(LOG_INFO,"Write protect check %x %x\n",rdata[0],rdata[1]);

        if((rdata[INDEX_0] & INDEX_4) == INDEX_1)
        {
            sd_journal_print(LOG_INFO,"Unlocking Memory protection\n");

            ret = i2c_smbus_write_byte_data(fd, UNLOCK_PROTECT, DATA_0);

            if (ret < SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Unlocking memory protection failed\n");
                return false;
            }
        }
        else
        {

            sd_journal_print(LOG_INFO,"Unlocking MTP protection\n");

            ret = i2c_smbus_write_byte_data(fd, UNLOCK_PROTECT, DATA_63);

            if (ret < SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Unlocking memory protection failed\n");
                return false;
            }
        }
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read write protect mode\n");
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

                if(current_page != Page)
                {
                    if(Page == INDEX_2)
                    {
                        sd_journal_print(LOG_INFO, "Sending F1h command to store data to MTP\n");

                        ret = i2c_smbus_write_byte(fd, CMD_F1);

                        if (ret < SUCCESS)
                        {
                            sd_journal_print(LOG_ERR, "Error: Sending F1h command to store data to MTP failed\n");
                            cFile.close();
                            return false;
                        }

                       // std::this_thread::sleep_for(std::chrono::milliseconds(500));
                       sleep(1);
                        uint16_t byte_data = i2c_smbus_read_word_data(fd,CMD_4F);

						sd_journal_print(LOG_INFO,"4Fh data read from the device 0x%x\n",byte_data);

                        if (byte_data < SUCCESS)
                        {
                            sd_journal_print(LOG_ERR, "Error: Failed to read 0x4F data\n");
                            return false;
                        }

                        byte_data |= (INDEX_1 << INDEX_5);

						sd_journal_print(LOG_INFO,"Data written back to 4F 0x%x\n",byte_data);

                        ret = i2c_smbus_write_word_data(fd, CMD_4F, byte_data);

                        if (ret < SUCCESS)
                        {
                            sd_journal_print(LOG_ERR, "Error: Writing back to command 0x4F failed\n");
                            cFile.close();
                            return false;
                        }
                    }

                    current_page = Page;

					sd_journal_print(LOG_INFO,"Setting current page number to %x\n",current_page);

                    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, current_page);

                    if (ret < SUCCESS)
                    {
                        sd_journal_print(LOG_ERR, "Error: Writing byte data to the device failed\n");
                        cFile.close();
                        return false;
                    }

                    if(Page == INDEX_2)
                    {
                        ret = i2c_smbus_write_byte(fd, CMD_F3);

                        sd_journal_print(LOG_INFO,"Written all 0 and 1 register. Sent F3h command\n");

                        if (ret < SUCCESS)
                        {
                            sd_journal_print(LOG_ERR, "Error: Sending F3h command failed\n");
                            cFile.close();
                            return false;
                        }

						sd_journal_print(LOG_INFO, "Setting page number to 0x2A\n");
						ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG,PAGE_2A);

                        sleep(1);

                    }
                }
            }
            else if(i == INDEX_2)
            {
                ss << std::hex << text_data[i];
                ss >> Register;
                ss.clear();
                if((std::find(RegisterSet.begin(), RegisterSet.end(), Register) != RegisterSet.end()) && (current_page == 2))
                {
                    Data = 0;
                    break;
                }
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
        }

        else if(Mode == BLOCK_3)
        {
            uint8_t rxdata[INDEX_3] = {0};
            rxdata[INDEX_0] = 0x02;
            rxdata[INDEX_1] = Data & BYTE_MASK;
            rxdata[INDEX_2] = (Data >> INDEX_8) & BYTE_MASK;

            ret = i2c_smbus_write_i2c_block_data(fd, Register, INDEX_3 , rxdata);
            if (ret < SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Writing block data to the device failed   %x   0x%x 0x%x\n", Register, rxdata[0],rxdata[1]);
                cFile.close();
                return false;
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

    return true;
}

bool vr_update_mps285x::ValidateFirmware()
{

    int ret = FAILURE;
    std::string line;
    uint16_t UserCrc = 0;
    uint16_t MultiCrc = 0;

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
             }
             else if(line.find(CRC_MULTI) != std::string::npos)
             {
                  std::stringstream ss;
                  size_t pos = line.find(CRC_MULTI);
                  std::string Id = line.substr(pos + sizeof(CRC_MULTI), INDEX_4);
                  ss<<Id;
                  ss>>std::hex>>MultiCrc;
             }
        }
    }
    sd_journal_print(LOG_INFO, "Config file : User CRC = 0x%x Multi CRC = 0x%x\n",UserCrc,MultiCrc);

    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_1);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
        return false;
    }

    uint16_t DeviceUserCrc = i2c_smbus_read_word_data(fd,USER_CRC_REG);

    uint16_t DeviceMultiCrc = i2c_smbus_read_word_data(fd,MULTI_CRC_REG);

    sd_journal_print(LOG_INFO, "VR Device : User CRC = 0x%x Multi CRC = 0x%x\n",DeviceUserCrc,DeviceMultiCrc);

    if((UserCrc != DeviceUserCrc) || (MultiCrc != DeviceMultiCrc))
    {
        sd_journal_print(LOG_ERR, "CRC mismatch. Update failed\n");
        return false;
    }
    return true;
}
