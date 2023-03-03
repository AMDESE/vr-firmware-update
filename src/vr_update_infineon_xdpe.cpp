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

vr_update_infineon_xdpe::vr_update_infineon_xdpe(std::string Processor,
          uint32_t Crc,std::string Model,uint16_t SlaveAddress,std::string ConfigFilePath):
          vr_update(Processor,Crc,Model,SlaveAddress,ConfigFilePath)
{

    DriverPath = XDPE_DRIVER_PATH;
}

bool vr_update_infineon_xdpe::crcCheckSum()
{

    uint32_t DeviceCrcData = 0;
    int rc = FAILURE;
    uint8_t wdata[MAXBUFFERSIZE];
    uint8_t rdata[MAXBUFFERSIZE];
    int length;
    int size;

    memset(wdata, INDEX_0, MAXBUFFERSIZE);

    rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);
    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
        return false;
    }

    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, GET_CRC);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
        return false;
    }

    /*Wait for 200ms */
    usleep(200000);

    length = i2c_smbus_read_block_data(fd, BLOCK_PREFIX, rdata);

    if (length > LENGTH_0)
    {
        DeviceCrcData = ((uint32_t)rdata[INDEX_3] << SHIFT_24) | ((uint32_t)rdata[INDEX_2] << SHIFT_16)
                     | ((uint32_t)rdata[INDEX_1] << SHIFT_8) | (uint32_t)rdata[INDEX_0];
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
        sd_journal_print(LOG_ERR, "CRC matches with previous image. Skipping the update\n");
        return false;
    }
    else
    {
        sd_journal_print(LOG_ERR, "CRC didnot match with previous image. Continuing the update\n");
    }
    return true;
}

bool vr_update_infineon_xdpe::isUpdatable()
{

    uint8_t wdata[MAXBUFFERSIZE];
    uint8_t rdata[MAXBUFFERSIZE];
    int length;
    int size;
    int rc = FAILURE;

    /*Find if the VR device is infineon*/
    wdata[INDEX_0] = DDBD0;
    wdata[INDEX_1] = DDBD1;
    wdata[INDEX_2] = DDBD2;
    wdata[INDEX_3] = DDBD3;

    rc = i2c_smbus_write_block_data(fd, RPTR, (uint8_t)LENGTHOFBLOCK, wdata);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
        return false;
    }

    length = i2c_smbus_read_block_data(fd, MFR_REG_READ, rdata);

    if (length > LENGTH_0)
    {
        if (rdata[INDEX_1] == PART1 || rdata[INDEX_1] == PART2 ||
              rdata[INDEX_1] == PART3 || rdata[INDEX_1] == PART4 || rdata[INDEX_1] == PART5)
        {
            sd_journal_print(LOG_INFO, "Infineon device detected\n");
        } else {
            sd_journal_print(LOG_ERR, "Error: No device detected\n");
            return false;
        }
    } else {
        sd_journal_print(LOG_ERR, "Error: Read failed\n");
        return false;
    }

    /*Check if space available on OTP*/

    memset(wdata, INDEX_0, MAXBUFFERSIZE);

    rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
        return false;
    }

    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, AVAIL_SPACE_BYTE);

    if (rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error: Failed to write data\n");
        return false;
    }

    length = i2c_smbus_read_block_data(fd, BLOCK_PREFIX, rdata);

    if (length > LENGTH_0)
    {
        // size = d0 + 256 * d1. Formula provided in Infineon document
        size = (256 * rdata[INDEX_1] + rdata[INDEX_0]);
        sd_journal_print(LOG_INFO, "Available size (bytes): %d\n",size);
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: Failed to read data\n");
        return false;
    }

    if (size < MINOTPSIZE)
    {
        sd_journal_print(LOG_ERR, "Available space is less, program manually\n");
        return false;
    }
    else
    {
        sd_journal_print(LOG_ERR, "Proceeding with VR programming\n");
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
    sd_journal_print(LOG_DEBUG, "%d %d %d %d\n",wdata[INDEX_3],wdata[INDEX_2],wdata[INDEX_1],wdata[INDEX_0]);

    rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);

    usleep(MINWAITTIME);

    if(rc != SUCCESS)
    {
        perror("Error while writing block data to invalidate OTP");
        return FAILURE;
    }

    sd_journal_print(LOG_DEBUG, "Invalidate OTP Byte Write with cmd 0xfe\n");
    sd_journal_print(LOG_DEBUG, "0x%x\n",INVAL_BYTE);

    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, INVAL_BYTE);

    usleep(MINWAITTIME);

    if( rc != SUCCESS)
    {
        perror("Error while writing byte data to invalidate OTP");
        return FAILURE;
    }

    return SUCCESS;
}

int writeDataToScratchpad(std::vector<std::string> section,int fd)
{
    uint8_t wdata[MAXBUFFERSIZE];
    int rc = FAILURE;
    wdata[INDEX_0] = SPBD0;
    wdata[INDEX_1] = SPBD1;
    wdata[INDEX_2] = SPBD2;
    wdata[INDEX_3] = SPBD3;

    sd_journal_print(LOG_DEBUG, "ScratchPad Initial Block Write with cmd 0xce\n");
    sd_journal_print(LOG_DEBUG, "0x%x 0x%x 0x%x 0x%x\n",wdata[INDEX_3],wdata[INDEX_2],wdata[INDEX_1],wdata[INDEX_0]);

    rc = i2c_smbus_write_block_data(fd, RPTR, (uint8_t)LENGTHOFBLOCK, wdata);

    usleep(MINWAITTIME);

    if(rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error while writing initial block data request to scratchpad\n");
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

        sd_journal_print(LOG_DEBUG, "0x%x 0x%x 0x%x 0x%x\n",sdata[INDEX_0],sdata[INDEX_1],sdata[INDEX_2],sdata[INDEX_3]);

        rc = i2c_smbus_write_block_data(fd, MFR_REG_WRITE, (uint8_t)LENGTHOFBLOCK, sdata);
        usleep(MINWAITTIME);
        if(rc !=SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Error while writing block data request to scratchpad\n");
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
    sd_journal_print(LOG_DEBUG, "0x%x 0x%x 0x%x 0x%x\n",wdata[INDEX_3],wdata[INDEX_2],wdata[INDEX_1],wdata[INDEX_0]);

    rc = i2c_smbus_write_block_data(fd, BLOCK_PREFIX, (uint8_t)LENGTHOFBLOCK, wdata);

    usleep(MINWAITTIME);

    if ( rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR, "Error while uploading block data from scratchpad to OTP\n");
        return FAILURE;
    }

    sd_journal_print(LOG_DEBUG, "Upload Byte data to OTP with cmd 0xfe\n");


    rc = i2c_smbus_write_byte_data(fd, BYTE_PREFIX, UPLOAD_BYTE);

    usleep(MAXWAITTIME);

    if ( rc != SUCCESS)
    {
        sd_journal_print(LOG_ERR,
            "Error while executing byte write command for uploading data from scratchpad to OTP\n");
        return FAILURE;
    }
    return SUCCESS;
}

bool vr_update_infineon_xdpe::UpdateFirmware()
{
    int exit_status = SUCCESS;

    std::vector<std::vector<std::string>> sections = parseCfgFile(ConfigFilePath);

    std::string trim_header = "00000002";    // Trim header programming needs to be ignored according to Infineon FAE
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
            sd_journal_print(LOG_ERR, "Invalidate OTP Data has failed. Skipping other steps\n");
            return false;
        }

        exit_status=writeDataToScratchpad(sections[i],fd);
        if(exit_status != SUCCESS)
        {
            sd_journal_print(LOG_ERR, "Writing Data to Scratchpad has failed. Skipping other steps\n");
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
