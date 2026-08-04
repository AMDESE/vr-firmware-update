#ifndef VR_UPDATE_FAN2510XX_H_
#define VR_UPDATE_FAN2510XX_H_

#include "vr_update.hpp"

#define USER_CRC_REG (0xCA)
#define STORE_USER_ALL_CMD (0x15)

class vr_update_fan2510xx : public vr_update
{
  public:
    vr_update_fan2510xx(std::string Processor, uint32_t Crc, std::string Model,
                        uint16_t SlaveAddress, std::string ConfigFilePath,
                        std::string Revision, uint16_t PmbusAddress);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware();
    bool crcReadbackValid() override { return true; }
};

#endif
