#pragma once

#include <blib/sound/soundBuffer.h>
#include <blib/sound/soundDevice.h>

#include <blib/thread/circleQueue.h>

namespace blib
{
    class SoundRecorderEvent
    {
        enum class Type
        {
            opened,
            closed,
            bufferReady,
            END_OF_ENUM
        };

        Type type;
    };
    class SoundRecorder
    {
    public:
        typedef void (*soundInHandlerProc)(const SoundRecorderEvent&);

        SoundRecorder();
        void start();
        void stop();
        const SoundDevices* getDevices() const;
        void select(size_t index);
        void setFormat(const SoundFormat& fmt);
        bool open();
        bool close();
        void setBufferInfo(uint32_t countOfBuffers, uint32_t timeOfBuffers);
        ~SoundRecorder();

    private:
        void* ctx;
    };
}
