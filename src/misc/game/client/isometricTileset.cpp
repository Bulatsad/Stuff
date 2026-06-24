#include "isometricTileset.h"


drawer::IsometricTileset::IsometricTileset()
{
    this->wnd = nullptr;

    for (float x = 0; x / 15 < 10; x += 15)
    {
        for (float y = 0; y / 8 < 10; y += 8)
        {
            blib::graphics::Romb romb;
            romb.setPosition({ x,y,-1000 });
            romb.setWidth(30);
            romb.setHeight(16);
            romb.setOrigin({ 0,0,0 });
            romb.setColor(blib::graphics::Color::White);

            this->tiles.push_back(romb);
        }
    }
}

void drawer::IsometricTileset::setWindow(blib::graphics::RenderWindow* _wnd)
{
    this->wnd = _wnd;
}

void drawer::IsometricTileset::draw()
{
    for (auto& tile : this->tiles)
    {
        tile.draw(*(this->wnd));
    }
}

void drawer::IsometricTileset::intersect(const blib::graphics::vector2i& pos)
{
    auto intersect = [](const blib::graphics::Transform3f& transform) -> bool
    {
        
    };
}
