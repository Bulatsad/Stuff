#pragma once

#include <blib/utilmacro.h>

#include <blib/graphics/mesh.h>
#include <blib/graphics/rendertarget.h>

namespace gravelands
{
    /**
     * IsometricTileset — сетка изометрических тайлов-ромбов.
     * 
     * Назначение:
     * - Рендерит сетку 10x10 ромбов одним Mesh (вершины, текстурные
     *   координаты и грани собираются в конструкторе)
     * - draw() отдаёт меш в рендер-таргет
     * 
     * Временная визуализация мира: позже тайлы переедут в ECS
     * (рендер-компоненты beng-client), см. ARCHITECTURE.md.
     */
    class IsometricTileset
    {
    public:
        IsometricTileset();

        /**
         * Нарисовать сетку в рендер-таргет.
         * 
         * @param target Рендер-таргет (владеет контекстом камеры)
         */
        void draw(_In blib::graphics::IRenderTarget& target);

    private:
        // Собранный меш сетки (белые ромбы, диффуз 1x1)
        blib::graphics::Mesh mesh;
    };

} // namespace gravelands
