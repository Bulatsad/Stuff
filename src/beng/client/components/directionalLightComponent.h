#pragma once

#include <beng/config.h>
#include <beng/core/icomponent.h>
#include <beng/core/componentReflection.h>

#include <blib/core/math/vector.h>

namespace beng
{
    /**
     * DirectionalLightComponent — направленный свет сцены («солнце»).
     *
     * Назначение:
     * - Сценный источник света движка: сущность с этим компонентом
     *   описывает направленный свет (зеркало blib::graphics::DirectionalLight);
     * - LightSystem (beng-client) копирует первый активный компонент
     *   в RenderContext::directionalLight перед отрисовкой — сцена без
     *   компонента оставляет дефолты RenderContext нетронутыми;
     * - Сериализуется со сценой (Scene::save/load) — свет переживает
     *   round-trip и сверяется strict-сравнением.
     *
     * Поля (как в blib::graphics::light.h):
     * - direction — направление ИЗ источника К поверхности
     *   (в шейдере нормализуется);
     * - color — линейный RGB [0,1]; в шейдер уходит color * intensity;
     * - intensity — множитель цвета.
     *
     * Ограничения (текущий RenderContext): один направленный свет —
     * LightSystem берёт первый активный компонент пула; дубликаты
     * не проверяются.
     *
     * Использование:
     *   scene.addComponent<DirectionalLightComponent>(lightEntity);
     */
    class __beng_api DirectionalLightComponent : public IComponent
    {
    private:
        blib::math::Vector<float, 3> direction;
        blib::math::Vector<float, 3> color;
        bfloat intensity;

    public:
        // Стабильное имя типа — идентичность типа в таблице типов Scene
        // (регистрация, резолв в шаблонных методах, save/load)
        static constexpr const char* componentTypeName = "beng.DirectionalLight";

        // Не прятать 1-аргументную точку входа строгого сравнения
        using blib::core::IStrongComparable::strongCompare;

        /**
         * Рефлексия компонента (контракт HasComponentReflection) —
         * см. componentReflection.h. Определение — в .cpp.
         *
         * Поля: direction, color (Vector3), intensity (Float).
         */
        static const ComponentTypeDescriptor& componentReflection();

        /**
         * Конструктор: дефолты как у RenderContext (мягкий тёплый
         * направленный свет — см. impl/rendercontext.cpp).
         */
        DirectionalLightComponent();

        ~DirectionalLightComponent() override = default;

        DirectionalLightComponent(const DirectionalLightComponent&) = delete;
        DirectionalLightComponent& operator=(const DirectionalLightComponent&) = delete;

        const blib::math::Vector<float, 3>& getDirection() const { return direction; }
        const blib::math::Vector<float, 3>& getColor() const { return color; }
        bfloat getIntensity() const { return intensity; }

        void setDirection(_In const blib::math::Vector<float, 3>& value) { direction = value; }
        void setColor(_In const blib::math::Vector<float, 3>& value) { color = value; }
        void setIntensity(bfloat value) { intensity = value; }

        // ========== ISaveLoadable: сериализация и сравнение ==========
        //
        // Формат save: JSON-объект {direction: [x,y,z], color: [x,y,z],
        // intensity, isActive}. Контекста нет — round-trip верифицируется
        // standalone (verify() = true).

        /**
         * Сохранить состояние компонента в поток (JSON-объект).
         */
        blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;

        /**
         * Загрузить состояние компонента из потока (JSON-объект).
         */
        blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;

        /**
         * Строгое (бит-в-бит) сравнение: базовые поля + все данные.
         */
        bool strongCompare(_In const blib::core::IStrongComparable& other,
            _In blib::core::CompareSession& session) const __blib_override;

        /**
         * Round-trip валидация (verifyRoundTrip).
         */
        bool verify() const __blib_override;
    };

} // namespace beng
