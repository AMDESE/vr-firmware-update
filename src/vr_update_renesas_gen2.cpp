/*
* vr_update_renesas_gen2.cpp
*
* Created on: Nov 10, 2022
* Author: Abinaya Dhandapani
*/

#include "vr_update.hpp"
#include "vr_update_renesas_gen2.hpp"

vr_update_renesas_gen2::vr_update_renesas_gen2(std::string Processor,
          uint32_t Crc,std::string Model,uint16_t SlaveAddress,std::string ConfigFilePath):
          vr_update(Processor,Crc,Model,SlaveAddress,ConfigFilePath)
{

    DriverPath = ISL_DRIVER_PATH;
}

bool vr_update_renesas_gen2::crcCheckSum()
{
    u_int8_t rdata[MAXIMUM_SIZE] = { 0 };
    u_int32_t DeviceCrc;
    int ret = FAILURE;

    //write to DMA Address Register
    ret = i2c_smbus_write_word_data(fd,DMA_WRITE,GEN2_CRC_ADDR );
    if(ret == SUCCESS)
    {
        ret = i2c_smbus_read_i2c_block_data(fd,DMA_READ, BYTE_COUNT_4, rdata);
        if(ret >= SUCCESS)
        {
            DeviceCrc = (rdata[INDEX_3] << SHIFT_24) | (rdata[INDEX_2] << SHIFT_16)
                                    | (rdata[INDEX_1] << SHIFT_8) | rdata[INDEX_0];

            sd_journal_print(LOG_INFO, "CRC from the device = 0x%x\n",DeviceCrc);
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
       CrcMatched = true;
       return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "CRC not matched with the previous image. Continuing the update\n");
        CrcMatched = false;
        return true;
    }
}

bool vr_update_renesas_gen2::isUpdatable()
{
    u_int8_t rdata[MAXIMUM_SIZE] = { 0 };
    int ret = FAILURE;
    bool rc = false;

    /*Determine number of NVM slots available*/

    std::fill_n(rdata,MAXIMUM_SIZE,0);
    u_int8_t AvailableNvmSlots = 0;

    ret = i2c_smbus_write_word_data(fd,DMA_WRITE, GEN2_NVM_SLOT_ADDR);

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
        sd_journal_print(LOG_ERR,"Find Available NVM slots: Setting DMA address failed\n");
        return false;
    }

    /*Device ID validation*/

    /*Read Device Id from the device*/
    std::fill_n(rdata,MAXIMUM_SIZE,0);
    u_int8_t FileDeviceId = 0;
    u_int8_t VrDeviceId = 0;
    std::string line;
    uint32_t DeviceRevision = 0;
    uint32_t FileRevision = 0;

    ret = i2c_smbus_read_i2c_block_data(fd, DEV_ID_CMD, BYTE_COUNT_5, rdata);

    if(ret >= SUCCESS)
    {
        VrDeviceId = rdata[INDEX_2];
        sd_journal_print(LOG_INFO, "Device ID from the VR = %d\n",VrDeviceId);
    }
    else
    {
        sd_journal_print(LOG_ERR,"Failed to read device ID from the VR device\n");
        return false;
    }

    /*Read device id from the config file*/
    std::ifstream cFile(ConfigFilePath);

    if (cFile.is_open())
    {
        if(getline(cFile, line))
        {
            std::string Id = line.substr(INDEX_12, INDEX_2);
            FileDeviceId = std::stoull(Id, nullptr, BASE_16);
        }
        else
        {
            sd_journal_print(LOG_ERR,"Failed to read device id from config file\n");
            rc = false;
            goto Clean;
        }
    }
    else
    {
        sd_journal_print(LOG_ERR,"Open() config file failed\n");
        return false;
    }

    sd_journal_print(LOG_INFO, "Device ID from config file = %d\n",FileDeviceId);

    if(VrDeviceId == FileDeviceId)
    {
        sd_journal_print(LOG_INFO, "VR device id and file devie id matched\n");
    }
    else
    {
        sd_journal_print(LOG_ERR,"VR device id and file devie did not match.Update failed\n");
        rc = false;
        goto Clean;

    }

    /*Device revision verification*/

    //Read IC_DEV_REVISION from the device
    std::fill_n(rdata,MAXIMUM_SIZE,0);

    ret = i2c_smbus_read_i2c_block_data(fd, DEV_REV_REV, BYTE_COUNT_5, rdata);

    if (ret >= SUCCESS)
    {
        DeviceRevision = (rdata[INDEX_4] << SHIFT_24) | (rdata[INDEX_3] << SHIFT_16)
                     | (rdata[INDEX_2] << SHIFT_8) | rdata[INDEX_1];

        sd_journal_print(LOG_INFO, "Device revision from VR device = 0x%x\n",DeviceRevision);
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
        sd_journal_print(LOG_INFO, "Device revision from config file = 0x%x\n",FileRevision);
    }
    else
    {
        sd_journal_print(LOG_ERR, "Failed to read device revision from config file\n");
        rc = false;
        goto Clean;
    }

    if(DeviceRevision < DEVICE_REVISION)
    {
        sd_journal_print(LOG_ERR, "The device revision number is lesser than 2.0.0.3.Contact Renesas for support\n");
        rc = false;
        goto Clean;
    }

    if (((FileRevision >> SHIFT_24 ) & INT_255) == ((DeviceRevision >> SHIFT_24) & INT_255))
    {
        sd_journal_print(LOG_INFO, "File Revision matched with Device Revision\n");
    }
    else
    {
        sd_journal_print(LOG_ERR, "File Revision didnot match with Device Revision. Aborting the update\n");
        rc = false;
        goto Clean;
    }
    rc = true;

Clean:
    cFile.close();
    return rc;
}

bool vr_update_renesas_gen2::UpdateFirmware()
{
    int ret = FAILURE;
    bool rc = false;
    std::string line;

    std::ifstream cFile(ConfigFilePath);

    if (!cFile.is_open())
    {
        sd_journal_print(LOG_ERR, "Open() Config file failed\n");
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

        if((ByteCount.compare("05")) == 0)
        {
            u_int8_t ByteData[INDEX_2] = {0};
            std::string WriteData = line.substr(INDEX_8,INDEX_2);
            ByteData[INDEX_0] = std::stoul(WriteData, nullptr, BASE_16);

            WriteData = line.substr(INDEX_10,INDEX_2);
            ByteData[INDEX_1] = std::stoul(WriteData, nullptr, BASE_16);

            sd_journal_print(LOG_DEBUG, "0x%x 0x%x 0x%x\n",Command,ByteData[INDEX_0],ByteData[INDEX_1]);

            ret = i2c_smbus_write_i2c_block_data(fd, Command, BYTE_COUNT_2, ByteData);

            if (ret < SUCCESS)
            {
                sd_journal_print(LOG_ERR, "Writing word data to the device failed \n");
                rc = false;
                goto Clean;
            }

        }
        else if((ByteCount.compare("07")) == 0)
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

            sd_journal_print(LOG_DEBUG, "0x%x 0x%x 0x%x 0x%x 0x%x\n",Command,
                                WordData[INDEX_0],WordData[INDEX_1],WordData[INDEX_2],WordData[INDEX_3]);

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

bool vr_update_renesas_gen2::ValidateFirmware()
{
    u_int8_t rdata[MAXIMUM_SIZE] = { 0 };
    int length, timeout = 0, ret = 0, status = 0;

    usleep(SLEEP_2);

    //Poll PROGRAMMER_STATUS Register
    while (timeout < INDEX_10)
    {
        timeout++;

        if (i2c_smbus_write_word_data(fd,DMA_WRITE, GEN2_PRGM_STATUS) == SUCCESS)
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
        sd_journal_print(LOG_ERR, "Bit 0 of programmer status register is 0."
                     "Programming has failed. Decoding the bits\n");

        if (((rdata[INDEX_0] >> SHIFT_4) & STATUS_BIT_1) == BIT_ENABLE) {
            sd_journal_print(LOG_ERR, "CRC mismatch exists within the configuration data\n");
        }
        if (((rdata[INDEX_0] >> SHIFT_6) & STATUS_BIT_1) == BIT_ENABLE) {
            sd_journal_print(LOG_ERR, "the CRC check fails on the OTP memory\n");
        }
        if ((rdata[INDEX_1] & STATUS_BIT_1) == BIT_ENABLE) {
            sd_journal_print(LOG_ERR, "The HEX file contains more configurations than are available\n");
        }
        return false;
    }
    sd_journal_print(LOG_ERR, "PROGRAMMER_STATUS register polling success\n");

	/*Read Bank Status Register Bank - 0 to 7*/

    usleep(SLEEP_2);

    if (i2c_smbus_write_word_data(fd,DMA_WRITE, GEN2_0_7_BANK_REG) == SUCCESS)
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

    int ByteCount = 0;
    int BankNumber = 0;

    for(ByteCount = INDEX_0 ; ByteCount < INDEX_4 ; ByteCount++)
    {
        int StatusData[INDEX_2];
        StatusData[INDEX_0] = rdata[ByteCount] & INT_15;
        StatusData[INDEX_1] = rdata[ByteCount] >> SHIFT_4;

        for(int BankStatus = INDEX_0 ; BankStatus < INDEX_2 ; BankStatus++)
        {
            if(StatusData[BankStatus] == STATUS_BIT_8)
            {
                sd_journal_print(LOG_ERR, "CRC mismatch OTP for bank = %d\n",BankNumber);
                return false;
            }
            else if(StatusData[BankStatus] == STATUS_BIT_4)
            {
                sd_journal_print(LOG_ERR, "CRC mismatch RAM for bank = %d\n",BankNumber);
                return false;
            }
            else if(StatusData[BankStatus] == STATUS_BIT_2)
            {
                sd_journal_print(LOG_ERR, "Bank %d : Reserved\n",BankNumber);
            }
            else if(StatusData[BankStatus] == STATUS_BIT_1)
            {
                sd_journal_print(LOG_ERR, "Bank %d : Bank Written (No Failures) \n",BankNumber);
            }
            else if(StatusData[BankStatus] == STATUS_BIT_0)
            {
                sd_journal_print(LOG_ERR, "Bank %d : Unaffected\n",BankNumber);
            }
            BankNumber++;
        }
    }

    /*Read Bank Status Register Bank - 8 to 15*/
    if (i2c_smbus_write_word_data(fd,DMA_WRITE, GEN2_8_15_BANK_REG) == SUCCESS)
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

    for(ByteCount = INDEX_0 ; ByteCount < INDEX_4 ; ByteCount++)
    {
        int StatusData[INDEX_2];
        StatusData[INDEX_0] = rdata[ByteCount] & INT_15;
        StatusData[INDEX_1] = rdata[ByteCount] >> SHIFT_4;

        for(int BankStatus = INDEX_0 ; BankStatus < INDEX_2 ; BankStatus++)
        {
            if(StatusData[BankStatus] == STATUS_BIT_8)
            {
                sd_journal_print(LOG_ERR, "CRC mismatch OTP for bank = %d\n",BankNumber);
                return false;
            }
            else if(StatusData[BankStatus] == STATUS_BIT_4)
            {
                sd_journal_print(LOG_ERR, "CRC mismatch RAM for bank = %d\n",BankNumber);
                return false;
            }
            else if(StatusData[BankStatus] == STATUS_BIT_2)
            {
                sd_journal_print(LOG_ERR, "Bank %d : Reserved\n",BankNumber);
            }
            else if(StatusData[BankStatus] == STATUS_BIT_1)
            {
                sd_journal_print(LOG_ERR, "Bank %d : Bank Written (No Failures)\n",BankNumber);
            }
            else if(StatusData[BankStatus] == STATUS_BIT_0)
            {
                sd_journal_print(LOG_ERR, "Bank %d Unaffected\n",BankNumber);
            }
            BankNumber++;
        }
    }

    return true;
}
