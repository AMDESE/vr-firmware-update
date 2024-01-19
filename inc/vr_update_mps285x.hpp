#ifndef VR_UPDATE_MPS285X_H_
#define VR_UPDATE_MPS285X_H_


#include"vr_update.hpp"

#define VENDOR_ID_REG     (0x99)
#define DEVICE_ID_REG     (0x9A)
#define CONFIG_ID_REG     (0x9D)
#define SET_PAGE_REG      (0x00)

#define PAGE_0            (0)
#define PAGE_1            (1)
#define PAGE_2            (2)
#define PAGE_29           (0x29)
#define PAGE_2A           (0x2A)

#define BYTE_MASK         (0xFF)
#define REGISTER_15       (15)
#define USER_CRC_ADDR     (0xFF)
#define MASK_8            (0x000000FF)
#define MASK_16           (0x0000FF00)
#define MASK_24           (0x00FF0000)
#define WRITE_PROTECT     (0x35)
#define UNLOCK_PROTECT    (0x10)
#define CMD_F1            (0xF1)
#define CMD_4F            (0x4F)
#define CMD_F3            (0xF3)
#define CMD_F8            (0xF8)
#define PWD_UNLOCK_REG    (0x7E)
#define MFR_PWD_USER      (0xC9)
#define USER_CRC_REG      (0xAD)
#define MULTI_CRC_REG     (0xAF)
#define DATA_0            (0)
#define DATA_63           (0x63)
#define BLOCK_1           (1)
#define BLOCK_2           (2)
#define BLOCK_3           (0xB2)


#define FILE_VENDOR_ID     ("VENDOR_ID")
#define FILE_DEVICE_ID     ("PRODUCT_ID")
#define FILE_CONFIG_ID     ("CONFIG_ID")
#define CRC_USER           ("CRC_USER")
#define CRC_MULTI          ("CRC_MULTI")
#define END_OF_REG         ("END")

#define MAXIMUM_SIZE      (255)
#define TOTAL_COLUMNS     (8)

class vr_update_mps285x: public vr_update
{

public:
    vr_update_mps285x(std::string Processor,uint32_t Crc,std::string Model,
          uint16_t SlaveAddress,std::string ConfigFilePath,
          std::string Revision,uint16_t PmbusAddress);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware();
};


#endif
