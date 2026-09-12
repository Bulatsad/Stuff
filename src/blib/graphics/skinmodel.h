#pragma once

#include <vector>

#include <assimp/scene.h>

#include <blib/config.h>
#include <blib/graphics/drawable.h>
#include <blib/graphics/transformable.h>
#include <blib/graphics/skinmesh.h>
#include <blib/graphics/skelet.h>
#include <blib/graphics/animator.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api SkinModel : public blib::graphics::IDrawable, public blib::graphics::ITransformable
        {
        public:
            bool loadFromAssimp(const aiScene* paiscene, const std::string& filename = std::string(), const aiScene* panimationScene = nullptr);
            void update(float deltaTimeMs);

            bool selectAnimation(const std::string& animationName);
            bool playAnimation();

            /**
             * Подмена мешей (skin) у существующего скелета. Новый файл
             * обязан содержать полностью совпадающий скелет
             * (Skelet::isCompatibleWith), иначе отказ. Скелет, аниматор
             * и состояние плейбека не трогаются; при ошибке модель
             * остаётся без изменений.
             *
             * @param force true — при несовместимом скелете не
             *        отказывать, а загружать как есть: веса костей,
             *        отсутствующих в текущем скелете, отбрасываются,
             *        остальные ренормализуются (SkinMesh::loadFromAssimpMesh)
             */
            bool replaceMeshesFromAssimp(_In const aiScene* paiscene, _In const std::string& filename, _In bool force = false);

            blib::graphics::Skelet& getSkelet();
            const blib::graphics::Skelet& getSkelet() const;
            blib::graphics::Animator& getAnimator();
            const blib::graphics::Animator& getAnimator() const;

            // IDrawable
            virtual void draw(blib::graphics::RenderContext& ctx) const override;

        private:
            mutable std::vector<blib::graphics::SkinMesh> meshes;
            blib::graphics::Skelet skelet;
            blib::graphics::Animator animator;
        };
    }
}
