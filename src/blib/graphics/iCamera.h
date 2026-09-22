#pragma once

#include <blib/config.h>

#include <blib/graphics/transformMatrix.h>
#include <blib/graphics/vector.h>

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
            virtual const blib::graphics::TransformMatrix& getViewMatrix() const __blib_pure_virtual_function;

            // Проекционная матрица (перспектива/орто)
            virtual const blib::graphics::TransformMatrix& getProjectionMatrix() const __blib_pure_virtual_function;

            // Позиция камеры в мире (нужна шейдерам: rim-light,
            // туман, спекуляр). Все реализации (Camera, OrbitCamera,
            // IsometricCamera) её имеют
            virtual const blib::graphics::Vector3f& getPosition() const __blib_pure_virtual_function;
        };
    }
}
