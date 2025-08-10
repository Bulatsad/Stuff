#include <blib/graphics/rendertarget.h>

#include <Windows.h>
#include <gl/GL.h>

blib::graphics::RenderTarget::~RenderTarget()
{
}

void blib::graphics::RenderTarget::clear(const Color& color)
{
    //this->rc.api.ogl.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //this->rc.api.ogl.glClearColor(
    //    (color.red) / static_cast<float>(255),
    //    color.green / static_cast<float>(255),
    //    color.blue / static_cast<float>(255),
    //    color.alpha / static_cast<float>(255)
    //);
    //this->rc.api.ogl.__blib_glPushMatrix();
}

void blib::graphics::RenderTarget::draw(const blib::graphics::IDrawable& drawable)
{
    drawable.draw(*this, rc);
}
