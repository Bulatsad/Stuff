#pragma once

#include <beng/config.h>
#include <beng/core/system.h>

#include <beng/client/components/meshRenderComponent.h>

#include <blib/graphics/rendertarget.h>

namespace beng
{
    class __beng_api MeshRenderComponent;

    template<typename T>
    class ComponentPool;

    class __beng_api Scene;

    /**
     * RenderSystem — единая точка отрисовки мира (ECS):
     *   создать сцену → добавить объекты на сцену → scene.update().
     *
     * Рисует в фиксированном порядке слоёв (см. RenderLayer):
     *   Ground → Shadow (blend, без записи глубины) →
     *   AlphaTested → Opaque (включая SkinnedMeshComponent).
     * Трансформации — из TransformComponent сущностей.
     *
     * Рендер-таргет задаётся вызывающим (клиент/эдитор), система
     * им не владеет. Пулы берутся через tryGetComponentPool —
     * сцены без MeshRenderComponent (вьювер) работают как раньше.
     *
     * Приоритет 100: после всей симуляции, рисует финальное
     * состояние кадра.
     */
    class __beng_api RenderSystem : public beng::ISystem
    {
    private:
        blib::graphics::IRenderTarget* renderTarget;

        // Отрисовать один слой статических мешей (см. update)
        void drawLayer(
            _In beng::Scene& scene,
            _In beng::ComponentPool<MeshRenderComponent>& meshPool,
            RenderLayer layer);

    public:
        static constexpr bint32 priority = 100;

        RenderSystem();

        /**
         * Привязать рендер-таргет (FBO вьюпорта). Без него система
         * ничего не рисует (update() просто выходит).
         */
        void setRenderTarget(_In blib::graphics::IRenderTarget* target);

        void update(_In beng::Scene& scene, float deltaTime) override;
        bint32 getPriority() const override { return RenderSystem::priority; }
        const char* getName() const override { return "RenderSystem"; }
    };

} // namespace beng
