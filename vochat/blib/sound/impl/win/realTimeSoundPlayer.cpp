#undef WIN32_LEAN_AND_MEAN

#include <blib/sound/realTimeSoundPlayer.h>
#include <blib/sound/realTimeSoundFrame.h>

#include <blib/sound/impl/win/winSoundUtil.h>

#include <blib/thread/circleQueue.h>

struct OutWinCtx
{
    blib::SoundDevices sndDevices;
    size_t selectedDevice;
    HWAVEOUT hOut;
    blib::SoundFormat format;
    WAVEFORMATEX wfex;
    bool isOpened;

    __blib_cache_aligned std::vector<blib::SoundBuffer>buffers;
    __blib_cache_aligned std::vector<WAVEHDR>platformBuffersHeaders;

    __blib_cache_aligned blib::LocklessProducerConcumerCircleQueue<blib::RealTimeSoundFrame>queueToUser;
    __blib_cache_aligned blib::LocklessProducerConcumerCircleQueue<blib::RealTimeSoundFrame>queueToDriver;
};

#define __blib_this_context(__this) reinterpret_cast<OutWinCtx*>(__this->ctx)

static void CALLBACK defaultWaveOutProc(
    HWAVEIN   hwi,
    UINT      uMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2
)
{
    switch (uMsg)
    {
    case WOM_OPEN:
        break;
    case WOM_CLOSE:
        break;
    case WOM_DONE:
    {
        OutWinCtx* ctx = reinterpret_cast<OutWinCtx*>(reinterpret_cast<blib::RealTimeSoundPlayer*>(dwInstance)->__getCtx());
        WAVEHDR* wHdr = reinterpret_cast<WAVEHDR*>(dwParam1);
        blib::RealTimeSoundFrame currentFrame;

        while (!ctx->queueToDriver.pop(currentFrame));
        while (!ctx->queueToUser.push(currentFrame));
    }
    break;

    default:
        break;
    }
}

blib::RealTimeSoundPlayer::RealTimeSoundPlayer()
{
    this->ctx = new OutWinCtx;

    __blib_this_context(this)->selectedDevice = MAXSIZE_T;
    __blib_this_context(this)->isOpened = false;

    UINT deviceCount = waveOutGetNumDevs();

    for (UINT i = 0; i < deviceCount; ++i)
    {
        WAVEOUTCAPSW currentDeviceInfo;
        memset(&currentDeviceInfo, 0, sizeof(WAVEOUTCAPSW));

        waveOutGetDevCaps(i, reinterpret_cast<LPWAVEOUTCAPSA>(&currentDeviceInfo), sizeof(WAVEOUTCAPSW));

        std::wstring wname = std::wstring(currentDeviceInfo.szPname);
        std::string name(wname.begin(), wname.end());

        __blib_this_context(this)->sndDevices.emplace_back(
            SoundDevice(
                std::move(name),
                winapiFormatsToBLibFormats(currentDeviceInfo.dwFormats)
            )
        );
    }
}

blib::RealTimeSoundPlayer::~RealTimeSoundPlayer()
{
    delete this->ctx;
}

const blib::SoundDevices blib::RealTimeSoundPlayer::getDevices() const
{
    return __blib_this_context(this)->sndDevices;
}

void blib::RealTimeSoundPlayer::select(size_t index)
{
    __blib_this_context(this)->selectedDevice = index;
}

void blib::RealTimeSoundPlayer::setFormat(const SoundFormat& fmt)
{
    __blib_this_context(this)->format = fmt;
}

void blib::RealTimeSoundPlayer::setBufferInfo(uint32_t countOfBuffers, uint32_t timeOfBuffers)
{
    __blib_this_context(this)->buffers.reserve(countOfBuffers);

    __blib_this_context(this)->queueToUser.reset(countOfBuffers, nullptr, nullptr);
    __blib_this_context(this)->queueToDriver.reset(countOfBuffers, nullptr, nullptr);

    for (size_t i = 0; i < countOfBuffers; i++)
    {
        __blib_this_context(this)->buffers.emplace_back(blib::SoundBuffer(timeOfBuffers, __blib_this_context(this)->format));

        WAVEHDR wHdr;
        memset(&wHdr, 0, sizeof(WAVEHDR));

        wHdr.dwBufferLength = static_cast<DWORD>(__blib_this_context(this)->buffers[__blib_this_context(this)->buffers.size() - 1].getSize());
        wHdr.lpData = reinterpret_cast<LPSTR>(__blib_this_context(this)->buffers[__blib_this_context(this)->buffers.size() - 1].getData());

        __blib_this_context(this)->platformBuffersHeaders.push_back(wHdr);
    }
}

bool blib::RealTimeSoundPlayer::open()
{
    if (__blib_this_context(this)->format.isEmpty())
        return false;

    memset(&(__blib_this_context(this)->wfex), 0, sizeof(WAVEFORMATEX));
    __blib_this_context(this)->wfex.wFormatTag = WAVE_FORMAT_PCM; // WAVE_FORMAT_PCM;
    __blib_this_context(this)->wfex.nChannels = __blib_this_context(this)->format.channel;
    __blib_this_context(this)->wfex.nSamplesPerSec = __blib_this_context(this)->format.sampleRate;
    __blib_this_context(this)->wfex.wBitsPerSample = __blib_this_context(this)->format.bitRate;
    __blib_this_context(this)->wfex.nBlockAlign = __blib_this_context(this)->wfex.nChannels * (__blib_this_context(this)->wfex.wBitsPerSample / 8);
    __blib_this_context(this)->wfex.nAvgBytesPerSec = __blib_this_context(this)->wfex.nSamplesPerSec * __blib_this_context(this)->wfex.nBlockAlign;
    __blib_this_context(this)->wfex.cbSize = 0;

    UINT deviceID = __blib_this_context(this)->selectedDevice == MAXSIZE_T ? WAVE_MAPPER : static_cast<UINT>(__blib_this_context(this)->selectedDevice);

    MMRESULT result = waveOutOpen(
        reinterpret_cast<HWAVEOUT*>(&(__blib_this_context(this)->hOut)),
        deviceID,
        &(__blib_this_context(this)->wfex),
        reinterpret_cast<DWORD_PTR>(&defaultWaveOutProc),
        reinterpret_cast<DWORD_PTR>(this),
        CALLBACK_FUNCTION
    );

    if (result != MMSYSERR_NOERROR)
    {
        return false;
    }

    for (size_t i = 0; i < __blib_this_context(this)->platformBuffersHeaders.size(); ++i)
    {
        MMRESULT isPrepared = waveOutPrepareHeader(
            __blib_this_context(this)->hOut,
            &(__blib_this_context(this)->platformBuffersHeaders[i]),
            sizeof(WAVEHDR)
        );

        if (isPrepared != MMSYSERR_NOERROR)
            return false;
    
        while (
            !__blib_this_context(this)->queueToUser.push(
                RealTimeSoundFrame(&(__blib_this_context(this)->platformBuffersHeaders[i]),
                    &(__blib_this_context(this)->buffers[i])))
            );
    }

    __blib_this_context(this)->isOpened = true;
    return true;
}

const blib::RealTimeSoundFrame blib::RealTimeSoundPlayer::acquireBuffer()
{
    RealTimeSoundFrame res;
    while (!__blib_this_context(this)->queueToUser.pop(res));
    return res;
}

void blib::RealTimeSoundPlayer::releaseBuffer(RealTimeSoundFrame&& sndFrame)
{
    waveOutWrite(
        __blib_this_context(this)->hOut,
        reinterpret_cast<LPWAVEHDR>(sndFrame.platrofmCtx),
        sizeof(WAVEHDR)
    );
    while (!__blib_this_context(this)->queueToDriver.push(sndFrame));
}

bool blib::RealTimeSoundPlayer::close()
{
    MMRESULT isClosed = waveOutClose(__blib_this_context(this)->hOut);

    for (size_t i = 0; i < __blib_this_context(this)->platformBuffersHeaders.size(); ++i)
    {
        waveOutUnprepareHeader(
            __blib_this_context(this)->hOut,
            &(__blib_this_context(this)->platformBuffersHeaders[i]),
            sizeof(WAVEHDR)
        );
    }

    if (isClosed != MMSYSERR_NOERROR)
    {
        __blib_this_context(this)->isOpened = false;
        return false;
    }

    return true;
}

void* blib::RealTimeSoundPlayer::__getCtx()
{
    return this->ctx;
}
