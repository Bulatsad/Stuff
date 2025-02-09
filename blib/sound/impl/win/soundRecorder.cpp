#include <blib/sound/soundRecorder.h>

void soundInProc(blib::SoundRecorder* sr)
{
    while (sr->capturing)
    {
        blib::RealTimeSoundFrame frame = sr->recorder.acquireBuffer();
        sr->buffers.push_back(*(frame.sndFrame));
        sr->recorder.releaseBuffer(std::move(frame));
    }
    while (sr->recorder.isBufferReady())
    {
        blib::RealTimeSoundFrame frame = sr->recorder.acquireBuffer();
        sr->buffers.push_back(*(frame.sndFrame));
        sr->recorder.releaseBuffer(std::move(frame));
    }
}

blib::SoundRecorder::SoundRecorder()
{
    this->procThread = nullptr;
    this->capturing = false;
}

const blib::SoundDevices blib::SoundRecorder::getDevices() const
{
    return this->recorder.getDevices();
}

void blib::SoundRecorder::select(size_t index)
{
    this->recorder.select(index);
}

void blib::SoundRecorder::setFormat(const SoundFormat& fmt)
{
    this->recorder.setFormat(fmt);
}

void blib::SoundRecorder::setBufferInfo(uint32_t countOfBuffers, uint32_t timeOfBuffers)
{
    this->recorder.setBufferInfo(countOfBuffers, timeOfBuffers);
}

bool blib::SoundRecorder::open()
{
    return this->recorder.open();
}

void blib::SoundRecorder::start()
{
    this->capturing = true;
    this->recorder.start();
    this->procThread = new std::thread(soundInProc, this);
}

void blib::SoundRecorder::stop()
{
    this->recorder.stop();
    this->capturing = false;
    this->procThread->join();
}

bool blib::SoundRecorder::close()
{
    return this->recorder.close();
}

std::vector<blib::SoundBuffer> blib::SoundRecorder::getBuffer()
{
    return this->buffers;
}

blib::SoundRecorder::~SoundRecorder()
{
    delete this->procThread;
}