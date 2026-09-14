#pragma once

#include <blib/utilmacro.h>

#include <blib/graphics/mesh.h>

namespace gravelands
{
    /**
     * IsometricTileset — билдер меша сетки тайлов.
     * 
     * Назначение:
     * - Собирает меш сетки 10x10 квадратов на плоскости XZ одним
     *   Mesh (вершины, нормали, текстурные координаты, грани и
     *   шахматная текстура); «ромб» на экране — результат наклонной
     *   проекции изокамеры (blib::graphics::IsometricCamera), а не
     *   форма самого тайла
     * - Меш передаётся в ECS-рендер: вся отрисовка мира идёт через
     *   сцену (MeshRenderComponent, слой Ground) — см. BENG.md
     */
    class IsometricTileset
    {
    public:
        /**
         * Собрать меш сетки тайлов.
         * 
         * @return Меш (move-only): передаётся в рендер-компонент
         */
        static blib::graphics::Mesh buildMesh();
    };

} // namespace gravelands
