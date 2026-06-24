#pragma once

#include <blib/sound/soundBuffer.h>
#include <blib/sound/soundDevice.h>
#include <blib/sound/realTimeSoundFrame.h>

namespace blib
{
    class RealTimeSoundPlayer
    {
    public:
        RealTimeSoundPlayer();
        ~RealTimeSoundPlayer();
        const SoundDevices getDevices() const;
        void select(size_t index);
        void setFormat(const SoundFormat& fmt);
        void setBufferInfo(uint32_t countOfBuffers, uint32_t timeOfBuffers);
        bool open();
        //void start();

        const RealTimeSoundFrame acquireBuffer();
        void releaseBuffer(RealTimeSoundFrame&& sndFrame);

        //void stop();
        bool close();

        void* __getCtx();
    private:
        void* ctx;
    };
}