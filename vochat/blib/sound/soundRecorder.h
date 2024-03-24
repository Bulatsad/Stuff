#pragma once

#include <blib/sound/soundDevice.h>
#include <blib/sound/realTimeSoundRecorder.h>
#include <blib/core/linkedList.h>
#include <thread>

namespace blib
{
    class SoundRecorder
    {
    public:
        SoundRecorder();
        const SoundDevices getDevices() const;
        void select(size_t index);
        void setFormat(const SoundFormat& fmt);
        void setBufferInfo(uint32_t countOfBuffers, uint32_t timeOfBuffers);
        bool open();
        void start();

        void stop();
        bool close();

        std::vector<SoundBuffer> getBuffer();

        ~SoundRecorder();

    //private:
        RealTimeSoundRecorder recorder;
        SoundBuffers buffers;
        std::thread* procThread;
        __blib_cache_aligned bool capturing;
    };
}
