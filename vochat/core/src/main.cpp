#include <soundRecorder.h>
#include <sound.h>

#include <Windows.h>

#include <iostream>

#include <blib/core/linkedList.h>

int main()
{
    blib::SoundRecorder recorder;
    recorder.setFormat(blib::SoundFormat(1, 44100, 16));
    bool isopened = recorder.open();

    if (isopened)
    {
        std::cout << "opened!" << std::endl;
    }

    //recorder.start();
    //
    //Sleep(2000);
    //
    //recorder.stop();

    return 0;
}
