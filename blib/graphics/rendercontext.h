#pragma once

#include <blib/config.h>

#include <blib/graphics/transform.h>
#include <blib/graphics/texture.h>
#include <blib/graphics/shader.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api RenderContext
        {
        public:
            Transform transform;
            const Texture* ptexture;
            const Shader* pshader;
        };
    }
}
