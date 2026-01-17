#pragma once

#include <vector>

#include <assimp/scene.h>

#include <blib/graphics/drawable.h>
#include <blib/graphics/transformable.h>
#include <blib/graphics/mesh.h>

#include <beng/config.h>

#include <functional>

namespace beng
{
    namespace graphics
    {
        class __beng_api Model : public blib::graphics::IDrawable, public blib::graphics::ITransformable
        {
        public:

            mutable std::vector<blib::graphics::Mesh> meshes;

            void loadFromFile(const char* path);
            void parseFromAssimpScene(const aiScene* pscene, const std::string& filename);

            static blib::graphics::Mesh bakeMeshes(const std::vector<blib::graphics::Mesh>& meshes);

            // Унаследовано через IDrawable
            virtual void draw(blib::graphics::RenderContext& ctx) const override;
        };
    }
}
