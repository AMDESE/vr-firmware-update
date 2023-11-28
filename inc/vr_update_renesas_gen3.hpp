#ifndef VR_UPDATE_RENESAS_GEN3_H_
#define VR_UPDATE_RENESAS_GEN3_H_

#define DISABLE_PACKET       (0x00A2)
#define FINISH_CAPTURE       (0x0002)
#define DEVICE_FW_VERSION    (0x0030)
#define HALT_FW              (0x0002)
#define POLL_REG             (0x004E)
#define FINISH_CMD_CODE      (0xE7)
#define FW_WRITE             (0xE6)
#define DMA_WRITE             0xC7
#define DMA_READ             (0xC5)
#define GEN3_CRC_ADDR        0x0094
#define GEN3_NVM_SLOT_ADDR   (0x0035)
#define DEV_ID_CMD           (0xAD)
#define DEV_REV_REV          (0xAE)
#define GEN3_BANK_REG        (0x007F)
#define GEN3_PRGM_STATUS     (0x007E)
#define DEVICE_REVISION      (0x6000000)

#define MAXIMUM_SIZE         (255)
#define MIN_NVM_SLOT         (5)

#define BIT_ENABLE            (1)

class vr_update_renesas_gen3: public vr_update
{

public:
    vr_update_renesas_gen3(std::string Processor,
          uint32_t Crc,std::string Model,
          uint16_t SlaveAddress,std::string ConfigFilePath,std::string Revision);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware();
};

#endif
