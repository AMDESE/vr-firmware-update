/*
* vr_update_renesas_gen3.cpp
*
* Created on: Nov 10, 2022
* Author: Abinaya Dhandapani
*/


#include "vr_update.hpp"

#define DISABLE_PACKET       (0x00A2)
#define FINISH_CAPTURE       (0x0002)
#define DEVICE_FW_VERSION    (0x0030)
#define HALT_FW              (0x0002)
#define POLL_REG             (0x004E)
#define FINISH_CMD_CODE      (0xE7)
#define FW_WRITE             (0xE6)
#define DMA_WRITE             0xC7
#define DMA_READ             (0xC5)
#define GEN3_CRC_ADDR        0x0094
#define GEN3_NVM_SLOT_ADDR   (0x0035)
#define DEV_ID_CMD           (0xAD)
#define DEV_REV_REV          (0xAE)
#define GEN3_BANK_REG        (0x007F)
#define GEN3_PRGM_STATUS     (0x007E)
#define DEVICE_REVISION      (0x6000000)

#define MAXIMUM_SIZE         (255)
#define MIN_NVM_SLOT         (5)

#define BIT_ENABLE            (1)

vr_update_renesas_gen3::vr_update_renesas_gen3(std::string Processor,
          std::string MachineName,uint32_t Crc,std::string Model,
          uint16_t SlaveAddress,std::string ConfigFilePath):
          vr_update(Processor,MachineName,Crc,Model,SlaveAddress,ConfigFilePath)
{

    DriverPath = ISL_DRIVER_PATH;
}

bool vr_update_renesas_gen3::crcCheckSum()
{
    u_int8_t rdata[MAXIMUM_SIZE] = { 0 };
    u_int32_t DeviceCrc;
    int ret = FAILURE;

    //write to DMA Address Register
    ret = i2c_smbus_write_word_data(fd,DMA_WRITE,GEN3_CRC_ADDR );
    if(ret == SUCCESS)
    {
        ret = i2c_smbus_read_i2c_block_data(fd,DMA_READ, BYTE_COUNT_4, rdata);
        if(ret >= SUCCESS)
        {
            DeviceCrc = (rdata[INDEX_3] << SHIFT_24) | (rdata[INDEX_2] << SHIFT_16)
                                    | (rdata[INDEX_1] << SHIFT_8) | rdata[INDEX_0];

            sd_journal_print(LOG_INFO, "CRC from the device = 0x%x\n ",DeviceCrc);
            sd_journal_print(LOG_INFO, "CRC from the manifest file = 0x%x\n", Crc);
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

    if(DeviceCrc == Crc)
    {
       sd_journal_print(LOG_ERR, "Device CRC matches with file CRC. Skipping the update\n");
       return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "CRC not matched with the previous image. Continuing the update\n");
        return true;
    }
}

bool vr_update_renesas_gen3::isUpdatable()
{
    u_int8_t rdata[MAXIMUM_SIZE] = { 0 };
    int ret = FAILURE;
    bool rc = false;

    /*Disable packet capture.This step should be completed for
      parts with a configuration in RAM or NVM*/
    ret = i2c_smbus_write_word_data(fd, DMA_WRITE, DISABLE_PACKET);
    if(ret == SUCCESS)
    {
        ret = i2c_smbus_read_i2c_block_data(fd, DMA_READ, BYTE_COUNT_4, rdata);
        if(ret >= SUCCESS)
        {
            rdata[INDEX_0] = rdata[INDEX_0] & 0xDF;

            ret = i2c_smbus_write_i2c_block_data(fd, DMA_READ, BYTE_COUNT_4, rdata);
            if(ret == SUCCESS)
            {
                ret = i2c_smbus_write_word_data(fd, FINISH_CMD_CODE, FINISH_CAPTURE);
                if(ret == SUCCESS)
                {
                    sd_journal_print(LOG_INFO, "Disable packet capture finished\n");
                }
                else
                {
                    sd_journal_print(LOG_ERR, "Finish disable packet capture: DMA write failed\n");
                    return false;
                }
            }
            else
            {
                sd_journal_print(LOG_ERR, "Finish disable packet capture: DMA write failed\n");
                return false;
            }
        }
        else
        {
            sd_journal_print(LOG_ERR, "Disable packet capture: DMA read failed\n");
            return false;
        }
    }
    else
    {
        sd_journal_print(LOG_ERR, "Disbale packet capture:Setting the DMA address failed\n");
        return false;
    }

    /*Determine number of NVM slots available*/

    std::fill_n(rdata,MAXIMUM_SIZE,0);
    u_int8_t AvailableNvmSlots = 0;

    ret = i2c_smbus_write_word_data(fd,DMA_WRITE, GEN3_NVM_SLOT_ADDR);

    if(ret == SUCCESS)
    {
        ret = i2c_smbus_read_i2c_block_data(fd,DMA_READ, BYTE_COUNT_4, rdata);

        if(ret >= SUCCESS)
        {
            AvailableNvmSlots = (rdata[INDEX_3] << SHIFT_24) | (rdata[INDEX_2] << SHIFT_16)
                                     | (rdata[INDEX_1] << SHIFT_8) | rdata[INDEX_0];

            sd_journal_print(LOG_INFO, "Number of available NVM slots = %d\n",AvailableNvmSlots);

            if(AvailableNvmSlots <= MIN_NVM_SLOT)
            {
                sd_journal_print(LOG_ERR, "Available NVM slots is less than 5.Hence aborting the update\n");
                return false;
            }
        }
        else
        {
            sd_journal_print(LOG_ERR, "Find Available NVM slots: DMA read failed\n");
            return false;
        }
    }
    else
    {
        sd_journal_print(LOG_ERR, "Find Available NVM slots: Setting DMA address failed\n");
        return false;
    }

    /*Device ID validation*/

    /*Read Device Id from the device*/
    std::fill_n(rdata,MAXIMUM_SIZE,0);
    u_int32_t FileDeviceId = 0;
    u_int32_t VrDeviceId = 0;
    std::string line;
    uint32_t DeviceRevision = 0;
    uint32_t FileRevision = 0;

    ret = i2c_smbus_read_i2c_block_data(fd, DEV_ID_CMD, BYTE_COUNT_5, rdata);

    if(ret >= SUCCESS)
    {
        VrDeviceId = (rdata[INDEX_4] << SHIFT_24) | (rdata[INDEX_3] << SHIFT_16) |
                    (rdata[INDEX_2] << SHIFT_8) | rdata[INDEX_1];

        sd_journal_print(LOG_INFO, "Device ID from the VR = %d\n",VrDeviceId );
    }
    else
    {
        sd_journal_print(LOG_ERR, "Failed to read device ID from the VR device\n");
        return false;
    }

    /*Read device id from the config file*/
    std::ifstream cFile(ConfigFilePath);

    if (cFile.is_open())
    {
        if(getline(cFile, line))
        {
            std::string Id = line.substr(INDEX_8, INDEX_8);
            FileDeviceId = std::stoull(Id, nullptr, BASE_16);
        }
        else
        {
            sd_journal_print(LOG_ERR, "Failed to read device id from config file\n");
            rc = false;
            goto Clean;
        }
    }
    else
    {
        sd_journal_print(LOG_ERR, "Open() config file failed\n");
        return false;
    }

    sd_journal_print(LOG_INFO, "Device ID from config file = 0x%x\n" ,FileDeviceId);

    if(VrDeviceId == FileDeviceId)
    {
        sd_journal_print(LOG_INFO, "VR device id and file devie id matched\n");
    }
    else
    {
        sd_journal_print(LOG_ERR, "VR device id and file devie did not match.Update failed\n");
        rc = false;
        goto Clean;

    }

    /*Device revision verification*/

    //Read IC_DEV_REVISION from the device
    std::fill_n(rdata,MAXIMUM_SIZE,0);

    ret = i2c_smbus_read_i2c_block_data(fd, DEV_REV_REV, BYTE_COUNT_5, rdata);

    if (ret >= SUCCESS)
    {
        DeviceRevision = (rdata[INDEX_1] << SHIFT_24) | (rdata[INDEX_2] << SHIFT_16)
                                      | (rdata[INDEX_3] << SHIFT_8) | rdata[INDEX_4];

        sd_journal_print(LOG_INFO, "Device revision from VR device = 0x%x\n", DeviceRevision);
    }
    else
    {
        sd_journal_print(LOG_ERR, "Failed to read device revision from the VR device\n");
        rc = false;
        goto Clean;
    }

     /*Read device revison from config file*/
    if(getline(cFile, line))
    {
        std::string Rev = line.substr(INDEX_8, INDEX_8);
        FileRevision = std::stoull(Rev, nullptr, BASE_16);
        sd_journal_print(LOG_INFO, "Device revision from config file = 0x%x\n", FileRevision);
    }
    else
    {
        sd_journal_print(LOG_ERR, "Failed to read device revision from config file\n");
        rc = false;
        goto Clean;
    }

    if(DeviceRevision < DEVICE_REVISION)
    {
        sd_journal_print(LOG_ERR, "The device revision number is lesser than 6.0.0.0.Contact Renesas for support\n");
        rc = false;
        goto Clean;
    }

    if(((FileRevision >> SHIFT_24 ) && INT_255) < ((DeviceRevision >> SHIFT_24) && INT_255))
    {
        sd_journal_print(LOG_ERR, "File Revision is less than device revision. Aborting the update\n");
        rc = false;
    }
    else
    {
        sd_journal_print(LOG_ERR, "Device Revision and File Revision matched\n");
        rc = true;
    }

Clean:
    cFile.close();
    return rc;
}

bool vr_update_renesas_gen3::UpdateFirmware()
{
    int ret = FAILURE;
    bool rc = false;
    std::string line;

    std::ifstream cFile(ConfigFilePath);

    if (!cFile.is_open())
    {
        sd_journal_print(LOG_ERR, "Open() Config file failed \n");
        return false;
    }

    /*Parse Hex file and Write to HW*/
    while(getline(cFile, line))
    {
        std::string Header = line.substr(INDEX_0,INDEX_2);
        if(Header.compare("00"))
            continue;

        std::string CommandCode = line.substr(INDEX_6,INDEX_2);
        u_int8_t Command = std::stoul(CommandCode, nullptr, BASE_16);

        std::string ByteCount = line.substr(INDEX_2,INDEX_2);

        if((ByteCount.compare("05")) == STATUS_BIT_0)
        {
            u_int8_t ByteData[INDEX_2] = {0};
            std::string WriteData = line.substr(INDEX_8,INDEX_2);
            ByteData[INDEX_0] = std::stoul(WriteData, nullptr, BASE_16);

            WriteData = line.substr(INDEX_10,INDEX_2);
            ByteData[INDEX_1] = std::stoul(WriteData, nullptr, BASE_16);

            sd_journal_print(LOG_INFO, "0x%x 0x%x 0x%x\n",Command,ByteData[INDEX_0],ByteData[INDEX_1]);

            ret = i2c_smbus_write_i2c_block_data(fd, Command, BYTE_COUNT_2, ByteData);
            if (ret < SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Writing word data to the device failed \n");
                rc = false;
                goto Clean;
            }

        }
        else if((ByteCount.compare("07")) == STATUS_BIT_0)
        {
            u_int8_t WordData[INDEX_4] = {0};
            std::string WriteData = line.substr(INDEX_8,INDEX_2);
            WordData[INDEX_0] = std::stoul(WriteData, nullptr, BASE_16);

            WriteData = line.substr(INDEX_10,INDEX_2);
            WordData[INDEX_1] = std::stoul(WriteData, nullptr, BASE_16);

            WriteData = line.substr(INDEX_12,INDEX_2);
            WordData[INDEX_2] = std::stoul(WriteData, nullptr, BASE_16);

            WriteData = line.substr(INDEX_14,INDEX_2);
            WordData[INDEX_3] = std::stoul(WriteData, nullptr, BASE_16);

            sd_journal_print(LOG_INFO, "0x%x 0x%x 0x%x 0x%x 0x%x\n",Command,WordData[INDEX_0],WordData[INDEX_1],WordData[INDEX_2],WordData[INDEX_3]);

            ret = i2c_smbus_write_i2c_block_data(fd, Command, BYTE_COUNT_4, WordData);
            if (ret < SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Writing block data to the device failed \n");
                rc = false;
                goto Clean;
            }
        }
    }
    rc = true;

Clean:
    cFile.close();
    return rc;
}

bool vr_update_renesas_gen3::ValidateFirmware()
{
    u_int8_t rdata[MAXIMUM_SIZE] = { 0 };
    int length, timeout = 0, ret = 0, status = 0;

    sleep(SLEEP_2);

    //Poll PROGRAMMER_STATUS Register
    while (timeout < INDEX_10)
    {
        timeout++;

        if (i2c_smbus_write_word_data(fd,DMA_WRITE, GEN3_PRGM_STATUS) == SUCCESS)
        {
            ret = i2c_smbus_read_i2c_block_data(fd,DMA_READ, BYTE_COUNT_4, rdata);
            if (ret >= SUCCESS)
            {
                if ((rdata[INDEX_0] & STATUS_BIT_1) == STATUS_BIT_1)
                {
                    status = SUCCESS;
                    break;
                }
                else
                {
                    status = FAILURE;
                }
            }
            else
            {
                sd_journal_print(LOG_ERR, "Poll programmer status register:DMA read failed\n");
                return false;
            }
        }
        else
        {
            sd_journal_print(LOG_ERR, "Poll programmer status register:Setting DMA register failed\n");
            return false;
        }
    }

    //Programming Failure
    if (status == FAILURE)
    {
        sd_journal_print(LOG_INFO, "Bit 0 of programmer status register is 0."
             "Programming has failed.Decoding the bits\n");

        if (((rdata[INDEX_0] >> STATUS_BIT_1) & STATUS_BIT_1) == BIT_ENABLE)
        {
            sd_journal_print(LOG_ERR, "Bit 1 is set.Programming has failed\n");
        }
        if (((rdata[INDEX_0] >> STATUS_BIT_2) & STATUS_BIT_1) == BIT_ENABLE)
        {
            sd_journal_print(LOG_ERR, "The HEX file contains more configurations than are available\n");
        }
        if (((rdata[INDEX_0] >> STATUS_BIT_3) & STATUS_BIT_1) == BIT_ENABLE)
        {
            sd_journal_print(LOG_ERR, "CRC mismatch exists within the configuration data\n");
        }
        if (((rdata[INDEX_0] >> STATUS_BIT_4) & STATUS_BIT_1) == BIT_ENABLE)
        {
            sd_journal_print(LOG_ERR, "CRC check fails on the OTP memory\n");
        }
        if (((rdata[INDEX_0] >> STATUS_BIT_5) & STATUS_BIT_1) == BIT_ENABLE)
        {
            sd_journal_print(LOG_ERR, "Programming has failed! OTP banks consumed\n");
        }
        return false;
    }
    sd_journal_print(LOG_INFO, "PROGRAMMER_STATUS register polling success\n");

    /*Read Bank Status Register*/

    sleep(SLEEP_2);

    if (i2c_smbus_write_word_data(fd,DMA_WRITE, GEN3_BANK_REG) == SUCCESS)
    {
        ret = i2c_smbus_read_i2c_block_data(fd,DMA_READ, BYTE_COUNT_4, rdata);
        if (ret < SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Read bank status register: DMA read failed\n");
            return false;
        }
    }
    else
    {
        sd_journal_print(LOG_ERR, "Read bank status register:Setting DMA address failed\n");
    }

    int BankNumber = 0;

    for(int ByteCount = INDEX_0 ; ByteCount < INDEX_4 ; ByteCount++)
    {
        int StatusData[INDEX_2];
        StatusData[INDEX_0] = rdata[ByteCount] & INT_15;
        StatusData[INDEX_1] = rdata[ByteCount] >> SHIFT_4;

        for(int BankStatus = INDEX_0 ; BankStatus < INDEX_2 ; BankStatus++)
        {
            if(StatusData[BankStatus] == STATUS_BIT_8)
            {
                sd_journal_print(LOG_INFO, "CRC mismatch OTP for bank = %d\n",BankNumber);
                return false;
            }
            else if(StatusData[BankStatus] == STATUS_BIT_4)
            {
                sd_journal_print(LOG_INFO, "CRC mismatch RAM for bank = %d\n", BankNumber);
                return false;
            }
            else if(StatusData[BankStatus] == STATUS_BIT_2)
            {
                sd_journal_print(LOG_INFO, "Reserved : Bank %d\n",BankNumber);;
            }
            else if(StatusData[BankStatus] == STATUS_BIT_1)
            {
                sd_journal_print(LOG_INFO, "Bank Written (No Failures) : Bank %d\n",BankNumber);
            }
            else if(StatusData[BankStatus] == STATUS_BIT_0)
            {
                sd_journal_print(LOG_INFO, " : Unaffected : Bank %d\n",BankNumber);
            }
            BankNumber++;
        }
    }
    return true;
}
