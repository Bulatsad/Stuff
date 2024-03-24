#include <blib/sound/soundPlayer.h>

static void soundOutProc(blib::SoundPlayer* sp)
{
    size_t i = 0;
    while (sp->playing.load(std::memory_order::memory_order_relaxed))
    {
        blib::RealTimeSoundFrame frame = sp->player.acquireBuffer();
        memcpy(frame.sndFrame->getData(), sp->buffers[i].getData(), sp->buffers[i].getSize());
        ++i;
        sp->player.releaseBuffer(std::move(frame));
        if (i == sp->buffers.size())
            break;
    }
}

blib::SoundPlayer::SoundPlayer()
{
}

const blib::SoundDevices blib::SoundPlayer::getDevices() const
{
    return this->player.getDevices();
}

void blib::SoundPlayer::select(size_t index)
{
    this->player.select(index);
}

void blib::SoundPlayer::setFormat(const SoundFormat& fmt)
{
    this->player.setFormat(fmt);
    this->player.setBufferInfo(10, 1000);
}

bool blib::SoundPlayer::open()
{
    return this->player.open();
}

void blib::SoundPlayer::play()
{
    this->playing.store(true, std::memory_order::memory_order_release);
    this->procThread = new std::thread(soundOutProc, this);
}

void blib::SoundPlayer::setData(std::vector<SoundBuffer>&& data)
{
    this->buffers = std::move(data);
}

void blib::SoundPlayer::stop()
{
    this->playing.store(false, std::memory_order::memory_order_release);
    this->procThread->join();
}

bool blib::SoundPlayer::close()
{
    return this->player.close();
}

blib::SoundPlayer::~SoundPlayer()
{
    delete this->procThread;
}
