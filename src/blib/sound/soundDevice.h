#pragma once

#include <blib/sound/soundFormat.h>

#include <string>
#include <vector>
#include <stdint.h>

namespace blib
{
    class SoundDevice
    {
    public:
        SoundDevice(const std::string& a_name, SoundFormats a_supportedFormats);
        SoundDevice() = delete;

        std::string name;
        SoundFormats supportedFormats;
    };

    typedef std::vector<SoundDevice> SoundDevices;
}
