#pragma once

#include <vector>
#include <string>

#include <assimp/scene.h>

#include <blib/graphics/transformMatrix.h>

#include <beng/graphics/hierarchal.h>
#include <beng/config.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api Bone : public beng::graphics::IHierarchal
        {
        public:

            bool loadFromAssimp(const aiBone* pbone);

            std::string name;
            std::vector<std::pair<size_t/*VertexId*/, float /*Weight*/> >weights;
            blib::graphics::TransformMatrix offsetMatrix;
        };
    }
}
