#include <blib/sound/realTimeSoundFrame.h>

blib::RealTimeSoundFrame::RealTimeSoundFrame()
{
    this->platrofmCtx = nullptr;
    this->sndFrame = nullptr;
}

blib::RealTimeSoundFrame::RealTimeSoundFrame(void* _platrofmCtx, blib::SoundBuffer* _sndFrame)
{
    this->platrofmCtx = _platrofmCtx;
    this->sndFrame = _sndFrame;
}

bool blib::RealTimeSoundFrame::isValid() const
{
    return !!this->platrofmCtx && !!this->sndFrame;
}
