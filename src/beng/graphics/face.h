#pragma once

#include <vector>

#include <assimp/mesh.h>

#include <beng/config.h>

#include<blib/blibint.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api Face
        {
        public:
            std::vector<buint32> indices;

            void loadFromAssimpFace(const aiFace* paiface);
        };

        typedef std::vector<beng::graphics::Face> Faces;

        std::vector<buint32> compileFaces(const beng::graphics::Faces& faces);
    }
}
