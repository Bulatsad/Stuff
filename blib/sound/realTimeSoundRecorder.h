#pragma once

#include <blib/sound/soundBuffer.h>
#include <blib/sound/soundDevice.h>
#include <blib/sound/realTimeSoundFrame.h>

#include <blib/thread/circleQueue.h>

namespace blib
{
    class RealTimeSoundRecorder
    {
    public:
        RealTimeSoundRecorder();
        const SoundDevices getDevices() const;
        void select(size_t index);
        void setFormat(const SoundFormat& fmt);
        void setBufferInfo(uint32_t countOfBuffers, uint32_t timeOfBuffers);
        bool open();
        void start();

        bool isBufferReady() const;
        const RealTimeSoundFrame acquireBuffer();
        void releaseBuffer(RealTimeSoundFrame&& sndFrame);
        
        void stop();
        bool close();

        ~RealTimeSoundRecorder();

        void* __getCtx();
    private:
        void* ctx;
    };
}
