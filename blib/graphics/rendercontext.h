#pragma once

#include <blib/config.h>

#include <blib/graphics/transform.h>
#include <blib/graphics/texture.h>
#include <blib/graphics/shader.h>

#include <blib/graphics/opengl.h>


#include <blib/graphics/transformable.h>
#include <blib/math/matrix.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api RenderContext
        {
        public:
            Transform transform;

            blib::math::Matrix<float, 4, 4>VievMatrix;

            const Texture* ptexture;
            const Shader* pshader;

            RenderApi api;

            void applyTransform(const Transformable& transform);

        };
    }
}
