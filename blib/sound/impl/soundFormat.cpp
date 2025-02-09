#include <blib/sound/soundFormat.h>

blib::SoundFormat::SoundFormat()
{
    this->channel = 0;
    this->sampleRate = 0;
    this->bitRate = 0;
}

blib::SoundFormat::SoundFormat(uint16_t a_channel, uint32_t a_sampleRate, uint16_t a_bitRate)
{
    this->channel = a_channel;
    this->sampleRate = a_sampleRate;
    this->bitRate = a_bitRate;
}

bool blib::SoundFormat::isEmpty() const
{
    return this->channel == 0 || this->sampleRate == 0 || this->bitRate == 0;
}