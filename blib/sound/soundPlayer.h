#pragma once

#include <blib/sound/realTimeSoundPlayer.h>
#include <thread>
#include <atomic>

namespace blib
{
    class SoundPlayer 
    {
    public:
        SoundPlayer();
        const SoundDevices getDevices() const;
        void select(size_t index);
        void setFormat(const SoundFormat& fmt);
        bool open();
        void play();

        void setData(std::vector<SoundBuffer>&& data);

        void stop();
        bool close();

        ~SoundPlayer();

    //private:
        RealTimeSoundPlayer player;
        SoundBuffers buffers;
        std::thread* procThread;
        __blib_cache_aligned std::atomic<bool> playing;
    };
}
