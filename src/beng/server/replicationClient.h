#pragma once

#include <beng/config.h>
#include <beng/core/replicationClientState.h>
#include <beng/core/replicationCodec.h>
#include <beng/core/replicationFramer.h>

#include <blib/blibint.h>
#include <blib/network/tcpSocket.h>
#include <blib/utilmacro.h>

namespace beng
{
    class Scene;

    /**
     * ReplicationClient — сетевая сторона рефлексивной репликации
     * (beng-server — сетевой слой движка): неблокирующий TCP-клиент,
     * который принимает Welcome/снапшоты и применяет их к mirror-сцене
     * через ReplicationClientState, а команды игры возит как непрозрачный
     * payload (кодек команд — игра).
     *
     * Без потоков: опрос в кадре клиента (паттерн NetworkServer —
     * неблокирующие сокеты + WouldBlock-цикл, см. SERVER.md).
     *
     * Ограничения (MVP, см. SERVER.md):
     * - снапшоты применяются НЕМЕДЛЕННО по приходу (интерполяция —
     *   слой beng-client, фаза 3);
     * - декодированный снапшот живёт В КУЧЕ (GlobalAllocator) — его
     *   размер не умещается на стеке.
     */
    class __beng_api ReplicationClient
    {
    public:
        /**
         * Состояние подключения.
         */
        enum class Connection : buint8
        {
            Disconnected, // нет соединения (или потеряно)
            Connecting,   // connect в процессе (WouldBlock-цикл)
            Connected
        };

        ReplicationClient();
        ~ReplicationClient();

        ReplicationClient(const ReplicationClient&) = delete;
        ReplicationClient& operator=(const ReplicationClient&) = delete;

        /**
         * Начать асинхронное подключение к loopback-серверу на порту.
         * Повторный вызов из Connected — закрывает текущее соединение.
         */
        void connect(buint32 port);

        /**
         * Опрос: продвижение подключения, чтение Welcome/снапшотов,
         * применение снапшотов к mirror-сцене.
         *
         * @param scene Mirror-сцена клиента (типы игры зарегистрированы);
         *        сцена передаётся каждый кадр — при сбросе сцены хостом
         *        зеркало пересоздаётся по пришедшему Welcome заново
         */
        void poll(_In Scene& scene);

        /**
         * Состояние подключения.
         */
        Connection getConnection() const { return this->connection; }

        /**
         * Принят ли Welcome и открыта ли сессия (зеркало готово).
         */
        bool isSessionReady() const { return this->sessionReady; }

        /**
         * Параметры сессии (валидны при isSessionReady).
         */
        buint32 getServerTickRate() const { return this->serverTickRate; }

        /**
         * Сущность игрока ЭТОГО клиента (из Welcome; invalidEntity —
         * наблюдатель/сессия не открыта).
         */
        EntityID getPlayerEntityId() const { return this->playerEntityId; }

        /**
         * Отправить команду игры (payload — непрозрачные байты;
         * при WouldBlock пакет пропускается — команда придёт со
         * следующим кадром).
         * @return false — нет соединения/размер превышен
         */
        bool sendCommand(_In const buint8* payload, buint32 payloadSize);

        /**
         * Закрыть соединение (идемпотентно; зеркало сохраняется —
         * хост может сбросить его через ReplicationClientState::reset).
         */
        void shutdown();

        /**
         * Клиентское зеркало (применение снапшотов) — для сброса
         * и диагностики хостом.
         */
        ReplicationClientState& getMirror() { return this->mirror; }

    private:
        Connection connection;
        bool sessionReady;
        buint32 serverPort;

        // Сетевая сторона
        blib::network::TcpSocket socket;
        ReplicationFramer framer;
        buint8 recvBuffer[maxReplicationPacketBytes];
        buint8 sendBuffer[maxReplicationPacketBytes];
        buint8 payloadBuffer[replicationPayloadMaxSize];

        // Сессия
        buint32 serverTickRate;
        EntityID playerEntityId;

        // Клиентское зеркало (применение снапшотов к сцене)
        ReplicationClientState mirror;

        // Декодированный снапшот — в куче (GlobalAllocator): структура
        // ~128 КБ, стек не резиновый (см. SERVER.md «Подводные камни»)
        DecodedReplicationSnapshot* decodedSnapshot;

        // Приём потока: фрейминг → маршрутизация пакетов
        void receiveStream(_In Scene& scene);

        // Открыть сессию по Welcome (сверка схем + маппинг)
        void openSession(_In Scene& scene, _In const buint8* payload, buint32 payloadSize);

        // Применить снапшот к зеркалу
        void applySnapshot(_In Scene& scene, _In const buint8* payload, buint32 payloadSize);
    };

} // namespace beng
