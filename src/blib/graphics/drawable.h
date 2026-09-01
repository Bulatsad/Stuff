#pragma once

#include <blib/config.h>

#include <blib/graphics/rendercontext.h>
#include <blib/graphics/rendertarget.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api IRenderTarget;

        class __blib_graphics_api IDrawable
        {
        public:
            virtual ~IDrawable() {}

        //protected:
            //friend class RenderTarget;

            virtual void draw(RenderContext& ctx) const = 0;
        };
    }
}
