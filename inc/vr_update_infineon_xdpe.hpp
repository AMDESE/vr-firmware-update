#ifndef VR_UPDATE_XDPE_H_
#define VR_UPDATE_XDPE_H_


#define MAXBUFFERSIZE     (255)
#define LENGTHOFBLOCK     (4)
#define MINOTPSIZE        (6000)
#define MINWAITTIME       (2000)
#define MAXWAITTIME       (500000)
#define GETCRCWAITTIME    (20000)
#define AVAILBYTEWAITTIME (1000)
#define INVALBYTEWAITTIME (4000)
#define GETFWADDRWAITTIME (500)
#define MINARGS           (4)
#define INT_255           (0xFF)
#define TRIM_HEADER_CODE  "00000002"

/* Applicable Digital Controllers */
#define PART1             (0x95)
#define PART2             (0x96)
#define PART3             (0x97)
#define PART4             (0x98)
#define PART5             (0x99)
#define PART6             (0xae)
#define PART7             (0xa5)

/* CMD PREFIX */

#define DEVICE_ID_CMD     (0xad)
#define RPTR              (0xce)
#define MFR_REG_WRITE     (0xde)
#define MFR_REG_READ      (0xdf)
#define BLOCK_PREFIX      (0xfd)
#define BYTE_PREFIX       (0xfe)
#define PAGE_NUM_REG      (0xFF)
#define USER_IMG_PTR1     (0xB4)
#define USER_IMG_PTR2     (0xB6)
#define USER_IMG_PTR3     (0xB8)
#define CRC_REG_1         (0xAE)
#define CRC_REG_2         (0xB0)
#define CONFIG_IMG_PTR    (0xB2)
#define UNLOCK_REG        (0xD4)
#define USER_PROG_CMD     (0x00D6)
#define PROG_STATUS_REG   (0xD7)
#define USER_READ_CMD     (0x41)
#define GET_FW_ADDRESS    (0x2e)

/* BYTE DATA */
#define AVAIL_SPACE_BYTE  (0x10)
#define GET_CRC           (0x2d)
#define UPLOAD_BYTE       (0x11)
#define INVAL_BYTE        (0x12)

/* DEVICE DISCOVERY BLOCK DATA (DDBD) */
#define DDBD0             (0x8c)
#define DDBD1             (0x00)
#define DDBD2             (0x00)
#define DDBD3             (0x70)

#define USER_PROG_STATUS  (0x80)

#define REV_A             ("RevA")
#define REV_B             ("RevB")
#define REV_C             ("RevC")
#define REV_D             ("RevD")

#define REVISION_0        (0x0)
#define REVISION_1        (0x1)
#define REVISION_2        (0x2)
#define REVISION_3        (0x3)

class vr_update_infineon_xdpe: public vr_update
{


public:
    vr_update_infineon_xdpe(std::string Processor,uint32_t Crc,
          std::string Model,uint16_t SlaveAddress,std::string ConfigFilePath,
          std::string Revision,uint16_t PmbusAddress);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware();
};

#endif
