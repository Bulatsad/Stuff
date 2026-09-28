#pragma once

#include <blib/utilmacro.h>

#include <blib/graphics/mesh.h>

namespace gravelands
{
    /**
     * IsometricTileset — билдер меша сетки тайлов (часть мира Gravelands).
     * 
     * Назначение:
     * - Собирает меш сетки 10x10 квадратов на плоскости XZ одним
     *   Mesh (вершины, нормали, текстурные координаты, грани и
     *   шахматная текстура); «ромб» на экране — результат наклонной
     *   проекции изокамеры (blib::graphics::IsometricCamera), а не
     *   форма самого тайла
     * - Меш передаётся в ECS-рендер: вся отрисовка мира идёт через
     *   сцену (MeshRenderComponent, слой Ground) — см. BENG.md.
     *   buildMeshInto собирает ПРЯМО в слот кеша ресурсов сцены
     *   (Mesh move-присваивание удалено — на месте не перезалить)
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

        /**
         * Собрать сетку тайлов ПРЯМО в существующий меш (слот кеша
         * ресурсов): вершинные буферы и грани перезаливаются на месте.
         * 
         * @param outMesh Приёмник (обычно пустой слот ResourceManager)
         */
        static void buildMeshInto(_Out blib::graphics::Mesh& outMesh);
    };

} // namespace gravelands
