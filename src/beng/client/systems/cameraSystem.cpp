#include <beng/client/systems/cameraSystem.h>

#include <beng/client/components/cameraComponent.h>
#include <beng/core/componentPool.h>
#include <beng/core/scene.h>

namespace beng
{
    void CameraSystem::update(_In beng::Scene& scene, float deltaTime)
    {
        // Система нормализует активность камер, но сама их не
        // использует — dt не нужен (единая сигнатура ISystem)
        (void)deltaTime;

        beng::ComponentPool<CameraComponent>* pool =
            scene.tryGetComponentPool<CameraComponent>();
        if (__blib_unlikely(pool == nullptr))
        {
            return;
        }

        // Проход 1: активные камеры (итератор пула пропускает
        // неактивные) — «последняя включённая» = максимальный штамп;
        // при равенстве побеждает первая в порядке пула
        CameraComponent* winner = nullptr;
        for (auto it = pool->begin(); it != pool->end(); ++it)
        {
            CameraComponent& camera = *it;
            if (winner == nullptr || camera.getActivationStamp() > winner->getActivationStamp())
            {
                winner = &camera;
            }
        }

        if (__blib_unlikely(winner == nullptr))
        {
            return;
        }

        // Проход 2: гасим остальные активные камеры. Переключение
        // isActive влияет на обход итератора «вживую» — гасим только
        // ТЕКУЩУЮ камеру, обход корректен (см. IteratorBase)
        for (auto it = pool->begin(); it != pool->end(); ++it)
        {
            CameraComponent& camera = *it;
            if (&camera != winner)
            {
                camera.setActive(false);
            }
        }
    }

} // namespace beng
