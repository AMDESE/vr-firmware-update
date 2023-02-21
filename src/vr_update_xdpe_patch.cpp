/*
* vr_update_infineon_xdpe.cpp
*
* Created on: Nov 10, 2022
* Author: Abinaya Dhandapani
*/

#include "vr_update.hpp"
#include "vr_update_xdpe_patch.hpp"

vr_update_xdpe_patch::vr_update_xdpe_patch(std::string Processor,
          uint32_t Crc,std::string Model,uint16_t SlaveAddress,std::string ConfigFilePath):
          vr_update(Processor,Crc,Model,SlaveAddress,ConfigFilePath)
{

    DriverPath = XDPE_DRIVER_PATH;
}

bool vr_update_xdpe_patch::crcCheckSum()
{
    return true;
}

bool vr_update_xdpe_patch::isUpdatable()
{

    return true;
}

bool vr_update_xdpe_patch::UpdateFirmware()
{
    return true;
}

bool vr_update_xdpe_patch::ValidateFirmware()
{
    return true;
}
