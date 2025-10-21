#pragma once

#include <vector>

#include <assimp/scene.h>

#include <blib/graphics/drawable.h>
#include <blib/graphics/transformable.h>

#include <beng/config.h>
#include <blib/graphics/mesh.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api Model : public blib::graphics::IDrawable, public blib::graphics::Transformable
        {
        public:

            std::vector<blib::graphics::Mesh> meshes;

            void loadFromFile(const char* path);
            void parseFromAssimpScene(const aiScene* pscene, const std::string& filename);

            // Унаследовано через IDrawable
            virtual void draw(blib::graphics::RenderTarget& target, blib::graphics::RenderContext& ctx) const override;
        };
    }
}
