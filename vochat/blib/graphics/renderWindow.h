#pragma once

#include <stdint.h>
#include <string>

#include <blib/graphics/color.h>

namespace blib
{
    namespace graphics
    {
        enum class WindowStile
        {
            None,
            Fullscreen,
            Close,
            Resize,

            END_OF_ENUM
        };

        class RenderWindow
        {
        private:
            void* ctx;
        public:
            RenderWindow(uint16_t width, uint16_t height, const std::string& title, WindowStile style = WindowStile::None);
            void update();
            bool isOpen();
            void clear(const Color& color);
            void display();
        };
    }
}
