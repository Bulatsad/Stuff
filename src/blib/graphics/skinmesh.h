#pragma once

#include <assimp/scene.h>

#include <blib/config.h>
#include <blib/core/isaveloadable.h>
#include <blib/core/json/json.h>
#include <blib/graphics/mesh.h>
#include <blib/graphics/skelet.h>

namespace blib
{
    namespace graphics
    {
        /**
         * SkinMesh — скиннутый меш: обычный Mesh (геометрия + веса
         * костей в boneIds/boneWeights + материал), сериализуется
         * целиком делегированием Mesh (ISaveLoadable).
         */
        class __blib_graphics_api SkinMesh : public blib::core::ISaveLoadable
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

            // ========== ISaveLoadable: сериализация и сравнение ==========
            //
            // Делегирует мешу (формат = формат Mesh::toJson). Семантика
            // fromJson как у Mesh: destroy + placement new, перезапись
            // запечённого меша требует живого RenderContext.
            // Не прятать 1-аргументную точку входа строгого сравнения.

            using blib::core::IStrongComparable::strongCompare;

            blib::core::json::JsonValue toJson() const;
            blib::core::LoadStatus fromJson(_In const blib::core::json::JsonValue& json);
            blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;
            blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;
            bool strongCompare(_In const blib::core::IStrongComparable& other,
                _In blib::core::CompareSession& session) const __blib_override;
            bool verify() const __blib_override;
        };
    }
}

