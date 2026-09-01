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
            bool loadFromAssimpMesh(const aiMesh* paimesh, const blib::graphics::Skelet& skelet);

            mutable blib::graphics::Mesh mesh;
        };
    }
}
