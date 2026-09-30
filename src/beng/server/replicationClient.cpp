#include <beng/server/replicationClient.h>

#include <beng/core/scene.h>

#include <blib/core/console/console.h>
#include <blib/system/memory/globalAllocator.h>

namespace beng
{
    ReplicationClient::ReplicationClient()
        : connection(Connection::Disconnected)
        , sessionReady(false)
        , serverPort(0)
        , serverTickRate(0)
        , playerEntityId(invalidEntity)
        , decodedSnapshot(nullptr)
    {
        // Декодированный снапшот — в куче через GlobalAllocator
        // (структура ~128 КБ — стек не резиновый)
        this->decodedSnapshot = static_cast<DecodedReplicationSnapshot*>(
            blib::memory::GlobalAllocator::instance().allocate(
                sizeof(DecodedReplicationSnapshot)));
    }

    ReplicationClient::~ReplicationClient()
    {
        this->shutdown();
        if (this->decodedSnapshot != nullptr)
        {
            blib::memory::GlobalAllocator::instance().deallocate(
                this->decodedSnapshot, sizeof(DecodedReplicationSnapshot));
            this->decodedSnapshot = nullptr;
        }
    }

    void ReplicationClient::connect(buint32 port)
    {
        // Повторный вызов: закрыть текущее соединение и начать заново
        this->shutdown();

        // Сокет пересоздаётся на месте (TcpSocket некопируем и
        // неперемещаем; close+destroy+create — паттерн TcpListener::open,
        // см. NETWORK.md)
        blib::network::Socket* raw = this->socket.getSocket();
        raw->close();
        raw->destroy();
        raw->create(blib::network::AddressType::IPv4,
            blib::network::SocketType::Stream, blib::network::SocketProtocol::TCP);
        this->socket.setBlocking(false);

        this->serverPort = port;
        this->connection = Connection::Connecting;
        this->sessionReady = false;
        this->serverTickRate = 0;
        this->playerEntityId = invalidEntity;
        this->mirror.reset();
    }

    void ReplicationClient::shutdown()
    {
        if (this->connection != Connection::Disconnected)
        {
            this->socket.getSocket()->close();
            this->connection = Connection::Disconnected;
        }
        this->sessionReady = false;
        this->framer.reset();
    }

    bool ReplicationClient::sendCommand(_In const buint8* payload, buint32 payloadSize)
    {
        if (this->connection != Connection::Connected)
        {
            return false;
        }
        if (payloadSize > replicationPayloadMaxSize)
        {
            return false;
        }

        // Пакет: [type][size][payload]
        this->sendBuffer[0] = static_cast<buint8>(ReplicationPacketType::Command);
        this->sendBuffer[1] = static_cast<buint8>(payloadSize & 0xFF);
        this->sendBuffer[2] = static_cast<buint8>((payloadSize >> 8) & 0xFF);
        for (buint32 i = 0; i < payloadSize; ++i)
        {
            this->sendBuffer[replicationHeaderSize + i] = payload[i];
        }

        int sent = 0;
        const blib::network::SocketStatus status = this->socket.send(
            this->sendBuffer, static_cast<int>(replicationHeaderSize + payloadSize), &sent);

        // WouldBlock — команда пропускается (следующая придёт в
        // следующем кадре); команды ввода идемпотентны по своей природе
        return status == blib::network::SocketStatus::OK;
    }

    void ReplicationClient::openSession(_In Scene& scene, _In const buint8* payload, buint32 payloadSize)
    {
        DecodedReplicationWelcome welcome;
        if (!decodeReplicationWelcome(payload, payloadSize, welcome))
        {
            __blib_log_error("ReplicationClient: malformed welcome");
            return;
        }

        buint32 tickRate = 0;
        EntityID playerEntity = invalidEntity;
        if (!this->mirror.acceptWelcome(scene, welcome, tickRate, playerEntity))
        {
            // Рассинхрон схем: сессия не открывается (см. SERVER.md)
            __blib_log_error("ReplicationClient: session rejected (schema mismatch)");
            return;
        }

        this->serverTickRate = tickRate;
        this->playerEntityId = playerEntity;
        this->sessionReady = true;

        __blib_log_info("ReplicationClient: session ready (tick rate %u, player entity %llu)",
            tickRate, static_cast<unsigned long long>(playerEntity));
    }

    void ReplicationClient::applySnapshot(_In Scene& scene, _In const buint8* payload, buint32 payloadSize)
    {
        if (!this->sessionReady)
        {
            // Снапшот до Welcome — протокол нарушен, игнорируем
            return;
        }

        if (!decodeReplicationSnapshot(payload, payloadSize, *this->decodedSnapshot))
        {
            __blib_log_warning("ReplicationClient: malformed snapshot dropped");
            return;
        }

        if (!this->mirror.applySnapshot(scene, *this->decodedSnapshot))
        {
            __blib_log_error("ReplicationClient: snapshot apply failed");
        }
    }

    void ReplicationClient::receiveStream(_In Scene& scene)
    {
        bool alive = true;
        while (alive)
        {
            int received = static_cast<int>(sizeof(this->recvBuffer));
            const blib::network::SocketStatus status = this->socket.recv(this->recvBuffer, received);

            switch (status)
            {
                case blib::network::SocketStatus::OK:
                    if (received > 0 &&
                        !this->framer.pushBytes(this->recvBuffer, static_cast<buint32>(received)))
                    {
                        __blib_log_warning("ReplicationClient: stream overflow, disconnecting");
                        this->shutdown();
                        return;
                    }

                    // Разбор целых сообщений
                    {
                        buint32 payloadSize = 0;
                        ReplicationPacketType type = ReplicationPacketType::None;
                        while (this->framer.nextMessage(this->payloadBuffer,
                            replicationPayloadMaxSize, payloadSize, type))
                        {
                            switch (type)
                            {
                                case ReplicationPacketType::Welcome:
                                    this->openSession(scene, this->payloadBuffer, payloadSize);
                                    break;
                                case ReplicationPacketType::Snapshot:
                                    this->applySnapshot(scene, this->payloadBuffer, payloadSize);
                                    break;
                                default:
                                    // Command от сервера не легитимна — игнорируем
                                    break;
                            }
                        }
                    }
                    break;

                case blib::network::SocketStatus::WouldBlock:
                    alive = false;
                    break;

                default:
                    // Disconnected/Error — сервер ушёл
                    __blib_log_info("ReplicationClient: server disconnected");
                    this->shutdown();
                    return;
            }
        }
    }

    void ReplicationClient::poll(_In Scene& scene)
    {
        if (this->connection == Connection::Connecting)
        {
            // Асинхронный connect: WouldBlock = «в процессе»
            blib::network::Address address(blib::network::Address::LocalhostIPv4);
            address.setPort(static_cast<int>(this->serverPort));
            const blib::network::SocketStatus status = this->socket.connect(address);
            if (status == blib::network::SocketStatus::OK)
            {
                this->socket.setTcpNoDelay(true);
                this->connection = Connection::Connected;
                __blib_log_info("ReplicationClient: connected to port %u", this->serverPort);
            }
            else if (status != blib::network::SocketStatus::WouldBlock)
            {
                __blib_log_warning("ReplicationClient: connect failed");
                this->shutdown();
                return;
            }
        }

        if (this->connection == Connection::Connected)
        {
            this->receiveStream(scene);
        }
    }

} // namespace beng
