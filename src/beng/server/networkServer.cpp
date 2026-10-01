#include <beng/server/networkServer.h>

#include <blib/core/console/console.h>

#include <cstring>

namespace beng
{
    namespace server
    {
        NetworkServer::NetworkServer()
            : listening(false)
            , pendingCommandCount(0)
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
                __blib_log_error("NetworkServer: failed to initialize winsock");
                return false;
            }

            // Слушатель пересоздаётся open() (идемпотентно и для
            // первого запуска, и для перезапуска после shutdown)
            if (this->listener.open(blib::network::address::AddressType::IPv4) != blib::network::SocketStatus::OK)
            {
                __blib_log_error("NetworkServer: failed to create listener");
                return false;
            }

            // Loopback-эндпоинт (LocalhostIPv4 — статический
            // адрес-шаблон; Tcp — value-семантика)
            blib::network::address::Tcp endpoint;
            endpoint.ip = blib::network::address::Address::LocalhostIPv4;
            endpoint.port = static_cast<buint16>(port);
            if (this->listener.bind(endpoint) != blib::network::SocketStatus::OK)
            {
                __blib_log_error("NetworkServer: failed to bind port %u", port);
                return false;
            }

            if (this->listener.listen() != blib::network::SocketStatus::OK)
            {
                __blib_log_error("NetworkServer: listen failed");
                return false;
            }

            this->listener.setBlocking(false);
            this->listening = true;
            this->pendingCommandCount = 0;
            return true;
        }

        void NetworkServer::shutdown()
        {
            for (buint32 i = 0; i < maxClients; ++i)
            {
                this->releaseClient(i);
            }

            if (this->listening)
            {
                this->listener.close();
                this->listening = false;
            }
        }

        void NetworkServer::releaseClient(buint32 clientId)
        {
            ClientSlot& slot = this->slots[clientId];
            if (slot.connected)
            {
                slot.socket.getSocket()->close();
            }
            slot.connected = false;
            slot.justConnected = false;
            slot.justDisconnected = false;
            slot.framer.reset();
            slot.sendQueueSize = 0;
            slot.sendQueueOffset = 0;
            slot.needsFullSnapshot = false;
        }

        void NetworkServer::markDisconnected(buint32 clientId)
        {
            // Отключение, обнаруженное сетью: закрываем сокет и ставим
            // одноразовое событие хосту (takeDisconnectedEvents);
            // justConnected гасится — пары подключился/отключился
            // без слива не копятся
            ClientSlot& slot = this->slots[clientId];
            if (!slot.connected)
            {
                return;
            }
            slot.socket.getSocket()->close();
            slot.connected = false;
            slot.justConnected = false;
            slot.justDisconnected = true;
            slot.framer.reset();
            slot.sendQueueSize = 0;
            slot.sendQueueOffset = 0;
            slot.needsFullSnapshot = false;
        }

        bool NetworkServer::hasClient(buint32 clientId) const
        {
            return clientId < maxClients && this->slots[clientId].connected;
        }

        void NetworkServer::poll()
        {
            // ===== Приём подключений =====
            if (this->listening)
            {
                // Свободный слот — цель accept (сокет несменяем:
                // accept заполняет сокет вызывающего)
                buint32 freeSlot = maxClients;
                for (buint32 i = 0; i < maxClients; ++i)
                {
                    if (!this->slots[i].connected)
                    {
                        freeSlot = i;
                        break;
                    }
                }

                if (freeSlot != maxClients)
                {
                    ClientSlot& slot = this->slots[freeSlot];
                    const blib::network::SocketStatus status = this->listener.accept(slot.socket);
                    if (status == blib::network::SocketStatus::OK)
                    {
                        // Принятый сокет: неблокирующий, Nagle выключен
                        slot.socket.setBlocking(false);
                        slot.socket.setTcpNoDelay(true);
                        slot.connected = true;
                        slot.justConnected = true;
                        slot.framer.reset();
                        slot.sendQueueSize = 0;
                        slot.sendQueueOffset = 0;
                        slot.needsFullSnapshot = false;

                        __blib_log_info("NetworkServer: client %u connected", freeSlot);
                    }
                    else if (status == blib::network::SocketStatus::WouldBlock)
                    {
                        // Подключений нет — штатно
                    }
                    else
                    {
                        __blib_log_warning("NetworkServer: accept failed");
                    }
                }
            }

            // ===== Приём данных и отправка очередей =====
            for (buint32 i = 0; i < maxClients; ++i)
            {
                if (!this->slots[i].connected)
                {
                    continue;
                }
                this->pollClient(i);
                if (this->slots[i].connected)
                {
                    this->flushClientSend(i);
                }
            }
        }

        void NetworkServer::pollClient(buint32 clientId)
        {
            ClientSlot& slot = this->slots[clientId];

            // Сливаем весь доступный поток (цикл: TCP может отдать меньше)
            bool alive = true;
            while (alive)
            {
                int received = static_cast<int>(sizeof(this->recvBuffer));
                const blib::network::SocketStatus status =
                    slot.socket.recv(this->recvBuffer, received);

                switch (status)
                {
                    case blib::network::SocketStatus::OK:
                        if (received > 0)
                        {
                            if (!slot.framer.pushBytes(this->recvBuffer, static_cast<buint32>(received)))
                            {
                                __blib_log_warning("NetworkServer: client %u stream overflow, disconnecting", clientId);
                                this->markDisconnected(clientId);
                                return;
                            }

                            // Разбор целых сообщений → команды
                            buint32 payloadSize = 0;
                            ReplicationPacketType type = ReplicationPacketType::None;
                            while (slot.framer.nextMessage(this->commandPayloadBuffer,
                                replicationPayloadMaxSize, payloadSize, type))
                            {
                                if (type == ReplicationPacketType::Command &&
                                    this->pendingCommandCount < maxCommandsPerPoll)
                                {
                                    IncomingCommand& command = this->pendingCommands[this->pendingCommandCount];
                                    command.clientId = clientId;
                                    std::memcpy(command.payload, this->commandPayloadBuffer, payloadSize);
                                    command.payloadSize = payloadSize;
                                    ++this->pendingCommandCount;
                                }
                            }
                        }
                        break;

                    case blib::network::SocketStatus::WouldBlock:
                        // Поток вычерпан — штатно
                        alive = false;
                        break;

                    default:
                        // Disconnected/Error — клиент ушёл
                        __blib_log_info("NetworkServer: client %u disconnected", clientId);
                        this->markDisconnected(clientId);
                        return;
                }
            }
        }

        void NetworkServer::flushClientSend(buint32 clientId)
        {
            ClientSlot& slot = this->slots[clientId];
            if (slot.sendQueueSize == 0)
            {
                return;
            }

            int sent = 0;
            const int pending = static_cast<int>(slot.sendQueueSize - slot.sendQueueOffset);
            const blib::network::SocketStatus status =
                slot.socket.send(slot.sendQueue + slot.sendQueueOffset, pending, &sent);

            switch (status)
            {
                case blib::network::SocketStatus::OK:
                    slot.sendQueueOffset += static_cast<buint32>(sent);
                    if (slot.sendQueueOffset >= slot.sendQueueSize)
                    {
                        slot.sendQueueSize = 0;
                        slot.sendQueueOffset = 0;
                    }
                    break;

                case blib::network::SocketStatus::WouldBlock:
                    // Недосланное допишется следующим кадром (TCP-буфер
                    // заполнен — штатная ситуация на медленном клиенте)
                    slot.sendQueueOffset += static_cast<buint32>(sent);
                    break;

                default:
                    __blib_log_info("NetworkServer: client %u disconnected (send)", clientId);
                    this->markDisconnected(clientId);
                    break;
            }
        }

        buint32 NetworkServer::takeConnectedEvents(_Out buint32* outClientIds, buint32 capacity)
        {
            buint32 count = 0;
            for (buint32 i = 0; i < maxClients && count < capacity; ++i)
            {
                if (this->slots[i].justConnected)
                {
                    this->slots[i].justConnected = false;
                    outClientIds[count++] = i;
                }
            }
            return count;
        }

        buint32 NetworkServer::takeDisconnectedEvents(_Out buint32* outClientIds, buint32 capacity)
        {
            buint32 count = 0;
            for (buint32 i = 0; i < maxClients && count < capacity; ++i)
            {
                if (this->slots[i].justDisconnected)
                {
                    this->slots[i].justDisconnected = false;
                    outClientIds[count++] = i;
                }
            }
            return count;
        }

        buint32 NetworkServer::drainCommands(_Out IncomingCommand* outCommands, buint32 capacity)
        {
            const buint32 count = this->pendingCommandCount < capacity
                ? this->pendingCommandCount
                : capacity;
            for (buint32 i = 0; i < count; ++i)
            {
                outCommands[i] = this->pendingCommands[i];
            }
            this->pendingCommandCount = 0;
            return count;
        }

        bool NetworkServer::sendPacket(buint32 clientId, ReplicationPacketType type,
            _In const buint8* payload, buint32 payloadSize)
        {
            if (clientId >= maxClients || !this->slots[clientId].connected)
            {
                return false;
            }
            if (payloadSize > replicationPayloadMaxSize)
            {
                return false;
            }

            ClientSlot& slot = this->slots[clientId];

            // Размер всего пакета: заголовок + payload
            const buint32 packetSize = replicationHeaderSize + payloadSize;
            if (slot.sendQueueSize + packetSize > maxSendQueueBytes)
            {
                // Очередь переполнена (клиент не успевает читать):
                // сбрасываем и помечаем — следующий снапшот полный
                __blib_log_warning("NetworkServer: client %u send queue overflow, dropping backlog", clientId);
                slot.sendQueueSize = 0;
                slot.sendQueueOffset = 0;
                slot.needsFullSnapshot = true;
            }

            buint8* cursor = slot.sendQueue + slot.sendQueueSize;
            cursor[0] = static_cast<buint8>(type);
            cursor[1] = static_cast<buint8>(payloadSize & 0xFF);
            cursor[2] = static_cast<buint8>((payloadSize >> 8) & 0xFF);
            std::memcpy(cursor + replicationHeaderSize, payload, payloadSize);
            slot.sendQueueSize += packetSize;
            return true;
        }

        bool NetworkServer::takeNeedsFullSnapshot(buint32 clientId)
        {
            if (clientId >= maxClients)
            {
                return false;
            }
            const bool value = this->slots[clientId].needsFullSnapshot;
            this->slots[clientId].needsFullSnapshot = false;
            return value;
        }

    } // namespace server
} // namespace beng
