#pragma once

#include <blib/config.h>

#include <blib/graphics/color.h>
#include <blib/graphics/rendercontext.h>
#include <blib/graphics/drawable.h>

#include <blib/graphics/transformable.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api RenderTarget
        {
        public:
            virtual ~RenderTarget();

            void applyTransform(const Transformable& transform);

            void clear(const Color& color = Color::Black);
            //void draw(const IDrawable& drawable, RenderContext& ctx);
        };
    }
}