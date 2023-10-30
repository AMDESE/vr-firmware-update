/*
* vr_update_infineon_tda.cpp
*
* Created on: Nov 10, 2022
* Author: Abinaya Dhandapani
*/

#include "vr_update.hpp"
#include "vr_update_infineon_tda.hpp"

vr_update_infineon_tda::vr_update_infineon_tda(std::string Processor,
          uint32_t Crc,std::string Model,uint16_t SlaveAddress,std::string ConfigFilePath):
          vr_update(Processor,Crc,Model,SlaveAddress,ConfigFilePath)
{

    DriverPath = PMBUS_DRIVER_PATH;
}

bool vr_update_infineon_tda::crcCheckSum()
{

    uint64_t UserImgPtr = 0;
    uint64_t mask = 1;
    uint16_t rdata = 0;
    int i;
    int rc = FAILURE;

    /* Change to page 0 by writing 0 to register 0xFF */
    rc = i2c_smbus_write_byte_data(fd, PAGE_NUM_REG, INDEX_0);

    if (rc != SUCCESS) {
        sd_journal_print(LOG_ERR, "Error: Changing page number to 0 failed\n");
        return false;
    }

    /* Read register 0x0B4 [15:0] + 0x0B6 [15:0] + 0x0B8 [15:0]
       to determine next USER image pointer*/

    rdata = i2c_smbus_read_word_data(fd,USER_IMG_PTR3);
    if (rdata < 0)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
        return false ;
    }

    UserImgPtr = UserImgPtr | rdata;

    rdata = i2c_smbus_read_word_data(fd,USER_IMG_PTR2);

    if (rdata < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
        return false;
    }

    UserImgPtr = UserImgPtr | ((uint64_t )rdata << BASE_16) ;

    rdata = i2c_smbus_read_word_data(fd,USER_IMG_PTR1);

    if (rdata < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
        return false;
    }

    UserImgPtr = UserImgPtr | ((uint64_t )rdata << INDEX_32) ;

    for(i = INDEX_0 ; i < INDEX_48 ; i++)
    {
        if( (UserImgPtr & ( mask << i)) == SUCCESS)
        {
            break;
        }
    }
    NextImgPtr = i;

    /*Check Previous Image CRC*/

    /*Send user section command*/
    int CrcImgPtr = NextImgPtr - INDEX_1;
    int UserSectionCmd = USER_READ_CMD;

    UserSectionCmd = ( CrcImgPtr << INDEX_8) | UserSectionCmd;

    rc = i2c_smbus_write_word_data(fd, USER_PROG_CMD , UserSectionCmd);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error:User Program Command: Failed to write data\n");
        return false;
    }

    /*Wait for 200ms */
    usleep(200 * 1000);

    /*Check program Status*/
    /*Check register 0xD7 [7] for programming progress.
      0 = fail. 1 = done*/

    uint8_t read_data = 0;

    read_data = i2c_smbus_read_byte_data(fd,PROG_STATUS_REG);

    if (read_data < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
        return false;
    }

    if((read_data & USER_PROG_STATUS) == 0)
    {
        sd_journal_print(LOG_ERR, "User section programming failed\n");
        return false;
    }

    rdata = i2c_smbus_read_word_data(fd,CRC_REG_1);

    if (rdata < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
        return false;
    }

    uint32_t DeviceCrcData = 0;

    DeviceCrcData = DeviceCrcData | ((uint32_t )rdata << BASE_16) ;

    rdata = i2c_smbus_read_word_data(fd,CRC_REG_2);
    if (rdata < SUCCESS)
    {
        sd_journal_print(LOG_ERR,"Error: Failed to read data\n");
        return false;
    }
    DeviceCrcData = DeviceCrcData | rdata;

    sd_journal_print(LOG_INFO, "CRC from the device = 0x%x", DeviceCrcData);
    sd_journal_print(LOG_INFO, "CRC from the manifest file = 0x%x ", Crc );

    if(DeviceCrcData == Crc)
    {
        sd_journal_print(LOG_ERR, "CRC matches with previous image. Skipping the update\n");
        CrcMatched = true;
        return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "CRC didnot match with previous image. Continuing the update\n");
        CrcMatched = false;
    }


    return true;
}

bool vr_update_infineon_tda::isUpdatable()
{

    if(NextImgPtr > 40 )
    {
        std::cout << "OTP for user section is not available\n";
        return false;
    }
    else
    {
        return true;
    }
}

bool vr_update_infineon_tda::UpdateFirmware()
{

    uint16_t rdata = 0;
    int rc = FAILURE;

    /* Write data 0x03 to register 0xD4 to
       unlock i2 and PMBus address registers*/
    rc = i2c_smbus_write_byte_data(fd, UNLOCK_REG, 0x03);
    if (rc != SUCCESS)
    {
        std::cout << "Error: Failed to write data" << std::endl;
        return false;
    }

    /*Write user section data*/
   std::fstream newfile;
   newfile.open(ConfigFilePath,std::ios::in);

   if (newfile.is_open())
   {
      std::string tp;
      int data_section = 0;
      int current_page_num = -1;
      while(getline(newfile, tp))
      {
            /*Starting of user section */
            if (tp.find("[Config Data]") != std::string::npos) {
                data_section = 1;
                continue;
            }

            /*Starting of user section */
            if (tp.find("[Configuration Data]") != std::string::npos) {
                data_section = 1;
                continue;
            }

            /*Ending of user section */
            if (tp.find("[End Config Data]") != std::string::npos) {
                data_section = 1;
                break;
            }

            /*Ending of user section */
            if (tp.find("[End Configuration Data]") != std::string::npos) {
                data_section = 1;
                break;
            }

            if(data_section == INDEX_1)
            {
                int index = 0;
                int page_number = 0;
                std::string page_num;
                std::string word;
                std::string starting_index;
                std::string userdata;
                int index_num = 0;
                int user_data  = 0;
                int index_range = 0;

                std::stringstream iss(tp);
                while (iss >> word)
                {
                    /*First word which containes page num and index*/
                    if(index == INDEX_0 )
                    {
                            index_range = std::stoi(word, nullptr, BASE_16);
                            if(index_range < INDEX_40)
                                break;
                            else if(index_range > INDEX_70 && index_range < INDEX_200)
                                break;
                            else if(index_range > INDEX_2FF)
                                break;

                            std::cout << word << std::endl;
                            page_num = "";
                            page_num.push_back(word[INDEX_0]);
                            page_num.push_back(word[INDEX_1]);
                            page_number = std::stoi(page_num, nullptr, BASE_16);


                            if(current_page_num != page_number)
                            {
                                current_page_num = page_number;
                                sd_journal_print(LOG_DEBUG, "Writing page number register 0x%x\n",page_number);
                                rc = i2c_smbus_write_byte_data(fd, PAGE_NUM_REG, page_number);
                                if (rc != SUCCESS) {
                                    std::cout << "Error: Failed to write data" << std::endl;
                                    return false;
                                }
                            }
                            starting_index="";
                            starting_index.push_back(word[INDEX_2]);
                            starting_index.push_back(word[INDEX_3]);
                            index_num = std::stoi(starting_index, nullptr, BASE_16);
                            index++;
                            continue;
                    }

                    userdata = "";
                    userdata.push_back(word[INDEX_0]);
                    userdata.push_back(word[INDEX_1]);
                    user_data = std::stoi(userdata, nullptr, BASE_16);
                    sd_journal_print(LOG_DEBUG, "Index Number = 0x%x User Data = 0x%x\n",index_num,user_data);

                    if (rc != SUCCESS) {
                        sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
                        return false;
                    }
                    index_num++;
                }
            }
        }
        newfile.close();
    }

    int UserSectionCmd = 0x42;

    UserSectionCmd = ( NextImgPtr << INDEX_8) | UserSectionCmd;

    rc = i2c_smbus_write_word_data(fd, USER_PROG_CMD , UserSectionCmd);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error:User Program Command: Failed to write data\n");
        return false;
    }

    /*Wait for 200ms */
    usleep(200 * 1000);

    return true;
}

bool vr_update_infineon_tda::ValidateFirmware()
{
    /*Check program Status*/
    /*Check register 0xD7 [7] for programming progress.
      0 = fail. 1 = done*/

    uint8_t read_data = 0;

    read_data = i2c_smbus_read_byte_data(fd,PROG_STATUS_REG);

    if (read_data < SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
        return false;
    }

    if((read_data & USER_PROG_STATUS) == SUCCESS)
    {
        sd_journal_print(LOG_ERR, "User section programming failed\n");
        return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "User section programming success\n");
        return true;
    }
}
