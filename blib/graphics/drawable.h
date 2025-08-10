#pragma once

#include <blib/config.h>

#include <blib/graphics/rendercontext.h>
#include <blib/graphics/rendertarget.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api RenderTarget;

        class __blib_api IDrawable
        {
        public:
            virtual ~IDrawable() {}

        //protected:
            //friend class RenderTarget;

            virtual void draw(RenderTarget& target, RenderContext& ctx) const = 0;
        };
    }
}
