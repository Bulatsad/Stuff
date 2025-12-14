#include <blib/graphics/color.h>

blib::graphics::Color::Color()
{
    this->red = 0;
    this->green = 0;
    this->blue = 0;
    this->alpha = 0;
}

blib::graphics::Color::Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    this->red = red;
    this->green = green;
    this->blue = blue;
    this->alpha = alpha;
}

const blib::graphics::Color blib::graphics::Color::Black = blib::graphics::Color(0, 0, 0, 0);
const blib::graphics::Color blib::graphics::Color::BlackAlpha = blib::graphics::Color(0, 0, 0, 255);
const blib::graphics::Color blib::graphics::Color::White = blib::graphics::Color(255, 255, 255, 0);
const blib::graphics::Color blib::graphics::Color::Red = blib::graphics::Color(255, 0, 0, 0);
const blib::graphics::Color blib::graphics::Color::Transparent = blib::graphics::Color(0, 0, 0, 255);

std::vector<float> blib::graphics::makeFloatData(const Colors& colors)
{
    std::vector<float>res;
    res.reserve(colors.size() * 4);
    for (const auto& color : colors)
    {
        res.push_back(color.red / 255);
        res.push_back(color.green / 255);
        res.push_back(color.blue / 255);
        res.push_back(color.alpha / 255);
    }
    return res;
}
