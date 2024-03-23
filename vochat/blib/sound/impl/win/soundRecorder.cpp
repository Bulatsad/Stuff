#undef WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmeapi.h>
#include <mmreg.h>

#pragma comment(lib,"Winmm")

#include <blib/sound/soundFormat.h>
#include <blib/sound/soundRecorder.h>

struct WinCtx
{
    blib::SoundFormat format;
    blib::SoundDevices sndDevices;
    uint32_t selectedDevice;
    bool isOpened;
    HWAVEIN hIn;
    blib::SoundRecorder::soundInHandlerProc callback;
    blib::CircleQueue<std::pair<WAVEHDR, blib::SoundBuffer> >*sndQueue;
};

#define __blib_this_context(__this) reinterpret_cast<WinCtx*>(__this->ctx)

static blib::SoundFormats winapiFormatsToBLibFormats(DWORD dwFormats)
{
    blib::SoundFormats res = blib::SoundFormats();
    if(dwFormats & WAVE_FORMAT_1M08)
        res.push_back(blib::SoundFormat(1, 11025, 8));
    if(dwFormats & WAVE_FORMAT_1S08)
        res.push_back(blib::SoundFormat(2, 11025, 8));
    if(dwFormats & WAVE_FORMAT_1M16)
        res.push_back(blib::SoundFormat(1, 11025, 16));
    if(dwFormats & WAVE_FORMAT_1S16)
        res.push_back(blib::SoundFormat(2, 11025, 16));

    if(dwFormats & WAVE_FORMAT_2M08)
        res.push_back(blib::SoundFormat(1, 22050, 8));
    if(dwFormats & WAVE_FORMAT_2S08)
        res.push_back(blib::SoundFormat(2, 22050, 8));
    if(dwFormats & WAVE_FORMAT_2M16)
        res.push_back(blib::SoundFormat(1, 22050, 16));
    if(dwFormats & WAVE_FORMAT_2S16)
        res.push_back(blib::SoundFormat(2, 22050, 16));

    if(dwFormats & WAVE_FORMAT_4M08)
    //if(dwFormats & WAVE_FORMAT_44M08)
        res.push_back(blib::SoundFormat(1, 44100, 8));
    if(dwFormats & WAVE_FORMAT_4S08)
    //if(dwFormats & WAVE_FORMAT_44S08)
        res.push_back(blib::SoundFormat(2, 44100, 8));
    if(dwFormats & WAVE_FORMAT_4M16)
    //if(dwFormats & WAVE_FORMAT_44M16)
        res.push_back(blib::SoundFormat(1, 44100, 16));
    if(dwFormats & WAVE_FORMAT_4S16)
    //if(dwFormats & WAVE_FORMAT_44S16)
        res.push_back(blib::SoundFormat(2, 44100, 16));

    if(dwFormats & WAVE_FORMAT_48M08)
        res.push_back(blib::SoundFormat(1, 48000, 8));
    if(dwFormats & WAVE_FORMAT_48S08)
        res.push_back(blib::SoundFormat(2, 48000, 8));
    if(dwFormats & WAVE_FORMAT_48M16)
        res.push_back(blib::SoundFormat(1, 48000, 16));
    if(dwFormats & WAVE_FORMAT_48S16)
        res.push_back(blib::SoundFormat(2, 48000, 16));

    if(dwFormats & WAVE_FORMAT_96M08)
        res.push_back(blib::SoundFormat(1, 96000, 8));
    if(dwFormats & WAVE_FORMAT_96S08)
        res.push_back(blib::SoundFormat(2, 96000, 8));
    if(dwFormats & WAVE_FORMAT_96M16)
        res.push_back(blib::SoundFormat(1, 96000, 16));
    if(dwFormats & WAVE_FORMAT_96S16)
        res.push_back(blib::SoundFormat(2, 96000, 16));

    return res;
}

static void CALLBACK defaultWaveInProc(
    HWAVEIN   hwi,
    UINT      uMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2
)
{

}

blib::SoundRecorder::SoundRecorder()
{
    this->ctx = new WinCtx;
    
    __blib_this_context(this)->selectedDevice = UINT32_MAX;
    __blib_this_context(this)->isOpened = false;
    __blib_this_context(this)->hIn = 0;
    __blib_this_context(this)->sndQueue = nullptr;

    UINT deviceCount = waveInGetNumDevs();
    for (decltype(deviceCount) i = 0; i < deviceCount; ++i)
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
void blib::SoundRecorder::start()
{

}
void blib::SoundRecorder::stop()
{

}

const blib::SoundDevices* blib::SoundRecorder::getDevices() const
{
    return &(__blib_this_context(this)->sndDevices);
}

void blib::SoundRecorder::select(size_t index)
{
    __blib_this_context(this)->selectedDevice = static_cast<uint32_t>(index);
}

void blib::SoundRecorder::setFormat(const SoundFormat& fmt)
{
    __blib_this_context(this)->format = fmt;
}

bool blib::SoundRecorder::open()
{
    if (__blib_this_context(this)->format.isEmpty())
        return false;

    WAVEFORMATEX waveForamtEx;
    memset(&waveForamtEx, 0, sizeof(WAVEFORMATEX));
    waveForamtEx.wFormatTag = WAVE_FORMAT_PCM; // WAVE_FORMAT_PCM;
    waveForamtEx.nChannels = __blib_this_context(this)->format.channel;
    waveForamtEx.nSamplesPerSec = __blib_this_context(this)->format.sampleRate;
    waveForamtEx.wBitsPerSample = __blib_this_context(this)->format.bitRate;
    waveForamtEx.nBlockAlign = waveForamtEx.nChannels * (waveForamtEx.wBitsPerSample / 8);
    waveForamtEx.nAvgBytesPerSec = waveForamtEx.nSamplesPerSec * waveForamtEx.nBlockAlign;
    waveForamtEx.cbSize = 0;

    MMRESULT result = waveInOpen(
        reinterpret_cast<HWAVEIN*>(&(__blib_this_context(this)->hIn)),
        __blib_this_context(this)->selectedDevice,
        &waveForamtEx,
        reinterpret_cast<DWORD_PTR>(&defaultWaveInProc),
        reinterpret_cast<DWORD_PTR>(this),
        CALLBACK_FUNCTION
    );

    if (result != MMSYSERR_NOERROR)
    {
        return false;
    }

    __blib_this_context(this)->isOpened = true;
    return true;
}

bool blib::SoundRecorder::close()
{
    return false;
}

void blib::SoundRecorder::setBufferInfo(uint32_t countOfBuffers, uint32_t timeOfBuffers)
{
    __blib_this_context(this)->sndQueue =
        new blib::CircleQueue<std::pair<WAVEHDR, blib::SoundBuffer> >(
            countOfBuffers,
            std::make_pair(WAVEHDR(), SoundBuffer(timeOfBuffers, __blib_this_context(this)->format))
            );
}

blib::SoundRecorder::~SoundRecorder()
{
    if (__blib_this_context(this)->isOpened)
        this->stop();

    this->close();

    __blib_this_context(this)->selectedDevice = -1;
    __blib_this_context(this)->isOpened = false;
    
    delete __blib_this_context(this)->sndQueue;
    delete this->ctx;
}
