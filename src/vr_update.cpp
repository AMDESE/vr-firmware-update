/*
* vr-update.cpp
*
* Created on: Nov 10, 2022
* Author: Abinaya Dhandapani
*/

#include"vr_update.hpp"

#define MODEL			("Model")
#define SLAVE_ADDRESS	("SlaveAddress")
#define PROCESSOR		("Processor")
#define MACHINE			("MachineName")
#define CRC				("CRC")

namespace fs = std::experimental::filesystem;

vr_update::vr_update(std::string Processor,std::string MachineName,
           uint32_t Crc,std::string Model,uint16_t SlaveAddress,
           std::string ConfigFilePath) : Processor(Processor),
           MachineName(MachineName), Crc(Crc), Model(Model),
           SlaveAddress(SlaveAddress), ConfigFilePath(ConfigFilePath)
{

    BusNumber = 0;
    fd = FAILURE;

}

vr_update* vr_update::CreateVRFrameworkObject(std::string Model,
              uint16_t SlaveAddress, uint32_t Crc, std::string Processor,
              std::string MachineName, std::string configFilePath){

	vr_update* p;

    if (configFilePath.find(PATCH) != std::string::npos)
    {
        p = new vr_update_renesas_patch(Processor,MachineName,Crc,Model,SlaveAddress,configFilePath);
    }
	else if ((Model.compare(MPS2861) == SUCCESS) ||
		(Model.compare(MPS2862) == SUCCESS))
    {
			p = new vr_update_mps(Processor,MachineName,Crc,Model,SlaveAddress,configFilePath);
	}

	else if ((Model.compare(RAA229613) == SUCCESS) ||
	         (Model.compare(RAA229625) == SUCCESS)||
             (Model.compare(RAA229620) == SUCCESS)){
		p = new vr_update_renesas_gen3(Processor,MachineName,Crc,Model,SlaveAddress,configFilePath);
	}

	else if (Model.compare(ISL68220) == SUCCESS){
		p = new vr_update_renesas_gen2(Processor,MachineName,Crc,Model,SlaveAddress,configFilePath);
	}

	else if(Model.compare(INFINEON_XDPE) == SUCCESS)
    {
		p = new vr_update_infineon_xdpe(Processor,MachineName,Crc,Model,SlaveAddress,configFilePath);
    }
    else if(Model.compare(INFINEON_TDA) == SUCCESS)
    {
        p = new vr_update_infineon_tda(Processor,MachineName,Crc,Model,SlaveAddress,configFilePath);
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

        if(Processor.compare(SOCKET_0) == SUCCESS)
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
