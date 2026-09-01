#pragma once

#include <vector>

#include <assimp/mesh.h>

#include <blib/config.h>
#include <blib/blibint.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Face
        {
        public:
            std::vector<buint32> indices;

            void loadFromAssimpFace(const aiFace* paiface);
        };

        typedef std::vector<blib::graphics::Face> Faces;

        // bake indices for GL_ELEMENT_ARRAY_BUFFER
        std::vector<buint32> compileFaces(const blib::graphics::Faces& faces);
    }
}
