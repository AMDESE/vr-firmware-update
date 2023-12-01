#ifndef VR_UPDATE_TDA_H_
#define VR_UPDATE_TDA_H_

#define PAGE_NUM_REG 0xFF
#define SILICON_VER_REG 0xFD
#define USER_IMG_PTR1 0xB4
#define USER_IMG_PTR2 0xB6
#define USER_IMG_PTR3 0xB8
#define USER_READ_CMD 0x41
#define USER_READ_CMD 0x41
#define USER_PROG_CMD 0x00D6
#define UNLOCK_REG 0xD4
#define USER_PROG_CMD 0x00D6
#define PROG_STATUS_REG 0xD7
#define USER_READ_CMD 0x41
#define INDEX_40  0x40
#define INDEX_70  0x70
#define INDEX_200 0x200
#define INDEX_2FF 0x2FF
#define USER_PROG_STATUS 0x80
#define CRC_REG_1 0xAE
#define CRC_REG_2 0xB0

#define R2_REV1 0x8402
#define R2_REV2 0x9202
#define R4_REV1 0x8404
#define R4_REV2 0x9204
#define R5_REV1 0x8405
#define R5_REV2 0x9205

#define R2_REV ("R2")
#define R4_REV ("R4")
#define R5_REV ("R5")

class vr_update_infineon_tda: public vr_update
{

protected:
    int NextImgPtr;

public:
    vr_update_infineon_tda(std::string Processor,uint32_t Crc,std::string Model,
          uint16_t SlaveAddress,std::string ConfigFilePath,
          std::string Revision,uint16_t PmbusAddress);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware();
};

#endif
