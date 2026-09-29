#include <gravelands/server/core/movementSystem.h>

#include <gravelands/common/config.h>
#include <gravelands/common/unitComponent.h>

#include <beng/components/transform.h>
#include <beng/core/componentPool.h>
#include <beng/core/scene.h>

#include <blib/core/math/utilfuncs.h>

namespace gravelands
{
    void MovementSystem::update(_In beng::Scene& scene, float deltaTime)
    {
        beng::ComponentPool<UnitComponent>* unitPool =
            scene.tryGetComponentPool<UnitComponent>();
        if (unitPool == nullptr)
        {
            return; // юнитов в сцене нет (или тип не зарегистрирован)
        }

        for (auto it = unitPool->begin(); it != unitPool->end(); ++it)
        {
            UnitComponent& unit = *it;
            const bint8 moveX = unit.getMoveX();
            const bint8 moveZ = unit.getMoveZ();
            if (moveX == 0 && moveZ == 0)
            {
                continue; // нет ввода — нет движения
            }

            beng::TransformComponent* transform =
                scene.tryGetComponent<beng::TransformComponent>(it.getEntityId());
            if (transform == nullptr)
            {
                continue;
            }

            // Направление: диагональ нормализуется (скорость постоянна)
            blib::math::Vector<float, 3> direction(
                static_cast<float>(moveX), 0.0f, static_cast<float>(moveZ));
            direction = blib::math::normalize(direction);

            blib::math::Vector<float, 3> position = transform->getLocalPosition();
            position = position + direction * (unit.getMoveSpeed() * deltaTime);

            // Кламп границами мира (квадрат worldBounds на XZ)
            if (position.x > worldBounds) position.x = worldBounds;
            if (position.x < -worldBounds) position.x = -worldBounds;
            if (position.z > worldBounds) position.z = worldBounds;
            if (position.z < -worldBounds) position.z = -worldBounds;

            transform->setLocalPosition(position);
        }
    }

} // namespace gravelands
