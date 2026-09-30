#include <beng/server/serverApplication.h>

#include <blib/core/console/console.h>

namespace beng
{
    namespace server
    {
        ServerApplication::ServerApplication()
            : game(nullptr)
            , accumulator(0.0f)
            , tickCounter(0)
            , running(false)
        {
        }

        ServerApplication::~ServerApplication()
        {
            this->shutdown();
        }

        bool ServerApplication::initialize(_In IServerGame& game, buint32 port)
        {
            this->game = &game;

            // Игровая сторона: регистрация типов/систем (ДО схемы и контента)
            game.onServerInitialize(this->scene);

            // Схема репликации по реестру сцены
            this->schema.build(this->scene);
            this->replicationManager.initialize(this->schema);

            // Контент мира (сцена пуста — buildWorld сбрасывает и строит)
            this->worldManager.bind(this->scene, game);
            this->worldManager.buildWorld();

            // Сетевой слушатель (порт по умолчанию — из игры)
            const buint32 actualPort = (port != 0) ? port : game.getDefaultPort();
            if (!this->networkServer.initialize(actualPort))
            {
                return false;
            }

            // Таймер свежий (Time не имеет reset — первый tick задаст dt)
            this->accumulator = 0.0f;
            this->tickCounter = 0;
            this->running = true;

            __blib_log_info("ServerApplication: '%s' started on port %u, tick rate %u",
                game.getGameName(), actualPort, game.getTickRate());
            return true;
        }

        void ServerApplication::shutdown()
        {
            if (!this->running)
            {
                return;
            }

            // Сеть первой (клиенты отключаются штатно)
            this->networkServer.shutdown();

            // Сцена/мир — после сети (репликация больше не трогает сцену)
            this->running = false;
            this->game = nullptr;

            __blib_log_info("ServerApplication: shutdown (%llu ticks simulated)",
                static_cast<unsigned long long>(this->tickCounter));
        }

        void ServerApplication::processNetworkEvents()
        {
            // ===== Подключения: зеркала + Welcome =====
            buint32 connected[NetworkServer::maxClients];
            const buint32 connectedCount =
                this->networkServer.takeConnectedEvents(connected, NetworkServer::maxClients);
            for (buint32 i = 0; i < connectedCount; ++i)
            {
                const buint32 clientId = connected[i];
                this->replicationManager.onClientConnected(clientId);

                // Игрок игры (сущность уйдёт в Welcome как playerEntityId)
                const EntityID playerEntity = this->game->onClientJoin(clientId);

                const buint32 welcomeSize = encodeReplicationWelcome(
                    this->welcomeBuffer, sizeof(this->welcomeBuffer),
                    this->game->getTickRate(), playerEntity, this->schema);
                if (welcomeSize == 0 ||
                    !this->networkServer.sendPacket(clientId, ReplicationPacketType::Welcome,
                        this->welcomeBuffer, welcomeSize))
                {
                    __blib_log_error("ServerApplication: failed to send welcome to client %u", clientId);
                }
            }

            // ===== Отключения =====
            buint32 disconnected[NetworkServer::maxClients];
            const buint32 disconnectedCount =
                this->networkServer.takeDisconnectedEvents(disconnected, NetworkServer::maxClients);
            for (buint32 i = 0; i < disconnectedCount; ++i)
            {
                this->replicationManager.onClientDisconnected(disconnected[i]);
                this->game->onClientLeave(disconnected[i]);
            }

            // ===== Команды: маршрутизация игре (payload — непрозрачный) =====
            NetworkServer::IncomingCommand commands[NetworkServer::maxCommandsPerPoll];
            const buint32 commandCount =
                this->networkServer.drainCommands(commands, NetworkServer::maxCommandsPerPoll);
            for (buint32 i = 0; i < commandCount; ++i)
            {
                this->game->onClientCommand(commands[i].clientId, commands[i].payload, commands[i].payloadSize);
            }
        }

        void ServerApplication::stepSimulation(bfloat fixedDelta)
        {
            // Авторитетная симуляция: TransformSystem (-100) и системы игры
            this->scene.update(fixedDelta);
            ++this->tickCounter;

            // Снапшоты каждому подключённому клиенту после тика
            for (buint32 clientId = 0; clientId < NetworkServer::maxClients; ++clientId)
            {
                if (!this->networkServer.hasClient(clientId))
                {
                    continue;
                }

                const bool forceFull = this->networkServer.takeNeedsFullSnapshot(clientId);
                buint32 entityCount = 0;
                if (!this->replicationManager.buildSnapshot(clientId, this->scene,
                    static_cast<buint32>(this->tickCounter), forceFull,
                    this->snapshotPatches, entityCount))
                {
                    __blib_log_warning("ServerApplication: snapshot for client %u exceeds limits, skipped",
                        clientId);
                    continue;
                }

                const buint32 snapshotSize = encodeReplicationSnapshot(
                    this->snapshotBuffer, sizeof(this->snapshotBuffer),
                    static_cast<buint32>(this->tickCounter),
                    this->snapshotPatches, entityCount, this->schema);
                if (snapshotSize == 0 ||
                    !this->networkServer.sendPacket(clientId, ReplicationPacketType::Snapshot,
                        this->snapshotBuffer, snapshotSize))
                {
                    __blib_log_warning("ServerApplication: failed to queue snapshot for client %u", clientId);
                }
            }
        }

        void ServerApplication::tick()
        {
            if (!this->running)
            {
                return;
            }

            // Реальное dt кадра (бенговское время)
            this->time.tick();
            const bfloat deltaTime = static_cast<bfloat>(this->time.getDeltaTime());

            // Сеть: accept/recv/отправка очередей
            this->networkServer.poll();
            this->processNetworkEvents();

            // Фиксированные тики симуляции (аккумулятор с защитой от
            // спирали догона — см. maxTicksPerFrame)
            const bfloat fixedDelta = 1.0f / static_cast<bfloat>(this->game->getTickRate());
            this->accumulator += deltaTime;

            buint32 ticksThisFrame = 0;
            while (this->accumulator >= fixedDelta && ticksThisFrame < maxTicksPerFrame)
            {
                this->stepSimulation(fixedDelta);
                this->accumulator -= fixedDelta;
                ++ticksThisFrame;
            }

            if (ticksThisFrame == maxTicksPerFrame)
            {
                // Кадр длился дольше лимита тиков: остаток отбрасываем —
                // симуляция не накапливает бесконечный долг
                this->accumulator = 0.0f;
            }
        }

    } // namespace server
} // namespace beng
