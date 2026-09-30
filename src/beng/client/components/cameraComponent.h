#pragma once

#include <beng/config.h>
#include <beng/core/icomponent.h>
#include <beng/core/componentReflection.h>

namespace beng
{
    /**
     * CameraComponent — камера сцены (как Camera в Unity): сущность
     * с этим компонентом описывает камеру, «взгляд» которой задаёт
     * её TransformComponent (позиция/поворот; локальная ось +Z —
     * направление взгляда), а параметры проекции и разрешение кадра —
     * поля компонента.
     *
     * Назначение:
     * - Единая активная камера на сцену (инвариант поддерживает
     *   CameraSystem — см. cameraSystem.h): «активной» считается
     *   последняя включённая (setActive(true)) или последняя активная
     *   при загрузке файла сцены; прочие активные гасятся системой;
     * - Потребители: Game-превью эдитора (рендер сцены из активной
     *   камеры в свой FBO), стартовая камера клиента игры;
     * - Сериализуется со сценой (Scene::save/load) — камера переживает
     *   round-trip; штамп активации — контекст, не сериализуется;
     * - Рефлексия: fovDegrees/nearDistance/farDistance (Float),
     *   pixelWidth/pixelHeight (Int), active (Bool) — Inspector через
     *   стандартные поля.
     *
     * Поля:
     * - fovDegrees — вертикальный угол обзора перспективы (градусы);
     * - nearDistance/farDistance — плоскости отсечения;
     * - pixelWidth/pixelHeight — разрешение кадра камеры (Game-превью
     *   рендерится в FBO этого размера; аспект = w/h);
     * - active — флаг активности (см. «единая активная камера»).
     *
     * Ограничения:
     * - Валидность значений (fov > 0, near < far, w/h >= 1) проверяет
     *   потребитель — компонент хранит данные как есть;
     * - Штамп активации — статический счётчик процесса (однопоточный
     *   сценарий эдитора/клиента; см. .cpp).
     *
     * Использование:
     *   scene.addComponent<CameraComponent>(entity); // активна по умолчанию
     *   scene.addComponent<CameraComponent>(entity).setActive(true);
     */
    class __beng_api CameraComponent : public IComponent
    {
    private:
        // Угол обзора по вертикали (градусы)
        bfloat fovDegrees;
        // Ближняя/дальняя плоскости отсечения
        bfloat nearDistance;
        bfloat farDistance;
        // Разрешение кадра камеры (Game-превью/будущая игра)
        buint32 pixelWidth;
        buint32 pixelHeight;
        // Монотонный штамп активации: последний включившийся —
        // единственная активная камера (см. CameraSystem)
        buint64 activationStamp;

    public:
        // Стабильное имя типа — идентичность типа в таблице типов Scene
        // (регистрация, резолв в шаблонных методах, save/load)
        static constexpr const char* componentTypeName = "beng.Camera";

        // Не прятать 1-аргументную точку входа строгого сравнения
        using blib::core::IStrongComparable::strongCompare;

        /**
         * Рефлексия компонента (контракт HasComponentReflection) —
         * см. componentReflection.h. Определение — в .cpp.
         *
         * Поля: fovDegrees, nearDistance, farDistance (Float),
         * pixelWidth, pixelHeight (Int), active (Bool).
         */
        static const ComponentTypeDescriptor& componentReflection();

        /**
         * Конструктор: дефолты Unity-подобные — FOV 60°, near 0.1,
         * far 1000, кадр 1280x720 (аспект 16:9), камера активна.
         */
        CameraComponent();

        ~CameraComponent() override = default;

        CameraComponent(const CameraComponent&) = delete;
        CameraComponent& operator=(const CameraComponent&) = delete;

        bfloat getFovDegrees() const { return fovDegrees; }
        bfloat getNearDistance() const { return nearDistance; }
        bfloat getFarDistance() const { return farDistance; }
        buint32 getPixelWidth() const { return pixelWidth; }
        buint32 getPixelHeight() const { return pixelHeight; }
        bool getActive() const { return isActive; }

        void setFovDegrees(bfloat value) { fovDegrees = value; }
        void setNearDistance(bfloat value) { nearDistance = value; }
        void setFarDistance(bfloat value) { farDistance = value; }
        void setPixelWidth(buint32 value) { pixelWidth = value; }
        void setPixelHeight(buint32 value) { pixelHeight = value; }

        /**
         * Флаг активности. Включение ставит свежий штамп активации —
         * CameraSystem на ближайшем кадре погасит остальные активные
         * камеры сцены («последняя включённая побеждает»).
         */
        void setActive(bool active);

        /**
         * Штамп активации (монотонный счётчик процесса): больше —
         * «включена позже». Сравнивает CameraSystem при выборе
         * единственной активной камеры.
         */
        buint64 getActivationStamp() const { return activationStamp; }

        // ========== IComponent: восстановление контекста ==========

        /**
         * При загрузке сцены активная камера получает свежий штамп
         * (вызовы идут в порядке файла) — при нескольких активных
         * в файле побеждает ПОСЛЕДНЯЯ (консистентно с setActive).
         */
        void onLoaded(_In Scene& scene) __blib_override;

        // ========== ISaveLoadable: сериализация и сравнение ==========
        //
        // Формат save: JSON-объект {fovDegrees, nearDistance,
        // farDistance, pixelWidth, pixelHeight, isActive}. Штамп —
        // контекст, не сериализуется; round-trip верифицируется
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
