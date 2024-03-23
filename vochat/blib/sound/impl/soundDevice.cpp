#include <blib/sound/soundDevice.h>

blib::SoundDevice::SoundDevice(const std::string& a_name, SoundFormats a_supportedFormats)
{
    this->name = a_name;
    this->supportedFormats = a_supportedFormats;
}
