// WinSock2.h обязан быть ПЕРВЫМ системным инклюдом (до windows.h из
// прочих заголовков): иначе winsock1 конфликтует с winsock2
#include <WinSock2.h>

#include <blib/test/src/test.h>

#include <blib/network/address.h>
#include <blib/network/socket.h>
#include <blib/network/tcpListener.h>
#include <blib/network/tcpSocket.h>
#include <blib/network/impl/win/winNetworkUtil.h>

#include <cstring>

// Тесты сети — loopback в одном процессе (детерминированные: не
// требуют внешнего сервера). Все сокеты — неблокирующие: ожидание
// состояний — опрос с таймаутом (loopback надёжен).

namespace
{
    using namespace blib::network::address;

    // Тестовый порт (маловероятно занятый)
    constexpr int testPort = 44251;

    // Буферы и сообщения
    constexpr int messageBufferSize = 256;
    constexpr int bigMessageSize = 10240;

    // Таймауты ожидания (попыток × пауза)
    constexpr int maxPollAttempts = 1000;
    constexpr int pollSleepMs = 1;

    // Ожидание условия с таймаутом (опрашивает fn до maxAttempts раз)
    template<typename Fn>
    bool waitFor(Fn fn)
    {
        for (int attempt = 0; attempt < maxPollAttempts; ++attempt)
        {
            if (fn())
            {
                return true;
            }
            Sleep(static_cast<DWORD>(pollSleepMs));
        }
        return false;
    }

    // Неблокирующее подключение клиента к слушателю: connect в
    // неблокирующем режиме асинхронен — ждём OK (WSAEISCONN)
    bool connectNonBlocking(blib::network::TcpSocket& client, blib::network::address::Tcp& endpoint)
    {
        if (!client.setBlocking(false))
        {
            return false;
        }

        for (int attempt = 0; attempt < maxPollAttempts; ++attempt)
        {
            const blib::network::SocketStatus status = client.connect(endpoint);
            if (status == blib::network::SocketStatus::OK)
            {
                return true;
            }
            if (status == blib::network::SocketStatus::Error)
            {
                return false;
            }
            // WouldBlock — соединение устанавливается
            Sleep(static_cast<DWORD>(pollSleepMs));
        }
        return false;
    }

    // Принятие подключения неблокирующим слушателем (WouldBlock —
    // подключений нет)
    bool acceptNonBlocking(blib::network::TcpListener& listener, blib::network::TcpSocket& accepted)
    {
        for (int attempt = 0; attempt < maxPollAttempts; ++attempt)
        {
            const blib::network::SocketStatus status = listener.accept(accepted);
            if (status == blib::network::SocketStatus::OK)
            {
                return true;
            }
            if (status == blib::network::SocketStatus::Error)
            {
                return false;
            }
            Sleep(static_cast<DWORD>(pollSleepMs));
        }
        return false;
    }

    // Loopback TCP-эндпоинт тестового порта (+ смещение для разнесения
    // параллельных тестов по портам)
    blib::network::address::Tcp makeLoopbackTcp(int portOffset = 0)
    {
        blib::network::address::Tcp endpoint;
        endpoint.ip = blib::network::address::Address::LocalhostIPv4;
        endpoint.port = static_cast<buint16>(testPort + portOffset);
        return endpoint;
    }
}

BLIB_TEST_CASE("network: InitBlibSocket is idempotent and succeeds")
{
    BLIB_TEST_CHECK(blib::network::InitBlibSocket());
    BLIB_TEST_CHECK(blib::network::InitBlibSocket());
}

BLIB_TEST_CASE("network: tcp loopback send/recv roundtrip")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    // Сервер: слушатель на loopback (неблокирующий accept)
    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));

    blib::network::address::Tcp listenAddress = makeLoopbackTcp();
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    // Клиент: подключение (неблокирующее)
    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp();
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));

    // Сервер принимает
    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    // Roundtrip: клиент → сервер
    const char* clientMessage = "hello server";
    BLIB_TEST_CHECK(client.send(clientMessage, static_cast<int>(std::strlen(clientMessage))) == blib::network::SocketStatus::OK);

    char receiveBuffer[messageBufferSize] = {};
    int received = messageBufferSize;
    BLIB_TEST_CHECK(waitFor([&]() {
        received = messageBufferSize;
        return accepted.recv(receiveBuffer, received) == blib::network::SocketStatus::OK;
    }));
    BLIB_TEST_CHECK(received == static_cast<int>(std::strlen(clientMessage)));
    BLIB_TEST_CHECK(std::strcmp(receiveBuffer, clientMessage) == 0);

    // Roundtrip: сервер → клиент
    const char* serverMessage = "hello client";
    BLIB_TEST_CHECK(accepted.send(serverMessage, static_cast<int>(std::strlen(serverMessage))) == blib::network::SocketStatus::OK);

    received = messageBufferSize;
    BLIB_TEST_CHECK(waitFor([&]() {
        received = messageBufferSize;
        return client.recv(receiveBuffer, received) == blib::network::SocketStatus::OK;
    }));
    BLIB_TEST_CHECK(received == static_cast<int>(std::strlen(serverMessage)));
    BLIB_TEST_CHECK(std::strcmp(receiveBuffer, serverMessage) == 0);
}

BLIB_TEST_CASE("network: recv returns actual size (partial reads are not errors)")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::address::Tcp listenAddress = makeLoopbackTcp(1);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp(1);
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));

    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    // Два сообщения подряд — TCP-потоком (приёмщик видит их слитно)
    const char* first = "first";
    const char* second = "second";
    const int expectedSize = static_cast<int>(std::strlen(first) + std::strlen(second));
    BLIB_TEST_CHECK(client.send(first, static_cast<int>(std::strlen(first))) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(client.send(second, static_cast<int>(std::strlen(second))) == blib::network::SocketStatus::OK);

    // Читаем поток МАЛЫМИ кусками: частичный приём — не ошибка
    char smallBuffer[3] = {};
    int smallReceived = 3;
    BLIB_TEST_CHECK(waitFor([&]() {
        smallReceived = 3;
        return accepted.recv(smallBuffer, smallReceived) == blib::network::SocketStatus::OK;
    }));
    BLIB_TEST_CHECK(smallReceived == 3);
    BLIB_TEST_CHECK(std::strncmp(smallBuffer, "fir", 3) == 0);

    char restBuffer[messageBufferSize] = {};
    int restReceived = messageBufferSize;
    BLIB_TEST_CHECK(waitFor([&]() {
        restReceived = messageBufferSize;
        return accepted.recv(restBuffer, restReceived) == blib::network::SocketStatus::OK;
    }));
    const int restExpected = expectedSize - 3;
    BLIB_TEST_CHECK(restReceived == restExpected);
    BLIB_TEST_CHECK(std::strncmp(restBuffer, "stsecond", static_cast<size_t>(restExpected)) == 0);
}

BLIB_TEST_CASE("network: disconnect is reported to the peer")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::address::Tcp listenAddress = makeLoopbackTcp(2);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp(2);
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));

    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    // Клиент закрывает соединение — сервер видит Disconnected
    client.getSocket()->close();

    // Сервер видит Disconnected
    char buffer[messageBufferSize] = {};
    int received = messageBufferSize;
    const blib::network::SocketStatus status = [&]() {
        blib::network::SocketStatus s = blib::network::SocketStatus::WouldBlock;
        waitFor([&]() {
            received = messageBufferSize;
            s = accepted.recv(buffer, received);
            return s != blib::network::SocketStatus::WouldBlock;
        });
        return s;
    }();
    BLIB_TEST_CHECK(status == blib::network::SocketStatus::Disconnected);
}

BLIB_TEST_CASE("network: non-blocking accept/recv return WouldBlock when idle")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::address::Tcp listenAddress = makeLoopbackTcp(3);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    // Подключений нет — accept возвращает WouldBlock (а не Error)
    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(listener.accept(accepted) == blib::network::SocketStatus::WouldBlock);

    // Подключённый неблокирующий сокет без данных — recv WouldBlock
    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp(3);
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    char buffer[16] = {};
    int received = 16;
    BLIB_TEST_CHECK(accepted.recv(buffer, received) == blib::network::SocketStatus::WouldBlock);
}

BLIB_TEST_CASE("network: large message send/recv roundtrip")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::address::Tcp listenAddress = makeLoopbackTcp(4);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp(4);
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));

    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    // Большое сообщение: send крутит до полной отправки
    char bigSend[bigMessageSize];
    char bigRecv[bigMessageSize] = {};
    for (int i = 0; i < bigMessageSize; ++i)
    {
        bigSend[i] = static_cast<char>(i % 251);
    }

    BLIB_TEST_CHECK(client.send(bigSend, bigMessageSize) == blib::network::SocketStatus::OK);

    int totalReceived = 0;
    BLIB_TEST_CHECK(waitFor([&]() {
        int received = bigMessageSize - totalReceived;
        const blib::network::SocketStatus status = accepted.recv(bigRecv + totalReceived, received);
        if (status == blib::network::SocketStatus::OK)
        {
            totalReceived += received;
        }
        return totalReceived == bigMessageSize;
    }));

    BLIB_TEST_CHECK(std::memcmp(bigSend, bigRecv, bigMessageSize) == 0);
}

BLIB_TEST_CASE("network: setTcpNoDelay succeeds on connected loopback pair")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    // Nagle — причина латентности мелких real-time пакетов; опция
    // должна выставляться на обоих концах соединения без ошибки
    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::address::Tcp listenAddress = makeLoopbackTcp(5);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp(5);
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));

    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    BLIB_TEST_CHECK(client.setTcpNoDelay(true));
    BLIB_TEST_CHECK(accepted.setTcpNoDelay(true));

    // Roundtrip с включённым NODELAY — передача по-прежнему корректна
    const char* message = "nodelay";
    BLIB_TEST_CHECK(client.send(message, static_cast<int>(std::strlen(message))) == blib::network::SocketStatus::OK);
    char buffer[messageBufferSize] = {};
    int received = messageBufferSize;
    BLIB_TEST_CHECK(waitFor([&]() {
        received = messageBufferSize;
        return accepted.recv(buffer, received) == blib::network::SocketStatus::OK;
    }));
    BLIB_TEST_CHECK(received == static_cast<int>(std::strlen(message)));
    BLIB_TEST_CHECK(std::strcmp(buffer, message) == 0);
}

// =====================================================================
// Адресные типы: разбор/форматирование, вариант Address, подсети,
// эндпоинты Tcp/Udp
// =====================================================================

BLIB_TEST_CASE("network: Mac toString/fromString roundtrip")
{
    char buffer[maxAddressStringLength];

    // Нижний регистр, ':' — каноническая форма
    Mac mac;
    BLIB_TEST_CHECK(mac.fromString("aa:bb:cc:dd:ee:ff"));
    BLIB_TEST_CHECK(mac.getType() == AddressType::Mac);
    BLIB_TEST_CHECK(mac.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "aa:bb:cc:dd:ee:ff") == 0);
    BLIB_TEST_CHECK(mac.bytes[0] == 0xAA && mac.bytes[5] == 0xFF);

    // Верхний регистр и '-' — допустимы на входе, канонизируются
    BLIB_TEST_CHECK(mac.fromString("AA-BB-CC-DD-EE-FF"));
    BLIB_TEST_CHECK(mac.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "aa:bb:cc:dd:ee:ff") == 0);

    // Ошибки: смешанные разделители, 5 групп, 3 цифры в группе,
    // не-hex символ
    BLIB_TEST_CHECK(!mac.fromString("aa:bb-cc:dd:ee:ff"));
    BLIB_TEST_CHECK(!mac.fromString("aa:bb:cc:dd:ee"));
    BLIB_TEST_CHECK(!mac.fromString("aaa:bb:cc:dd:ee:ff"));
    BLIB_TEST_CHECK(!mac.fromString("gg:bb:cc:dd:ee:ff"));
    // После неудачи — объект сброшен к нулям
    Mac zero;
    BLIB_TEST_CHECK(mac == zero);

    // Слишком маленький буфер — отказ без записи
    char tinyBuffer[macStringLength] = {};
    BLIB_TEST_CHECK(!mac.toString(tinyBuffer, macStringLength));

    // Равенство
    Mac a;
    Mac b;
    BLIB_TEST_CHECK(a.fromString("00:11:22:33:44:55"));
    BLIB_TEST_CHECK(b.fromString("00:11:22:33:44:55"));
    BLIB_TEST_CHECK(a == b);
    b.bytes[5] = 0x56;
    BLIB_TEST_CHECK(a != b);
}

BLIB_TEST_CASE("network: IPv4 toString/fromString roundtrip")
{
    char buffer[maxAddressStringLength];

    IPv4 ipv4;
    BLIB_TEST_CHECK(ipv4.fromString("192.168.1.77"));
    BLIB_TEST_CHECK(ipv4.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(ipv4.bytes[0] == 192 && ipv4.bytes[3] == 77);
    BLIB_TEST_CHECK(ipv4.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "192.168.1.77") == 0);

    // Граничные значения
    BLIB_TEST_CHECK(ipv4.fromString("0.0.0.0"));
    BLIB_TEST_CHECK(ipv4.fromString("255.255.255.255"));

    // Ошибки: октет > 255, неполный, лишний, пустой октет, не-цифра
    BLIB_TEST_CHECK(!ipv4.fromString("256.1.1.1"));
    BLIB_TEST_CHECK(!ipv4.fromString("1.2.3"));
    BLIB_TEST_CHECK(!ipv4.fromString("1.2.3.4.5"));
    BLIB_TEST_CHECK(!ipv4.fromString("1..2.3"));
    BLIB_TEST_CHECK(!ipv4.fromString("1.2.3.x"));

    // Агрегатная инициализация байтов
    const IPv4 direct{ 10, 0, 0, 1 };
    BLIB_TEST_CHECK(direct.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "10.0.0.1") == 0);
}

BLIB_TEST_CASE("network: IPv6 toString/fromString roundtrip")
{
    char buffer[maxAddressStringLength];

    IPv6 ipv6;
    // Сжатая форма
    BLIB_TEST_CHECK(ipv6.fromString("2001:db8::1"));
    BLIB_TEST_CHECK(ipv6.getType() == AddressType::IPv6);
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "2001:db8::1") == 0);

    // Полная форма без сжатия
    BLIB_TEST_CHECK(ipv6.fromString("1:2:3:4:5:6:7:8"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "1:2:3:4:5:6:7:8") == 0);

    // Все нули → "::", loopback → "::1"
    BLIB_TEST_CHECK(ipv6.fromString("::"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::") == 0);
    BLIB_TEST_CHECK(ipv6.fromString("::1"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::1") == 0);

    // Верхний регистр канонизируется в нижний
    BLIB_TEST_CHECK(ipv6.fromString("FE80::1"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "fe80::1") == 0);

    // Сжимается САМЫЙ ДЛИННЫЙ прогон нулей; при равных — первый (RFC 5952)
    BLIB_TEST_CHECK(ipv6.fromString("1:0:0:0:2:0:0:3"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "1::2:0:0:3") == 0);

    // Ошибки: 9 групп, два "::", не-hex, >4 цифр в группе
    BLIB_TEST_CHECK(!ipv6.fromString("1:2:3:4:5:6:7:8:9"));
    BLIB_TEST_CHECK(!ipv6.fromString("1::2::3"));
    BLIB_TEST_CHECK(!ipv6.fromString("gggg::1"));
    BLIB_TEST_CHECK(!ipv6.fromString("1:2:3:4:5:6:7:8:"));
    BLIB_TEST_CHECK(!ipv6.fromString(":1:2:3:4:5:6:7"));

    // Embedded-IPv4 хвост (RFC 4291): "::ffff:192.168.1.1"
    // (IPv4-mapped — форму, которую winsock отдаёт IPv4-клиенту
    // на IPv6-сокете)
    BLIB_TEST_CHECK(ipv6.fromString("::ffff:192.168.1.1"));
    BLIB_TEST_CHECK(ipv6.bytes[10] == 0xFF && ipv6.bytes[11] == 0xFF);
    BLIB_TEST_CHECK(ipv6.bytes[12] == 192 && ipv6.bytes[13] == 168);
    BLIB_TEST_CHECK(ipv6.bytes[14] == 1 && ipv6.bytes[15] == 1);
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::ffff:192.168.1.1") == 0);

    // Та же форма в hex (::ffff:c0a8:101) — канонизируется в dotted
    BLIB_TEST_CHECK(ipv6.fromString("::ffff:c0a8:101"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::ffff:192.168.1.1") == 0);

    // Полная (несжатая) embedded-форма: 6 групп + хвост
    const IPv6 mappedFull = []() {
        IPv6 parsed;
        parsed.fromString("0:0:0:0:0:ffff:192.168.1.1");
        return parsed;
    }();
    BLIB_TEST_CHECK(ipv6.fromString("::ffff:192.168.1.1"));
    BLIB_TEST_CHECK(mappedFull == ipv6);
    BLIB_TEST_CHECK(mappedFull.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::ffff:192.168.1.1") == 0);

    // IPv4-compatible (deprecated, RFC 4291): парсится, но dotted-хвост
    // по RFC 5952 — только у mapped; печатается обычным hex
    BLIB_TEST_CHECK(ipv6.fromString("::192.168.1.1"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::c0a8:101") == 0);

    // Mapped с нулевым хвостом — печатается dotted (не "::")
    BLIB_TEST_CHECK(ipv6.fromString("::ffff:0.0.0.0"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::ffff:0.0.0.0") == 0);

    // Ошибки embedded-хвоста: >6 hex-групп, хвост не в конце,
    // неполный IPv4, октет > 255, лишний октет, голый IPv4
    BLIB_TEST_CHECK(!ipv6.fromString("1:2:3:4:5:6:7:192.168.1.1"));
    BLIB_TEST_CHECK(!ipv6.fromString("::ffff:1.2.3.4:5678"));
    BLIB_TEST_CHECK(!ipv6.fromString("::ffff:1.2.3"));
    BLIB_TEST_CHECK(!ipv6.fromString("::ffff:256.1.1.1"));
    BLIB_TEST_CHECK(!ipv6.fromString("::ffff:1.2.3.4.5"));
    BLIB_TEST_CHECK(!ipv6.fromString("192.168.1.1"));

    // Равенство
    IPv6 a;
    IPv6 b;
    BLIB_TEST_CHECK(a.fromString("2001:db8::1"));
    BLIB_TEST_CHECK(b.fromString("2001:0db8:0:0:0:0:0:0001"));
    BLIB_TEST_CHECK(a == b);
    b.bytes[15] = 2;
    BLIB_TEST_CHECK(a != b);
}

BLIB_TEST_CASE("network: Address variant dispatch and conversions")
{
    char buffer[maxAddressStringLength];

    // Диспетчеризация fromString
    const IPv4 expectedIpv4{ 1, 2, 3, 4 };
    bool ok = false;
    Address addr = Address::fromString("1.2.3.4", &ok);
    BLIB_TEST_CHECK(ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(addr.toIPv4() == expectedIpv4);

    addr = Address::fromString("aa:bb:cc:dd:ee:ff", &ok);
    BLIB_TEST_CHECK(ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::Mac);
    BLIB_TEST_CHECK(addr.toMac().bytes[0] == 0xAA);

    addr = Address::fromString("::1", &ok);
    BLIB_TEST_CHECK(ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::IPv6);
    BLIB_TEST_CHECK(addr.toIPv6().bytes[15] == 1);

    const IPv4 subnetNetwork{ 192, 168, 0, 0 };
    addr = Address::fromString("192.168.0.0/16", &ok);
    BLIB_TEST_CHECK(ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::IPv4Subnet);
    BLIB_TEST_CHECK(addr.toIPv4Subnet().prefix == 16);
    BLIB_TEST_CHECK(addr.toIPv4Subnet().network == subnetNetwork);

    addr = Address::fromString("2001:db8::/32", &ok);
    BLIB_TEST_CHECK(ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::IPv6Subnet);
    BLIB_TEST_CHECK(addr.toIPv6Subnet().prefix == 32);

    // Неразборная строка — UNDEFINED + ok = false
    addr = Address::fromString("not-an-address", &ok);
    BLIB_TEST_CHECK(!ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::UNDEFINED);
    BLIB_TEST_CHECK(!addr.toString(buffer, maxAddressStringLength));

    // Пустая строка и nullptr
    BLIB_TEST_CHECK(Address::fromString("").getType() == AddressType::UNDEFINED);
    BLIB_TEST_CHECK(Address::fromString(nullptr).getType() == AddressType::UNDEFINED);

    // toString варианта — формат активного члена
    BLIB_TEST_CHECK(Address::fromString("10.20.30.40").toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "10.20.30.40") == 0);

    // Равенство вариантов (теги различают типы)
    BLIB_TEST_CHECK(Address::fromString("1.2.3.4") == Address(expectedIpv4));
    BLIB_TEST_CHECK(Address::fromString("1.2.3.4") != Address::fromString("::1"));

    // Копирование value-семантики
    Address copy = Address::fromString("192.168.0.0/16");
    BLIB_TEST_CHECK(copy.toIPv4Subnet().prefix == 16);
    BLIB_TEST_CHECK(copy == Address::fromString("192.168.0.0/16"));
}

BLIB_TEST_CASE("network: address statics and endpoint conversions")
{
    char buffer[maxAddressStringLength];

    // Статические адреса-шаблоны
    BLIB_TEST_CHECK(Address::LocalhostIPv4.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(Address::LocalhostIPv4.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "127.0.0.1") == 0);
    BLIB_TEST_CHECK(Address::AnyIPv4.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "0.0.0.0") == 0);
    BLIB_TEST_CHECK(Address::LocalhostIPv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::1") == 0);

    // Граница winsock: порт — сетевой порядок байт (htons), байты
    // адреса — как есть (сетевой порядок хранения)
    sockaddr_in sockAddress;
    BLIB_TEST_CHECK(blibToSockaddr(Address::LocalhostIPv4,
        static_cast<buint16>(testPort), sockAddress));
    BLIB_TEST_CHECK(sockAddress.sin_family == AF_INET);
    BLIB_TEST_CHECK(sockAddress.sin_port == htons(static_cast<u_short>(testPort)));

    // Обратная конвертация возвращает исходные адрес и порт (ntohs)
    Address backIp;
    buint16 backPort = 0;
    BLIB_TEST_CHECK(blibFromSockaddr(sockAddress, backIp, backPort));
    BLIB_TEST_CHECK(backIp.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(backIp == Address::LocalhostIPv4);
    BLIB_TEST_CHECK(backPort == static_cast<buint16>(testPort));

    // Не-IPv4 (Mac) в sockaddr не конвертируется
    BLIB_TEST_CHECK(!blibToSockaddr(Address::fromString("aa:bb:cc:dd:ee:ff"),
        static_cast<buint16>(testPort), sockAddress));
}

BLIB_TEST_CASE("network: IPv4Subnet isInSubnet")
{
    IPv4Subnet subnet;
    BLIB_TEST_CHECK(subnet.fromString("192.168.1.0/24"));
    BLIB_TEST_CHECK(subnet.getType() == AddressType::IPv4Subnet);

    // Внутри, на границах сети
    const IPv4 insideLow{ 192, 168, 1, 1 };
    const IPv4 insideHigh{ 192, 168, 1, 255 };
    const IPv4 outsideHigh{ 192, 168, 2, 1 };
    const IPv4 outsideLow{ 192, 167, 255, 255 };
    BLIB_TEST_CHECK(subnet.isInSubnet(insideLow));
    BLIB_TEST_CHECK(subnet.isInSubnet(insideHigh));
    BLIB_TEST_CHECK(!subnet.isInSubnet(outsideHigh));
    BLIB_TEST_CHECK(!subnet.isInSubnet(outsideLow));

    // /0 — совпадает всё; /32 — точное совпадение
    const IPv4 anyIp{ 255, 255, 255, 255 };
    const IPv4 exactIp{ 10, 0, 0, 5 };
    const IPv4 neighborIp{ 10, 0, 0, 6 };
    BLIB_TEST_CHECK(subnet.fromString("0.0.0.0/0"));
    BLIB_TEST_CHECK(subnet.isInSubnet(anyIp));
    BLIB_TEST_CHECK(subnet.fromString("10.0.0.5/32"));
    BLIB_TEST_CHECK(subnet.isInSubnet(exactIp));
    BLIB_TEST_CHECK(!subnet.isInSubnet(neighborIp));

    // Префикс больше 32 — отказ разбора
    BLIB_TEST_CHECK(!subnet.fromString("10.0.0.0/33"));
    // Нет префикса — отказ
    BLIB_TEST_CHECK(!subnet.fromString("10.0.0.0"));

    // Roundtrip строки
    char buffer[maxAddressStringLength];
    BLIB_TEST_CHECK(subnet.fromString("172.16.0.0/12"));
    BLIB_TEST_CHECK(subnet.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "172.16.0.0/12") == 0);
}

BLIB_TEST_CASE("network: IPv6Subnet isInSubnet")
{
    IPv6Subnet subnet;
    BLIB_TEST_CHECK(subnet.fromString("2001:db8::/32"));
    BLIB_TEST_CHECK(subnet.getType() == AddressType::IPv6Subnet);

    IPv6 inside;
    IPv6 outside;
    BLIB_TEST_CHECK(inside.fromString("2001:db8::1"));
    BLIB_TEST_CHECK(outside.fromString("2001:db9::1"));
    BLIB_TEST_CHECK(subnet.isInSubnet(inside));
    BLIB_TEST_CHECK(!subnet.isInSubnet(outside));

    // Границы префикса: /128 — точное совпадение; /0 — всё
    const IPv6 loopbackV6{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    BLIB_TEST_CHECK(subnet.fromString("::1/128"));
    BLIB_TEST_CHECK(subnet.isInSubnet(loopbackV6));
    BLIB_TEST_CHECK(!subnet.isInSubnet(IPv6{}));
    BLIB_TEST_CHECK(subnet.fromString("::/0"));
    BLIB_TEST_CHECK(subnet.isInSubnet(outside));

    // Префикс больше 128 — отказ разбора
    BLIB_TEST_CHECK(!subnet.fromString("2001:db8::/129"));

    // Roundtrip строки
    char buffer[maxAddressStringLength];
    BLIB_TEST_CHECK(subnet.fromString("fe80::/10"));
    BLIB_TEST_CHECK(subnet.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "fe80::/10") == 0);
}

BLIB_TEST_CASE("network: Tcp endpoint parse/format")
{
    char buffer[maxAddressStringLength];

    // IPv4-форма
    const IPv4 loopbackV4{ 127, 0, 0, 1 };
    Tcp endpoint;
    BLIB_TEST_CHECK(endpoint.fromString("127.0.0.1:8080"));
    BLIB_TEST_CHECK(endpoint.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(endpoint.getPort() == 8080);
    BLIB_TEST_CHECK(endpoint.getIP().toIPv4() == loopbackV4);
    BLIB_TEST_CHECK(endpoint.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "127.0.0.1:8080") == 0);

    // IPv6-форма (скобки)
    BLIB_TEST_CHECK(endpoint.fromString("[2001:db8::1]:443"));
    BLIB_TEST_CHECK(endpoint.getType() == AddressType::IPv6);
    BLIB_TEST_CHECK(endpoint.getPort() == 443);
    BLIB_TEST_CHECK(endpoint.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "[2001:db8::1]:443") == 0);

    // IPv6-форма с mapped-адресом (хвост печатается dotted)
    BLIB_TEST_CHECK(endpoint.fromString("[::ffff:192.168.1.1]:443"));
    BLIB_TEST_CHECK(endpoint.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "[::ffff:192.168.1.1]:443") == 0);

    // Границы порта
    BLIB_TEST_CHECK(endpoint.fromString("1.2.3.4:0"));
    BLIB_TEST_CHECK(endpoint.getPort() == 0);
    BLIB_TEST_CHECK(endpoint.fromString("1.2.3.4:65535"));
    BLIB_TEST_CHECK(endpoint.getPort() == 65535);

    // Ошибки: порт > 65535, нет порта, голый IPv6 (неоднозначен),
    // пустой адрес, невалидный адрес
    BLIB_TEST_CHECK(!endpoint.fromString("1.2.3.4:65536"));
    BLIB_TEST_CHECK(!endpoint.fromString("127.0.0.1"));
    BLIB_TEST_CHECK(!endpoint.fromString("::1:8080"));
    BLIB_TEST_CHECK(!endpoint.fromString(":8080"));
    BLIB_TEST_CHECK(!endpoint.fromString("abc:8080"));
    // После неудачи — сброс к UNDEFINED/0
    BLIB_TEST_CHECK(endpoint.getType() == AddressType::UNDEFINED);
    BLIB_TEST_CHECK(endpoint.getPort() == 0);

    // setPort/getPort + value-копирование
    Tcp base;
    BLIB_TEST_CHECK(base.fromString("127.0.0.1:8080"));
    Tcp copy = base;
    copy.setPort(9090);
    BLIB_TEST_CHECK(base.getPort() == 8080);
    BLIB_TEST_CHECK(copy.getPort() == 9090);
    BLIB_TEST_CHECK(base != copy);
}

BLIB_TEST_CASE("network: Udp endpoint parse/format")
{
    char buffer[maxAddressStringLength];

    Udp endpoint;
    BLIB_TEST_CHECK(endpoint.fromString("10.0.0.1:5353"));
    BLIB_TEST_CHECK(endpoint.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(endpoint.getPort() == 5353);
    BLIB_TEST_CHECK(endpoint.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "10.0.0.1:5353") == 0);

    // Ошибка — сброс
    BLIB_TEST_CHECK(!endpoint.fromString("10.0.0.1:99999"));
    BLIB_TEST_CHECK(endpoint.getType() == AddressType::UNDEFINED);
}
