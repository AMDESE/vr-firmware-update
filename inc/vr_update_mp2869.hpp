#ifndef VR_UPDATE_MP2869_H_
#define VR_UPDATE_MP2869_H_


#include"vr_update.hpp"

#define VENDOR_ID_REG     (0x99)
#define DEVICE_ID_REG     (0xAD)
#define CONFIG_ID_REG     (0x9E)
#define SET_PAGE_REG      (0x00)
#define VENDOR_ID         (0x4D5053)
#define PRODUCT_ID        (0x9608)
#define PRODUCT_ID_1      (0x2869)

#define PAGE_0            (0)
#define PAGE_1            (1)
#define PAGE_2            (2)

#define BYTE_MASK         (0xFF)
#define REGISTER_15       (15)
#define USER_CRC_ADDR     (0xFF)
#define MASK_8            (0x000000FF)
#define MASK_16           (0x0000FF00)
#define MASK_24           (0x00FF0000)
#define UNLOCK_PROTECT    (0x10)
#define CMD_ED            (0xED)
#define CMD_F7            (0xF7)
#define CMD_CC            (0xCC)
#define CMD_17            (0x17)
#define CMD_18            (0x18)
#define PWD_UNLOCK_REG    (0x7E)
#define MFR_PWD_USER      (0xD7)
#define USER_CRC_REG      (0xED)
#define DATA_0            (0)
#define BLOCK_1           (1)
#define BLOCK_2           (2)
#define INT_16            (0x10)
#define INT_32            (0x20)

#define FILE_CONFIG_ID    ("MFR_CONFIG_ID")
#define CRC_USER          ("CRC_USER")
#define END_OF_REG        ("END")

#define MAXIMUM_SIZE      (255)
#define TOTAL_COLUMNS     (8)

class vr_update_mp2869: public vr_update
{

public:
    vr_update_mp2869(std::string Processor,uint32_t Crc,std::string Model,
          uint16_t SlaveAddress,std::string ConfigFilePath,
          std::string Revision,uint16_t PmbusAddress);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware();
};


#endif
