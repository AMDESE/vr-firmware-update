#ifndef VR_UPDATE_MPS_H_
#define VR_UPDATE_MPS_H_


#include"vr_update.hpp"

#define VENDOR_ID_REG     (0x99)
#define DEVICE_ID_REG     (0xAD)
#define CONFIG_ID_REG     (0X9B)
#define SET_PAGE_REG      (0x00)

#define PAGE_0            (0)
#define PAGE_1            (1)
#define PAGE_2            (2)

#define REGISTER_15       (15)
#define CRC_ADDR          (0xB8)
#define MASK_8            (0x000000FF)
#define MASK_16           (0x0000FF00)
#define MASK_24           (0x00FF0000)

#define BLOCK_1           (1)
#define BLOCK_2           (2)
#define BLOCK_3           (3)


#define FILE_VENDOR_ID     ("Vendor ID")
#define FILE_DEVICE_ID     ("Device ID")
#define FILE_CONFIG_ID     ("Config ID")
#define END_OF_REG         ("END")

#define MAXIMUM_SIZE      (255)
#define TOTAL_COLUMNS     (8)

class vr_update_mps: public vr_update
{

public:
    vr_update_mps(std::string Processor,
          uint32_t Crc,std::string Model,
          uint16_t SlaveAddress,std::string ConfigFilePath,std::string Revision);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware(){return true;};
};


#endif
