#ifndef VR_UPDATE_RENESAS_PATCH_H_
#define VR_UPDATE_RENESAS_PATCH_H_

#define FW_WRITE             (0xE6)
#define DMA_WRITE            (0xC7)
#define DMA_READ             (0xC5)
#define CANDIDATE_FW         (0x02000002)
#define HALT_FW              (0x0002)
#define DEVICE_FW_VERSION    (0x0030)
#define POLL_REG             (0x004E)
#define CMD_CODE_C6          (0xC6)
#define CMD_CODE_E6          (0xE6)
#define CMD_CODE_C7          (0xC7)
#define COMMIT_DATA          (0x000f)

#define COMPLETE_BIT         (0x10)
#define PASS_BIT             (0x80)
#define FAIL_BIT             (0x40)

#define INDEX_0              (0)
#define INDEX_1              (1)
#define INDEX_2              (2)
#define INDEX_3              (3)
#define INDEX_4              (4)
#define INDEX_5              (5)

#define INT_255               (0xFF)
#define MASK_BYTE_2           (0xFF00)
#define MASK_BYTE_3           (0xFF0000)
#define MASK_BYTE_4           (0xFF000000)
#define MASK_TWO_BYTES        (0xFFFF)
#define MAXIMUM_SIZE          (255)

#define MAX_RETRY             (10)

class vr_update_renesas_patch: public vr_update
{

public:
    vr_update_renesas_patch(std::string Processor,
          uint32_t Crc,std::string Model,
          uint16_t SlaveAddress,std::string ConfigFilePath);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware();
};

#endif
