#include <blib/graphics/sprite.h>

#include <Windows.h>
#include <gl/GL.h>

blib::graphics::Sprite::Sprite()
{
    this->pTexture = nullptr;
}

blib::graphics::Sprite::~Sprite()
{
}

void blib::graphics::Sprite::setTexture(const Texture& texture)
{
    this->pTexture = &texture;
}

void blib::graphics::Sprite::draw(RenderTarget& target, RenderContext& ctx) const
{
    glEnable(GL_TEXTURE_2D);

    glPushMatrix();
    {
        //auto const& transform = this->getTransform();
        //glMultMatrixf(transform.getMatrix());
        
        ctx.applyTransform(*this);

        //glRotatef(-this->getRotation().x, 1, 0, 0);
        //glRotatef(-this->getRotation().y, 0, 1, 0);
        //glRotatef(-this->getRotation().z, 0, 0, 1);
        //
        //glTranslatef(-(this->getPosition().x), -(this->getPosition().y), -(this->getPosition().z));

        glBindTexture(GL_TEXTURE_2D, *((GLuint*)this->pTexture->getContext()));

        glBegin(GL_QUADS);
        //glBegin(GL_TRIANGLE_STRIP);
        {
            float x0 =this->getPosition().x - this->getOrigin().x;
            float x1 =this->getPosition().x - this->getOrigin().x + this->pTexture->width;
            float y0 =this->getPosition().y - this->getOrigin().y;
            float y1 =this->getPosition().y - this->getOrigin().y + this->pTexture->height;

            glTexCoord3f(0, 0, 0); glVertex3f(x0, y0, 0);
            glTexCoord3f(0, 1, 0); glVertex3f(x0, y1, 0);
            glTexCoord3f(1, 1, 0); glVertex3f(x1, y1, 0);
            glTexCoord3f(1, 0, 0); glVertex3f(x1, y0, 0);
        }
        glEnd();
        
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glPopMatrix();
}
