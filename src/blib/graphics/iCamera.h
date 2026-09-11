#pragma once

#include <blib/config.h>

#include <blib/graphics/transformMatrix.h>

namespace blib
{
    namespace graphics
    {
        // ---------------------------------------------------------------
        // ICamera — интерфейс камеры для рендера.
        //
        // RenderContext нужны только view/projection матрицы: какая
        // конкретно камера их считает (fly-cam Camera, орбитальная
        // OrbitCamera, изометрия...) — неважно. Интерфейс позволяет
        // подключать разные реализации без сцепки с классом Camera.
        // ---------------------------------------------------------------
        class __blib_graphics_api ICamera
        {
        public:
            virtual ~ICamera() = default;

            // View-матрица (камера → мир), пересчитывается при
            // каждом изменении параметров камеры
            virtual const blib::graphics::TransformMatrix& getViewMatrix() const = 0;

            // Проекционная матрица (перспектива/орто)
            virtual const blib::graphics::TransformMatrix& getProjectionMatrix() const = 0;
        };
    }
}
