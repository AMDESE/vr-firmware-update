/*
 * main.cpp
 *
 * Created on: Nov 10, 2022
 * Author: Abinaya Dhandapani
 */

#include "vr_update.hpp"

#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/property.hpp>

#include <filesystem>
#include <iostream>
#include <map>

#define VR_PLATFORM_FILE ("/var/lib/vr-config/platform-vr.json")

using json = nlohmann::json;

using DbusVariant = uint32_t;

std::string bmcUpdaterService = "xyz.openbmc_project.Software.BMC.Updater";
std::string vrBundlePath = "/xyz/openbmc_project/software/vr_bundle_active";
constexpr auto bundleVersionInterface =
    "xyz.openbmc_project.Software.BundleVersion";

#define COMMAND_BOARD_ID ("/sbin/fw_printenv -n board_id")
#define COMMAND_LEN 3

/* SP5 Platform IDs */
#define ONYX_SLT 61   // 0x3D
#define ONYX_1 64     // 0x40
#define ONYX_2 65     // 0x41
#define ONYX_3 66     // 0x42
#define ONYX_FR4 82   // 0x52
#define QUARTZ_DAP 62 // 0x3E
#define QUARTZ_1 67   // 0x43
#define QUARTZ_2 68   // 0x44
#define QUARTZ_3 69   // 0x45
#define QUARTZ_FR4 81 // 0x51
#define RUBY_1 70     // 0x46
#define RUBY_2 71     // 0x47
#define RUBY_3 72     // 0x48
#define TITANITE_1 73 // 0x49
#define TITANITE_2 74 // 0x4A
#define TITANITE_3 75 // 0x4B
#define TITANITE_4 76 // 0x4C
#define TITANITE_5 77 // 0x4D
#define TITANITE_6 78 // 0x4E

/* SH5 Platform IDs */
#define SH5_1P_PWR 92     // 0x5C
#define SH5_1P_OEM 93     // 0x5D
#define SH5_1P_SLT 94     // 0x5E
#define SH5_2P_CABLED 108 // 0x6C
#define SH5_1P_OEM_P 109  // 0x6D
#define SH5_SIDLEY 95     // 0x5F
#define SH5_PARRY_PEAK 96 // 0x60

/* SP6 Platform IDs */
#define SHALE_64 89     // 0x59
#define SHALE_SLT 98    // 0x62
#define SHALE 101       // 0x65
#define SUNSTONE_DAP 97 // 0x61
#define CINNABAR 99     // 0x63
#define SUNSTONE 100    // 0x64

/* Turin Platform IDs */
#define CHALUPA 102   // 0x66
#define CHALUPA_1 110 // 0x6E
#define CHALUPA_2 111 // 0x6F
#define HUAMBO 103    // 0x67
#define GALENA 104    // 0x68
#define GALENA_1 112  // 0x70
#define GALENA_2 113  // 0x71
#define RECLUSE 105   // 0x69
#define PURICO 106    // 0x6A
#define PURICO_1 114  // 0x72
#define PURICO_2 115  // 0x73
#define VOLCANO 107   // 0x6B
#define VOLCANO_1 116 // 0x74
#define VOLCANO_2 117 // 0x75

/* Venice Platform IDs */
#define CONGO 128     // 0x80
#define CONGO_1 129   // 0x81
#define CONGO_2 134   // 0x86

#define MOROCCO 130   // 0x82
#define MOROCCO_1 131 // 0x83
#define MOROCCO_2 135 // 0x87
#define MALAWI 138    // 0x8A
#define MARRAKESH 176 // 0xB0

#define KENYA 132     // 0x84

#define NIGERIA 133   // 0x85

#define GHANA 142 // 0x8E

/*Venice SLT boards*/
#define SENEGAL_SLT 136 // 0x88
#define SAHARA 137      // 0x89
#define ZAMBIA 139      // 0x8B
#define ZIMBABWE 140    // 0x8C
#define ZANZIBAR 141    // 0x8D
#define ZAIRE 158       // 0x9E

/*SP8 Platform IDs */
#define EAGLE 159      // 0x9F
#define EAGLE_1 160    // 0xA0
#define EAGLE_2 161    // 0xA1
#define HORNBILL 165   // 0xA5
#define HORNBILL_1 166 // 0xA6
#define HORNBILL_2 167 // 0xA7
#define HORNBILL_3 168 // 0xA8
#define HORNBILL_4 169 // 0xA9
#define HORNBILL_5 170 // 0xAA
#define HORNBILL_6 171 // 0xAB
#define HORNBILL_7 172 // 0xAC
#define HORNBILL_8 173 // 0xAD

/*SP8 Venice SLT boards*/
#define ROBIN 174     // 0xAE
#define SANDPIPER 175 // 0xAF
#define PEACOCK 184   // 0xB8
#define DUCK 162      // 0xA2
#define DUCK_1 163    // 0xA3
#define DUCK_2 164    // 0xA4

#define FALCON 177    // 0xB1
#define FALCON_1 178  // 0xB2
#define FALCON_2 179  // 0xB3
#define FALCON_3 180  // 0xB4
     
#define SEAGULL 181   // 0xB5
#define SEAGULL_1 182 // 0xB6
#define SEAGULL_2 183 // 0xB7

struct bundleInterfaceStruct
{
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
             uint32_t* Version, std::string Processor,
             std::string configFilePath, std::string UpdateType,
             bool* CrcMatched, std::string Revision, uint16_t PmbusAddress,
             std::vector<std::string>& configFilePathArr)
{
    int ret = FAILURE;

    bool rc = false;
    vr_update* vr_update_obj;

    vr_update_obj = vr_update::CreateVRFrameworkObject(
        Model, SlaveAddress, Crc, Processor, configFilePath, UpdateType,
        Revision, PmbusAddress, configFilePathArr);

    if (vr_update_obj != NULL)
    {
        rc = vr_update_obj->findBusNumber();

        if (rc != true)
        {
            sd_journal_print(LOG_ERR, "Unable to find the bus number\n");
            ret = FAILURE;
            goto Clean;
        }

        rc = vr_update_obj->openI2cDevice();
        if (rc != true)
        {
            sd_journal_print(LOG_ERR, "Unable to Open I2c slave device\n");
            ret = FAILURE;
            goto Clean;
        }

        rc = vr_update_obj->crcCheckSum();
        if (rc != true)
        {
            *CrcMatched = vr_update_obj->CrcMatched;
            ret = FAILURE;
            goto Clean;
        }

        rc = vr_update_obj->isUpdatable();
        *Version = vr_update_obj->devVersion;

        if (rc != true)
        {
            ret = FAILURE;
            goto Clean;
        }

        rc = vr_update_obj->UpdateFirmware();

        if (rc != true)
        {
            ret = FAILURE;
            goto Clean;
        }

        rc = vr_update_obj->ValidateFirmware();
        *Version = vr_update_obj->devVersion;

        if (rc != true)
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
static auto
    setProperty(sdbusplus::bus::bus& bus, const char* service, const char* path,

                const char* interface, const char* propertyName, const T& value)
{
    std::variant<T> data = value;

    try
    {
        auto method = bus.new_method_call(
            service, path, "org.freedesktop.DBus.Properties", "Set");
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
    std::vector<std::string> FirmwareID = getProperty<std::vector<std::string>>(
        bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
        bundleVersionInterface, "FirmwareID");

    std::vector<std::string> SlaveAddress =
        getProperty<std::vector<std::string>>(
            bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
            bundleVersionInterface, "SlaveAddress");

    std::vector<std::string> Processor = getProperty<std::vector<std::string>>(
        bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
        bundleVersionInterface, "Processor");

    std::vector<std::string> Versions = getProperty<std::vector<std::string>>(
        bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
        bundleVersionInterface, "Versions");

    std::vector<std::string> Status = getProperty<std::vector<std::string>>(
        bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
        bundleVersionInterface, "Status");

    std::vector<std::string> Checksum = getProperty<std::vector<std::string>>(
        bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
        bundleVersionInterface, "Checksum");

    if (FirmwareID.empty())
    {
        sd_journal_print(LOG_ERR,
                         "VR details are not available in dbus interface \n");
        return false;
    }
    else
    {
        bundleInterfaceObj.FirmwareID.assign(FirmwareID.begin(),
                                             FirmwareID.end());
        bundleInterfaceObj.SlaveAddress.assign(SlaveAddress.begin(),
                                               SlaveAddress.end());
        bundleInterfaceObj.Processor.assign(Processor.begin(), Processor.end());
        bundleInterfaceObj.Versions.assign(Versions.begin(), Versions.end());
        bundleInterfaceObj.Status.assign(Status.begin(), Status.end());
        bundleInterfaceObj.Checksum.assign(Checksum.begin(), Checksum.end());

        bundleInterfaceObj.UpdateStatus.assign(
            bundleInterfaceObj.SlaveAddress.size(), false);
        bundleInterfaceObj.PmbusAddress.assign(
            bundleInterfaceObj.SlaveAddress.size(), "Unknown");
        return true;
    }
}

void setBundleVersionInterface(sdbusplus::bus::bus& bus)
{
    setProperty<std::vector<std::string>>(
        bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
        bundleVersionInterface, "FirmwareID", bundleInterfaceObj.FirmwareID);

    setProperty<std::vector<std::string>>(
        bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
        bundleVersionInterface, "SlaveAddress",
        bundleInterfaceObj.SlaveAddress);

    setProperty<std::vector<std::string>>(
        bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
        bundleVersionInterface, "Processor", bundleInterfaceObj.Processor);

    setProperty<std::vector<std::string>>(
        bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
        bundleVersionInterface, "Versions", bundleInterfaceObj.Versions);

    setProperty<std::vector<std::string>>(
        bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
        bundleVersionInterface, "Status", bundleInterfaceObj.Status);

    setProperty<std::vector<std::string>>(
        bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
        bundleVersionInterface, "Checksum", bundleInterfaceObj.Checksum);
}

bool PlatformIDValidation(std::string BoardName)
{
    FILE* pf;
    unsigned int board_id = 0;
    char data[COMMAND_LEN];
    bool PLATID = false;
    std::stringstream ss;
    std::string PlatformName;

    pf = popen(COMMAND_BOARD_ID, "r");
    // Error handling
    if (pf)
    {
        // Get the data from the process execution
        if (fgets(data, COMMAND_LEN, pf))
        {
            ss << std::hex << (std::string)data;
            ss >> board_id;
            PLATID = true;
            sd_journal_print(LOG_DEBUG, "Board ID: 0x%x, Board ID String: %s\n",
                             board_id, data);
        }
        // the data is now in 'data'
        pclose(pf);

        if ((board_id == ONYX_1) || (board_id == ONYX_2) ||
            (board_id == ONYX_3) || (board_id == ONYX_FR4) ||
            (board_id == ONYX_SLT))
        {
            PlatformName = "Onyx";
        }
        else if ((board_id == QUARTZ_DAP) || (board_id == QUARTZ_1) ||
                 (board_id == QUARTZ_2) || (board_id == QUARTZ_3) ||
                 (board_id == QUARTZ_FR4))
        {
            PlatformName = "Quartz";
        }
        else if ((board_id == RUBY_1) || (board_id == RUBY_2) ||
                 (board_id == RUBY_3))
        {
            PlatformName = "Ruby";
        }
        else if ((board_id == TITANITE_1) || (board_id == TITANITE_2) ||
                 (board_id == TITANITE_3) || (board_id == TITANITE_4) ||
                 (board_id == TITANITE_5) || (board_id == TITANITE_6))
        {
            PlatformName = "Titanite";
        }
        else if ((board_id == SHALE_64) || (board_id == SHALE_SLT) ||
                 (board_id == SHALE))
        {
            PlatformName = "Shale";
        }
        else if (board_id == CINNABAR)
        {
            PlatformName = "Cinnabar";
        }
        else if ((board_id == SUNSTONE) || (board_id == SUNSTONE_DAP))
        {
            PlatformName = "Sunstone";
        }
        else if ((board_id == CHALUPA) || (board_id == CHALUPA_1) ||
                 (board_id == CHALUPA_2))
        {
            PlatformName = "Chalupa";
        }
        else if (board_id == HUAMBO)
        {
            PlatformName = "Huambo";
        }
        else if ((board_id == GALENA) || (board_id == GALENA_1) ||
                 (board_id == GALENA_2))
        {
            PlatformName = "Galena";
        }
        else if (board_id == RECLUSE)
        {
            PlatformName = "Recluse";
        }
        else if ((board_id == PURICO) || (board_id == PURICO_1) ||
                 (board_id == PURICO_2))
        {
            PlatformName = "Purico";
        }
        else if ((board_id == VOLCANO) || (board_id == VOLCANO_1) ||
                 (board_id == VOLCANO_2))
        {
            PlatformName = "Volcano";
        }
        else if ((board_id == SH5_1P_PWR) || (board_id == SH5_1P_OEM) ||
                 (board_id == SH5_1P_SLT) || (board_id == SH5_1P_OEM_P) ||
                 (board_id == SH5_2P_CABLED))
        {
            PlatformName = "SH5";
        }
        else if ((board_id == CONGO) || (board_id == CONGO_1) ||
                 (board_id == CONGO_2) || (board_id == SENEGAL_SLT) ||
                 (board_id == ZAMBIA) || (board_id == ZIMBABWE) ||
                 (board_id == ZANZIBAR) || (board_id == SAHARA) ||
                 (board_id == ZAIRE))
        {
            PlatformName = "Congo";
        }
        else if ((board_id == MOROCCO) || (board_id == MOROCCO_1) ||
                 (board_id == MOROCCO_2) || (board_id == MALAWI)  ||
                 (board_id == MARRAKESH))
        {
            PlatformName = "Morocco";
        }
        else if (board_id == KENYA)
        {
            PlatformName = "Kenya";
        }
        else if (board_id == NIGERIA)
        {
            PlatformName = "Nigeria";
        }
        else if (board_id == GHANA)
        {
            PlatformName = "Ghana";
        }
        else if ((board_id == EAGLE) || (board_id == EAGLE_1) ||
                 (board_id == EAGLE_2) || (board_id == ROBIN) ||
                 (board_id == SANDPIPER) || (board_id == PEACOCK))
        {
            PlatformName = "Eagle";
        }
        else if ((board_id == HORNBILL) || (board_id == HORNBILL_1) ||
                 (board_id == HORNBILL_2) || (board_id == HORNBILL_3) ||
                 (board_id == HORNBILL_4) || (board_id == HORNBILL_5) ||
                 (board_id == HORNBILL_6) || (board_id == HORNBILL_7) ||
                 (board_id == HORNBILL_8) || (board_id == DUCK) ||
                 (board_id == DUCK_1) || (board_id == DUCK_2))
        {
            PlatformName = "Hornbill";
        }
        else if ((board_id == FALCON) || (board_id == FALCON_1) ||
                 (board_id == FALCON_2) || (board_id == FALCON_3))
        {
            PlatformName = "Falcon";
        }
        else if ((board_id == SEAGULL) || (board_id == SEAGULL_1) ||
                 (board_id == SEAGULL_2))
        {
            PlatformName = "Seagull";
        }

        if ((strcasecmp(BoardName.c_str(), PlatformName.c_str())) != SUCCESS)
        {
            sd_journal_print(
                LOG_ERR,
                "The board name from config file does not match with the platform "
                "Skipping the update\n");
            return false;
        }
    }
    return true;
}

inline void trim(std::string& str)
{
    str.erase(str.begin(),
              std::find_if(str.begin(), str.end(),
                           [](unsigned char ch) { return !std::isspace(ch); }));
    str.erase(std::find_if(str.rbegin(), str.rend(),
                           [](unsigned char ch) { return !std::isspace(ch); })
                  .base(),
              str.end());
}

int main(int argc, char* argv[])
{
    int ret = FAILURE;
    int rc = SUCCESS;
    uint16_t SlaveAddress;
    uint16_t PmbusAddress = 0;
    std::string BoardName;
    uint32_t Crc;
    uint32_t deviceVersion = 0;
    std::string version;
    std::string UpdateType;
    std::string Revision;
    std::string configFilePath;
    std::vector<std::string> configFilePathArr;
    std::string Processor;
    std::string Model;
    std::string CrcConfig;
    std::string SlaveAddr;
    std::string PmbusAddr;

    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    if (argc < INDEX_2)
    {
        sd_journal_print(LOG_ERR, "Invalid Number of Command line Arguments\n");
        rc = FAILURE;
    }
    else
    {
        std::string filePath = argv[INDEX_1];
        std::string vrBundleJsonFile = filePath + "/vrbundle.json";

        if (!(std::filesystem::exists(VR_PLATFORM_FILE)))
        {
            // Copy platfom specific VR config file
            std::string command =
                "/usr/sbin/vr-config-info install_vr_platform_config";

            int ret = system(command.c_str());

            if (ret != SUCCESS)
            {
                sd_journal_print(LOG_ERR,
                                 "Copying vr-platform-config file failed\n");
                return FAILURE;
            }
        }

        if (std::filesystem::exists(PATCH_VERSION_FILE))
        {
            std::filesystem::remove(PATCH_VERSION_FILE);
        }

        std::ofstream patchFile;
        patchFile.open(PATCH_VERSION_FILE, std::ios::app);
        patchFile
            << "#SlaveAddress,BusNUmber,VersionBeforeUpdate,VersionAfterUpdate"
            << std::endl;
        patchFile.close();

        if (getBundleVersionInterface(bus) == false)
        {
            if (std::filesystem::exists(VR_PLATFORM_FILE))
            {
                std::ifstream json_file(VR_PLATFORM_FILE);
                json data;
                json_file >> data;

                for (json record : data["VRConfigs"])
                {
                    bundleInterfaceObj.SlaveAddress.push_back(
                        record["SlaveAddress"]);
                    bundleInterfaceObj.FirmwareID.push_back(record["VrName"]);
                    bundleInterfaceObj.Processor.push_back(record["Processor"]);
                    bundleInterfaceObj.Versions.push_back("Unknown");
                    bundleInterfaceObj.Status.push_back("Unknown");
                    bundleInterfaceObj.Checksum.push_back("Unknown");
                    bundleInterfaceObj.UpdateStatus.push_back(false);

                    if (record.contains("PmbusAddress"))
                    {
                        bundleInterfaceObj.PmbusAddress.push_back(
                            record["PmbusAddress"]);
                    }
                    else
                    {
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

            // iterate over the array of VR's
            for (json record : data["VR"])
            {
                if (record.contains("ModelNumber"))
                {
                    Model = record["ModelNumber"];
                }
                else
                {
                    sd_journal_print(
                        LOG_ERR,
                        "Json file doesnt have model number. Update aborted\n");
                    return false;
                }

                if (record.contains("SlaveAddress"))
                {
                    SlaveAddr = record["SlaveAddress"];

                    SlaveAddress = std::stoul(SlaveAddr, nullptr, BASE_16);
                    if (std::filesystem::exists(VR_PLATFORM_FILE))
                    {
                        std::ifstream vr_json_file(VR_PLATFORM_FILE);
                        nlohmann::json vr_data;
                        vr_json_file >> vr_data;

                        for (nlohmann::json platform_record : vr_data["VRConfigs"])
                        {
                            std::string PlatformSlaveAddr = platform_record["SlaveAddress"];
                            uint16_t PlatformSlaveAddress=std::stoul(PlatformSlaveAddr, nullptr, BASE_16);
                            if (PlatformSlaveAddress == SlaveAddress)
                            {
                                if (platform_record.contains("PmbusAddress"))
                                {
                                    std::string PmbusAddr =
                                        platform_record["PmbusAddress"];
                                    PmbusAddress =
                                        std::stoul(PmbusAddr, nullptr, BASE_16);
                                }
                            }
                        }
                    }
                }
                else
                {
                    sd_journal_print(
                        LOG_ERR,
                        "Json file doesnt have slave address. Update aborted\n");
                    return false;
                }

                if (record.contains("CRC"))
                {
                    CrcConfig = record["CRC"];
                    Crc = std::stoul(CrcConfig, nullptr, BASE_16);
                }

                if (record.contains("Processor"))
                {
                    Processor = record["Processor"];
                }
                else
                {
                    sd_journal_print(
                        LOG_ERR,
                        "Json file doesnt have Processor details. Update aborted\n");
                    return false;
                }

                if (record.contains("BoardName"))
                {
                    BoardName = record["BoardName"];
                }
                else
                {
                    sd_journal_print(
                        LOG_ERR,
                        "Json file doesnt have BoadrdName details. Update aborted\n");
                    return false;
                }

                if (record.contains("ConfigFile"))
                {
                    std::string configFileName = record["ConfigFile"];
                    configFilePath = filePath + '/' + configFileName;
                }
                else
                {
                    sd_journal_print(
                        LOG_ERR,
                        "Json file doesnt have ConfigFile details. Update aborted\n");
                    return false;
                }

                if (record.contains("ConfigFile"))
                {
                    std::string configFileName = record["ConfigFile"];
                    // Check if the ConfigFile contains multiple file names
                    if (configFileName.find(',') != std::string::npos)
                    {
                        std::string fileName;
                        std::stringstream ss(configFileName);

                        while (std::getline(ss, fileName, ','))
                        {
                            trim(fileName);
                            configFilePathArr.push_back(
                                filePath + '/' + fileName);
                        }
                    }
                    else
                    {
                        configFilePath = filePath + '/' + configFileName;
                    }
                }
                else
                {
                    sd_journal_print(
                        LOG_ERR,
                        "Json file doesnt have ConfigFile details. Update aborted\n");
                    return false;
                }

                if (record.contains("Version"))
                {
                    version = record["Version"];
                }

                if (record.contains("UpdateType"))
                {
                    UpdateType = record["UpdateType"];
                }
                else
                {
                    sd_journal_print(
                        LOG_ERR,
                        "Json file doesnt have UpdateType details. Update aborted\n");
                    return false;
                }

                if (record.contains("Revision"))
                {
                    Revision = record["Revision"];
                }
                else
                {
                    Revision = "None";
                }

                bool CrcMatched = false;

                if (PlatformIDValidation(BoardName) == false)
                {
                    return false;
                }

                sd_journal_print(LOG_INFO,
                                 "Updating VR for the Slave Address = 0x%x",
                                 SlaveAddress);

                ret = vrUpdate(Model, SlaveAddress, Crc, &deviceVersion,
                               Processor, configFilePath, UpdateType,
                               &CrcMatched, Revision, PmbusAddress,
                               configFilePathArr);

                for (int i = 0; i < bundleInterfaceObj.SlaveAddress.size(); i++)
                {
                    std::string BundleSlaveAddr = bundleInterfaceObj.SlaveAddress[i];
                    uint16_t BundleSlaveAddress=std::stoul(BundleSlaveAddr, nullptr, BASE_16);
                    bool addressMatched = (BundleSlaveAddress == SlaveAddress) ||
                      (BundleSlaveAddress == PmbusAddress);
                    if (addressMatched && (bundleInterfaceObj.UpdateStatus[i] == false))
                    {
                        if (strcasecmp(bundleInterfaceObj.Processor[i].c_str(),
                                       Processor.c_str()) == SUCCESS)
                        {
                            if (deviceVersion != 0)
                            {
                                std::string hexVersion =
                                    std::format("{:08X}", deviceVersion);
                                bundleInterfaceObj.Versions[i] =
                                    "0x" + hexVersion;
                            }
                            else
                            {
                                bundleInterfaceObj.Versions[i] = version;
                            }
                            if (ret == SUCCESS)
                            {
                                bundleInterfaceObj.Checksum[i] = CrcConfig;
                                bundleInterfaceObj.Status[i] =
                                    "Update Completed";
                            }
                            else
                            {
                                if (CrcMatched == true)
                                {
                                    bundleInterfaceObj.Status[i] =
                                        "Already UpToDate";
                                }
                                else
                                {
                                    bundleInterfaceObj.Status[i] =
                                        "Update Failed";
                                    rc = FAILURE;
                                }
                            }
                            bundleInterfaceObj.UpdateStatus[i] = true;
                        }
                    }
                }
            }
            setProperty<std::vector<std::string>>(
                bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
                bundleVersionInterface, "Versions",
                bundleInterfaceObj.Versions);

            setProperty<std::vector<std::string>>(
                bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
                bundleVersionInterface, "Status", bundleInterfaceObj.Status);

            setProperty<std::vector<std::string>>(
                bus, bmcUpdaterService.c_str(), vrBundlePath.c_str(),
                bundleVersionInterface, "Checksum",
                bundleInterfaceObj.Checksum);

            sd_journal_print(
                LOG_INFO,
                "********SYSTEM SHOULD BE AC CYCLED TO ACTIAVTE THE SUCCESSFULLY UPGRADED FIRMWARES********");
        }
        else
        {
            sd_journal_print(
                LOG_ERR, "VR bundle json file doesn't exist. Update failed\n");
            rc = FAILURE;
        }
    }
    return rc;
}
