#include <blib/graphics/rendertarget.h>

#include <Windows.h>
#include <gl/GL.h>

blib::graphics::RenderTarget::~RenderTarget()
{
}

void blib::graphics::RenderTarget::applyTransform(const Transformable& transformable)
{
        glRotatef(-transformable.getRotation().x, 1, 0, 0);
        glRotatef(-transformable.getRotation().y, 0, 1, 0);
        glRotatef(-transformable.getRotation().z, 0, 0, 1);
        
        glTranslatef(-(transformable.getPosition().x), -(transformable.getPosition().y), -(transformable.getPosition().z));

        glScalef(transformable.getScale().x, transformable.getScale().y, transformable.getScale().z);
}

void blib::graphics::RenderTarget::clear(const Color& color)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(
        (color.red) / static_cast<float>(255),
        color.green / static_cast<float>(255),
        color.blue / static_cast<float>(255),
        color.alpha / static_cast<float>(255)
    );
    glPushMatrix();
}

//void blib::graphics::RenderTarget::draw(const blib::graphics::IDrawable& drawable, blib::graphics::RenderContext& ctx)
//{
//    drawable.draw(*this, ctx);
//}
