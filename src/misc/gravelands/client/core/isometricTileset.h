#pragma once

#include <blib/utilmacro.h>

#include <blib/graphics/mesh.h>
#include <blib/graphics/rendertarget.h>

namespace gravelands
{
    /**
     * IsometricTileset — сетка тайлов-квадратов на плоскости XZ.
     * 
     * Назначение:
     * - Рендерит сетку 10x10 квадратов одним Mesh (вершины, нормали,
     *   текстурные координаты и грани собираются в конструкторе);
     *   «ромб» на экране — результат наклонной проекции изокамеры
     *   (blib::graphics::IsometricCamera), а не форма самого тайла
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
        // Собранный меш сетки (квадраты на земле, шахматная текстура)
        blib::graphics::Mesh mesh;
    };

} // namespace gravelands
