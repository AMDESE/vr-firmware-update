/*
* vr-update.hpp
*
* Created on: Nov 29, 2022
* Author: alkulkar
*/
#include "vr_update.hpp"
#include "vr_update_mps.hpp"

vr_update_mps::vr_update_mps(std::string Processor,uint32_t Crc,
        std::string Model,uint16_t SlaveAddress,std::string ConfigFilePath,std::string Revision,uint16_t PmbusAddress):
        vr_update(Processor,Crc,Model,SlaveAddress,ConfigFilePath,Revision,PmbusAddress)
{
        DriverPath = MPS_DRIVER_PATH;

}

bool vr_update_mps::crcCheckSum(){

    uint16_t DeviceCrc = 0;
    int ret = FAILURE;
    uint8_t rdata[BYTE_COUNT_4] = { 0 };

    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_0);
    if (ret < SUCCESS)
     {
         sd_journal_print(LOG_ERR, "Error: Setting Page number failed\n");
         return false;
     }

    ret = i2c_smbus_read_i2c_block_data(fd, CRC_ADDR, BYTE_COUNT_4, rdata);
    if (rdata < SUCCESS)
    {
           sd_journal_print(LOG_ERR, "Error: Reading CRC from device failed\n");
           return false;
    }

    DeviceCrc = (rdata[INDEX_0] << SHIFT_8) | rdata[INDEX_1];
    if(DeviceCrc == Crc)
    {
       sd_journal_print(LOG_ERR, "Error: Device CRC matches with file CRC. Skipping the update\n");
       CrcMatched = true;
       return false;
    }
    else
    {
        sd_journal_print(LOG_ERR, "Info: CRC does not match with the previous image. Continuing the update\n");
        CrcMatched = false;
        return true;
    }
}


bool vr_update_mps::isUpdatable(){

    uint8_t rdata[MAXIMUM_SIZE] = {0};
    bool rc = FAILURE;
    uint32_t VrVendorId = 0;
    uint32_t FileVendorId = 0;
    uint16_t VrDeviceId = 0;
    uint16_t FileDeviceId = 0;
    uint32_t VrConfigId = 0;
    uint32_t FileConfigId = 0;
    int ret = FAILURE;
    std::string line;


    ret = i2c_smbus_read_i2c_block_data(fd, VENDOR_ID_REG, BYTE_COUNT_4, rdata);

    if(ret >= SUCCESS)
    {
        VrVendorId = (rdata[INDEX_0] << SHIFT_24) | (rdata[INDEX_1] << SHIFT_16)
                                    | (rdata[INDEX_2] << SHIFT_8) | rdata[INDEX_3];
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read Vendor ID from the VR device\n");
        return false;
    }

    ret = i2c_smbus_read_i2c_block_data(fd, DEVICE_ID_REG, BYTE_COUNT_2, rdata); 

    if(rdata >= SUCCESS)
    {
        VrDeviceId = (rdata[INDEX_0] << SHIFT_8) | rdata[INDEX_1];
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read Device ID from the VR device\n");
        return false;
    }

    ret = i2c_smbus_read_i2c_block_data(fd, CONFIG_ID_REG, BYTE_COUNT_3, rdata);

    if(rdata >= SUCCESS)
    {
        VrConfigId = (rdata[INDEX_0] << SHIFT_16) | (rdata[INDEX_1] << SHIFT_8) | rdata[INDEX_2];
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
             if(line.find(FILE_VENDOR_ID) != std::string::npos){
                  std::stringstream ss;
                  std::string Id = line.substr(sizeof(FILE_VENDOR_ID)+INDEX_2, 8);
                  ss<<Id;
                  ss>>std::hex>>FileVendorId;
           }
             else if(line.find(FILE_DEVICE_ID) != std::string::npos){
                  std::stringstream ss;
                  std::string Id = line.substr(sizeof(FILE_DEVICE_ID)+INDEX_2, 4);
                  ss<<Id;
                  ss>>std::hex>>FileDeviceId;
           }
             else if(line.find(FILE_CONFIG_ID) != std::string::npos){
                  std::stringstream ss;
                  std::string Id = line.substr(sizeof(FILE_CONFIG_ID)+INDEX_2, 6);
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


bool vr_update_mps::UpdateFirmware()
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
                 current_page = Page;
                 ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, current_page);
                 if (ret < SUCCESS){
                    sd_journal_print(LOG_ERR, "Error: Writing byte data to the device failed\n");
                    cFile.close();
                    return FAILURE;
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

      if(Mode == BLOCK_1){
        uint16_t rxdata = (uint16_t)Data;
        ret = i2c_smbus_write_byte_data(fd, Register, rxdata);
        if (ret < SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Writing block data to the device failed\n");
                cFile.close();
                return FAILURE;
            }
      }

      else if(Mode == BLOCK_2){
        uint16_t rxdata = (uint16_t)Data;
        ret = i2c_smbus_write_word_data(fd, Register, rxdata);
        if (ret < SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Writing block data to the device failed\n");
                cFile.close();
                return FAILURE;
            }
      }

      else if(Mode == BLOCK_3){
        uint8_t rxdata[INDEX_3];
        rxdata[INDEX_2] = (Data & MASK_8);
        rxdata[INDEX_1] = (Data & MASK_16)>>8;
        rxdata[INDEX_0] = (Data & MASK_24)>>16;

        ret = i2c_smbus_write_i2c_block_data(fd, Register, BYTE_COUNT_3, rxdata);
        if (ret < SUCCESS)
             {
                 sd_journal_print(LOG_ERR, "Error: Writing word data to the device failed   %x   %x\n", Register, rxdata);
                 cFile.close();
                 return FAILURE;
             }
        }

      else{
       sd_journal_print(LOG_ERR, "Error: Invalid Mode: %d for Register: %x \n", Mode, Register);
       cFile.close();
       return FAILURE;
      }
    }


    //Write to Page 0 and write 0 to Register 15
    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_0);
    if (ret < SUCCESS){
       sd_journal_print(LOG_ERR, "Error: Writing byte data to the device failed\n");
       cFile.close();
       return FAILURE;
    }
    ret = i2c_smbus_write_byte_data(fd, REGISTER_15, PAGE_0);
    if (ret < SUCCESS){
       sd_journal_print(LOG_ERR, "Error: Writing byte data to the device failed\n");
       cFile.close();
       return FAILURE;
    }

    return SUCCESS;
}

