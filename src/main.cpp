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
#include <experimental/filesystem>
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

struct bundleInterfaceStruct {
    std::vector<std::string> FirmwareID;
    std::vector<std::string> SlaveAddress;
    std::vector<std::string> Processor;
    std::vector<std::string> Status;
    std::vector<std::string> Versions;
};

bundleInterfaceStruct bundleInterfaceObj;

int vrUpdate(std::string Model, uint16_t SlaveAddress, uint32_t Crc,
             std::string Processor, std::string MachineName,std::string configFilePath)
{

    int ret = FAILURE;

    bool rc = false;
    vr_update* vr_update_obj;

    vr_update_obj = vr_update::CreateVRFrameworkObject(Model,
                    SlaveAddress,Crc,Processor,MachineName,configFilePath);

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

}
int main(int argc, char* argv[])
{

    int ret = FAILURE;
    int rc = SUCCESS;
    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    if(argc != INDEX_2)
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

            if (std::experimental::filesystem::exists(VR_PLATFORM_FILE))
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
                }
                setBundleVersionInterface(bus);
            }
        }

        if (std::experimental::filesystem::exists(vrBundleJsonFile))
        {
            std::ifstream json_file(vrBundleJsonFile);
            json data;
            json_file >> data;

            //iterate over the array of VR's
            for (json record : data["VR"])
            {
                std::string Model = record["ModelNumber"];
                std::string SlaveAddr = record["SlaveAddress"];
                uint16_t SlaveAddress = std::stoul(SlaveAddr, nullptr, BASE_16);
                std::string  CrcConfig = record["CRC"];
                uint32_t Crc = std::stoul(CrcConfig, nullptr, BASE_16);
                std::string Processor = record["Processor"];
                std::string MachineName =  record["MachineName"];
                std::string configFileName = record["ConfigFile"];
                std::string version = record["Version"];
                std::string configFilePath = filePath + '/' + configFileName;

                sd_journal_print(LOG_INFO, "Updating VR for the Slave Address = 0x%x", SlaveAddress);

                ret = vrUpdate(Model,SlaveAddress,Crc,Processor,MachineName,configFilePath);

                for (int i = 0; i < bundleInterfaceObj.SlaveAddress.size(); i++)
                {
                    if(bundleInterfaceObj.SlaveAddress[i] == SlaveAddr)
                    {
                        if(bundleInterfaceObj.Processor[i] == Processor)
                        {
                            bundleInterfaceObj.Versions[i] = version;

                            if(ret == SUCCESS)
                            {
                                bundleInterfaceObj.Status[i] = "PASS";
                            }
                            else
                            {
                                bundleInterfaceObj.Status[i] = "FAIL";
                                rc = FAILURE;
                            }
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
        }
        else
        {
            sd_journal_print(LOG_ERR, "VR bundle json file doesn't exist. Update failed\n");
        }
    }
    return rc;
}
