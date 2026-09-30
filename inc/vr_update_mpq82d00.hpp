#ifndef VR_UPDATE_MPQ82D00_H_
#define VR_UPDATE_MPQ82D00_H_

#include "vr_update.hpp"

/* AN243 configuration sequence for MPQ82D00 */
#define MPQ82D00_PAGE_REG (0x00)
#define MPQ82D00_PAGE0 (0x00)
#define MPQ82D00_VENDOR_ID_REG (0x99)
#define MPQ82D00_DEVICE_ID_REG (0x9A)
#define MPQ82D00_CONFIG_ID_REG (0xB5)
#define MPQ82D00_STORE_ALL (0x15)
#define MPQ82D00_USER_CRC_REG (0xC8)

#define MPQ82D00_VENDOR_ID (0x53504D)
#define MPQ82D00_DEVICE_ID ("MPQ82D00")
#define MPQ82D00_VENDOR_ID_LEN (3)
#define MPQ82D00_DEVICE_ID_LEN (8)

#define MPQ82D00_BYTE_COUNT (1)
#define MPQ82D00_WORD_COUNT (2)
#define MPQ82D00_ATE_COLUMNS (8)
#define MPQ82D00_STORE_WAIT_US (1500000)

#define MPQ82D00_CFG_OK (0)
#define MPQ82D00_CFG_CRC_MISMATCH (1)
#define MPQ82D00_CFG_FAILED (-1)

class vr_update_mpq82d00 : public vr_update
{
  public:
    vr_update_mpq82d00(std::string Processor, uint32_t Crc, std::string Model,
                       uint16_t SlaveAddress, std::string ConfigFilePath,
                       std::string Revision, uint16_t PmbusAddress);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware();

  private:
    bool selectPage(uint8_t page);
    bool readUserCrc(uint16_t& crc);
    bool readFileConfigId(uint16_t& configId);
    bool readFileUserCrc(uint16_t& userCrc);
    bool writeConfigRegisters();
    int configureOnce(uint16_t expectedCrc);
};

#endif
