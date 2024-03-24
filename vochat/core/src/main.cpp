#include <SoundRecorder.h>
#include <soundPlayer.h>

#include <Windows.h>

#include <iostream>
#include<blib/algorithm/dtfExp.h>
#include<blib/algorithm/dft.h>

//int main()
//{
//    {
//        std::cout << sizeof(blib::LocklessProducerConcumerCircleQueue<int>) << std::endl;
//        blib::SoundRecorder recorder;
//        recorder.setFormat(blib::SoundFormat(1, 44100, 16));
//        bool isopened = recorder.open();
//
//        if (isopened)
//        {
//            std::cout << "opened!" << std::endl;
//        }
//
//        recorder.start();
//
//        Sleep(1000);
//
//        recorder.stop();
//
//        blib::SoundPlayer player;
//        player.setFormat(blib::SoundFormat(1, 44100, 16));
//        isopened = player.open();
//
//        clock_t start = clock();
//
//        auto spectre = blib::fourierTransform<short>(recorder.buffers[0].getData(), 
//            recorder.buffers[0].getSize());
//
//        clock_t end = clock() - start;
//
//        std::cout << "ft : " << end << std::endl;
//
//        auto data = blib::inverseFourierTransform<short>(&spectre);
//
//        clock_t iftend = clock() - end;
//
//        std::cout << "ift : " << end << std::endl;
//
//        blib::SoundBuffer iftbuffer(blib::SoundFormat(1, 44100, 16),data.size() / 2,data.size() / 2,data.data());
//
//        blib::SoundBuffers sbs;
//        sbs.emplace_back(std::move(iftbuffer));
//
//        player.setData(std::move(sbs));
//
//        if (isopened)
//        {
//            std::cout << "opened!" << std::endl;
//            player.play();
//
//            Sleep(5000);
//        }
//
//        player.close();
//        recorder.close();
//
//    }
//    return 0;
//
//}

typedef std::complex<double> base;

void fft(std::vector<base>& a, bool invert) {
    int n = (int)a.size();
    if (n == 1)  
        return;

    std::vector<base> a0(n / 2), a1(n / 2);
    for (int i = 0, j = 0; i < n; i += 2, ++j) {
        a0[j] = a[i];
        a1[j] = a[i + 1];
    }
    fft(a0, invert);
    fft(a1, invert);

    double ang = 2 * PI / n * (invert ? -1 : 1);
    base w(1), wn(cos(ang), sin(ang));
    for (int i = 0; i < n / 2; ++i) {
        a[i] = a0[i] + w * a1[i];
        a[i + n / 2] = a0[i] - w * a1[i];
        if (invert)
            a[i] /= 2, a[i + n / 2] /= 2;
        w *= wn;
    }
}

int main()
{
    std::vector<double>wave({ 
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
        });

    std::vector<short>simpleWave({ 4,3,0,-3,-4,-3,0,3 });
    
    std::vector<std::complex<double> >sw;
    for (auto& s : wave)
        sw.push_back(std::complex<double>(s, 0));

    sw.resize(512);
    wave.resize(512);
    std::vector<std::complex<double> >sw1(sw);


    clock_t fftstart = clock();
    fft(sw, false);
    clock_t fftend = clock();
    clock_t ffttime = fftend - fftstart;
    fft(sw, true);
    clock_t ifftt = clock() - fftend;

    //auto c = blib::expFastFourierTransform(sw1);
    //auto wave1 = blib::expInverseFourierTransform(c);
    
    //auto c = blib::fourierTransform<short>(simpleWave.data(), simpleWave.size());
    //auto wave1 = blib::inverseFourierTransform<short>(&c);
    
    clock_t ftstart = clock();
    auto spectre = blib::expFastFourierTransform(sw1);
    clock_t ftend = clock();
    clock_t fttime = ftend - ftstart;
    auto wave1 = blib::expFastInverseFourierTransform(spectre);
    clock_t iftt = clock() - ftend;

    //clock_t ftstart = clock();
    //auto spectre = blib::fourierTransform<double, double>(wave.data(), wave.size());
    //clock_t ftend = clock();
    //clock_t fttime = ftend - ftstart;
    //auto wave1 = blib::inverseFourierTransform<double, double>(&spectre);
    //clock_t iftt = clock() - ftend;



    return 0;
}
