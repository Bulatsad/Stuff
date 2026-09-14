#pragma once

#include <beng/config.h>
#include <beng/core/system.h>

namespace beng
{
    /**
     * BlobShadowSystem — ведёт blob-тени за анимируемыми моделями.
     *
     * Каждый кадр для каждой BlobShadowComponent:
     * 1. позиция кости цели (root-motion анимации, model-space);
     * 2. мировая позиция = position + rotate(scale * bone, rotation)
     *    (через API TransformComponent — Matrix::operator* не
     *    использовать, см. CORE.md);
     * 3. тень ставится СТРОГО под цель: (world.x, groundOffset,
     *    world.z) — blob-тень по определению лежит под объектом,
     *    без проекции от источника света;
     * 4. запись в TransformComponent сущности-тени (RenderSystem
     *    подхватит через dirty-флаг).
     *
     * Приоритет 50: после AnimationSystem (-50, поза продвинута),
     * до RenderSystem (100, отрисовка).
     */
    class __beng_api BlobShadowSystem : public beng::ISystem
    {
    public:
        static constexpr bint32 priority = 50;

        BlobShadowSystem();

        void update(_In beng::Scene& scene, float deltaTime) override;
        bint32 getPriority() const override { return BlobShadowSystem::priority; }
        const char* getName() const override { return "BlobShadowSystem"; }
    };

} // namespace beng
