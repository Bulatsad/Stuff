#include <blib/graphics/color.h>

blib::graphics::Color::Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    this->red = red;
    this->green = green;
    this->blue = blue;
    this->alpha = alpha;
}

const blib::graphics::Color blib::graphics::Color::Black = blib::graphics::Color(0, 0, 0, 0);
