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
        class __blib_api SkinModel : public blib::graphics::IDrawable, public blib::graphics::ITransformable
        {
        public:
            bool loadFromAssimp(const aiScene* paiscene, const std::string& filename = std::string(), const aiScene* panimationScene = nullptr);
            void update(float deltaTimeMs);

            bool selectAnimation(const std::string& animationName);
            bool playAnimation();

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
