#pragma once

#include <vector>
#include <string>

#include <assimp/scene.h>

#include <blib/config.h>
#include <blib/graphics/transformMatrix.h>

#include <blib/graphics/hierarchal.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Bone : public blib::graphics::IHierarchal
        {
        public:

            bool loadFromAssimp(const aiBone* pbone);

            std::string name;
            std::vector<std::pair<size_t/*VertexId*/, float /*Weight*/> >weights;
            blib::graphics::TransformMatrix offsetMatrix;
            blib::graphics::TransformMatrix localTransform;
            blib::graphics::TransformMatrix globalTransform;
        };
    }
}
