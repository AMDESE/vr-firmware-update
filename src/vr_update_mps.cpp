/*
* vr-update.hpp
*
* Created on: Nov 29, 2022
* Author: alkulkar
*/

#include"vr_update.hpp"

#define VENDOR_ID_REG     (0x99)
#define DEVICE_ID_REG     (0xAD)
#define CONFIG_ID_REG     (0X9B)
#define SET_PAGE_REG      (0x00)

#define PAGE_0            (0)
#define PAGE_1            (1)
#define PAGE_2            (2)

#define REGISTER_15       (15)
#define CRC_ADDR          (0xB8)
#define MASK_8            (0x000000FF)
#define MASK_16           (0x0000FF00)
#define MASK_24           (0x00FF0000)



#define BLOCK_1           (1)
#define BLOCK_2           (2)
#define BLOCK_3           (3)


#define FILE_VENDOR_ID     ("Vendor ID")
#define FILE_DEVICE_ID     ("Device ID")
#define FILE_CONFIG_ID     ("Config ID")
#define END_OF_REG         ("END")


#define INDEX_0           (0)
#define INDEX_1           (1)
#define INDEX_2           (2)
#define INDEX_3           (3)
#define INDEX_4           (4)
#define INDEX_5           (5)
#define INDEX_6           (6)
#define INDEX_7           (7)

#define MAXIMUM_SIZE      (255)
#define TOTAL_COLUMNS     (8)


vr_update_mps::vr_update_mps(std::string Processor,
        std::string MachineName,uint32_t Crc,std::string Model,
        uint16_t SlaveAddress,std::string ConfigFilePath):
        vr_update(Processor,MachineName,Crc,Model,SlaveAddress,ConfigFilePath)
{
        DriverPath = MPS_DRIVER_PATH;
        std::cout << "MPS Framework Constructor"<<std::endl;
}

bool vr_update_mps::crcCheckSum(){

    uint16_t DeviceCrc = 0;
    int ret = FAILURE;
    uint8_t rdata[BYTE_COUNT_4] = { 0 };
    bool rc = false;

    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_0);
    if (ret < SUCCESS)
     {
         std::cout << "Setting Page number failed " << std::endl;
         return false;
     }

    ret = i2c_smbus_read_i2c_block_data(fd, CRC_ADDR, BYTE_COUNT_4, rdata);
    if (rdata < SUCCESS)
    {
           std::cout << "Reading CRC from device failed " << std::endl;
           return false;
    }

    DeviceCrc = (rdata[INDEX_0] << SHIFT_8) | rdata[INDEX_1];
    if(DeviceCrc == Crc)
    {
       std::cout << "Device CRC matches with file CRC. Skipping the update" << std::endl;
       return false;
    }
    else
    {
        std::cout << "CRC not matched with the previous image. Continuing the update" << std::endl;
        return true;
    }
}


bool vr_update_mps::isUpdatable(){

    uint8_t rdata[MAXIMUM_SIZE] = {0};
    bool rc = false;
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

        std::cout << "Vendor ID from the VR = " << std::hex << unsigned(VrVendorId) << std::endl;
    }
    else
    {
        std::cout << "Failed to read Vendor ID from the VR device" << std::endl;
        return false;
    }

    ret = i2c_smbus_read_i2c_block_data(fd, DEVICE_ID_REG, BYTE_COUNT_2, rdata); 

    if(rdata >= SUCCESS)
    {
        VrDeviceId = (rdata[INDEX_0] << SHIFT_8) | rdata[INDEX_1];
        std::cout << "Device ID from the VR = " << std::hex << unsigned(VrDeviceId) << std::endl;
    }
    else
    {
        std::cout << "Failed to read Device ID from the VR device" << std::endl;
        return false;
    }

    ret = i2c_smbus_read_i2c_block_data(fd, CONFIG_ID_REG, BYTE_COUNT_3, rdata);

    if(rdata >= SUCCESS)
    {
        VrConfigId = (rdata[INDEX_0] << SHIFT_16) | (rdata[INDEX_1] << SHIFT_8) | rdata[INDEX_2];
        std::cout << "Config ID from the VR = " << std::hex << unsigned(VrConfigId) << std::endl;
    }
    else
    {
        std::cout << "Failed to read Config ID from the VR device" << std::endl;
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
        std::cout << "Open() config file failed" << std::endl;
        return false;
    }

     if((VrVendorId == FileVendorId) && (VrDeviceId == FileDeviceId) && (VrConfigId == FileConfigId))
    {
        std::cout << "VR ids and file ids matched" << std::endl;
    }
    else
    {
        std::cout << "VR IDs and File devie IDs did not match.Update failed" << std::endl;
        rc = false;
        goto Clean;

    }
rc = true;
Clean:
    cFile.close();
    return rc;

}


bool vr_update_mps::UpdateFirmware()
{
    int ret = FAILURE;
    bool rc = false;
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
        std::cout << "Open() Config file failed " << std::endl;
        return false;
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
                 std::cout<<"  Writing Page "<<Page<<" to register 0x00"<<std::endl;
                 ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, current_page);
                 if (ret < SUCCESS){
                    std::cout << "Writing byte data to the device failed " << std::endl;
                    rc = false;
                    goto Clean;
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
                std::cout << "Writing block data to the device failed " << std::endl;
                rc = false;
                goto Clean;
            }
      }

      else if(Mode == BLOCK_2){
        uint16_t rxdata = (uint16_t)Data;
        ret = i2c_smbus_write_word_data(fd, Register, rxdata);
           if (ret < SUCCESS)
            {
                std::cout << "Writing block data to the device failed " << std::endl;
                rc = false;
                goto Clean;
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
                 std::cout << "Writing word data to the device failed " << std::endl;
                 rc = false;
                 goto Clean;
             }
        std::cout << "Wrote Block data" << std::endl;
        }

      else{
       std::cout<<"Invalid Mode: "<<Mode<<" for Register: "<<Register<<std::endl;
       rc = false;
       goto Clean;
      }
    }


    //Write to Page 0 and write 0 to Register 15
    ret = i2c_smbus_write_byte_data(fd, SET_PAGE_REG, PAGE_0);
    if (ret < SUCCESS){
       std::cout << "Writing byte data to the device failed " << std::endl;
       rc = false;
       goto Clean;
    }
    ret = i2c_smbus_write_byte_data(fd, REGISTER_15, PAGE_0);
    if (ret < SUCCESS){
       std::cout << "Writing byte data to the device failed " << std::endl;
       rc = false;
       goto Clean;
    }

    rc = true;

Clean:
    cFile.close();
    return rc;
}


