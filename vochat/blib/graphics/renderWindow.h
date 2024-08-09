#pragma once

#include <blib/config.h>

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

        class __blib_api RenderWindow
        {
        private:
            void* ctx;

            uint16_t width = 0;
            uint16_t height = 0;

        public:
            RenderWindow(uint16_t _width, uint16_t _height, const std::string& title, WindowStile style = WindowStile::None);
            uint16_t getHeight() const { return this->width; }
            uint16_t getWight() const { return this->height; }

            void enableIsometricTileGreed();

            void update();
            bool isOpen();
            void clear(const Color& color);
            void display();

            void* __getCtx();
        };
    }
}
