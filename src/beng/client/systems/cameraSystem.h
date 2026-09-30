#pragma once

#include <beng/config.h>
#include <beng/core/system.h>

namespace beng
{
    class __beng_api Scene;

    template<typename T>
    class ComponentPool;

    /**
     * CameraSystem — инвариант «одна активная камера на сцену».
     *
     * Назначение:
     * - Находит среди АКТИВНЫХ камер сцены (CameraComponent) «последнюю
     *   включённую» (максимальный activationStamp — см.
     *   cameraComponent.h) и гасит остальные активные;
     * - Активных камер нет — система ничего не делает (потребители
     *   должны сами решать, что показывать);
     * - Штампы равны (теоретически возможная гонка/ручная правка
     *   isActive) — побеждает первая в порядке пула (детерминизм).
     *
     * Порядок и производительность:
     * - Приоритет -150: до TransformSystem (-100) — ни одна система
     *   кадра не видит «промежуточного» состояния с двумя активными
     *   камерами;
     * - Два прохода по активным камерам пула: камер в сцене —
     *   единицы, стоимость пренебрежима. Сцены без пула камер —
     *   ранний выход.
     *
     * Потребители активной камеры (Game-превью эдитора, старт клиента)
     * читают её ПОСЛЕ scene.update() — нормализация уже выполнена.
     *
     * Систему на сцену вешает владелец мира (gravelands::World):
     * сцены без камер (сервер) систему не добавляют.
     */
    class __beng_api CameraSystem : public beng::ISystem
    {
    public:
        static constexpr bint32 priority = -150;

        CameraSystem() = default;

        void update(_In beng::Scene& scene, float deltaTime) override;
        bint32 getPriority() const override { return CameraSystem::priority; }
        const char* getName() const override { return "CameraSystem"; }
    };

} // namespace beng
