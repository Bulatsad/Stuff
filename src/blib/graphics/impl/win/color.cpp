#include <blib/graphics/color.h>

blib::graphics::Color::Color()
{
    this->red = 0;
    this->green = 0;
    this->blue = 0;
    this->alpha = 0;
}

blib::graphics::Color::Color(buint8 red, buint8 green, buint8 blue, buint8 alpha)
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
// BUG-FIX: раньше здесь был Color(0, 0, 0, 255) — «прозрачный» был
// чёрным непрозрачным (копипаст с BlackAlpha). Потребители
// (TGX/GM1-заливки, процедурные текстуры) рассчитывают на alpha = 0
const blib::graphics::Color blib::graphics::Color::Transparent = blib::graphics::Color(0, 0, 0, 0);

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
