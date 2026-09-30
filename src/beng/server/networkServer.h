#pragma once

#include <beng/config.h>
#include <beng/core/replicationFramer.h>

#include <blib/blibint.h>
#include <blib/network/tcpListener.h>
#include <blib/network/tcpSocket.h>
#include <blib/utilmacro.h>

namespace beng
{
    namespace server
    {
        /**
         * NetworkServer — сетевая сторона сервера beng (beng-server):
         * неблокирующие сокеты + опрос в кадре сервера (без потоков).
         *
         * Обязанности:
         * - приём подключений (accept в poll) в слоты клиентов;
         * - фрейминг входящего потока (ReplicationFramer) и слив команд;
         * - очередь отправки на клиента с backpressure: при WouldBlock
         *   недосланное дописывается следующей отправкой; при
         *   переполнении очереди она сбрасывается, а клиент помечается
         *   needsFullSnapshot — следующий снапшот репликации полный
         *   (зеркало клиента на сервере обновляется заново);
         * - детект отключений (recv/send: Disconnected/Error).
         *
         * TCP упорядочен и надёжен: дроп снапшота — НЕ потеря данных
         * (клиент получит полный ресинк), а экономия очереди.
         */
        class __beng_api NetworkServer
        {
        public:
            // Максимум одновременных клиентов (слоты — фиксированный массив)
            static constexpr buint32 maxClients = 4;

            // Размер очереди отправки одного клиента (байт)
            static constexpr buint32 maxSendQueueBytes = maxReplicationPacketBytes * 4;

            // Максимум команд, сливаемых за один poll (буфер хоста)
            static constexpr buint32 maxCommandsPerPoll = 16;

            /**
             * Входящая команда клиента (payload — непрозрачные байты
             * для игры; ёмкость — replicationPayloadMaxSize).
             */
            struct IncomingCommand
            {
                buint32 clientId;
                buint8 payload[replicationPayloadMaxSize];
                buint32 payloadSize;
            };

            NetworkServer();
            ~NetworkServer();

            NetworkServer(const NetworkServer&) = delete;
            NetworkServer& operator=(const NetworkServer&) = delete;

            /**
             * Инициализация: слушатель на loopback-порту.
             * @return false — порт занят/недоступен
             */
            bool initialize(buint32 port);

            /**
             * Опрос сети: accept, приём команд всех клиентов, отправка
             * очередей. Вызывается каждый кадр сервера.
             */
            void poll();

            /**
             * Слив событий подключения (одноразовые флаги слота).
             * @return число записанных id
             */
            buint32 takeConnectedEvents(_Out buint32* outClientIds, buint32 capacity);

            /**
             * Слив событий отключения (одноразовые флаги слота).
             * @return число записанных id
             */
            buint32 takeDisconnectedEvents(_Out buint32* outClientIds, buint32 capacity);

            /**
             * Подключён ли клиент.
             */
            bool hasClient(buint32 clientId) const;

            /**
             * Слив всех накопленных команд (порядок — по клиентам и
             * по приходу). @return число записанных команд
             */
            buint32 drainCommands(_Out IncomingCommand* outCommands, buint32 capacity);

            /**
             * Поставить пакет в очередь отправки клиента (payload без
             * заголовка — заголовок добавит сетевой слой).
             * @return false — клиент не подключён
             */
            bool sendPacket(buint32 clientId, ReplicationPacketType type,
                _In const buint8* payload, buint32 payloadSize);

            /**
             * Очередь клиента была сброшена переполнением (нужен полный
             * снапшот репликации); чтение сбрасывает флаг.
             */
            bool takeNeedsFullSnapshot(buint32 clientId);

            /**
             * Корректное гашение (закрытие слушателя и сокетов клиентов).
             */
            void shutdown();

        private:
            /**
             * Слот клиента: сокет + фреймер + очередь отправки.
             */
            struct ClientSlot
            {
                blib::network::TcpSocket socket;
                bool connected;
                bool justConnected;
                bool justDisconnected;

                ReplicationFramer framer;

                // Очередь отправки: байты [0..size), недосланное — с offset.
                // Переполнение: сброс + needsFullSnapshot
                buint8 sendQueue[maxSendQueueBytes];
                buint32 sendQueueSize;
                buint32 sendQueueOffset;
                bool needsFullSnapshot;

                ClientSlot()
                    : connected(false)
                    , justConnected(false)
                    , justDisconnected(false)
                    , sendQueueSize(0)
                    , sendQueueOffset(0)
                    , needsFullSnapshot(false)
                {
                }
            };

            blib::network::TcpListener listener;
            bool listening;

            ClientSlot slots[maxClients];

            // Команды, накопленные текущим poll (сливаются хостом)
            IncomingCommand pendingCommands[maxCommandsPerPoll];
            buint32 pendingCommandCount;

            buint8 recvBuffer[maxReplicationPacketBytes];
            buint8 commandPayloadBuffer[replicationPayloadMaxSize];

            // Обслужить входящий поток одного клиента (фрейминг → команды)
            void pollClient(buint32 clientId);

            // Протолкнуть очередь отправки одного клиента
            void flushClientSend(buint32 clientId);

            // Освободить слот (закрытие сокета, сброс состояния) —
            // локальная очистка без события (shutdown/пересоздание)
            void releaseClient(buint32 clientId);

            // Отключение, обнаруженное сетью: освобождение слота +
            // одноразовое событие хосту (takeDisconnectedEvents)
            void markDisconnected(buint32 clientId);
        };

    } // namespace server
} // namespace beng
