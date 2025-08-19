#ifndef VR_UPDATE_ISM6636X_H_
#define VR_UPDATE_ISM6636X_H_

#include "vr_update.hpp"

#define PRODUCT_ID_REG (0xA)
#define IC_VERSION_REG (0xB)
#define PRODUCT_ID (0x36)
#define IC_VERSION (0x1)
#define USR_PTR_CMD (0x20)
#define USR_PTR_MAX (0x3F)
#define USER_OTP_ON (0x1D)
#define USER_OTP_DATA (0x2)
#define OTP_PROG_CMD (0x2B)
#define OTP_PROG_DATA (0x15)

class vr_update_ism6636x : public vr_update
{
  public:
    vr_update_ism6636x(std::string Processor, uint32_t Crc, std::string Model,
                       uint16_t SlaveAddress, std::string ConfigFilePath,
                       std::string Revision, uint16_t PmbusAddress);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware();
};

#endif
