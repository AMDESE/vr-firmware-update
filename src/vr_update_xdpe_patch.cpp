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
    CrcMatched = false;
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
    if (length > LENGTH_0)
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

    if (length > LENGTH_0)
    {
        // size = d0 + 256 * d1. Formula provided in Infineon document
        OtpSize = (INT_256 * rdata[INDEX_1] + rdata[INDEX_0]);
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

bool WriteDataToScratchpad(std::string ConfigFilePath, uint16_t PatchFileSize,int fd, bool EarlierControlRev)
{

    uint8_t ScratchpadAddr[MAXBUFFERSIZE]; 
    uint8_t buffer[PatchFileSize];
    int length;

    /*Find scratchpad address */
    int rc = FAILURE;
    uint8_t wdata[MAXBUFFERSIZE];
    memset(ScratchpadAddr, INDEX_0, MAXBUFFERSIZE);

    if(EarlierControlRev == false)
    {
        memset(wdata, INDEX_0, MAXBUFFERSIZE);

        wdata[INDEX_0] = INDEX_4;
        wdata[INDEX_1] = INDEX_2;

        sd_journal_print(LOG_DEBUG, "Block write : Command = 0x%x Data = 0x%x 0x%x 0x%x 0x%x\n",
                   BLOCK_PREFIX, wdata[INDEX_0],wdata[INDEX_1],wdata[INDEX_2],wdata[INDEX_3]);

        rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);

        if (rc != SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
            return false;
        }

        sd_journal_print(LOG_INFO, "Write byte : Command = 0x%x Data = 0x%x\n",BYTE_PREFIX, GET_FW_ADDRESS);

        rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, GET_FW_ADDRESS);

        if (rc != SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
            return false;
        }

        length = i2c_smbus_read_block_data(fd, BLOCK_PREFIX, ScratchpadAddr);

        if (length < LENGTH_0)
        {
            sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
            return false;
        }
    }
    else
    {
        ScratchpadAddr[INDEX_1] = SCRATCHPAD_1;
        ScratchpadAddr[INDEX_2] = SCRATCHPAD_2;
        ScratchpadAddr[INDEX_3] = SCRATCHPAD_3;
    }
    /*Read ptch file and write to the device*/
    std::ifstream ConfigFileFd(ConfigFilePath, std::ios::in | std::ios::binary);

    if (!ConfigFileFd.is_open())
    {
        sd_journal_print(LOG_ERR, "Failed to open file\n");
        return false;
    }

    ConfigFileFd.read(reinterpret_cast<char*>(buffer), PatchFileSize);

    for (uint32_t i = 0; i < (PatchFileSize / INT_1024); i++)
    {

        sd_journal_print(LOG_INFO ,"Block write: Command code = 0x%x Scratchpad data = 0x%x 0x%x 0x%x 0x%x\n",
                  RPTR ,ScratchpadAddr[INDEX_0],ScratchpadAddr[INDEX_1],ScratchpadAddr[INDEX_2],ScratchpadAddr[INDEX_3]);

        rc = i2c_smbus_write_block_data(fd, RPTR, (uint8_t)LENGTHOFBLOCK, ScratchpadAddr);

        if (rc != SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
            return false;
        }

        for (uint32_t j = 0; j <= INT_1020; j += INDEX_4)
        {
            memset(wdata, INDEX_0, MAXBUFFERSIZE);

            wdata[INDEX_0] = buffer[i * INT_1024 + j];
            wdata[INDEX_1] = buffer[i * INT_1024 + j + INDEX_1];
            wdata[INDEX_2] = buffer[i * INT_1024 + j + INDEX_2];
            wdata[INDEX_3] = buffer[i * INT_1024 + j + INDEX_3];

            sd_journal_print(LOG_INFO, "Block write: Command code = 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                     MFR_REG_WRITE,wdata[INDEX_0],wdata[INDEX_1],wdata[INDEX_2],wdata[INDEX_3]);

            rc = i2c_smbus_write_block_data(fd, MFR_REG_WRITE, (uint8_t)LENGTHOFBLOCK, wdata);
            if (rc != SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                return false;
            }
        }

        if(i == INDEX_0)
        {
            memset(wdata, INDEX_0, MAXBUFFERSIZE);
            wdata[INDEX_1] = INDEX_4;
            wdata[INDEX_2] = INDEX_1;
            wdata[INDEX_3] = PN;
        }
        else
        {
            memset(wdata, INDEX_0, MAXBUFFERSIZE);
            wdata[INDEX_1] = INDEX_4;
            wdata[INDEX_3] = PN;
        }

        sd_journal_print(LOG_DEBUG, "Block write : Command = 0x%x Data = 0x%x 0x%x 0x%x 0x%x\n",
                                   BLOCK_PREFIX, wdata[INDEX_0],wdata[INDEX_1],wdata[INDEX_2],wdata[INDEX_3]);

        rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);

        if (rc != SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
            return false;
        }

        sd_journal_print(LOG_DEBUG, "Write byte : Command = 0x%x Data = 0x%x\n",BYTE_PREFIX, MFR_SPECIFIC_COMMAND);

        rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, MFR_SPECIFIC_COMMAND);

        if (rc != SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
            return false;
        }
        sleep(INDEX_2);
    }
    return true;
}

bool vr_update_xdpe_patch::UpdateFirmware()
{
    int length;
    int rc = FAILURE;
    uint8_t rdata[MAXBUFFERSIZE];
    uint8_t wdata[MAXBUFFERSIZE];
    uint32_t DeviceFwId;
    bool EarlierCtrlRev;

    std::array<uint8_t, INDEX_8> ProductId {0x90,0x8A,0x8C,0x95,0x96,0x97,0x98,0x99};

    for(uint16_t value : ProductId)
    {
        if(ProductID == value)
        {
            EarlierCtrlRev = true;
            break;
        }
    }

    /*Find FW VERSION from the device*/
    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, FW_VERSION);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
        return false;
    }

    length = i2c_smbus_read_block_data(fd, BLOCK_PREFIX, rdata);

    if (length > LENGTH_0)
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

        if(EarlierCtrlRev == true)
        {
            sd_journal_print(LOG_INFO, "Invalidating earlier controller revision\n");

            int rc = FAILURE;

            memset(wdata, INDEX_0, MAXBUFFERSIZE);

            wdata[INDEX_0] = CTRL_ADDR_0;
            wdata[INDEX_1] = CTRL_ADDR_1;
            wdata[INDEX_2] = CTRL_ADDR_2;
            wdata[INDEX_3] = CTRL_ADDR_3;

            sd_journal_print(LOG_INFO, "Block write : Command = 0x%x Data = 0x%x 0x%x 0x%x 0x%x\n",
                    RPTR, wdata[INDEX_0],wdata[INDEX_1],wdata[INDEX_2],wdata[INDEX_3]);
            rc = i2c_smbus_write_block_data(fd, RPTR, (uint8_t)LENGTHOFBLOCK, wdata);
            if (rc != SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                return false;
            }

            memset(wdata, INDEX_0, MAXBUFFERSIZE);

            wdata[INDEX_0] = INT_15;

            sd_journal_print(LOG_DEBUG, "Block write : Command = 0x%x Data = 0x%x 0x%x 0x%x 0x%x\n",
                  MFR_REG_WRITE, wdata[INDEX_0],wdata[INDEX_1],wdata[INDEX_2],wdata[INDEX_3]);

            rc = i2c_smbus_write_block_data(fd, MFR_REG_WRITE, (uint8_t)LENGTHOFBLOCK, wdata);

            if (rc != SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                return false;
            }

            sd_journal_print(LOG_DEBUG, "Write byte : Command = 0x%x Data = 0x%x\n",BYTE_PREFIX, INVALIDATE_OTP);

            rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, INVALIDATE_OTP);

            if (rc != SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                return false;
            }

            sleep(INDEX_1);

            memset(wdata, INDEX_0, MAXBUFFERSIZE);

            wdata[INDEX_0] = INT_16;
            wdata[INDEX_3] = INDEX_1;

            sd_journal_print(LOG_INFO, "Block write : Command = 0x%x Data = 0x%x 0x%x 0x%x 0x%x\n",
                          BLOCK_PREFIX, wdata[INDEX_0],wdata[INDEX_1],wdata[INDEX_2],wdata[INDEX_3]);

            rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);

            if (rc != SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                return false;
            }
            sd_journal_print(LOG_INFO, "Write byte : Command = 0x%x Data = 0x%x\n",BYTE_PREFIX, FS_FILE_INVALIDATE);
            rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, FS_FILE_INVALIDATE);

            if (rc != SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                return false;
            }

            memset(wdata, INDEX_0, MAXBUFFERSIZE);

            wdata[INDEX_0] = CTRL_ADDR_0;
            wdata[INDEX_1] = CTRL_ADDR_1;
            wdata[INDEX_2] = CTRL_ADDR_2;
            wdata[INDEX_3] = CTRL_ADDR_3;

            sd_journal_print(LOG_DEBUG, "Block write : Command = 0x%x Data = 0x%x 0x%x 0x%x 0x%x\n",
                           RPTR, wdata[INDEX_0],wdata[INDEX_1],wdata[INDEX_2],wdata[INDEX_3]);

            rc = i2c_smbus_write_block_data(fd, RPTR, (uint8_t)LENGTHOFBLOCK, wdata);
            if (rc != SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                return false;
            }

            memset(wdata, INDEX_0, MAXBUFFERSIZE);
            wdata[INDEX_0] = INDEX_3;

            sd_journal_print(LOG_INFO, "Block write : Command = 0x%x Data = 0x%x 0x%x 0x%x 0x%x\n",
                        MFR_REG_WRITE, wdata[INDEX_0],wdata[INDEX_1],wdata[INDEX_2],wdata[INDEX_3]);

            rc = i2c_smbus_write_block_data(fd, MFR_REG_WRITE, (uint8_t)LENGTHOFBLOCK, wdata);
            if (rc != SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                return false;
            }

            sd_journal_print(LOG_INFO, "Write byte : Command = 0x%x Data = 0x%x\n",BYTE_PREFIX, INVALIDATE_OTP);
            rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, INVALIDATE_OTP);

            if (rc != SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                return false;
            }

            sleep(INDEX_1);
        }
        else
        {
            sd_journal_print(LOG_INFO, "Invalidating OTP for latest controller\n");
            memset(wdata, INDEX_0, MAXBUFFERSIZE);

            wdata[INDEX_0] = INT_16;
            wdata[INDEX_3] = INDEX_1;

            rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);
            if (rc != SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                return false;
            }

            rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, FS_FILE_INVALIDATE);

            if (rc != SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                return false;
            }
        }
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

            sd_journal_print(LOG_INFO, "Device firmware ID after invalidating OTP = 0x%x\n",DeviceFwId);
        }
        else
        {
            sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
            return false;
        }
    }

    if(WriteDataToScratchpad(ConfigFilePath,PatchFileSize,fd,EarlierCtrlRev) == false)
    {
        return false;
    }

    /*Enable the patch*/
    sd_journal_print(LOG_INFO, "Write byte : Command = 0x%x Data = 0x%x\n",BYTE_PREFIX, INVALIDATE_OTP);
    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, INVALIDATE_OTP);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
        return false;
    }

    return true;
}

bool vr_update_xdpe_patch::ValidateFirmware()
{
    return true;
}
