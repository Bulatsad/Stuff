#pragma once

#include <Windows.h>
#include <mmeapi.h>
#include <mmreg.h>

#include <blib/sound/soundFormat.h>

blib::SoundFormats winapiFormatsToBLibFormats(DWORD dwFormats);
