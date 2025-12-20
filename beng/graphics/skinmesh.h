#pragma once

#include <blib/graphics/mesh.h>

#include <beng/config.h>
#include <beng/graphics/skelet.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api SkinMesh
        {
        public:
            
            void loadFromAssimp(const aiScene* paiscene);

        private:
            std::vector<blib::graphics::Mesh> meshes;
            beng::graphics::Skelet skelet;
        };
    }
}
