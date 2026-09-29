#include <gravelands/client/core/networkClient.h>

#include <blib/core/console/console.h>
#include <blib/network/address.h>
#include <blib/network/socket.h>

namespace gravelands
{
    NetworkClient::NetworkClient()
        : state(State::Disconnected)
        , socket()
        , serverPort(serverDefaultPort)
        , framer()
        , pendingWelcome{ 0, 0 }
        , pendingWelcomeValid(false)
        , pendingEntryCount(0)
        , pendingTickNumber(0)
        , pendingSnapshotValid(false)
    {
    }

    NetworkClient::~NetworkClient()
    {
        this->shutdown();
    }

    void NetworkClient::connect(buint32 port)
    {
        if (this->state == State::Connecting || this->state == State::Connected)
        {
            return; // уже подключаемся/подключены
        }

        if (!blib::network::InitBlibSocket())
        {
            __blib_log_error("network client: failed to initialize socket system");
            return;
        }

        this->serverPort = port;

        // Сокет создаётся вручную (TcpSocket — некопируемый член)
        this->socket.getSocket()->create(
            blib::network::AddressType::IPv4,
            blib::network::SocketType::Stream,
            blib::network::SocketProtocol::TCP);
        this->socket.setBlocking(false);

        blib::network::Address serverAddress = blib::network::Address::LocalhostIPv4;
        serverAddress.setPort(static_cast<int>(port));
        const blib::network::SocketStatus status = this->socket.connect(serverAddress);
        if (status == blib::network::SocketStatus::OK)
        {
            this->state = State::Connected;
            this->framer.reset();
            __blib_log_info("network client connected to 127.0.0.1:%u", port);
            return;
        }
        if (status == blib::network::SocketStatus::WouldBlock)
        {
            this->state = State::Connecting;
            return;
        }

        // Ошибка подключения (сервер не запущен?) — остаёмся без сети
        __blib_log_warning("network client: connect to 127.0.0.1:%u failed (%d) — running without server",
            port, static_cast<int>(status));
        this->socket.getSocket()->close();
        this->state = State::Disconnected;
    }

    void NetworkClient::poll()
    {
        if (this->state == State::Connecting)
        {
            // Асинхронное подключение: повторяем connect до OK
            blib::network::Address serverAddress = blib::network::Address::LocalhostIPv4;
            serverAddress.setPort(static_cast<int>(this->serverPort));
            const blib::network::SocketStatus status = this->socket.connect(serverAddress);
            if (status == blib::network::SocketStatus::OK)
            {
                this->state = State::Connected;
                this->framer.reset();
                __blib_log_info("network client connected to 127.0.0.1:%u", this->serverPort);
            }
            else if (status == blib::network::SocketStatus::Error)
            {
                __blib_log_warning("network client: connect failed — running without server");
                this->socket.getSocket()->close();
                this->state = State::Disconnected;
            }
            return;
        }

        if (this->state != State::Connected)
        {
            return;
        }

        // Чтение потока: recv → фреймер → сообщения
        for (;;)
        {
            int received = static_cast<int>(maxPacketBytes);
            const blib::network::SocketStatus status =
                this->socket.recv(this->recvBuffer, received);

            if (status == blib::network::SocketStatus::WouldBlock)
            {
                break; // данных больше нет
            }
            if (status == blib::network::SocketStatus::Disconnected ||
                status == blib::network::SocketStatus::Error)
            {
                __blib_log_info("network client: server connection lost");
                this->socket.getSocket()->close();
                this->state = State::Disconnected;
                this->framer.reset();
                return;
            }

            if (received <= 0)
            {
                continue;
            }

            if (!this->framer.pushBytes(this->recvBuffer, static_cast<buint32>(received)))
            {
                __blib_log_error("network client: incoming stream overflow — disconnecting");
                this->socket.getSocket()->close();
                this->state = State::Disconnected;
                this->framer.reset();
                return;
            }

            buint8 payloadBuffer[maxPacketBytes];
            for (;;)
            {
                buint32 payloadSize = 0;
                PacketType type = PacketType::None;
                if (!this->framer.nextMessage(payloadBuffer, maxPacketBytes, payloadSize, type))
                {
                    break;
                }

                if (type == PacketType::Welcome)
                {
                    if (decodeWelcomePacket(payloadBuffer, payloadSize, this->pendingWelcome))
                    {
                        this->pendingWelcomeValid = true;
                    }
                }
                else if (type == PacketType::Snapshot)
                {
                    buint32 tickNumber = 0;
                    buint32 entryCount = 0;
                    if (decodeSnapshotPacket(payloadBuffer, payloadSize,
                        tickNumber, entryCount, this->pendingEntries, maxNetworkUnits))
                    {
                        this->pendingTickNumber = tickNumber;
                        this->pendingEntryCount = entryCount;
                        this->pendingSnapshotValid = true;
                    }
                }
                // Прочие типы — игнорируются
            }
        }
    }

    bool NetworkClient::takeWelcome(_Out WelcomePacket& outWelcome)
    {
        if (!this->pendingWelcomeValid)
        {
            return false;
        }
        outWelcome = this->pendingWelcome;
        this->pendingWelcomeValid = false;
        return true;
    }

    bool NetworkClient::takeSnapshot(
        _Out buint32& outTickNumber,
        _Out SnapshotEntry* outEntries, buint32 entryCapacity,
        _Out buint32& outEntryCount)
    {
        if (!this->pendingSnapshotValid)
        {
            return false;
        }

        outTickNumber = this->pendingTickNumber;
        outEntryCount = (this->pendingEntryCount < entryCapacity)
            ? this->pendingEntryCount : entryCapacity;
        for (buint32 i = 0; i < outEntryCount; ++i)
        {
            outEntries[i] = this->pendingEntries[i];
        }
        this->pendingSnapshotValid = false;
        return true;
    }

    void NetworkClient::sendCommand(_In const PlayerCommand& command)
    {
        if (this->state != State::Connected)
        {
            return;
        }

        const buint32 size = encodeCommandPacket(this->sendBuffer, maxPacketBytes, command);
        if (size == 0)
        {
            __blib_log_error("network client: failed to encode command packet");
            return;
        }

        const blib::network::SocketStatus status =
            this->socket.send(this->sendBuffer, static_cast<int>(size));
        if (status == blib::network::SocketStatus::Error)
        {
            __blib_log_warning("network client: command send failed — disconnecting");
            this->socket.getSocket()->close();
            this->state = State::Disconnected;
            this->framer.reset();
        }
        // WouldBlock — пропускаем (следующая команда в следующем кадре)
    }

    void NetworkClient::shutdown()
    {
        if (this->state != State::Disconnected)
        {
            this->socket.getSocket()->close();
        }
        this->state = State::Disconnected;
        this->framer.reset();
    }

} // namespace gravelands
