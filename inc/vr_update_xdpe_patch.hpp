#ifndef VR_UPDATE_XDPE_PATCH_H_
#define VR_UPDATE_XDPE_PATCH_H_

#define PATCH_BYTE        (0x10)
#define MAXBUFFERSIZE     (255)
#define PN                (1)
#define BLOCK_PREFIX      (0xfd)
#define BYTE_PREFIX       (0xfe)
#define DEVICE_ID_CMD     (0xad)
#define GET_OTP_SIZE      (0x10)
#define FW_VERSION        (0x1)
#define LENGTHOFBLOCK     (4)
#define FW_ROM_ID         (0x6192EE1E)

class vr_update_xdpe_patch: public vr_update
{

protected:
    uint32_t FirmwareCode;
    uint16_t ProductID;
public:
    vr_update_xdpe_patch(std::string Processor,
          uint32_t Crc,std::string Model,
          uint16_t SlaveAddress,std::string ConfigFilePath);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware();
};

#endif
