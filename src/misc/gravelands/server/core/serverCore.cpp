#include <gravelands/server/core/serverCore.h>

#include <gravelands/common/unitComponent.h>

#include <beng/components/transform.h>

#include <blib/core/console/console.h>

namespace gravelands
{
    ServerCore::ServerCore()
        : accumulator(0.0f)
        , tickCounter(0)
        , lastHeartbeatSecond(0)
        , playerEntity(beng::invalidEntity)
        , running(false)
    {
    }

    bool ServerCore::initialize(buint32 port)
    {
        // Повторный запуск (PIE: Play → Stop → Play): ядро переживает
        // сессии — сцена сбрасывается на месте, счётчики обнуляются.
        // Реестр типов и список систем переживают Scene::reset() —
        // регистрация и addSystem выполняются только при ПЕРВОМ
        // запуске (повторные — fatal: коллизия имени типа; дубли систем)
        const bool firstStart = !scene.isRegisteredComponentType<UnitComponent>();
        scene.reset();

        if (firstStart)
        {
            // TransformComponent регистрируется сценой автоматически
            // (инвариант: каждая сущность рождается с Transform);
            // игровой компонент — регистрируем явно
            scene.registerComponentType<UnitComponent>();

            // Системы выполняются в порядке приоритета (ISystem::getPriority):
            // TransformSystem (-100) → MovementSystem (0)
            scene.addSystem(&transformSystem);
            scene.addSystem(&movementSystem);
        }

        // Состояние сессии — с нуля
        accumulator = 0.0f;
        tickCounter = 0;
        lastHeartbeatSecond = 0;
        playerEntity = beng::invalidEntity;

        // Авторитетный юнит игрока (управляется по сети)
        playerEntity = scene.createEntity();
        UnitComponent& unit = scene.addComponent<UnitComponent>(playerEntity);
        unit.setIsPlayer(true);
        beng::TransformComponent& transform =
            scene.getComponent<beng::TransformComponent>(playerEntity);
        transform.setLocalPosition(blib::math::Vector<float, 3>(playerStartX, 0.0f, playerStartZ));

        // Сеть: слушатель на loopback-порту
        if (!networkServer.initialize(port))
        {
            __blib_log_error("%s server core: network initialization failed", gameTitle);
            return false;
        }

        // Сброс точки отсчёта dt (первый tick даст корректное значение)
        time.tick();

        running = true;

        __blib_log_info("%s server core initialized (simulation tick rate: %u Hz, fixed delta: %.4f s, port: %u)",
            gameTitle, serverTickRate, serverFixedDelta, port);

        return true;
    }

    void ServerCore::tick()
    {
        if (__blib_unlikely(!running))
        {
            return;
        }

        // Опрос сети: подключения + команды игрока
        networkServer.poll();

        // Новый клиент — приветствие сессии (тикрейт + EntityID игрока)
        if (networkServer.takeClientConnectedEvent())
        {
            networkServer.sendWelcome(serverTickRate, playerEntity);
        }

        // Реальный dt текущего кадра (секунды)
        time.tick();
        accumulator += time.getDeltaTime();

        // Шагаем симуляцию фиксированными тиками; остаток остаётся
        // в аккумуляторе и догоняется следующими кадрами
        while (accumulator >= serverFixedDelta)
        {
            // Команды игрока применяются к его юниту ДО шага симуляции
            PlayerCommand command{ 0, 0 };
            if (networkServer.takePendingCommand(command))
            {
                UnitComponent* unit = scene.tryGetComponent<UnitComponent>(playerEntity);
                if (unit != nullptr)
                {
                    unit->setMoveX(command.moveX);
                    unit->setMoveZ(command.moveZ);
                }
            }

            scene.update(serverFixedDelta);
            accumulator -= serverFixedDelta;

            ++tickCounter;

            // Снапшот авторитетного состояния — после каждого тика
            broadcastSnapshot();

            // Heartbeat раз в секунду (debug-уровень, вырезается в release).
            // Сравниваем секунды, а не tick % rate: иначе при нуле тиков
            // за кадр одно и то же сообщение печаталось бы каждый кадр.
            const buint64 heartbeatSecond = tickCounter / serverTickRate;
            if (heartbeatSecond != lastHeartbeatSecond)
            {
                lastHeartbeatSecond = heartbeatSecond;

                __blib_log_debug("%s server heartbeat: tick %llu, entities: %u, systems: %u",
                    gameTitle,
                    static_cast<unsigned long long>(tickCounter),
                    static_cast<unsigned int>(scene.getEntityCount()),
                    static_cast<unsigned int>(scene.getSystemCount()));
            }
        }
    }

    void ServerCore::broadcastSnapshot()
    {
        if (!networkServer.hasClient())
        {
            return; // слать некому
        }

        // Сбор позиций юнитов (Transform + UnitComponent)
        SnapshotEntry entries[maxSnapshotUnits];
        buint32 entryCount = 0;

        const buint32 entityCount = scene.getEntityCount();
        for (buint32 i = 0; i < entityCount && entryCount < maxSnapshotUnits; ++i)
        {
            const beng::EntityID id = scene.getEntityId(i);
            if (id == beng::invalidEntity)
            {
                continue;
            }

            UnitComponent* unit = scene.tryGetComponent<UnitComponent>(id);
            if (unit == nullptr)
            {
                continue; // не юнит — в снапшот не попадает
            }

            beng::TransformComponent* transform =
                scene.tryGetComponent<beng::TransformComponent>(id);
            if (transform == nullptr)
            {
                continue;
            }

            const blib::math::Vector<float, 3> position = transform->getLocalPosition();
            entries[entryCount].entityId = id;
            entries[entryCount].positionX = position.x;
            entries[entryCount].positionY = position.y;
            entries[entryCount].positionZ = position.z;
            ++entryCount;
        }

        networkServer.sendSnapshot(static_cast<buint32>(tickCounter), entries, entryCount);
    }

    void ServerCore::shutdown()
    {
        if (!running)
        {
            return;
        }

        running = false;
        networkServer.shutdown();

        __blib_log_info("%s server core shut down after %llu ticks",
            gameTitle, static_cast<unsigned long long>(tickCounter));
    }

} // namespace gravelands
