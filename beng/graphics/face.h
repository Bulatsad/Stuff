#pragma once

#include <vector>

#include <assimp/mesh.h>

#include <beng/config.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api Face
        {
        public:
            std::vector<size_t> indices;

            void loadFromAssimpFace(const aiFace* paiface);
        };
    }
}
