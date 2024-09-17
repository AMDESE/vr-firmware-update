/*
* vr_update_infineon_xdpe.cpp
*
* Created on: Nov 10, 2022
* Author: Abinaya Dhandapani
*/

#include "vr_update.hpp"
#include "vr_update_infineon_xdpe.hpp"

int partial_pmbus_section_count_number = 0;
uint16_t partial_crc = 0;

vr_update_infineon_xdpe::vr_update_infineon_xdpe(std::string Processor,uint32_t Crc,
          std::string Model,uint16_t SlaveAddress,std::string ConfigFilePath,std::string Revision,uint16_t PmbusAddress):
          vr_update(Processor,Crc,Model,SlaveAddress,ConfigFilePath,Revision,PmbusAddress)
{

    DriverPath = XDPE_DRIVER_PATH;
}

bool vr_update_infineon_xdpe::crcCheckSum()
{

    uint32_t DeviceCrcData = 0;
    int rc = FAILURE;
    uint8_t wdata[MAXBUFFERSIZE];
    uint8_t rdata[MAXBUFFERSIZE];

    memset(wdata, INDEX_0, MAXBUFFERSIZE);

    rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);
    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR,
        "Error: Failed to write block data: 0x%x 0x%x 0x%x 0x%x using "
        "PMBUS Cmd: BLOCK_WRITE (0x%x), rc=%d\n",
        wdata[0], wdata[1], wdata[2], wdata[3], BLOCK_PREFIX, rc);
        return false;
    }

    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, GET_CRC);
    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR,
        "Error: Failed to read CRC using "
        "MFR Cmd: GET_CRC (0x%x), rc=%d\n",
        GET_CRC, rc);
        return false;
    }

    /*Wait for 20ms */
    usleep(GETCRCWAITTIME);

    rc = i2c_smbus_read_block_data(fd, BLOCK_PREFIX, rdata);
    if (rc > LENGTH_0)
    {
        DeviceCrcData = ((uint32_t)rdata[INDEX_3] << SHIFT_24) | 
                        ((uint32_t)rdata[INDEX_2] << SHIFT_16) | 
                        ((uint32_t)rdata[INDEX_1] << SHIFT_8) | 
                        (uint32_t)rdata[INDEX_0];
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
        return false;
    }

    sd_journal_print(LOG_INFO, "CRC from the device = 0x%x\n", DeviceCrcData);
    sd_journal_print(LOG_INFO, "CRC from the manifest file = 0x%x\n", Crc);

    if(DeviceCrcData == Crc)
    {
        sd_journal_print(LOG_ERR, 
        "CRC matches with previous image. Skipping the update\n");
        CrcMatched = true;
        return false;
    }
    else
    {
        sd_journal_print(LOG_ERR, 
        "CRC didnot match with previous image. Continuing the update\n");
        CrcMatched = false;
    }
    return true;
}

bool vr_update_infineon_xdpe::isUpdatable()
{

    uint8_t wdata[MAXBUFFERSIZE];
    uint8_t rdata[MAXBUFFERSIZE];
    int size;
    int rc = FAILURE;

    sd_journal_print(LOG_DEBUG, 
    "Reading the Product ID and Revision code using " 
    "PMBus Cmd: IC_DEVICE_ID (0x%x)", DEVICE_ID_CMD);

    rc = i2c_smbus_read_block_data(fd, DEVICE_ID_CMD, rdata);
    if (rc > LENGTH_0)
    {
        if (rdata[INDEX_1] == PART1 || rdata[INDEX_1] == PART2 ||
              rdata[INDEX_1] == PART3 || rdata[INDEX_1] == PART4 ||
              rdata[INDEX_1] == PART5 || rdata[INDEX_1] == PART6 ||
              rdata[INDEX_1] == PART7)
        {
            if (((rdata[INDEX_0]) == REVISION_0) &&
                ((strcasecmp(Revision.c_str(), REV_A)) == SUCCESS)) 
            {
                sd_journal_print(LOG_INFO, "Revision A matched\n");
            }
            else if ((rdata[INDEX_0] == REVISION_1) && 
                ((strcasecmp(Revision.c_str(), REV_B)) == SUCCESS)) 
            {
                sd_journal_print(LOG_INFO, "Revision B matched\n");
            }
            else if ((rdata[INDEX_0] == REVISION_2) && 
                ((strcasecmp(Revision.c_str(), REV_C)) == SUCCESS)) 
            {
                sd_journal_print(LOG_INFO, "Revision C matched\n");
            }
            else if ((rdata[INDEX_0] == REVISION_3) && 
                ((strcasecmp(Revision.c_str(), REV_D)) == SUCCESS)) 
            {
                sd_journal_print(LOG_INFO, "Revision D matched\n");
            }
            else 
            {
                sd_journal_print(LOG_ERR, 
                "VR update failed: Invalid revision 0x%x\n", rdata[INDEX_0]);
                return false;
            }
        }
        else 
        {
            sd_journal_print(LOG_ERR, 
            "VR update failed: Invalid Product ID 0x%x\n", rdata[INDEX_1]);
            return false;
        }
    } 
    else 
    {
        sd_journal_print(LOG_ERR,
        "Failed to read Product ID and Revision code using PMBus Cmd: "
        "IC_DEVICE_ID (0x%x)", DEVICE_ID_CMD);
        return false;
    }

    /*Check if space available on OTP*/
    memset(wdata, INDEX_0, MAXBUFFERSIZE);

    rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);
    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, 
        "Error: Failed to write block data: 0x%x 0x%x 0x%x 0x%x using "
        "PMBUS Cmd: BLOCK_WRITE (0x%x), rc=%d\n", 
        wdata[0], wdata[1], wdata[2], wdata[3], BLOCK_PREFIX, rc);
        return false;
    }

    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, AVAIL_SPACE_BYTE);
    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, 
        "Error: Failed to read available space using "
        "MFR Cmd: OTP_PARTITION_SIZE_REMAINING (0x%x), rc=%d\n", 
        AVAIL_SPACE_BYTE, rc);
        return false;
    }

    //sleep 1ms
    usleep(AVAILBYTEWAITTIME);
    rc = i2c_smbus_read_block_data(fd, BLOCK_PREFIX, rdata);
    if (rc > LENGTH_0)
    {
        // size = d0 + 256 * d1. Formula provided in Infineon document
        size = (256 * rdata[INDEX_1] + rdata[INDEX_0]);
        sd_journal_print(LOG_INFO, "Available size (bytes): %d\n",size);
    }
    else
    {
        sd_journal_print(LOG_ERR, 
        "Error: Failed to read block data using "
        "PMBUS Cmd: BLOCK_READ (0x%x), rc=%d\n", BLOCK_PREFIX, rc);
        return false;
    }

    if (size < MINOTPSIZE)
    {
        sd_journal_print(LOG_ERR, "Available space is less, program manually\n");
        return false;
    }
    else
    {
        sd_journal_print(LOG_INFO, "Proceeding with VR programming\n");
    }

    return true;
}

std::vector<std::vector<std::string>> get_each_section_data(std::string data)
{

    std::vector<std::string> blobs;
    std::string start_pat = "[Configuration Data]";
    std::string end_pat = "[End Configuration Data]";
    size_t start_delim = data.find(start_pat);
    size_t end_delim = data.find(end_pat);
    data = data.substr(start_delim + start_pat.length(), end_delim - start_delim - start_pat.length());
    unsigned first = data.find("//XV");
    int section_count = 0;
    bool partial_section_found = false;
    std::string crc;

    while(first <= data.length())
    {
        unsigned second = data.find("//XV", first + INDEX_2);    // Adjust position of index to get required data
        std:: string blob = data.substr(first + INDEX_2, second - first - INDEX_2);
        std::stringstream ss(blob);
        std::string line;
        std::string n_blob = "";

        while(getline(ss, line))
        {
            if(size_t foundxv = line.find("XV") != std::string::npos)
            {
                section_count++;
                blob.erase(foundxv - INDEX_1, line.length());
                if(size_t foundxv = line.find("XV0 Partial") != std::string::npos)
                {
                   partial_pmbus_section_count_number = section_count;
                   partial_section_found = true;
                } else {
                   partial_section_found = false;
                }
            }
            else
            {
                std::string row_num = line.substr(INDEX_0,INDEX_4);    // Row number always contains 3 digits hence (0,4)
                if(size_t foundrn = line.find(row_num) != std::string::npos)
                {
                    line.erase(foundrn - INDEX_1, row_num.length());
                    n_blob.append(line + " ");
                    if(partial_section_found == true && (row_num.compare("000 ") == SUCCESS))
                    {
                        std::string crc = line.substr(13,INDEX_4);
                        partial_crc = partial_crc + std::stoi(crc, NULL, BASE_16);
                    }
                }
            }
        }
        blobs.push_back(n_blob);
        first = second;
    }
    std::vector<std::vector<std::string>> matrix;
    for (int i = 0; i < blobs.size(); i++)
    {
        std::vector<std::string> row;
        std::stringstream sd(blobs[i]);
        std::string dword;
        while (getline(sd, dword, ' '))
        {
            row.push_back(dword);
        }
        matrix.push_back(row);
    }
    sd_journal_print(LOG_DEBUG, "Partial section number = %d Partial CRC = %d \n",
                        partial_pmbus_section_count_number,partial_crc);

    return matrix;
}

std::vector<uint8_t> formatDword(std::string s_dword)
{
    unsigned long ul;
    std::vector<uint8_t> v_dword;
    uint8_t dword[INDEX_4] = {0};

    ul = std::strtoul(s_dword.c_str(), NULL, BASE_16);
    memcpy(dword,&ul,INDEX_4);

    for(int i=0; i<INDEX_4; i++)
    {
        v_dword.push_back(dword[i]);
    }

    return v_dword;
}

std::string get_file_contents(std::string filename)
{
  std::cout << filename << std::endl;
  std::ifstream in(filename, std::ifstream::in);
  if (in)
  {
    std::ostringstream contents;
    contents << in.rdbuf();
    in.close();
    return(contents.str());
  }
  throw(errno);
}

std::vector<std::vector<std::string>> parseCfgFile(std::string filename)
{
    std::string content = get_file_contents(filename);

    std::vector<std::vector<std::string>> sections = get_each_section_data(content);
    return sections;
}

int invalidateOtp(uint8_t xv, uint8_t hc,int fd)
{
    uint8_t wdata[MAXBUFFERSIZE];
    int rc = FAILURE;
    wdata[INDEX_0] = hc;
    wdata[INDEX_1] = xv;
    wdata[INDEX_2] = 0x00;
    wdata[INDEX_3] = 0x00;

    sd_journal_print(LOG_DEBUG, "Invalidate OTP Block Write with cmd 0xfd\n");
    sd_journal_print(LOG_DEBUG, "data: 0x%x 0x%x 0x%x 0x%x\n",
                     wdata[INDEX_3], wdata[INDEX_2], wdata[INDEX_1], wdata[INDEX_0]);

    rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);
    if(rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, 
        "Error: Failed to write block data: 0x%x 0x%x 0x%x 0x%x using "
        "PMBUS Cmd: BLOCK_WRITE (0x%x), rc=%d\n", 
        wdata[0], wdata[1], wdata[2], wdata[3], BLOCK_PREFIX, rc);
        return FAILURE;
    }

    sd_journal_print(LOG_DEBUG, "Invalidate OTP Byte Write with cmd 0xfe\n");
    sd_journal_print(LOG_DEBUG, "0x%x\n",INVAL_BYTE);

    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, INVAL_BYTE);

    //sleep for 4ms
    usleep(INVALBYTEWAITTIME);

    if(rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, 
        "Error: Failed Invalidate OTP using "
        "MFR Cmd: OTP_SECTION_INVALIDATE (0x%x), rc=%d\n", INVAL_BYTE, rc);
        return FAILURE;
    }

    return SUCCESS;
}

int writeDataToScratchpad(std::vector<std::string> section,int fd)
{
    uint8_t wdata[MAXBUFFERSIZE];
    int rc = FAILURE;
    wdata[INDEX_0] = 0x2;
    wdata[INDEX_1] = 0x0;
    wdata[INDEX_2] = 0x0;
    wdata[INDEX_3] = 0x0;

    sd_journal_print(LOG_DEBUG, "ScratchPad Initial Block Write with cmd 0xce\n");
    sd_journal_print(LOG_DEBUG, "data: 0x%x 0x%x 0x%x 0x%x\n", 
                     wdata[INDEX_3], wdata[INDEX_2], wdata[INDEX_1], wdata[INDEX_0]);

    rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);
    if(rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, 
        "Error: Failed to write block data: 0x%x 0x%x 0x%x 0x%x using "
        "PMBUS Cmd: BLOCK_WRITE (0x%x), rc=%d\n",
        wdata[0], wdata[1], wdata[2], wdata[3], BLOCK_PREFIX, rc);
        return FAILURE;
    }

    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, GET_FW_ADDRESS);
    if(rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, 
        "Error: Failed to retrieve SCPAD address using " 
        "MFR Cmd: (0x%x)\n", GET_FW_ADDRESS);
        return FAILURE;
    }

    memset(wdata, INDEX_0, MAXBUFFERSIZE);

    //sleep 500us
    usleep(GETFWADDRWAITTIME);

    rc = i2c_smbus_read_block_data(fd, BLOCK_PREFIX, wdata);
    if(rc <= LENGTH_0)
    {
        sd_journal_print(LOG_ERR, 
        "Error: Failed to read SCPAD address using"
        "PMBUS Cmd: BLOCK_READ (0x%x), rc=%d\n", BLOCK_PREFIX, rc);
        return FAILURE;
    }

    sd_journal_print(LOG_DEBUG, "SCPAD Address 0x%x 0x%x 0x%x 0x%x\n", 
                    wdata[INDEX_3], wdata[INDEX_2], wdata[INDEX_1], wdata[INDEX_0]);

    rc = i2c_smbus_write_block_data(fd, RPTR, (uint8_t)LENGTHOFBLOCK, wdata);

    usleep(MINWAITTIME);

    if(rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, 
        "Error: Failed to write SCPAD Address to RPTR, rc=%d\n", rc);
        return FAILURE;
    }

    sd_journal_print(LOG_DEBUG, "Write Block Data to Scratchpad with cmd 0xde\n");

    for(int i=0; i < section.size(); i++)
    {
        uint8_t sdata[MAXBUFFERSIZE];
        std::vector<uint8_t> dword = formatDword(section[i]);
        sdata[INDEX_0] = dword[INDEX_0];
        sdata[INDEX_1] = dword[INDEX_1];
        sdata[INDEX_2] = dword[INDEX_2];
        sdata[INDEX_3] = dword[INDEX_3];

        sd_journal_print(LOG_DEBUG, "0x%x 0x%x 0x%x 0x%x\n",
                        sdata[INDEX_0], sdata[INDEX_1], sdata[INDEX_2], sdata[INDEX_3]);

        rc = i2c_smbus_write_block_data(fd, MFR_REG_WRITE, (uint8_t)LENGTHOFBLOCK, sdata);
        usleep(MINWAITTIME);
        if(rc !=SUCCESS)
        {
            sd_journal_print(LOG_ERR, 
            "Error: Failed to write to SCPAD MFR_Reg: (0x%x), rc=%d\n",
            MFR_REG_WRITE, rc);
            return FAILURE;
        }
    }
    return SUCCESS;
}

int uploadDataToOtp(std::string s_dword, bool pmbus_section, int fd)
{
    uint8_t wdata[MAXBUFFERSIZE] = {0};
    int rc = FAILURE;
    if(pmbus_section == false)
    {
        std::vector<uint8_t> dword = formatDword(s_dword);
        wdata[INDEX_0] = dword[INDEX_0];
        wdata[INDEX_1] = dword[INDEX_1];
    }  else {
        wdata[INDEX_0] = partial_crc & INT_255;
        wdata[INDEX_1] = (partial_crc >> SHIFT_8 ) & INT_255;
    }

    sd_journal_print(LOG_DEBUG, "Upload Block Data to OTP with cmd 0xfd\n");
    sd_journal_print(LOG_DEBUG, "0x%x 0x%x 0x%x 0x%x\n",
                    wdata[INDEX_3], wdata[INDEX_2], wdata[INDEX_1], wdata[INDEX_0]);

    rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);

    usleep(MINWAITTIME);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, 
        "Error: Failed to write block data: 0x%x 0x%x 0x%x 0x%x using" 
        "PMBUS Cmd: BLOCK_WRITE (0x%x), rc=%d\n", 
        wdata[0], wdata[1], wdata[2], wdata[3], BLOCK_PREFIX, rc);
        return FAILURE;
    }

    sd_journal_print(LOG_DEBUG, "Upload Byte data to OTP with cmd 0xfe\n");

    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, UPLOAD_BYTE);

    usleep(MAXWAITTIME);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR,
        "Error: Failed to upload from scratchpad to OTP using" 
        "MFR Cmd: OTP_CONFIG_STORE (0x%x), rc=%d\n", UPLOAD_BYTE, rc);
        return FAILURE;
    }
    return SUCCESS;
}

bool vr_update_infineon_xdpe::UpdateFirmware()
{
    int exit_status = SUCCESS;

    std::vector<std::vector<std::string>> sections = parseCfgFile(ConfigFilePath);

    // Trim header programming needs to be ignored according to Infineon FAE
    std::string trim_header = TRIM_HEADER_CODE;
    for(int i=0; i < sections.size(); i++)
    {
        bool pmbus_section = false;

        if(sections[i][INDEX_0] == trim_header)
        {
            continue;
        }

        std::vector<uint8_t> xvhc = formatDword(sections[i][INDEX_0]);

        exit_status=invalidateOtp(xvhc[INDEX_1], xvhc[INDEX_0], fd);
        if(exit_status != SUCCESS) {
            sd_journal_print(LOG_ERR, 
            "Invalidate OTP Data has failed. Skipping other steps\n");
            return false;
        }

        exit_status=writeDataToScratchpad(sections[i],fd);
        if(exit_status != SUCCESS)
        {
            sd_journal_print(LOG_ERR, 
            "Writing Data to Scratchpad has failed. Skipping other steps\n");
            return false;
        }

        if(partial_pmbus_section_count_number == (i+INDEX_1))
            pmbus_section = true;
        exit_status=uploadDataToOtp(sections[i][INDEX_1],pmbus_section,fd);
    }

    if(exit_status != SUCCESS)
        return false;
    else
        return true;
}

bool vr_update_infineon_xdpe::ValidateFirmware()
{
    return true;
}
