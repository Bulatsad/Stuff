#include <blib/sound/impl/win/winSoundUtil.h>

blib::SoundFormats winapiFormatsToBLibFormats(DWORD dwFormats)
{
    blib::SoundFormats res = blib::SoundFormats();
    if (dwFormats & WAVE_FORMAT_1M08)
        res.push_back(blib::SoundFormat(1, 11025, 8));
    if (dwFormats & WAVE_FORMAT_1S08)
        res.push_back(blib::SoundFormat(2, 11025, 8));
    if (dwFormats & WAVE_FORMAT_1M16)
        res.push_back(blib::SoundFormat(1, 11025, 16));
    if (dwFormats & WAVE_FORMAT_1S16)
        res.push_back(blib::SoundFormat(2, 11025, 16));

    if (dwFormats & WAVE_FORMAT_2M08)
        res.push_back(blib::SoundFormat(1, 22050, 8));
    if (dwFormats & WAVE_FORMAT_2S08)
        res.push_back(blib::SoundFormat(2, 22050, 8));
    if (dwFormats & WAVE_FORMAT_2M16)
        res.push_back(blib::SoundFormat(1, 22050, 16));
    if (dwFormats & WAVE_FORMAT_2S16)
        res.push_back(blib::SoundFormat(2, 22050, 16));

    if (dwFormats & WAVE_FORMAT_4M08)
        //if(dwFormats & WAVE_FORMAT_44M08)
        res.push_back(blib::SoundFormat(1, 44100, 8));
    if (dwFormats & WAVE_FORMAT_4S08)
        //if(dwFormats & WAVE_FORMAT_44S08)
        res.push_back(blib::SoundFormat(2, 44100, 8));
    if (dwFormats & WAVE_FORMAT_4M16)
        //if(dwFormats & WAVE_FORMAT_44M16)
        res.push_back(blib::SoundFormat(1, 44100, 16));
    if (dwFormats & WAVE_FORMAT_4S16)
        //if(dwFormats & WAVE_FORMAT_44S16)
        res.push_back(blib::SoundFormat(2, 44100, 16));

    if (dwFormats & WAVE_FORMAT_48M08)
        res.push_back(blib::SoundFormat(1, 48000, 8));
    if (dwFormats & WAVE_FORMAT_48S08)
        res.push_back(blib::SoundFormat(2, 48000, 8));
    if (dwFormats & WAVE_FORMAT_48M16)
        res.push_back(blib::SoundFormat(1, 48000, 16));
    if (dwFormats & WAVE_FORMAT_48S16)
        res.push_back(blib::SoundFormat(2, 48000, 16));

    if (dwFormats & WAVE_FORMAT_96M08)
        res.push_back(blib::SoundFormat(1, 96000, 8));
    if (dwFormats & WAVE_FORMAT_96S08)
        res.push_back(blib::SoundFormat(2, 96000, 8));
    if (dwFormats & WAVE_FORMAT_96M16)
        res.push_back(blib::SoundFormat(1, 96000, 16));
    if (dwFormats & WAVE_FORMAT_96S16)
        res.push_back(blib::SoundFormat(2, 96000, 16));

    return res;
}
