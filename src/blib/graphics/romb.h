#pragma once

#include <blib/config.h>

#include <blib/graphics/color.h>
#include <blib/graphics/vertex.h>
#include <blib/graphics/renderWindow.h>
#include <blib/blibint.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Romb
        {
        private:
            Color color;
        public:
            bint32 width;
            bint32 height;

            void setWidth(bint32 _width);
            void setHeight(bint32 _height);
            void setColor(const Color color);
            void draw(RenderWindow& wnd);
        };
    }
}