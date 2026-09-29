#pragma once

#include <beng/config.h>
#include <beng/core/system.h>

namespace gravelands
{
    /**
     * MovementSystem — перемещение игровых юнитов по входному вектору
     * (авторитетная симуляция сервера).
     *
     * Назначение:
     * - Читает UnitComponent (moveX/moveZ, moveSpeed) и двигает
     *   TransformComponent юнита;
     * - Диагональ нормализуется (скорость не зависит от направления);
     * - Позиция клампится границами мира (worldBounds) на плоскости XZ.
     *
     * Приоритет 0: после TransformSystem (-100) — мировые матрицы
     * пересчитаны в начале кадра, движение применится до рендера/снапшота.
     */
    class MovementSystem : public beng::ISystem
    {
    public:
        void update(_In beng::Scene& scene, float deltaTime) __blib_override;

        bint32 getPriority() const __blib_override { return 0; }
        const char* getName() const __blib_override { return "MovementSystem"; }
    };

} // namespace gravelands
