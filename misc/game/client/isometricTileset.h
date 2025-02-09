#pragma once
#include <blib/graphics/renderWindow.h>
#include <blib/graphics/romb.h>
#include <blib/graphics/vertex.h>

#include <vector>

namespace drawer
{
    class IsometricTileset
    {
    private:
        blib::graphics::RenderWindow* wnd = nullptr;
        std::vector<blib::graphics::Romb> tiles;
    public:
        IsometricTileset();

        void setWindow(blib::graphics::RenderWindow* _wnd);
        void draw();
        void intersect(const blib::graphics::vector2i& pos);

    };
}
