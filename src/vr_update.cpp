/*
 * vr-update.cpp
 *
 * Created on: Nov 10, 2022
 * Author: Abinaya Dhandapani
 */

#include "vr_update.hpp"

#include "vr_update_infineon_tda.hpp"
#include "vr_update_infineon_xdpe.hpp"
#include "vr_update_mp2869.hpp"
#include "vr_update_mps.hpp"
#include "vr_update_mps285x.hpp"
#include "vr_update_renesas_gen2.hpp"
#include "vr_update_renesas_gen3.hpp"
#include "vr_update_renesas_gen3p5_patch.hpp"
#include "vr_update_renesas_patch.hpp"
#include "vr_update_xdpe_patch.hpp"
#include "vr_update_fan2510xx.hpp"
#include "vr_update_ism6636x.hpp"
#include <nlohmann/json.hpp>

#define MODEL ("Model")
#define SLAVE_ADDRESS ("SlaveAddress")
#define PROCESSOR ("Processor")
#define CRC ("CRC")
#define VR_PLATFORM_FILE ("/var/lib/vr-config/platform-vr.json")

namespace fs = std::filesystem;

vr_update::vr_update(std::string Processor, uint32_t Crc, std::string Model,
                     uint16_t SlaveAddress, std::string ConfigFilePath,
                     std::string Revision, uint16_t PmbusAddress) :
    Processor(Processor), Crc(Crc), Model(Model), SlaveAddress(SlaveAddress),
    ConfigFilePath(ConfigFilePath), Revision(Revision),
    PmbusAddress(PmbusAddress)
{
    BusNumber = 0;
    fd = FAILURE;
}

vr_update* vr_update::CreateVRFrameworkObject(
    std::string Model, uint16_t SlaveAddress, uint32_t Crc,
    std::string Processor, std::string configFilePath, std::string UpdateType,
    std::string Revision, uint16_t PmbusAddress, std::vector<std::string>& configFilePathArr)
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
            sd_journal_print(LOG_INFO, "Renesas patch update triggered\n");
            p = new vr_update_renesas_patch(Processor, Crc, Model, SlaveAddress,
                                            configFilePath, Revision,
                                            PmbusAddress);
        }
        else if ((strcasecmp(Model.c_str(), RAA229639) == SUCCESS) ||
                 (strcasecmp(Model.c_str(), RAA229641) == SUCCESS))
        {
            sd_journal_print(LOG_INFO,
                             "Renesas Gen3.5 patch update triggered\n");

            p = new vr_update_renesas_gen3p5_patch(
                Processor, Crc, Model, SlaveAddress, configFilePath, Revision,
                PmbusAddress,configFilePathArr);
        }
        else if (strcasecmp(Model.c_str(), INFINEON_XDPE) == SUCCESS)
        {
            sd_journal_print(LOG_INFO, "XDPE patch update triggered\n");
            p = new vr_update_xdpe_patch(Processor, Crc, Model, SlaveAddress,
                                         configFilePath, Revision,
                                         PmbusAddress);
        }
        else
        {
            sd_journal_print(LOG_ERR, "Invalid framework\n");
            return NULL;
        }
    }
    else if ((strcasecmp(Model.c_str(), RAA229613) == SUCCESS) ||
             (strcasecmp(Model.c_str(), RAA229625) == SUCCESS) ||
             (strcasecmp(Model.c_str(), RAA229620) == SUCCESS) ||
             (strcasecmp(Model.c_str(), RAA229621) == SUCCESS) ||
             (strcasecmp(Model.c_str(), RAA229639) == SUCCESS) ||
             (strcasecmp(Model.c_str(), RAA22964) == SUCCESS) ||
             (strcasecmp(Model.c_str(), RAA229641) == SUCCESS))
    {
        p = new vr_update_renesas_gen3(Processor, Crc, Model, SlaveAddress,
                                       configFilePath, Revision, PmbusAddress);
    }

    else if (strcasecmp(Model.c_str(), ISL68220) == SUCCESS)
    {
        p = new vr_update_renesas_gen2(Processor, Crc, Model, SlaveAddress,
                                       configFilePath, Revision, PmbusAddress);
    }

    else if (strcasecmp(Model.c_str(), INFINEON_XDPE) == SUCCESS)
    {
        p = new vr_update_infineon_xdpe(Processor, Crc, Model, SlaveAddress,
                                        configFilePath, Revision, PmbusAddress);
    }
    else if (strcasecmp(Model.c_str(), INFINEON_TDA) == SUCCESS)
    {
        p = new vr_update_infineon_tda(Processor, Crc, Model, SlaveAddress,
                                       configFilePath, Revision, PmbusAddress);
    }
    else if ((strcasecmp(Model.c_str(), MPS2861) == SUCCESS) ||
             (strcasecmp(Model.c_str(), MPS2862) == SUCCESS))
    {
        p = new vr_update_mps(Processor, Crc, Model, SlaveAddress,
                              configFilePath, Revision, PmbusAddress);
    }
    else if ((strcasecmp(Model.c_str(), MPS2856) == SUCCESS) ||
             (strcasecmp(Model.c_str(), MPS2857) == SUCCESS))
    {
        p = new vr_update_mps285x(Processor, Crc, Model, SlaveAddress,
                                  configFilePath, Revision, PmbusAddress);
    }
    else if ((strcasecmp(Model.c_str(), MP2869) == SUCCESS) ||
             (strcasecmp(Model.c_str(), MP29608) == SUCCESS))
    {
        p = new vr_update_mp2869(Processor, Crc, Model, SlaveAddress,
                                 configFilePath, Revision, PmbusAddress);
    }
    else if ((strcasecmp(Model.c_str(), FAN251015) == SUCCESS) ||
             (strcasecmp(Model.c_str(), FAN251030) == SUCCESS))
    {
        p = new vr_update_fan2510xx(Processor,Crc,Model,SlaveAddress,configFilePath,Revision,PmbusAddress);
    }
    else if ((strcasecmp(Model.c_str(), ISM6636A) == SUCCESS) ||
             (strcasecmp(Model.c_str(), ISM6636B) == SUCCESS) ||
             (strcasecmp(Model.c_str(), ISM6636C) == SUCCESS))
    {
        p = new vr_update_ism6636x(Processor, Crc, Model, SlaveAddress,
                                   configFilePath, Revision, PmbusAddress);
    }
    else
    {
        sd_journal_print(LOG_ERR, "Invalid Framework\n");
        return NULL;
    }
    return p;
}

static bool addressSharedBetweenSockets(uint16_t slaveAddr)
{
    bool hasP0 = false;
    bool hasP1 = false;
    try
    {
        std::ifstream f(VR_PLATFORM_FILE);
        if (!f.is_open())
        {
            return true;
        }
        nlohmann::json data;
        f >> data;
        for (const auto & rec : data["VRConfigs"])
        {
            if (!rec.contains("SlaveAddress") ||
                !rec["SlaveAddress"].is_string())
            {
                continue;
            }
            std::string s = rec["SlaveAddress"];
            uint16_t a = static_cast<uint16_t>(std::stoul(s, nullptr, 16));
            if (a != slaveAddr)
            {
                continue;
            }
            std::string proc = rec.value("Processor","");
            if (proc.compare(SOCKET_0) == SUCCESS)
            {
                hasP0 = true;
            }
            else if (proc.compare(SOCKET_1) == SUCCESS)
            {
                hasP1 = true;
            }
        }
    }
    catch (const std::exception&)
    {
        return true;
    }
    return hasP0 && hasP1;
}

bool vr_update::findBusNumber()
{
    /*Find bus number from the drivers binded*/
    DIR* dir;
    struct dirent* entry;
    std::vector<std::string> slaveDevice;
    std::string DeviceName;

    std::stringstream ss;

    if (PmbusAddress == 0)
    {
        ss << std::hex << SlaveAddress;
    }
    else
    {
        ss << std::hex << PmbusAddress;
    }
    std::string SlaveAddrStr = ss.str();

    std::vector<std::string> driverPaths{DriverPath};
    if (!AltDriverPath.empty())
    {
        driverPaths.push_back(AltDriverPath);
    }

    for (const auto& driverDir : driverPaths)
    {
        if ((dir = opendir(driverDir.c_str())) == NULL)
        {
            continue;
        }

        while ((entry = readdir(dir)) != NULL)
        {
            std::string fname = entry->d_name;

            if (fname.find("00" + SlaveAddrStr) != std::string::npos)
            {
                slaveDevice.push_back(fname);
            }
        }
        closedir(dir);

        if (!slaveDevice.empty())
        {
            /* Unbind and bind must use the driver that actually claimed it */
            DriverPath = driverDir;
            break;
        }
    }

    if (slaveDevice.empty())
    {
        sd_journal_print(LOG_ERR,
            "VR update failed: no device found for %s at slave address 0x%x.",
            Processor.c_str(),SlaveAddress);
        return false;
    }
    std::sort(slaveDevice.begin(), slaveDevice.end());

    int index = FAILURE;
    if ((Processor.compare(SOCKET_0) == SUCCESS))
    {
        index = INDEX_0;
    }
    else if (Processor.compare(SOCKET_1) == SUCCESS)
    {
        index = INDEX_1;
    }
    else
    {
        index = INDEX_0;
    }

    if(index >= static_cast<int>(slaveDevice.size()))
    {
        if (slaveDevice.size() == 1 &&
            !addressSharedBetweenSockets(SlaveAddress))
        {
            index = INDEX_0;
        }
        else
        {
            sd_journal_print(LOG_ERR,
                "VR update failed: device for %s at slave address 0x%x not found "
                "(found %zu device(s)). Aborting to avoid programming the wrong socket",
                Processor.c_str(),SlaveAddress,slaveDevice.size());
            return false;
        }
    }

    DeviceName = slaveDevice[index];

    size_t found = DeviceName.find("-");
    BusNumber = std::stoi(DeviceName.substr(0, found));

    std::string UnbindDriver =
        "echo " + DeviceName + "> " + DriverPath + "unbind";

    system(UnbindDriver.c_str());

    if (BusNumber != 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool vr_update::ReadbackVerify(bool verified)
{
    sd_journal_print(LOG_INFO,
        "Readback verification: reading back %s VR at slave address 0x%x",
         Processor.c_str(),SlaveAddress);
    
    usleep(SLEEP_1);
    
    bool ok;
    
    if(crcReadbackValid())
    {
        crcCheckSum();
        ok = CrcMatched;
    }
    else
    {
        ok = verified;
    }

    if(ok)
    {
        sd_journal_print(LOG_INFO,
            "Readback Passed: %s VR at slave address 0x%x -programmed firmware verified",
            Processor.c_str(),SlaveAddress);
        return true;
    }
    else
    {
        sd_journal_print(LOG_ERR,
            "Readback Failed: %s VR at slave address 0x%x -firmware verification failed",
            Processor.c_str(),SlaveAddress);
    }
    return false;
}

bool vr_update::openI2cDevice()
{
    char i2cDeviceName[FILE_PATH_SIZE];
    bool rc = false;

    std::cout << BusNumber << std::endl;
    std::snprintf(i2cDeviceName, FILE_PATH_SIZE, "/dev/i2c-%d", BusNumber);

    std::cout << i2cDeviceName << std::endl;
    fd = open(i2cDeviceName, O_RDWR);

    if (fd != FAILURE)
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
    }
    else
    {
        sd_journal_print(LOG_ERR, "Error: failed to open VR device\n");
        rc = false;
    }

    usleep(MIN_WAIT_TIME);
    return rc;
}

void vr_update::closeI2cDevice()
{
    if (fd >= SUCCESS)
    {
        close(fd);
    }
    fd = FAILURE;

    std::stringstream ss;

    if (PmbusAddress == 0)
    {
        ss << std::hex << SlaveAddress;
    }
    else
    {
        ss << std::hex << PmbusAddress;
    }
    std::string SlaveAddrStr = ss.str();

    std::string DeviceName = std::to_string(BusNumber) + "-00" + SlaveAddrStr;

    std::string BindDriver = "echo " + DeviceName + " > " + DriverPath + "bind";

    system(BindDriver.c_str());

    sd_journal_print(LOG_INFO, "Binded driver back after VR update\n");
}
