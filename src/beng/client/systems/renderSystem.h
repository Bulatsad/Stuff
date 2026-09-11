#pragma once

#include <beng/config.h>
#include <beng/core/system.h>

#include <blib/graphics/rendertarget.h>

namespace beng
{
    /**
     * RenderSystem — отрисовка сущностей со SkinnedMeshComponent.
     *
     * Пока система минимальная: просто рисует каждую модель через
     * IRenderTarget::draw(). В будущем обрастёт переключением
     * видимости, материалами, отсечением и т.д.
     *
     * Рендер-таргет задаётся вызывающим (viewer/эдитор), система
     * им не владеет.
     *
     * Приоритет 100: после всей симуляции, рисует финальное
     * состояние кадра.
     */
    class __beng_api RenderSystem : public beng::ISystem
    {
    private:
        blib::graphics::IRenderTarget* renderTarget;

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
