#pragma once

#include <beng/config.h>
#include <beng/core/system.h>

#include <blib/graphics/rendertarget.h>

namespace beng
{
    class __beng_api Scene;

    template<typename T>
    class ComponentPool;

    /**
     * LightSystem — применение света сцены к рендер-контексту.
     *
     * Свет описывается компонентами движка (см. directionalLightComponent.h /
     * ambientLightComponent.h): система находит первый АКТИВНЫЙ компонент
     * каждого типа и копирует его в RenderContext::directionalLight /
     * RenderContext::ambientLight. Компонентов нет — rc-дефолты не трогаются
     * (сцены без света, например вьювер, ведут себя как раньше).
     *
     * Ограничение (текущий RenderContext): по одному источнику каждого
     * типа; дубликаты не проверяются. Для deferred-шейдинга эта система —
     * точка сбора источников (сейчас — прямое применение в rc).
     *
     * Рендер-таргет задаётся вызывающим (как у RenderSystem), система
     * им не владеет. Приоритет 90: после симуляции, до RenderSystem
     * (100) — свет применяется до отрисовки кадра.
     */
    class __beng_api LightSystem : public beng::ISystem
    {
    private:
        blib::graphics::IRenderTarget* renderTarget;

    public:
        static constexpr bint32 priority = 90;

        LightSystem();

        /**
         * Привязать рендер-таргет (FBO вьюпорта). Без него система
         * ничего не делает (update() просто выходит).
         */
        void setRenderTarget(_In blib::graphics::IRenderTarget* target);

        void update(_In beng::Scene& scene, float deltaTime) override;
        bint32 getPriority() const override { return LightSystem::priority; }
        const char* getName() const override { return "LightSystem"; }
    };

} // namespace beng
