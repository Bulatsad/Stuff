#pragma once

#include <stdint.h>
#include <vector>

namespace blib
{
    struct SoundFormat
    {
        typedef uint16_t channel_t;
        typedef uint16_t bitRate_t;
        typedef uint32_t sampleRate_t;

        channel_t channel;
        bitRate_t bitRate;
        sampleRate_t sampleRate;

        SoundFormat();
        SoundFormat(uint16_t a_channel, uint32_t a_sampleRate, uint16_t a_bitRate);

        bool isEmpty() const;
    };

    typedef std::vector<SoundFormat> SoundFormats;
}
