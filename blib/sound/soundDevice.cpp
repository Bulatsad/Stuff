#include "soundDevice.h"

blib::SoundDevice::SoundDevice(const std::string& a_name, uint16_t a_channels, SoundFormat a_supportedFormats)
{
    this->name = a_name;
    this->channels = a_channels;
    this->supportedFormats = a_supportedFormats;
}
