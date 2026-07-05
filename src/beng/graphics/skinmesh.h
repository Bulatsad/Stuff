#pragma once

#include <beng/config.h>

#include <blib/graphics/mesh.h>
#include <blib/graphics/drawable.h>
#include <blib/graphics/transformable.h>

#include <beng/graphics/skelet.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api SkinMesh : public blib::graphics::IDrawable, public blib::graphics::ITransformable
        {
        public:
            void loadFromAssimp(const aiScene* paiscene);

            void draw(blib::graphics::RenderContext & ctx) const override;

        private:
            std::vector<blib::graphics::Mesh> meshes;
            beng::graphics::Skelet skelet;
        };
    }
}