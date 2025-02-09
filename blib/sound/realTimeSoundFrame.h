#pragma once

#include<blib/sound/soundBuffer.h>

namespace blib
{
    struct RealTimeSoundFrame
    {
        RealTimeSoundFrame();
        RealTimeSoundFrame(void* _platrofmCtx,blib::SoundBuffer* _sndFrame);
        bool isValid() const;

        void* platrofmCtx;
        blib::SoundBuffer* sndFrame;
    };
}
