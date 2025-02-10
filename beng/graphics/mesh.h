#pragma once

#include <vector>

#include <assimp/mesh.h>

#include <blib/graphics/vertex.h>
#include <blib/graphics/renderWindow.h>

#include <beng/config.h>
#include <beng/graphics/face.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api Mesh
        {
        private:
            
        public:
            std::vector<blib::graphics::Vertex>vertices;
            std::vector<blib::graphics::Vector3f>normals;
            std::vector<Face>faces;

            void loadFromAssimpMesh(const aiMesh* paimesh);

            void draw(blib::graphics::RenderWindow& wnd);
        };
    }
}
