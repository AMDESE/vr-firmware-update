/*
* vr_update_renesas_patch.cpp
*
* Created on: Nov 10, 2022
* Author: Abinaya Dhandapani
*/

#include "vr_update.hpp"
#include "vr_update_renesas_patch.hpp"

struct PMBusPayload
{
   char flag;
   u_int8_t command;
   u_int32_t data;
};

vr_update_renesas_patch::vr_update_renesas_patch(std::string Processor,
          uint32_t Crc,std::string Model, uint16_t SlaveAddress,std::string ConfigFilePath,std::string Revision):
          vr_update(Processor,Crc,Model,SlaveAddress,ConfigFilePath,Revision)
{

    DriverPath = ISL_DRIVER_PATH;
}

bool vr_update_renesas_patch::crcCheckSum()
{
    CrcMatched = false;
    return true;
}

bool vr_update_renesas_patch::isUpdatable()
{
    u_int8_t rdata[MAXIMUM_SIZE] = { 0 };
    int length;
    int ret = 0;
    u_int32_t DeviceId = 0;

    ret = i2c_smbus_write_word_data(fd, DMA_WRITE, DEVICE_FW_VERSION);
    if(ret == SUCCESS)
    {
        ret = i2c_smbus_read_i2c_block_data(fd, DMA_READ, BYTE_COUNT_4, rdata);

        if (ret >= SUCCESS)
        {

            DeviceId = (rdata[INDEX_3] << SHIFT_24) |
                        (rdata[INDEX_2] << SHIFT_16) |
                        (rdata[INDEX_1] << SHIFT_8) | rdata[INDEX_0];

            sd_journal_print(LOG_INFO, "Device Id from VR devcie = 0x%x\n",DeviceId);

            if(DeviceId == CANDIDATE_FW)
            {
                sd_journal_print(LOG_INFO, "Patch update is allowed for this device. Updating the patch\n");
                return true;
            }
            else
            {
                sd_journal_print(LOG_ERR, "Patch update is not allowed for this device. Aborting the update\n");
                return false;
            }
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
}

bool vr_update_renesas_patch::UpdateFirmware()
{

    int ret = 0;

    /*Halt device firmware*/
    ret = i2c_smbus_write_word_data(fd, FW_WRITE, HALT_FW);
    if (ret < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "halt device fw:Setting DMA register failed\n");
        return false;
    }

    usleep(SLEEP_1000);

    /*Write patch data to the device*/
    std::string command;
    std::fstream newfile;

    newfile.open(ConfigFilePath,std::ios::in);

    if (newfile.is_open())
    {
        std::string HexfileData;
        //Read hex file line by line
        while(getline(newfile, HexfileData))
        {
            struct PMBusPayload pmbus_payload;
            std::string data;
            int len = HexfileData.size();
            pmbus_payload.flag = HexfileData[INDEX_0];

            /* "w" in the hex file indicates that
            the line should be written to the hw*/
            //if(pmbus_payload.flag != 'w')
            //    continue;

            /*Extract command code */
            command = HexfileData.substr(INDEX_2,INDEX_2);
            //pmbus_payload.command = std::stoul(command, nullptr, BASE_16);

            /*Extract pmbus data*/
            data = HexfileData.substr(INDEX_5,len - INDEX_5);
            pmbus_payload.data = std::stoul(data, nullptr, BASE_16);


            if(pmbus_payload.command == CMD_CODE_C6)
            {
                u_int8_t write_pmbus_data[INDEX_4] = {0};
                write_pmbus_data[INDEX_3] = ((pmbus_payload.data & MASK_BYTE_4) >> SHIFT_24);
                write_pmbus_data[INDEX_2] = ((pmbus_payload.data & MASK_BYTE_3) >> SHIFT_16);
                write_pmbus_data[INDEX_1] = ((pmbus_payload.data & MASK_BYTE_2) >> SHIFT_8);
                write_pmbus_data[INDEX_0] = (pmbus_payload.data & INT_255);


                ret = i2c_smbus_write_i2c_block_data(fd, pmbus_payload.command, BYTE_COUNT_4, write_pmbus_data);
                if (ret < SUCCESS)
                {
                    sd_journal_print(LOG_ERR, "Writing block data to the device failed\n");
                    return false;
                }
            }
            else
            {
                u_int16_t write_pmbus_data;

                if(pmbus_payload.command == CMD_CODE_C7)
                {
                    write_pmbus_data = pmbus_payload.data & MASK_TWO_BYTES;
                }
                else if(pmbus_payload.command == CMD_CODE_E6)
                {
                   write_pmbus_data = (u_int16_t)pmbus_payload.data;
                }

                ret = i2c_smbus_write_word_data(fd, pmbus_payload.command , write_pmbus_data);
                if (ret < SUCCESS)
                {
                    sd_journal_print(LOG_ERR, "Writing word data to the device failed\n");
                    return false;
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

    if (i2c_smbus_write_word_data(fd,CMD_CODE_E6, COMMIT_DATA) == SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Commit patch to NVM : Setting DMA register failed\n");
        return false;
    }
    else
    {
       sd_journal_print(LOG_ERR, "Patch committed to NVM\n");
       return true;
    }
}

bool vr_update_renesas_patch::ValidateFirmware()
{
    u_int8_t rdata[MAXIMUM_SIZE] = { 0 };
    int length, timeout = 0, ret = 0, status = FAILURE;

    usleep(SLEEP_1);

    //Poll PROGRAMMER_STATUS Register
    while (timeout < MAX_RETRY)
    {
        timeout++;
        usleep(SLEEP_1000);

        ret = i2c_smbus_write_word_data(fd,DMA_WRITE, POLL_REG);

        if(ret == SUCCESS)
        {
            ret = i2c_smbus_read_i2c_block_data(fd,DMA_READ, BYTE_COUNT_4, rdata);
            if(ret >= SUCCESS)
            {
                if ((rdata[INDEX_3] & COMPLETE_BIT))
                {
                    if((rdata[INDEX_3] & PASS_BIT))
                    {
                        status = SUCCESS;
                        sd_journal_print(LOG_ERR, "Patch update Succeeded. Please do AC cycle\n");
                        break;
                    }
                    else if((rdata[INDEX_3] & FAIL_BIT))
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
                sd_journal_print(LOG_ERR, "Poll programmer status register: Read DMA register failed\n");
                return false;
            }
        }
        else
        {
            sd_journal_print(LOG_ERR, "Poll programmer status register: Setting DMA register address failed\n");
            return false;
        }
    }

    if(status  == FAILURE)
    {
        sd_journal_print(LOG_ERR, "Patch update failed\n");
        return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "Patch update success\n");
        return true;
    }
}
