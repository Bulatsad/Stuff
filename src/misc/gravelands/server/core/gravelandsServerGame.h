#pragma once

#include <gravelands/common/config.h>

#include <gravelands/server/core/movementSystem.h>

#include <beng/core/scene.h>
#include <beng/server/iServerGame.h>
#include <beng/server/networkServer.h>
#include <beng/systems/transformSystem.h>

namespace gravelands
{
    /**
     * GravelandsServerGame — игровая сторона сервера Gravelands
     * (реализация IServerGame, композиция с движковым
     * beng::server::ServerApplication — см. ARCHITECTURE.md):
     * типы компонентов, системы, игроки, кодек команд.
     *
     * Репликация — движковая: позиции юнитов возит TransformComponent
     * (рефлексивная репликация, см. SERVER.md); UnitComponent не
     * реплицируется (входной кеш ввода, клиенту не нужен).
     *
     * Перезапуск сервера (PIE: Play → Stop → Play) безопасен:
     * регистрация типов/систем — с guard'ом по реестру сцены
     * (повторная регистрация — fatal, см. Scene::registerComponentType);
     * сцена сбрасывается WorldManager::buildWorld.
     */
    class GravelandsServerGame : public beng::server::IServerGame
    {
    public:
        GravelandsServerGame();

        const char* getGameName() const __blib_override;
        buint32 getTickRate() const __blib_override;
        buint32 getDefaultPort() const __blib_override;

        void onServerInitialize(_In beng::Scene& scene) __blib_override;
        void onWorldBuild(_In beng::Scene& scene) __blib_override;
        beng::EntityID onClientJoin(buint32 clientId) __blib_override;
        void onClientLeave(buint32 clientId) __blib_override;
        void onClientCommand(buint32 clientId,
            _In const buint8* payload, buint32 payloadSize) __blib_override;

    private:
        // Авторитетная сцена (ссылка из onServerInitialize; живёт в
        // ServerApplication дольше игры)
        beng::Scene* scene;

        // Юнит каждого клиента (создан в onClientJoin; invalidEntity —
        // слота нет). Число слотов — лимит NetworkServer
        beng::EntityID clientUnits[beng::server::NetworkServer::maxClients];

        // Пересчёт мировых матриц (-100) и движение юнитов (0) —
        // системы игры (движок свои системы серверу не вешает)
        beng::TransformSystem transformSystem;
        MovementSystem movementSystem;
    };

} // namespace gravelands
