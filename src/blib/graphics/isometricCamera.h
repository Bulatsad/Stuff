#pragma once

#include <blib/config.h>
#include <blib/utilmacro.h>

#include <blib/core/math/angle.h>
#include <blib/graphics/iCamera.h>
#include <blib/graphics/transformMatrix.h>
#include <blib/graphics/vector.h>

namespace blib
{
    namespace graphics
    {
        // ---------------------------------------------------------------
        // IsometricCamera — фиксированная наклонная камера для
        // «изометрического» ракурса (Hades-подобный вид): наклон
        // и азимут зафиксированы, камера следует за целевой точкой
        // и может только зуммироваться. Свободного вращения нет
        // намеренно — стилизация (свет, контуры, туман) опирается
        // на константное направление взгляда.
        //
        // Управление извне:
        //   setTarget(...)   — куда смотреть (позиция персонажа);
        //   moveTarget(...)  — сдвиг цели в мировых координатах;
        //   zoom(delta)      — приближение/отдаление (дистанция);
        //   setPitch/setYaw  — конфигурация ракурса (до старта игры);
        //   update()         — пересчёт позиции и view-матрицы.
        //
        // Проекция — перспективная с малым FOV: лёгкая перспектива —
        // отличительная черта Hades, а не чистый ортографический вид.
        // ---------------------------------------------------------------
        class __blib_graphics_api IsometricCamera : public blib::graphics::ICamera
        {
        private:
            blib::graphics::Vector3f target;            // точка, за которой следует камера
            float distance;                             // дистанция до target
            blib::math::AngleDegreef yaw;               // азимут вокруг вертикали (фиксирован)
            blib::math::AngleDegreef pitch;             // наклон к горизонту (фиксирован)
            blib::graphics::Vector3f worldUp;

            blib::graphics::TransformMatrix projectionMatrix;
            blib::graphics::TransformMatrix viewMatrix;
            blib::graphics::Vector3f position;          // вычисленная позиция камеры

        public:
            IsometricCamera();

            void setTarget(_In const blib::graphics::Vector3f& newTarget);
            const blib::graphics::Vector3f& getTarget() const;

            // Сдвиг цели в мировых координатах (следование за персонажем)
            void moveTarget(_In const blib::graphics::Vector3f& delta);

            void setDistance(float newDistance);
            float getDistance() const;

            // Конфигурация ракурса: наклон к горизонту (0 — горизонтально,
            // 90 — строго вниз) и азимут поворота вокруг вертикали.
            // По умолчанию: pitch 55, yaw 45 — «ромбическая» изометрия
            void setPitch(_In const blib::math::AngleDegreef& pitchAngle);
            void setYaw(_In const blib::math::AngleDegreef& yawAngle);

            // Зум: изменение дистанции (положительное — отдаление)
            void zoom(float deltaDistance);

            // Перспективная проекция с малым FOV (лёгкая перспектива)
            void setPerspective(_In const blib::math::AngleDegreef& fov, float aspect, float nearDist, float farDist);

            // Пересчитывает позицию и view-матрицу из target/distance/углов.
            // Вызывать после любых изменений, перед рендером
            void update();

            // ICamera
            const blib::graphics::TransformMatrix& getProjectionMatrix() const override;
            const blib::graphics::TransformMatrix& getViewMatrix() const override;
            const blib::graphics::Vector3f& getPosition() const;
        };
    }
}
