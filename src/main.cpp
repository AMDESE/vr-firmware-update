/*
* main.cpp
*
* Created on: Nov 10, 2022
* Author: Abinaya Dhandapani
*/

#include "vr_update.hpp"
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/property.hpp>
#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#define VR_PLATFORM_FILE     ("/var/lib/vr-config/platform-vr.json")

using json = nlohmann::json;

using DbusVariant = uint32_t;

std::string bmcUpdaterService = "xyz.openbmc_project.Software.BMC.Updater";
std::string vrBundlePath = "/xyz/openbmc_project/software/vr_bundle_active";
constexpr auto bundleVersionInterface = "xyz.openbmc_project.Software.BundleVersion";

#define COMMAND_BOARD_ID    ("/sbin/fw_printenv -n board_id")
#define COMMAND_LEN         3

/* SP5 Platform IDs */
#define ONYX_SLT        61  //0x3D
#define ONYX_1          64  //0x40
#define ONYX_2          65  //0x41
#define ONYX_3          66  //0x42
#define ONYX_FR4        82  //0x52
#define QUARTZ_DAP      62  //0x3E
#define QUARTZ_1        67  //0x43
#define QUARTZ_2        68  //0x44
#define QUARTZ_3        69  //0x45
#define QUARTZ_FR4      81  //0x51
#define RUBY_1          70  //0x46
#define RUBY_2          71  //0x47
#define RUBY_3          72  //0x48
#define TITANITE_1      73  //0x49
#define TITANITE_2      74  //0x4A
#define TITANITE_3      75  //0x4B
#define TITANITE_4      76  //0x4C
#define TITANITE_5      77  //0x4D
#define TITANITE_6      78  //0x4E

/* SH5 Platform IDs */
#define SH5_1P_PWR      92  //0x5C
#define SH5_1P_OEM      93  //0x5D
#define SH5_1P_SLT      94  //0x5E
#define SH5_2P_CABLED   108 //0x6C
#define SH5_1P_OEM_P    109 //0x6D
#define SH5_SIDLEY      95  //0x5F
#define SH5_PARRY_PEAK  96  //0x60

/* SP6 Platform IDs */
#define SHALE_64        89  //0x59
#define SHALE_SLT       98  //0x62
#define SHALE           101 //0x65
#define SUNSTONE_DAP    97  //0x61
#define CINNABAR        99  //0x63
#define SUNSTONE        100 //0x64

/* Turin Platform IDs */
#define CHALUPA         102 // 0x66
#define CHALUPA_1       110 // 0x6E
#define CHALUPA_2       111 // 0x6F
#define HUAMBO          103 // 0x67
#define GALENA          104 // 0x68
#define GALENA_1        112 // 0x70
#define GALENA_2        113 // 0x71
#define RECLUSE         105 // 0x69
#define PURICO          106 // 0x6A
#define PURICO_1        114 // 0x72
#define PURICO_2        115 // 0x73
#define VOLCANO         107 // 0x6B
#define VOLCANO_1       116 // 0x74
#define VOLCANO_2       117 // 0x75

struct bundleInterfaceStruct {
    std::vector<std::string> FirmwareID;
    std::vector<std::string> SlaveAddress;
    std::vector<std::string> PmbusAddress;
    std::vector<std::string> Processor;
    std::vector<std::string> Status;
    std::vector<std::string> Versions;
    std::vector<std::string> Checksum;
    std::vector<bool> UpdateStatus;
};

bundleInterfaceStruct bundleInterfaceObj;

int vrUpdate(std::string Model, uint16_t SlaveAddress, uint32_t Crc,
             std::string Processor,std::string configFilePath, std::string UpdateType,bool *CrcMatched,std::string Revision,uint16_t PmbusAddress)
{

    int ret = FAILURE;

    bool rc = false;
    vr_update* vr_update_obj;

    vr_update_obj = vr_update::CreateVRFrameworkObject(Model,
                    SlaveAddress,Crc,Processor,configFilePath,UpdateType,Revision,PmbusAddress);

    if(vr_update_obj != NULL)
    {
        rc = vr_update_obj->findBusNumber();

        if(rc != true)
        {
            sd_journal_print(LOG_ERR, "Unable to find the bus number\n");
            ret = FAILURE;
            goto Clean;
        }

        rc = vr_update_obj->openI2cDevice();
        if(rc != true)
        {
            sd_journal_print(LOG_ERR, "Unable to Open I2c slave device\n");
            ret = FAILURE;
            goto Clean;
        }

        rc = vr_update_obj->crcCheckSum();
        if(rc != true)
        {
            *CrcMatched = vr_update_obj->CrcMatched;
            ret = FAILURE;
            goto Clean;
        }

        rc = vr_update_obj->isUpdatable();
        if(rc != true)
        {
            ret = FAILURE;
            goto Clean;
        }

        rc = vr_update_obj->UpdateFirmware();
        if(rc != true)
        {
            ret = FAILURE;
            goto Clean;
        }

        rc = vr_update_obj->ValidateFirmware();
        if(rc != true)
        {
            ret = FAILURE;
            goto Clean;
        }
        ret = SUCCESS;
Clean:
        vr_update_obj->closeI2cDevice();
    }
    return ret;
}

template <typename T>
T getProperty(sdbusplus::bus::bus& bus, const char* service, const char* path,

 const char* interface, const char* propertyName)
{
    auto method = bus.new_method_call(service, path,
                                      "org.freedesktop.DBus.Properties", "Get");
    method.append(interface, propertyName);
    std::variant<T> value{};
    try
    {
        auto reply = bus.call(method);
        reply.read(value);
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        sd_journal_print(LOG_ERR, "GetProperty call failed \n");
    }
    return std::get<T>(value);
}

template <typename T>
static auto setProperty(sdbusplus::bus::bus& bus, const char* service, const char* path,

 const char* interface, const char* propertyName,const T& value)
{

    std::variant<T> data = value;

    try
    {
        auto method = bus.new_method_call(service, path,
                                      "org.freedesktop.DBus.Properties", "Set");
        method.append(interface, propertyName);
        method.append(data);
        auto reply = bus.call(method);
    }
    catch (const std::exception& e)
    {
        sd_journal_print(LOG_ERR, "Set property call failed\n");
        return;
    }

}

bool getBundleVersionInterface(sdbusplus::bus::bus& bus)
{

    std::vector<std::string> FirmwareID = getProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "FirmwareID");

    std::vector<std::string> SlaveAddress = getProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "SlaveAddress");

    std::vector<std::string> Processor = getProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "Processor");

    std::vector<std::string> Versions = getProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "Versions");

    std::vector<std::string> Status = getProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "Status");

    std::vector<std::string> Checksum = getProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "Checksum");

    if(FirmwareID.empty())
    {
        sd_journal_print(LOG_ERR, "VR details are not available in dbus interface \n");
        return false;
    }
    else
    {
          bundleInterfaceObj.FirmwareID.assign(FirmwareID.begin(),FirmwareID.end());
          bundleInterfaceObj.SlaveAddress.assign(SlaveAddress.begin(),SlaveAddress.end());
          bundleInterfaceObj.Processor.assign(Processor.begin(),Processor.end());
          bundleInterfaceObj.Versions.assign(Versions.begin(),Versions.end());
          bundleInterfaceObj.Status.assign(Status.begin(),Status.end());
          bundleInterfaceObj.Checksum.assign(Checksum.begin(),Checksum.end());

          bundleInterfaceObj.UpdateStatus.assign(bundleInterfaceObj.SlaveAddress.size(),false);
          return true;
    }
}

void setBundleVersionInterface(sdbusplus::bus::bus& bus)
{

    setProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "FirmwareID",bundleInterfaceObj.FirmwareID);

    setProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "SlaveAddress",bundleInterfaceObj.SlaveAddress);

    setProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "Processor",bundleInterfaceObj.Processor);

    setProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "Versions",bundleInterfaceObj.Versions);

    setProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "Status",bundleInterfaceObj.Status);

    setProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "Checksum",bundleInterfaceObj.Checksum);


}

bool PlatformIDValidation(std::string BoardName)
{
    FILE *pf;
    unsigned int board_id = 0;
    char data[COMMAND_LEN];
    bool PLATID = false;
    std::stringstream ss;
    std::string PlatformName;

    pf = popen(COMMAND_BOARD_ID,"r");
    // Error handling
    if(pf)
    {
        // Get the data from the process execution
        if (fgets(data, COMMAND_LEN, pf))
        {
            ss << std::hex << (std::string)data;
            ss >> board_id;
            PLATID = true;
            sd_journal_print(LOG_DEBUG, "Board ID: 0x%x, Board ID String: %s\n", board_id, data);
        }
        // the data is now in 'data'
        pclose(pf);

        if((board_id == ONYX_1) || (board_id == ONYX_2) || (board_id == ONYX_3) ||
                                 (board_id == ONYX_FR4) || (board_id == ONYX_SLT))
        {
            PlatformName = "Onyx";
        }
        else if ((board_id == QUARTZ_DAP) || (board_id == QUARTZ_1) || (board_id == QUARTZ_2)
                                || (board_id == QUARTZ_3) || ( board_id == QUARTZ_FR4))
        {
            PlatformName = "Quartz";
        }
        else if ((board_id == RUBY_1) || (board_id == RUBY_2) || (board_id == RUBY_3))
        {
            PlatformName = "Ruby";
        }
        else if ((board_id == TITANITE_1) || (board_id == TITANITE_2) || (board_id == TITANITE_3) ||
                 (board_id == TITANITE_4) || (board_id == TITANITE_5) || (board_id == TITANITE_6))
        {
            PlatformName = "Titanite";
        }
        else if((board_id == SHALE_64) || (board_id == SHALE_SLT) || (board_id == SHALE))
        {
            PlatformName = "Shale";
        }
        else if(board_id == CINNABAR)
        {
            PlatformName = "Cinnabar";
        }
        else if((board_id == SUNSTONE) || (board_id == SUNSTONE_DAP))
        {
            PlatformName = "Sunstone";
        }
        else if((board_id == CHALUPA) || (board_id == CHALUPA_1) || (board_id == CHALUPA_2))
        {
            PlatformName = "Chalupa";
        }
        else if(board_id == HUAMBO)
        {
            PlatformName = "Huambo";
        }
        else if((board_id == GALENA) || (board_id == GALENA_1)|| (board_id == GALENA_2))
        {
            PlatformName = "Galena";
        }
        else if(board_id == RECLUSE)
        {
            PlatformName = "Recluse";
        }
        else if((board_id == PURICO) || (board_id == PURICO_1)|| (board_id == PURICO_2))
        {
            PlatformName = "Purico";
        }
        else if((board_id == VOLCANO) || (board_id == VOLCANO_1)|| (board_id == VOLCANO_2))
        {
            PlatformName = "Volcano";
        }
        else if((board_id == SH5_1P_PWR) || (board_id == SH5_1P_OEM ) || (board_id == SH5_1P_SLT) || (board_id == SH5_1P_OEM_P) || (board_id == SH5_2P_CABLED))
        {
            PlatformName = "SH5";
        }


        if ((strcasecmp(BoardName.c_str(), PlatformName.c_str())) != SUCCESS)
        {
            sd_journal_print(LOG_ERR, "The board name from config file does not match with the platform "
                                      "Skipping the update\n");
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[])
{
    int ret = FAILURE;
    int rc = SUCCESS;
    uint16_t SlaveAddress;
    uint16_t PmbusAddress = 0;
    std::string BoardName;
    uint32_t Crc;
    std::string version;
    std::string UpdateType;
    std::string Revision;
    std::string configFilePath;
    std::string Processor;
    std::string Model;
    std::string  CrcConfig;
    std::string SlaveAddr;
    std::string PmbusAddr;

    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    if(argc < INDEX_2)
    {
       sd_journal_print(LOG_ERR, "Invalid Number of Command line Arguments\n");
       rc = FAILURE;
    }
    else
    {
        std::string filePath = argv[INDEX_1];
        std::string vrBundleJsonFile = filePath + "/vrbundle.json";

        if(getBundleVersionInterface(bus) == false)
        {
            //Copy platfom specific VR config file
            std::string command = "/usr/sbin/vr-config-info install_vr_platform_config";

            int ret = system(command.c_str());

            if(ret != SUCCESS)
            {
                sd_journal_print(LOG_ERR,"Copying vr-platform-config file failed\n");
                return FAILURE;
            }

            if (std::filesystem::exists(VR_PLATFORM_FILE))
            {
                std::ifstream json_file(VR_PLATFORM_FILE);
                json data;
                json_file >> data;

                for (json record : data["VRConfigs"])
                {
                    bundleInterfaceObj.SlaveAddress.push_back(record["SlaveAddress"]);
                    bundleInterfaceObj.FirmwareID.push_back(record["VrName"]);
                    bundleInterfaceObj.Processor.push_back(record["Processor"]);
                    bundleInterfaceObj.Versions.push_back("Unknown");
                    bundleInterfaceObj.Status.push_back("Unknown");
                    bundleInterfaceObj.Checksum.push_back("Unknown");
                    bundleInterfaceObj.UpdateStatus.push_back(false);

                    if(record.contains("PmbusAddress"))
                    {
                        bundleInterfaceObj.PmbusAddress.push_back(record["PmbusAddress"]);
                    }
                    else {
                        bundleInterfaceObj.PmbusAddress.push_back("Unknown");
                    }
                }
                setBundleVersionInterface(bus);
            }
        }

        if (std::filesystem::exists(vrBundleJsonFile))
        {
            std::ifstream json_file(vrBundleJsonFile);
            json data;
            json_file >> data;

            //iterate over the array of VR's
            for (json record : data["VR"])
            {
                if(record.contains("ModelNumber"))
                {
                    Model = record["ModelNumber"];
                } else {
                    sd_journal_print(LOG_ERR,"Json file doesnt have model number. Update aborted\n");
                    return false;
                }

                if(record.contains("SlaveAddress"))
                {
                    SlaveAddr = record["SlaveAddress"];

                    auto it = std::find(bundleInterfaceObj.SlaveAddress.begin(),bundleInterfaceObj.SlaveAddress.end(),SlaveAddr);

                    if (it != bundleInterfaceObj.SlaveAddress.end())
                    {
                        int index = std::distance(bundleInterfaceObj.SlaveAddress.begin(), it);

                        if(bundleInterfaceObj.PmbusAddress[index] != "Unknown")
                        {
                            PmbusAddress = std::stoul(bundleInterfaceObj.PmbusAddress[index], nullptr, BASE_16);
                        }
                    }

                    SlaveAddress = std::stoul(SlaveAddr, nullptr, BASE_16);
                } else {
                    sd_journal_print(LOG_ERR,"Json file doesnt have slave address. Update aborted\n");
                    return false;
                }

                if(record.contains("CRC"))
                {
                    CrcConfig = record["CRC"];
                    Crc = std::stoul(CrcConfig, nullptr, BASE_16);
                }

                if(record.contains("Processor"))
                {
                    Processor = record["Processor"];
                } else {
                    sd_journal_print(LOG_ERR,"Json file doesnt have Processor details. Update aborted\n");
                    return false;
                }

                if(record.contains("BoardName"))
                {
                    BoardName =  record["BoardName"];
                } else {
                    sd_journal_print(LOG_ERR,"Json file doesnt have BoadrdName details. Update aborted\n");
                    return false;
                }

                if(record.contains("ConfigFile"))
                {
                    std::string configFileName = record["ConfigFile"];
                    configFilePath = filePath + '/' + configFileName;
                } else {
                    sd_journal_print(LOG_ERR,"Json file doesnt have ConfigFile details. Update aborted\n");
                    return false;
                }

                if(record.contains("Version"))
                {
                    version = record["Version"];
                }

                if(record.contains("UpdateType"))
                {
                    UpdateType = record["UpdateType"];
                } else {
                    sd_journal_print(LOG_ERR,"Json file doesnt have UpdateType details. Update aborted\n");
                    return false;
                }

                if(record.contains("Revision"))
                {
                    Revision = record["Revision"];
                } else {
                    Revision = "None";
                }

                bool CrcMatched = false;

                if(PlatformIDValidation(BoardName) == false)
                {
                    return false;
                }

                sd_journal_print(LOG_INFO, "Updating VR for the Slave Address = 0x%x", SlaveAddress);

                ret = vrUpdate(Model,SlaveAddress,Crc,Processor,configFilePath,UpdateType,&CrcMatched,Revision,PmbusAddress);

                for (int i = 0; i < bundleInterfaceObj.SlaveAddress.size(); i++)
                {
                    if((bundleInterfaceObj.SlaveAddress[i] == SlaveAddr) &&
                       (bundleInterfaceObj.UpdateStatus[i] == false))
                    {
                        if(strcasecmp(bundleInterfaceObj.Processor[i].c_str(), Processor.c_str()) == SUCCESS)
                        {
                            bundleInterfaceObj.Versions[i] = version;
                            if(ret == SUCCESS)
                            {
                                bundleInterfaceObj.Checksum[i] = CrcConfig;
                                bundleInterfaceObj.Status[i] = "Update Completed";
                            }
                            else
                            {
                                if(CrcMatched == true)
                                {
                                    bundleInterfaceObj.Status[i] = "Already UpToDate";
                                } else {
                                    bundleInterfaceObj.Status[i] = "Update Failed";
                                    rc = FAILURE;
                                }
                            }
                        bundleInterfaceObj.UpdateStatus[i] = true;
                        }
                    }
                }
            }
            setProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "Versions",bundleInterfaceObj.Versions);

           setProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "Status",bundleInterfaceObj.Status);

           setProperty<std::vector<std::string>>(bus,
                                           bmcUpdaterService.c_str(),
                                           vrBundlePath.c_str(),
                                           bundleVersionInterface, "Checksum",bundleInterfaceObj.Checksum);


           sd_journal_print(LOG_INFO, "********SYSTEM SHOULD BE AC CYCLED TO ACTIAVTE THE SUCCESSFULLY UPGRADED FIRMWARES********");
        }
        else
        {
            sd_journal_print(LOG_ERR, "VR bundle json file doesn't exist. Update failed\n");
            rc = FAILURE;
        }
    }
    return rc;
}
