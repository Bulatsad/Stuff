#undef WIN32_LEAN_AND_MEAN

#include <blib/sound/impl/win/winSoundUtil.h>

#include <blib/sound/realTimeSoundRecorder.h>

struct InWinCtx
{
    blib::SoundFormat format;
    blib::SoundDevices sndDevices;
    size_t selectedDevice;
    bool isOpened;
    HWAVEIN hIn;
    WAVEFORMATEX wfex;

    __blib_cache_aligned std::vector<blib::SoundBuffer>buffers;
    __blib_cache_aligned std::vector<WAVEHDR>platformBuffersHeaders;

    __blib_cache_aligned blib::LocklessProducerConcumerCircleQueue<blib::RealTimeSoundFrame>queueToUser;
    __blib_cache_aligned blib::LocklessProducerConcumerCircleQueue<blib::RealTimeSoundFrame>queueToDriver;

    InWinCtx()
    {
        memset(&(this->wfex), 0, sizeof(WAVEFORMATEX));
    }
};

#define __blib_this_context(__this) reinterpret_cast<InWinCtx*>(__this->ctx)

static void CALLBACK defaultWaveInProc(
    HWAVEIN   hwi,
    UINT      uMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2
)
{
    switch (uMsg)
    {
    case WIM_OPEN:
        break;
    case WIM_CLOSE:
        break;
    case WIM_DATA:
    {
        InWinCtx* ctx = reinterpret_cast<InWinCtx*>(reinterpret_cast<blib::RealTimeSoundRecorder*>(dwInstance)->__getCtx());
        WAVEHDR* wHdr = reinterpret_cast<WAVEHDR*>(dwParam1);
        blib::RealTimeSoundFrame currentFrame;
        
        while (!ctx->queueToDriver.pop(currentFrame));
        
        currentFrame.sndFrame->setSize(wHdr->dwBytesRecorded); // TODO : can be bug cause core coherence 
        
        while (!ctx->queueToUser.push(currentFrame));
    }
    break;

    default:
        break;
    }
}

blib::RealTimeSoundRecorder::RealTimeSoundRecorder()
{
    this->ctx = new InWinCtx;
    
    __blib_this_context(this)->selectedDevice = MAXSIZE_T;
    __blib_this_context(this)->isOpened = false;
    __blib_this_context(this)->hIn = 0;

    UINT deviceCount = waveInGetNumDevs();
    for (UINT i = 0; i < deviceCount; ++i)
    {
        WAVEINCAPSW currentDeviceInfo;
        memset(&currentDeviceInfo, 0, sizeof(WAVEINCAPSW));

        waveInGetDevCaps(i, reinterpret_cast<LPWAVEINCAPSA>(& currentDeviceInfo), sizeof(WAVEINCAPSW));

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
void blib::RealTimeSoundRecorder::start()
{
    waveInStart(__blib_this_context(this)->hIn);
}

bool blib::RealTimeSoundRecorder::isBufferReady() const
{
    return !__blib_this_context(this)->queueToUser.isEmpty();
}

const blib::RealTimeSoundFrame blib::RealTimeSoundRecorder::acquireBuffer()
{
    RealTimeSoundFrame res;
    while (!__blib_this_context(this)->queueToUser.pop(res));
    return res;
}

void blib::RealTimeSoundRecorder::releaseBuffer(RealTimeSoundFrame&& sndFrame)
{
    waveInAddBuffer(
        __blib_this_context(this)->hIn,
        reinterpret_cast<LPWAVEHDR>(sndFrame.platrofmCtx),
        sizeof(WAVEHDR)
    );
    while (!__blib_this_context(this)->queueToDriver.push(sndFrame));
}

void blib::RealTimeSoundRecorder::stop()
{
    waveInStop(__blib_this_context(this)->hIn);
}

const blib::SoundDevices blib::RealTimeSoundRecorder::getDevices() const
{
    return __blib_this_context(this)->sndDevices;
}

void blib::RealTimeSoundRecorder::select(size_t index)
{
    __blib_this_context(this)->selectedDevice = index;
}

void blib::RealTimeSoundRecorder::setFormat(const SoundFormat& fmt)
{
    __blib_this_context(this)->format = fmt;
}

bool blib::RealTimeSoundRecorder::open()
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

    MMRESULT result = waveInOpen(
        reinterpret_cast<HWAVEIN*>(&(__blib_this_context(this)->hIn)),
        deviceID,
        &(__blib_this_context(this)->wfex),
        reinterpret_cast<DWORD_PTR>(&defaultWaveInProc),
        reinterpret_cast<DWORD_PTR>(this),
        CALLBACK_FUNCTION
    );

    if (result != MMSYSERR_NOERROR)
    {
        return false;
    }

    for (size_t i = 0; i < __blib_this_context(this)->platformBuffersHeaders.size(); ++i)
    {
        MMRESULT isPrepared = waveInPrepareHeader(
            __blib_this_context(this)->hIn,
            &(__blib_this_context(this)->platformBuffersHeaders[i]),
            sizeof(WAVEHDR)
        );

        if (isPrepared != MMSYSERR_NOERROR)
            return false;

        MMRESULT isAddedtoDriver = waveInAddBuffer(
            __blib_this_context(this)->hIn,
            &(__blib_this_context(this)->platformBuffersHeaders[i]),
            sizeof(WAVEHDR)
        );

        while (
            !__blib_this_context(this)->queueToDriver.push(
                RealTimeSoundFrame(&(__blib_this_context(this)->platformBuffersHeaders[i]),
                    &(__blib_this_context(this)->buffers[i])))
            );

        if (isAddedtoDriver != MMSYSERR_NOERROR)
            return false;
    }

    __blib_this_context(this)->isOpened = true;
    return true;
}

bool blib::RealTimeSoundRecorder::close()
{
    MMRESULT isClosed = waveInClose(__blib_this_context(this)->hIn);

    for (size_t i = 0; i < __blib_this_context(this)->platformBuffersHeaders.size(); ++i)
    {
        waveInUnprepareHeader(
            __blib_this_context(this)->hIn,
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

void blib::RealTimeSoundRecorder::setBufferInfo(uint32_t countOfBuffers, uint32_t timeOfBuffers)
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

blib::RealTimeSoundRecorder::~RealTimeSoundRecorder()
{
    if (__blib_this_context(this)->isOpened)
    {
        this->stop();
        this->close();
    }

    
    delete this->ctx;
}

void* blib::RealTimeSoundRecorder::__getCtx()
{
    return this->ctx;
}

#undef __blib_this_context