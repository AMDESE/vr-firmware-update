/*
* vr-update.hpp
*
* Created on: Nov 9, 2022
* 	Author: alkulkar
*/


#ifndef VR_UPDATE_H_
#define VR_UPDATE_H_


#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <stdint.h>
#include <algorithm>
#include <dirent.h>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <filesystem>
#include <phosphor-logging/log.hpp>

extern "C"
{
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <sys/ioctl.h>
#include <fcntl.h>
}

#define COMMAND_OUTPUT_LEN  (50)
#define	MPS2861				("MPS2861")
#define	MPS2862				("MPS2862")

#define GEN2                  ("GEN2")
#define GEN3                  ("GEN3")
#define RAA229613             ("RAA229613")
#define RAA229625             ("RAA229625")
#define RAA229620             ("RAA229620")
#define RAA229621             ("RAA229621")
#define ISL68220              ("ISL68220")
#define RENESAS               ("RENESAS")
#define INFINEON_XDPE         ("XDPE")
#define INFINEON_TDA          ("TDA")
#define PATCH                 ("PATCH")

#define SOCKET_0              ("P0")
#define SOCKET_1              ("P1")

#define BYTE_COUNT_5		  (5)
#define BYTE_COUNT_4		  (4)
#define BYTE_COUNT_3		  (3)
#define BYTE_COUNT_2          (2)
#define SHIFT_24              (24)
#define SHIFT_16              (16)
#define SHIFT_8               (8)
#define SHIFT_6               (6)
#define SHIFT_4               (4)

#define SUCCESS               (0)
#define FAILURE               (-1)

#define INDEX_0               (0)
#define INDEX_1               (1)
#define INDEX_2               (2)
#define INDEX_3               (3)
#define INDEX_4               (4)
#define INDEX_5               (5)
#define INDEX_6               (6)
#define INDEX_8               (8)
#define INDEX_10              (10)
#define INDEX_12              (12)
#define INDEX_14              (14)
#define INDEX_20              (20)
#define INDEX_32              (32)
#define INDEX_48              (48)

#define STATUS_BIT_0          (0)
#define STATUS_BIT_1          (1)
#define STATUS_BIT_2          (2)
#define STATUS_BIT_3          (3)
#define STATUS_BIT_4          (4)
#define STATUS_BIT_5          (5)
#define STATUS_BIT_6          (6)
#define STATUS_BIT_7          (7)
#define STATUS_BIT_8          (8)
#define INT_255               (0xFF)
#define INT_15                (0x0F)
#define SLAVE_13              (0x13)
#define SLAVE_14              (0x14)
#define SLAVE_15              (0x15)

#define BASE_16               (16)
#define FILE_PATH_SIZE        (256)
#define MIN_WAIT_TIME         (10000)

#define ISL_DRIVER_PATH       ("/sys/bus/i2c/drivers/isl68137/")
#define MPS_DRIVER_PATH       ("/sys/bus/i2c/drivers/MP2862/")
#define XDPE_DRIVER_PATH      ("/sys/bus/i2c/drivers/xdpe12284/")

#define SLEEP_1               (1000000)
#define SLEEP_2               (2000000)
#define SLEEP_1000            (1000)

class vr_update {

protected:
    uint16_t SlaveAddress;
    uint16_t BusNumber;
    uint32_t Crc;
    std::string Processor;
    std::string Model;
    std::string ConfigFilePath;
    std::string DriverPath;
    int fd;
    char updateFilePath[FILE_PATH_SIZE];

public:
 vr_update(std::string Processor,uint32_t Crc,std::string Model,
          uint16_t SlaveAddress,std::string ConfigFilePath);

 static vr_update* CreateVRFrameworkObject(std::string Model,
              uint16_t SlaveAddress, uint32_t Crc, std::string Processor,
              std::string configFilePath,std::string UpdateType);

    virtual bool isUpdatable() = 0;
    virtual bool findBusNumber();
    virtual bool openI2cDevice();
    virtual void closeI2cDevice();
    virtual bool UpdateFirmware() = 0;
    virtual bool crcCheckSum() = 0;
    virtual bool ValidateFirmware() = 0;

};

#endif

