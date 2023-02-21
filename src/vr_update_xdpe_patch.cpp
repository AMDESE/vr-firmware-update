/*
* vr_update_infineon_xdpe.cpp
*
* Created on: Nov 10, 2022
* Author: Abinaya Dhandapani
*/

#include "vr_update.hpp"
#include "vr_update_xdpe_patch.hpp"

vr_update_xdpe_patch::vr_update_xdpe_patch(std::string Processor,
          uint32_t Crc,std::string Model,uint16_t SlaveAddress,std::string ConfigFilePath):
          vr_update(Processor,Crc,Model,SlaveAddress,ConfigFilePath)
{

    DriverPath = XDPE_DRIVER_PATH;
}

bool vr_update_xdpe_patch::crcCheckSum()
{
    return true;
}

bool vr_update_xdpe_patch::isUpdatable()
{

    std::array<uint8_t, INDEX_12> ProductId {0x90,0x8A,0x8C,0x95,0x96,0x97,0x98,0x99,0x9C,0x9A,0x9E,0x9B};

    uint8_t rdata[MAXBUFFERSIZE];
    int length;
    bool found = false;
    std::ifstream CfgFileFd;

    length = i2c_smbus_read_block_data(fd, DEVICE_ID_CMD, rdata);
    sd_journal_print(LOG_INFO, "Product ID from the device = 0x%x\n",rdata[INDEX_1]);

    /*Product ID check*/
    if (length > SUCCESS)
    {
       for(uint16_t value : ProductId)
       {
           if(rdata[INDEX_1] == value)
           {
               ProductID = rdata[INDEX_1];
               found = true;
               break;
           }
       }

       if(found == true)
       {
           sd_journal_print(LOG_INFO ,  "Infineon device detected\n");
       }
       else
       {
           sd_journal_print(LOG_ERR, "Infineon device not detected\n");
           return false;
       } 
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Read failed\n");
        return false;
    }

    CfgFileFd.open(ConfigFilePath, std::ios::binary);

    if(!CfgFileFd)
    {
        sd_journal_print(LOG_ERR, "Couldn't open the patch file\n");
        return false;
    }

    /*Check if first byte is 0x10 for a valid patch file*/
    uint8_t PatchByte;
    CfgFileFd.read(reinterpret_cast<char*>(&PatchByte), sizeof(PatchByte));

    sd_journal_print(LOG_INFO,"Header code of the patch file = 0x%x\n",PatchByte);

    if(PatchByte != PATCH_BYTE)
    {
        sd_journal_print(LOG_ERR, "Not a valid patch file\n");
        return false;
    }

    /*Extract patch size from patch file*/
    CfgFileFd.seekg(INDEX_4, std::ios_base::beg);

    uint16_t PatchFileSize;
    CfgFileFd.read(reinterpret_cast<char*>(&PatchFileSize), sizeof(PatchFileSize));

    sd_journal_print(LOG_INFO, "Patch file size = 0x%x\n",PatchFileSize);

    /*Extract firmwarecode from the patch file*/
    CfgFileFd.seekg(INDEX_20, std::ios_base::beg);
    CfgFileFd.read(reinterpret_cast<char*>(&FirmwareCode), sizeof(FirmwareCode));

    sd_journal_print(LOG_INFO, "FirmwareCode = 0x%x\n",FirmwareCode);
    CfgFileFd.close();

    /*Find availabe space on Partition 1*/
    int rc = FAILURE;
    uint8_t wdata[MAXBUFFERSIZE];
    int OtpSize;

    memset(wdata, INDEX_0, MAXBUFFERSIZE);
    memset(rdata, INDEX_0, MAXBUFFERSIZE);

    wdata[INDEX_3] = PN;

    rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);
    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
        return false;
    }

    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, GET_OTP_SIZE);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
        return false;
    }

    length = i2c_smbus_read_block_data(fd, BLOCK_PREFIX, rdata);

    if (length > SUCCESS)
    {
        // size = d0 + 256 * d1. Formula provided in Infineon document
        OtpSize = (256 * rdata[INDEX_1] + rdata[INDEX_0]);
        sd_journal_print(LOG_INFO, "Available size (bytes): 0x%x\n",OtpSize);
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
        return false;
    }

    if(PatchFileSize > OtpSize)
    {
        sd_journal_print(LOG_ERR, "Not enough space to update the patch to the partition\n");
        return false;
    } 
    return true;
}

bool vr_update_xdpe_patch::UpdateFirmware()
{
    int length;
    int rc = FAILURE;

    uint8_t rdata[MAXBUFFERSIZE];
    uint32_t DeviceFwId;

    std::array<uint8_t, INDEX_12> ProductId {0x90,0x8A,0x8C,0x95,0x96,0x97,0x98,0x99};

    /*Find FW VERSION from the device*/
    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, FW_VERSION);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
        return false;
    }

    length = i2c_smbus_read_block_data(fd, BLOCK_PREFIX, rdata);

    if (length > SUCCESS)
    {
        DeviceFwId = (rdata[INDEX_3] << SHIFT_24) | (rdata[INDEX_2] << SHIFT_16) |
                     (rdata[INDEX_1] << SHIFT_8) | rdata[INDEX_0];

        sd_journal_print(LOG_INFO, "Device firmware ID = 0x%x\n",DeviceFwId);
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
        return false;
    }

    if(DeviceFwId == FW_ROM_ID)
    {
        sd_journal_print(LOG_INFO, "Device FW ID is 0x6192EE1E.Hence, No existing patch in the system\n"); 
    }
    else
    {
        sd_journal_print(LOG_INFO, "Patch already running in partition 1. Invalidating patch\n");
           
    }

    return true;
}

bool vr_update_xdpe_patch::ValidateFirmware()
{
    return true;
}
