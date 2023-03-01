#ifndef VR_UPDATE_XDPE_PATCH_H_
#define VR_UPDATE_XDPE_PATCH_H_

#define PATCH_BYTE         (0x10)
#define MAXBUFFERSIZE      (255)
#define PN                 (1)
#define BLOCK_PREFIX       (0xfd)
#define MFR_REG_WRITE      (0xde)
#define RPTR               (0xce)
#define BYTE_PREFIX        (0xfe)
#define MFR_SPECIFIC_COMMAND (0x6)
#define DEVICE_ID_CMD      (0xad)
#define GET_OTP_SIZE       (0x10)
#define INVALIDATE_OTP     (0xe)
#define FS_FILE_INVALIDATE (0x12)
#define GET_FW_ADDRESS     (0x2e)
#define FW_VERSION         (0x1)
#define LENGTHOFBLOCK      (4)
#define FW_ROM_ID          (0x6192EE1E)
#define INT_256            (256)
#define INT_1024           (1024)
#define INT_1020           (1020)

#define SCRATCHPAD_1       (0xe0)
#define SCRATCHPAD_2       (0x05)
#define SCRATCHPAD_3       (0x20)

#define CTRL_ADDR_0        (0x18)
#define CTRL_ADDR_1        (0x85)
#define CTRL_ADDR_2        (0x05)
#define CTRL_ADDR_3        (0x20)
#define INT_15             (0x0f)
#define INT_16             (0x10)

class vr_update_xdpe_patch: public vr_update
{

protected:
    uint32_t FirmwareCode;
    uint16_t ProductID;
    uint16_t PatchFileSize;
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
