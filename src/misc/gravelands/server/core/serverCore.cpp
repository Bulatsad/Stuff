#include <gravelands/server/core/serverCore.h>

#include <beng/components/transform.h>

#include <blib/core/console/console.h>

namespace gravelands
{
    ServerCore::ServerCore()
        : accumulator(0.0f)
        , tickCounter(0)
        , lastHeartbeatSecond(0)
        , running(false)
    {
    }

    bool ServerCore::initialize()
    {
        // Регистрация движковых типов компонентов (пулы создаются лениво)
        scene.registerComponentType<beng::TransformComponent>();

        // Системы выполняются в порядке приоритета (ISystem::getPriority)
        scene.addSystem(&transformSystem);

        // Сброс точки отсчёта dt (первый tick даст корректное значение)
        time.tick();

        running = true;

        __blib_log_info("%s server core initialized (simulation tick rate: %u Hz, fixed delta: %.4f s)",
            gameTitle, serverTickRate, serverFixedDelta);

        return true;
    }

    void ServerCore::tick()
    {
        if (__blib_unlikely(!running))
        {
            return;
        }

        // Реальный dt текущего кадра (секунды)
        time.tick();
        accumulator += time.getDeltaTime();

        // Шагаем симуляцию фиксированными тиками; остаток остаётся
        // в аккумуляторе и догоняется следующими кадрами
        while (accumulator >= serverFixedDelta)
        {
            scene.update(serverFixedDelta);
            accumulator -= serverFixedDelta;

            ++tickCounter;

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

    void ServerCore::shutdown()
    {
        if (!running)
        {
            return;
        }

        running = false;

        __blib_log_info("%s server core shut down after %llu ticks",
            gameTitle, static_cast<unsigned long long>(tickCounter));
    }

} // namespace gravelands
