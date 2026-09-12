#pragma once

#include <assimp/scene.h>

#include <blib/config.h>
#include <blib/graphics/mesh.h>
#include <blib/graphics/skelet.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api SkinMesh
        {
        public:
            /**
             * Раскладка весов костей по вершинам против заданного
             * скелета. Кость меша ищется в скелете по имени; при
             * отсутствии:
             * - обычный режим — ошибка (меш не грузится);
             * - force = true — веса переносятся на ближайшего предка
             *   из candidateSkelet, существующего в текущем скелете
             *   (у Mixamo, например, ленты-волосы висят на Head);
             *   если подходящего предка нет — веса отбрасываются, а
             *   оставшиеся веса затронутых вершин ренормализуются к
             *   сумме 1 (вершины с нулевой суммой остаются нулевыми).
             * candidateSkelet — скелет файла-источника меша (нужен
             * только в force-режиме для обхода иерархии предков).
             */
            bool loadFromAssimpMesh(
                _In const aiMesh* paimesh,
                _In const blib::graphics::Skelet& skelet,
                _In bool force = false,
                _In_opt const blib::graphics::Skelet* candidateSkelet = nullptr);

            mutable blib::graphics::Mesh mesh;
        };
    }
}
