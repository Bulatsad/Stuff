#include <gravelands/server/core/gravelandsServerGame.h>

#include <gravelands/common/protocol.h>
#include <gravelands/common/unitComponent.h>

#include <beng/components/transform.h>

#include <blib/core/console/console.h>

namespace gravelands
{
    GravelandsServerGame::GravelandsServerGame()
        : scene(nullptr)
    {
        for (buint32 i = 0; i < beng::server::NetworkServer::maxClients; ++i)
        {
            this->clientUnits[i] = beng::invalidEntity;
        }
    }

    const char* GravelandsServerGame::getGameName() const
    {
        return gameTitle;
    }

    buint32 GravelandsServerGame::getTickRate() const
    {
        return serverTickRate;
    }

    buint32 GravelandsServerGame::getDefaultPort() const
    {
        return serverDefaultPort;
    }

    void GravelandsServerGame::onServerInitialize(_In beng::Scene& scene)
    {
        this->scene = &scene;

        // Повторный запуск (PIE: Play → Stop → Play): реестр типов и
        // список систем переживают Scene::reset() (его делает
        // WorldManager::buildWorld в ServerApplication::initialize) —
        // регистрация и addSystem выполняются только при ПЕРВОМ
        // запуске (повторные — fatal: коллизия имени типа; дубли систем)
        const bool firstStart = !scene.isRegisteredComponentType<UnitComponent>();
        if (!firstStart)
        {
            return;
        }

        // TransformComponent регистрируется сценой автоматически
        // (инвариант: каждая сущность рождается с Transform);
        // игровой компонент — регистрируем явно
        scene.registerComponentType<UnitComponent>();

        // Системы выполняются в порядке приоритета (ISystem::getPriority):
        // TransformSystem (-100) → MovementSystem (0)
        scene.addSystem(&this->transformSystem);
        scene.addSystem(&this->movementSystem);
    }

    void GravelandsServerGame::onWorldBuild(_In beng::Scene& scene)
    {
        (void)scene;

        // Статического контента у серверного мира нет (тайлы/свет —
        // клиентская презентация в gravelands-world): сущности юнитов
        // рождаются только в onClientJoin — у наблюдателей пустой мир
    }

    beng::EntityID GravelandsServerGame::onClientJoin(buint32 clientId)
    {
        if (clientId >= beng::server::NetworkServer::maxClients ||
            this->scene == nullptr)
        {
            __blib_log_warning("%s server: join for invalid client %u ignored",
                gameTitle, clientId);
            return beng::invalidEntity;
        }

        // Авторитетный юнит игрока: Transform (позиция реплицируется
        // движком) + UnitComponent (входной кеш, не реплицируется)
        const beng::EntityID unit = this->scene->createEntity();
        UnitComponent& unitComponent = this->scene->addComponent<UnitComponent>(unit);
        unitComponent.setIsPlayer(true);
        this->scene->getComponent<beng::TransformComponent>(unit).setLocalPosition(
            blib::math::Vector<float, 3>(playerStartX, 0.0f, playerStartZ));

        this->clientUnits[clientId] = unit;

        __blib_log_info("%s server: client %u joined (unit %llu)",
            gameTitle, clientId, static_cast<unsigned long long>(unit));
        return unit;
    }

    void GravelandsServerGame::onClientLeave(buint32 clientId)
    {
        if (clientId >= beng::server::NetworkServer::maxClients ||
            this->scene == nullptr)
        {
            return;
        }

        const beng::EntityID unit = this->clientUnits[clientId];
        if (unit != beng::invalidEntity)
        {
            // Уничтожение юнита уедет клиентам destroy-патчем зеркала
            // (ReplicationManager собирает его в следующем снапшоте)
            this->scene->destroyEntity(unit);
            this->clientUnits[clientId] = beng::invalidEntity;
        }
    }

    void GravelandsServerGame::onClientCommand(buint32 clientId,
        _In const buint8* payload, buint32 payloadSize)
    {
        if (clientId >= beng::server::NetworkServer::maxClients)
        {
            return;
        }

        const beng::EntityID unit = this->clientUnits[clientId];
        if (unit == beng::invalidEntity)
        {
            return; // юнита нет — команду отбрасываем (сервер не доверяет)
        }

        // Кодек команд игры (payload движка — непрозрачный для него)
        PlayerCommand command{ 0, 0 };
        if (!decodeCommandPayload(payload, payloadSize, command))
        {
            __blib_log_warning("%s server: malformed command from client %u dropped",
                gameTitle, clientId);
            return;
        }

        UnitComponent* unitComponent = this->scene->tryGetComponent<UnitComponent>(unit);
        if (unitComponent != nullptr)
        {
            unitComponent->setMoveX(command.moveX);
            unitComponent->setMoveZ(command.moveZ);
        }
    }

} // namespace gravelands
