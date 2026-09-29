#include <gravelands/server/core/networkServer.h>

#include <blib/core/console/console.h>

namespace gravelands
{
    NetworkServer::NetworkServer()
        : listener(blib::network::AddressType::IPv4)
        , listening(false)
        , client()
        , clientConnected(false)
        , clientJustConnected(false)
        , framer()
        , pendingCommand{ 0, 0 }
        , pendingCommandValid(false)
    {
    }

    NetworkServer::~NetworkServer()
    {
        this->shutdown();
    }

    bool NetworkServer::initialize(buint32 port)
    {
        if (!blib::network::InitBlibSocket())
        {
            __blib_log_error("network server: failed to initialize socket system");
            return false;
        }

        // Слушатель создан в конструкторе (TcpListener(IPv4));
        // здесь — режим/привязка
        if (!this->listener.setBlocking(false))
        {
            __blib_log_error("network server: failed to set non-blocking listener");
            return false;
        }

        blib::network::Address listenAddress = blib::network::Address::LocalhostIPv4;
        listenAddress.setPort(static_cast<int>(port));
        if (this->listener.bind(listenAddress) != blib::network::SocketStatus::OK)
        {
            __blib_log_error("network server: failed to bind port %u (already in use?)", port);
            return false;
        }
        if (this->listener.listen() != blib::network::SocketStatus::OK)
        {
            __blib_log_error("network server: failed to listen on port %u", port);
            return false;
        }

        this->listening = true;
        __blib_log_info("network server listening on 127.0.0.1:%u", port);
        return true;
    }

    void NetworkServer::poll()
    {
        if (!this->listening)
        {
            return;
        }

        this->clientJustConnected = false;

        // Приём подключений: MVP — один клиент; повторное подключение
        // нового клиента заменяет старое соединение
        if (!this->clientConnected)
        {
            const blib::network::SocketStatus status = this->listener.accept(this->client);
            if (status == blib::network::SocketStatus::OK)
            {
                this->client.setBlocking(false);
                this->clientConnected = true;
                this->clientJustConnected = true;
                this->framer.reset();
                __blib_log_info("network server: client connected");
            }
            return; // данных от клиента ещё нет
        }

        // Чтение потока клиента: recv → фреймер → команды
        for (;;)
        {
            int received = static_cast<int>(maxPacketBytes);
            const blib::network::SocketStatus status =
                this->client.recv(this->recvBuffer, received);

            if (status == blib::network::SocketStatus::WouldBlock)
            {
                break; // данных больше нет
            }
            if (status == blib::network::SocketStatus::Disconnected ||
                status == blib::network::SocketStatus::Error)
            {
                __blib_log_info("network server: client disconnected");
                this->client.getSocket()->close();
                this->clientConnected = false;
                this->framer.reset();
                return;
            }

            if (received <= 0)
            {
                continue;
            }

            if (!this->framer.pushBytes(this->recvBuffer, static_cast<buint32>(received)))
            {
                __blib_log_error("network server: incoming stream overflow — dropping client");
                this->client.getSocket()->close();
                this->clientConnected = false;
                this->framer.reset();
                return;
            }

            // Разбор полных сообщений (хвост накапливается во фреймере)
            buint8 payloadBuffer[maxPacketBytes];
            for (;;)
            {
                buint32 payloadSize = 0;
                PacketType type = PacketType::None;
                if (!this->framer.nextMessage(payloadBuffer, maxPacketBytes, payloadSize, type))
                {
                    break; // полных сообщений больше нет
                }

                if (type == PacketType::Command)
                {
                    PlayerCommand command{ 0, 0 };
                    if (decodeCommandPacket(payloadBuffer, payloadSize, command))
                    {
                        // Последняя команда побеждает (MVP)
                        this->pendingCommand = command;
                        this->pendingCommandValid = true;
                    }
                }
                // Прочие типы от клиента — игнорируются (MVP)
            }
        }
    }

    bool NetworkServer::takeClientConnectedEvent()
    {
        const bool result = this->clientJustConnected;
        this->clientJustConnected = false;
        return result;
    }

    bool NetworkServer::takePendingCommand(_Out PlayerCommand& outCommand)
    {
        if (!this->pendingCommandValid)
        {
            return false;
        }
        outCommand = this->pendingCommand;
        this->pendingCommandValid = false;
        return true;
    }

    void NetworkServer::sendWelcome(buint32 serverTickRate, buint64 playerEntityId)
    {
        if (!this->clientConnected)
        {
            return;
        }

        const buint32 size = encodeWelcomePacket(
            this->sendBuffer, maxPacketBytes, serverTickRate, playerEntityId);
        if (size == 0)
        {
            __blib_log_error("network server: failed to encode welcome packet");
            return;
        }

        const blib::network::SocketStatus status = this->client.send(this->sendBuffer, static_cast<int>(size));
        if (status == blib::network::SocketStatus::WouldBlock ||
            status == blib::network::SocketStatus::Error)
        {
            // Отправка не прошла — клиент получит состояние в снапшотах;
            // при ошибке соединение будет сброшено следующим recv
            __blib_log_warning("network server: welcome send returned %d",
                static_cast<int>(status));
        }
    }

    void NetworkServer::sendSnapshot(buint32 tickNumber, _In const SnapshotEntry* entries, buint32 entryCount)
    {
        if (!this->clientConnected)
        {
            return;
        }

        const buint32 size = encodeSnapshotPacket(
            this->sendBuffer, maxPacketBytes, tickNumber, entries, entryCount);
        if (size == 0)
        {
            __blib_log_error("network server: failed to encode snapshot (entries: %u)", entryCount);
            return;
        }

        const blib::network::SocketStatus status = this->client.send(this->sendBuffer, static_cast<int>(size));
        if (status == blib::network::SocketStatus::WouldBlock)
        {
            // Буферы заполнены — пропускаем кадр (клиент догонит)
            return;
        }
        if (status == blib::network::SocketStatus::Error)
        {
            __blib_log_warning("network server: snapshot send failed — dropping client");
            this->client.getSocket()->close();
            this->clientConnected = false;
            this->framer.reset();
        }
    }

    void NetworkServer::shutdown()
    {
        if (this->clientConnected)
        {
            this->client.getSocket()->close();
            this->clientConnected = false;
        }
        this->framer.reset();
        this->listening = false;
    }

} // namespace gravelands
