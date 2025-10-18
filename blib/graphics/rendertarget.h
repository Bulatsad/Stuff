#pragma once

#include <blib/config.h>

#include <blib/graphics/drawable.h>
#include <blib/graphics/color.h>
#include <blib/graphics/rendercontext.h>

#include <blib/graphics/transformable.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api IDrawable;

        class __blib_api RenderTarget
        {
        public:
            virtual ~RenderTarget();

            RenderContext rc;

            void setCamera(const blib::graphics::Camera* a_pCamera);
            void clear(const Color& color = Color::Black);
            void draw(const blib::graphics::IDrawable& drawable);
        };
    }
}