
#include <Windows.h>

#include <iostream>
#include<blib/core/algorithm/dtfExp.h>
#include<blib/core/algorithm/dft.h>

#include<blib/network/udpSocket.h>
#include<blib/network/tcpSocket.h>
#include<blib/network/tcpListener.h>
#include <blib/core/string.h>

#include <blib/graphics/renderWindow.h>
#include <blib/sound/soundRecorder.h>

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

//int main()
//{
//    std::vector<double>wave({ 
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//         1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
//        });
//
//    std::vector<short>simpleWave({ 4,3,0,-3,-4,-3,0,3 });
//    
//    std::vector<std::complex<double> >sw;
//    for (auto& s : wave)
//        sw.push_back(std::complex<double>(s - 8, 0));
//
//    sw.resize(16);//512
//    wave.resize(16); //512
//    std::vector<std::complex<double> >sw1(sw);
//
//    //auto c = blib::expFastFourierTransform(sw1);
//    //auto wave1 = blib::expInverseFourierTransform(c);
//    
//    //auto c = blib::fourierTransform<short>(simpleWave.data(), simpleWave.size());
//    //auto wave1 = blib::inverseFourierTransform<short>(&c);
//    
//    clock_t ftstart = clock();
//    auto spectre = blib::expFastFourierTransform(sw1);
//
//    for (size_t i = 0/*spectre.size() / 2*/; i < spectre.size(); ++i)
//        spectre[i].imag(0);
//
//    clock_t ftend = clock();
//    clock_t fttime = ftend - ftstart;
//    auto wave1 = blib::expFastInverseFourierTransform(spectre);
//    clock_t iftt = clock() - ftend;
//
//    //clock_t ftstart = clock();
//    //auto spectre = blib::fourierTransform<double, double>(wave.data(), wave.size());
//    //clock_t ftend = clock();
//    //clock_t fttime = ftend - ftstart;
//    //auto wave1 = blib::inverseFourierTransform<double, double>(&spectre);
//    //clock_t iftt = clock() - ftend;
//
//    return 0;
//}

//int main()
//{
//    blib::network::InitBlibSocket();
//
//    blib::network::UdpSocket udps(blib::network::AddressType::IPv4);
//
//    char mode;
//    std::cin >> mode;
//
//    char data[5] = { 1,2,3,4,5 };
//
//    if (mode == 'c')
//    {
//        bool ok = false;
//        auto addr = blib::network::Address::fromIPv4("192.168.1.51", &ok);
//        addr.setPort(8080);
//        udps.send(addr, data, 5);
//    }
//
//    if (mode == 's')
//    {
//        char rd[5] = {0,0,0,0,0};
//        int rs = 5;
//        bool ok = false;
//        auto addr = blib::network::Address::fromIPv4("0.0.0.0", &ok);
//        addr.setPort(8080);
//        udps.bind(addr);
//
//        blib::network::Address sender;
//
//        udps.recv(sender, rd, rs);
//
//        std::cout
//            << (int)rd[0] << " " 
//            << (int)rd[1] << " " 
//            << (int)rd[2] << " " 
//            << (int)rd[3] << " "
//            << (int)rd[4] << " ";
//    }
//
//
//    return 0;
//}

//int main()
//{
//    blib::network::InitBlibSocket();
//
//    char mode;
//    std::cin >> mode;
//
//    char data[5] = { 1,2,3,4,5 };
//
//    if (mode == 'c')
//    {
//        blib::network::TcpSocket connector(blib::network::AddressType::IPv4);
//        //connector.setBlocking(false);
//        bool ok = false;
//        auto addr = blib::network::Address::fromIPv4("192.168.1.51", &ok); //
//        addr.setPort(8080);
//        while (connector.connect(addr) != blib::network::SocketStatus::OK);
//        while (connector.send(data, 5) != blib::network::SocketStatus::OK);
//
//        char rd[5] = { 0,0,0,0,0 };
//        int rs = 5;
//
//        connector.setBlocking(false);
//        while (connector.recv(rd, rs) != blib::network::SocketStatus::OK);
//
//        std::cout
//            << (int)rd[0] << " "
//            << (int)rd[1] << " "
//            << (int)rd[2] << " "
//            << (int)rd[3] << " "
//            << (int)rd[4] << " ";
//    }
//
//    if (mode == 's')
//    {
//        blib::network::TcpListener listner(blib::network::AddressType::IPv4);
//        blib::network::TcpSocket acc;
//        //listner.setBlocking(false);
//
//        char rd[5] = {0,0,0,0,0};
//        int rs = 5;
//        bool ok = false;
//        auto addr = blib::network::Address::fromIPv4("0.0.0.0", &ok);
//        addr.setPort(8080);
//        listner.bind(addr);
//        while (listner.listen()    != blib::network::SocketStatus::OK);
//        while (listner.accept(acc) != blib::network::SocketStatus::OK);
//        
//        acc.setBlocking(false);
//        while (acc.recv(rd, rs) != blib::network::SocketStatus::OK);
//
//        std::cout
//            << (int)rd[0] << " " 
//            << (int)rd[1] << " " 
//            << (int)rd[2] << " " 
//            << (int)rd[3] << " "
//            << (int)rd[4] << " ";
//
//        while (acc.send(data, 5) != blib::network::SocketStatus::OK);
//
//    }
//
//
//    return 0;
//}

//int main()
//{
//    blib::RealTimeSoundRecorder recorder;
//    recorder.setFormat(blib::SoundFormat(1, 44100, 16));
//    recorder.setBufferInfo(10, 50);
//
//    blib::RealTimeSoundPlayer player;
//    player.setFormat(blib::SoundFormat(1, 44100, 16));
//    player.setBufferInfo(10, 50);
//
//    recorder.open();
//    player.open();
//
//    recorder.start();
//
//    while (1)
//    {
//        auto recordedFrame = recorder.acquireBuffer();
//
//        auto playingFrame = player.acquireBuffer();
//        memcpy(playingFrame.sndFrame->getData(), recordedFrame.sndFrame->getData(), recordedFrame.sndFrame->getSize());
//        
//        recorder.releaseBuffer(std::move(recordedFrame));
//
//        player.releaseBuffer(std::move(playingFrame));
//    }
//
//    recorder.stop();
//
//    return 0;
//}

//int main() //vo chat
//{
//    blib::network::InitBlibSocket();
//    std::string connectionIp;
//    std::string connectinPort;
//    std::string mode;
//
//    std::cout << "mode s/c" << std::endl;
//    std::cin >> mode;
//
//    std::cout << "connection ip" << std::endl;
//    std::cin >> connectionIp;
//    std::cout << "connection port" << std::endl;
//    std::cin >> connectinPort;
//
//    blib::network::TcpSocket socket(blib::network::AddressType::IPv4);
//
//    if (mode == "c")
//    {
//        bool ok = false;
//        auto addr = blib::network::Address::fromIPv4(connectionIp.c_str(), &ok); //
//        addr.setPort(atoi(connectinPort.c_str()));
//        while (socket.connect(addr) != blib::network::SocketStatus::OK);
//        std::cout << "connected" << std::endl;
//    }
//
//    if (mode == "s")
//    {
//        blib::network::TcpListener listner(blib::network::AddressType::IPv4);
//
//        bool ok = false;
//        auto addr = blib::network::Address::fromIPv4(connectionIp.c_str(), &ok);
//        addr.setPort(atoi(connectinPort.c_str()));
//        listner.bind(addr);
//        while (listner.listen() != blib::network::SocketStatus::OK);
//        while (listner.accept(socket) != blib::network::SocketStatus::OK);
//        std::cout << "accepted" << std::endl;
//    }
//
//    socket.setBlocking(false);
//
//    blib::RealTimeSoundRecorder recorder;
//    recorder.setFormat(blib::SoundFormat(1, 44100, 16));
//    recorder.setBufferInfo(10, 50);
//
//    blib::RealTimeSoundPlayer player;
//    player.setFormat(blib::SoundFormat(1, 44100, 16));
//    player.setBufferInfo(10, 50);
//
//    recorder.open();
//    player.open();
//
//    recorder.start();
//
//    while (1)
//    {
//        auto recordedFrame = recorder.acquireBuffer();
//        socket.send(recordedFrame.sndFrame->getData(), recordedFrame.sndFrame->getSize());
//        recorder.releaseBuffer(std::move(recordedFrame));
//
//
//        auto playingFrame = player.acquireBuffer();
//        memset(playingFrame.sndFrame->getData(), playingFrame.sndFrame->getSize(), 0);
//        
//        int recvsize = playingFrame.sndFrame->getSize();
//        socket.recv(playingFrame.sndFrame->getData(), recvsize);
//        player.releaseBuffer(std::move(playingFrame));
//    }
//
//    recorder.stop();
//
//    return 0;
//}//192.168.1.51
