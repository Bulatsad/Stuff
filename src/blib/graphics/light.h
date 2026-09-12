#pragma once

#include <blib/config.h>

#include <blib/graphics/vector.h>

namespace blib
{
    namespace graphics
    {
        // ---------------------------------------------------------------
        // Свет для NPR-пайплайна (мягкий toon-шейдинг, фаза 4).
        //
        // Цвета — линейный RGB в диапазоне [0, 1] (float): в шейдер
        // уходит color * intensity. sRGB-преобразование — задача
        // пост-пасса (Фаза 6), до него рендер живёт в линейном
        // пространстве.
        // ---------------------------------------------------------------

        // Направленный свет (солнце). direction — направление ИЗ
        // источника К поверхности (в шейдере нормализуется).
        struct DirectionalLight
        {
            blib::graphics::Vector3f direction;
            blib::graphics::Vector3f color;
            float intensity;
        };

        // Эмбиент — ровная подсветка тёмных сторон
        struct AmbientLight
        {
            blib::graphics::Vector3f color;
            float intensity;
        };
    }
}
