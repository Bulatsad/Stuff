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
        // OrbitCamera — орбитальная камера, стандарт для вьюверов
        // моделей и эдиторов: вращается вокруг целевой точки,
        // дистанция меняется зумом, сдвиг цели — панорама.
        //
        // Модель управления:
        //   rotate(dAz, dEl)  — вращение (драг мышью по вьюпорту);
        //   zoom(delta)       — приближение/отдаление (колёсико);
        //   pan(dx, dy)       — панорама (средняя кнопка).
        //
        // View-матрица пересчитывается только в update(): вызывать
        // после любой группы изменений углов/дистанции, перед рендером.
        // Проекция — перспективная (аналог Camera::setPerpective).
        // ---------------------------------------------------------------
        class __blib_graphics_api OrbitCamera : public blib::graphics::ICamera
        {
        private:
            blib::graphics::Vector3f target;            // точка, вокруг которой вращаемся
            float distance;                             // дистанция до target
            blib::math::AngleDegreef azimuth;           // горизонтальный угол (градусы)
            blib::math::AngleDegreef elevation;         // вертикальный угол (градусы), кламп [-maxElevation, maxElevation]
            blib::graphics::Vector3f worldUp;

            blib::graphics::TransformMatrix projectionMatrix;
            blib::graphics::TransformMatrix viewMatrix;
            blib::graphics::Vector3f position;          // вычисленная позиция камеры

        public:
            OrbitCamera();

            void setTarget(_In const blib::graphics::Vector3f& newTarget);
            const blib::graphics::Vector3f& getTarget() const;

            void setDistance(float newDistance);
            float getDistance() const;

            // Вращение на дельты углов (градусы) — из мышиного драга
            void rotate(float deltaAzimuth, float deltaElevation);

            // Панорама: сдвиг target в плоскости камеры.
            // Дельта задаётся в мировых единицах смещения (масштаб —
            // на совести вызывающего, обычно пропорционально дистанции)
            void pan(float deltaX, float deltaY);

            // Зум: изменение дистанции (положительное — отдаление)
            void zoom(float deltaDistance);

            // Перспективная проекция (аналог Camera::setPerpective)
            void setPerspective(_In const blib::math::AngleDegreef& fov, float aspect, float nearDist, float farDist);

            // Пересчитывает позицию и view-матрицу из target/distance/углов
            void update();

            // ICamera
            const blib::graphics::TransformMatrix& getProjectionMatrix() const override;
            const blib::graphics::TransformMatrix& getViewMatrix() const override;
            const blib::graphics::Vector3f& getPosition() const;
        };
    }
}
