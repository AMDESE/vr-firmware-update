/*
* vr-update.cpp
*
* Created on: Nov 10, 2022
* Author: Abinaya Dhandapani
*/

#include "vr_update.hpp"
#include "vr_update_infineon_tda.hpp"
#include "vr_update_infineon_xdpe.hpp"
#include "vr_update_renesas_gen2.hpp"
#include "vr_update_renesas_gen3.hpp"
#include "vr_update_renesas_patch.hpp"
#include "vr_update_xdpe_patch.hpp"
#include "vr_update_mps.hpp"

#define MODEL			("Model")
#define SLAVE_ADDRESS	("SlaveAddress")
#define PROCESSOR		("Processor")
#define CRC				("CRC")

namespace fs = std::filesystem;

vr_update::vr_update(std::string Processor,uint32_t Crc,std::string Model,
           uint16_t SlaveAddress, std::string ConfigFilePath) :
           Processor(Processor),Crc(Crc), Model(Model),
           SlaveAddress(SlaveAddress), ConfigFilePath(ConfigFilePath)
{

    BusNumber = 0;
    fd = FAILURE;

}

vr_update* vr_update::CreateVRFrameworkObject(std::string Model,
              uint16_t SlaveAddress, uint32_t Crc, std::string Processor,
              std::string configFilePath,std::string UpdateType)
{
	vr_update* p;
    if ((strcasecmp(UpdateType.c_str(), PATCH)) == SUCCESS)
    {
        if ((strcasecmp(Model.c_str(), RAA229613) == SUCCESS) ||
            (strcasecmp(Model.c_str(), RAA229625) == SUCCESS) ||
            (strcasecmp(Model.c_str(), RAA229620) == SUCCESS) ||
            (strcasecmp(Model.c_str(), RAA229621) == SUCCESS) ||
            (strcasecmp(Model.c_str(), ISL68220) == SUCCESS) ||
            (strcasecmp(Model.c_str(), RENESAS) == SUCCESS))
        {
            sd_journal_print(LOG_INFO,"Renesas patch update triggered\n");
            p = new vr_update_renesas_patch(Processor,Crc,Model,SlaveAddress,configFilePath);
        }
        else if (strcasecmp(Model.c_str(), INFINEON_XDPE) == SUCCESS)
        {
           sd_journal_print(LOG_INFO,"XDPE patch update triggered\n");
           p = new vr_update_xdpe_patch(Processor,Crc,Model,SlaveAddress,configFilePath);
        }
        else {
           sd_journal_print(LOG_ERR, "Invalid framework\n");
           return NULL;
        }

    }
	else if ((strcasecmp(Model.c_str(), RAA229613) == SUCCESS) ||
            (strcasecmp(Model.c_str(), RAA229625) == SUCCESS) ||
            (strcasecmp(Model.c_str(), RAA229620) == SUCCESS) ||
            (strcasecmp(Model.c_str(), RAA229621) == SUCCESS)) {
		p = new vr_update_renesas_gen3(Processor,Crc,Model,SlaveAddress,configFilePath);
	}

	else if (strcasecmp(Model.c_str(), ISL68220) == SUCCESS) {
		p = new vr_update_renesas_gen2(Processor,Crc,Model,SlaveAddress,configFilePath);
	}

	else if (strcasecmp(Model.c_str(), INFINEON_XDPE) == SUCCESS)
    {
		p = new vr_update_infineon_xdpe(Processor,Crc,Model,SlaveAddress,configFilePath);
    }
    else if (strcasecmp(Model.c_str(), INFINEON_TDA) == SUCCESS)
    {
        p = new vr_update_infineon_tda(Processor,Crc,Model,SlaveAddress,configFilePath);
    }
 else if ((strcasecmp(Model.c_str(), MPS2861) == SUCCESS) ||
            (strcasecmp(Model.c_str(), MPS2862) == SUCCESS))
    {
        p = new vr_update_mps(Processor,Crc,Model,SlaveAddress,configFilePath);
    }
	else{
		sd_journal_print(LOG_ERR, "Invalid Framework\n");
		return NULL;
	}
	return p;
}

bool vr_update::findBusNumber()
{

    /*Find bus number from the drivers binded*/
    DIR *dir;
    struct dirent *entry;
    std::vector<std::string> slaveDevice;
    std::string DeviceName;

    std::stringstream ss;
    ss << std::hex << SlaveAddress;
    std::string SlaveAddrStr = ss.str();
    const char* DriverPathStr = DriverPath.c_str();

    if ((dir = opendir(DriverPathStr)) != NULL)
    {
        while ((entry = readdir(dir)) != NULL)
        {
            std::string fname = entry->d_name;

            if(fname.find(SlaveAddrStr) != std::string::npos)
            {
                slaveDevice.push_back(fname);
            }
        }
        closedir(dir);

        if(slaveDevice.empty())
        {
            return false;
        }
        std::sort(slaveDevice.begin(),slaveDevice.end());

    for (const auto& element : slaveDevice) {
        std::cout << element << std::endl;
    }
        if((Processor.compare(SOCKET_0) == SUCCESS) || (slaveDevice.size() == 1))
        {
            DeviceName = slaveDevice[INDEX_0];;
            slaveDevice[INDEX_0].resize(INDEX_2);
            BusNumber = std::stoi(slaveDevice[INDEX_0]);
        }
        else if(Processor.compare(SOCKET_1) == SUCCESS)
        {
            DeviceName = slaveDevice[INDEX_1];
            slaveDevice[INDEX_1].resize(INDEX_2);
            BusNumber = std::stoi(slaveDevice[INDEX_1]);
        }
    }

    std::string UnbindDriver = "echo " + DeviceName + "> " + DriverPath + "unbind";

    system(UnbindDriver.c_str());

    if(BusNumber != 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool vr_update::openI2cDevice()
{

    char i2cDeviceName[FILE_PATH_SIZE];
    bool rc = false;

    std::snprintf(i2cDeviceName, FILE_PATH_SIZE, "/dev/i2c-%d", BusNumber);

    fd = open(i2cDeviceName, O_RDWR);

    if(fd != FAILURE)
    {
        if (ioctl(fd, I2C_SLAVE, SlaveAddress) != FAILURE)
        {
            rc = true;
        }
        else
        {
            sd_journal_print(LOG_ERR, "Error: Failed setting i2c dev addr\n");
            rc = false;
        }
    } else {
        sd_journal_print(LOG_ERR, "Error: failed to open VR device\n");
        rc = false;
    }

    usleep(MIN_WAIT_TIME);
    return rc;
}

void vr_update::closeI2cDevice()
{
    if (fd >= SUCCESS) {
        close(fd);
    }
    fd = FAILURE;

    std::stringstream ss;
    ss << std::hex << SlaveAddress;
    std::string SlaveAddrStr = ss.str();

    std::string DeviceName = std::to_string(BusNumber) + "-00" + SlaveAddrStr;

    std::string BindDriver = "echo " + DeviceName + " > " + DriverPath + "bind";

    system(BindDriver.c_str());

    sd_journal_print(LOG_INFO, "Binded driver back after VR update\n");

}
