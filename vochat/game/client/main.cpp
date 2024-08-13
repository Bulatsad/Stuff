#include <blib/graphics/image.h>
#include <blib/graphics/texture.h>
#include <blib/graphics/sprite.h>
#include <blib/graphics/camera.h>
#include <blib/graphics/renderWindow.h>
#include <blib/graphics/romb.h>
#include <blib/graphics/mouse.h>
#include <blib/graphics/rectangle.h>

#include "isometricTileset.h"

int main()
{
    auto rw = blib::graphics::RenderWindow(1800, 1000, "test");

    blib::graphics::Camera camera;
    camera.setProjectionMode(blib::graphics::Camera::ProjectionMode::Ortho, rw);
    camera.setPosition({ 0,0,0 });
    auto cp = camera.getPosition();

    blib::graphics::Rectangle rect;
    rect.setPosition({ 0,0,-1 });
    rect.setWidth(30);
    rect.setHeight(16);
    rect.setOrigin({ 0,0,0 });
    rect.setColor(blib::graphics::Color::White);

    drawer::IsometricTileset ism;
    ism.setWindow(&rw);

    blib::graphics::Mouse ms;
    ms.setPosition(rw, { 500,500 });

    while (rw.isOpen())
    {
        rw.clear(blib::graphics::Color::Black);
        rw.update();

        auto mspos = ms.getPosition(rw);
        mspos.x -= rw.getWight() / 2;
        mspos.y -= rw.getHeight() / 2;
        mspos.y = -mspos.y;

        rect.setPosition({ (float)mspos.x * 0.5000f,(float)mspos.y * 0.5000f,-1000 });

        printf("%d %d\n", mspos.x, mspos.y);

        rect.draw(rw);
        ism.draw();

        camera.display(rw);
        //rw.display();
    }

    return 0;
}
