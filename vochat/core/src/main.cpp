#include <SoundRecorder.h>
#include <soundPlayer.h>

#include <Windows.h>

#include <iostream>
#include<blib/algorithm/dft.h>

int main()
{
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

        Sleep(1000);

        recorder.stop();

        blib::SoundPlayer player;
        player.setFormat(blib::SoundFormat(1, 44100, 16));
        isopened = player.open();

        clock_t start = clock();

        auto spectre = blib::fourierTransform<short>(recorder.buffers[0].getData(), 
            recorder.buffers[0].getSize());

        clock_t end = clock() - start;

        std::cout << "ft : " << end << std::endl;

        auto data = blib::inverseFourierTransform<short>(&spectre);

        clock_t iftend = clock() - end;

        std::cout << "ift : " << end << std::endl;

        blib::SoundBuffer iftbuffer(blib::SoundFormat(1, 44100, 16),data.size() / 2,data.size() / 2,data.data());

        blib::SoundBuffers sbs;
        sbs.emplace_back(std::move(iftbuffer));

        player.setData(std::move(sbs));

        if (isopened)
        {
            std::cout << "opened!" << std::endl;
            player.play();

            Sleep(5000);
        }

        player.close();
        recorder.close();

    }
    return 0;

}

