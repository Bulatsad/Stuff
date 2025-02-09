#pragma once

#include <stdlib.h>

#include <blib/sound/soundBuffer.h>

namespace blib
{
    class Sound
    {
    private:
        SoundBuffer* pBuffer;
    public:
        void play();
        void stop();
    };

}
