#ifndef VR_UPDATE_MPS_H_
#define VR_UPDATE_MPS_H_

class vr_update_mps: public vr_update
{

public:
    vr_update_mps(std::string Processor,
          uint32_t Crc,std::string Model,
          uint16_t SlaveAddress,std::string ConfigFilePath);

    virtual bool crcCheckSum();
    virtual bool isUpdatable();
    virtual bool UpdateFirmware();
    virtual bool ValidateFirmware(){return true;};
};


#endif
