#include <SoundRecorder.h>
#include <soundPlayer.h>

#include <Windows.h>

#include <iostream>


int main()
{
    std::cout << sizeof(blib::LocklessProducerConcumerCircleQueue<int>) << std::endl;
    blib::SoundRecorder recorder;
    recorder.setFormat(blib::SoundFormat(1, 44100, 16));
    bool isopened = recorder.open();

    if (isopened)
    {
        std::cout << "opened!" << std::endl;
    }

    recorder.start();
    
    Sleep(3000);
    
    recorder.stop();

    blib::SoundPlayer player;
    player.setFormat(blib::SoundFormat(1, 44100, 16));
    isopened = player.open();

    player.setData(std::move(recorder.buffers));

    if (isopened)
    {
        std::cout << "opened!" << std::endl;
        player.play();

        Sleep(5000);
    }

    player.close();
    recorder.close();

    return 0;
}
