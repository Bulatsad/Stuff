#include <blib/graphics/sprite.h>

#include <Windows.h>
#include <gl/GL.h>

blib::graphics::Sprite::Sprite()
{
    memset(&(this->transform), 0, sizeof(this->transform));

    this->pTexture = NULL;

    this->transform.scale.x = 1.f;
    this->transform.scale.y = 1.f;

    this->verties[0].textureCoord.x = 0;
    this->verties[0].textureCoord.y = 0;

    this->verties[1].textureCoord.x = 1;
    this->verties[1].textureCoord.y = 0;

    this->verties[2].textureCoord.x = 1;
    this->verties[2].textureCoord.y = 1;

    this->verties[3].textureCoord.x = 0;
    this->verties[3].textureCoord.y = 1;

    this->updateCache = true;
    this->pTexture = nullptr;
}

blib::graphics::Sprite::~Sprite()
{
}

void blib::graphics::Sprite::setTexture(const Texture& texture)
{
    this->pTexture = &texture;
}

void blib::graphics::Sprite::setPosition(const Vector3f& _position)
{
    this->transform.position = _position;
}

blib::graphics::Vector3f blib::graphics::Sprite::getPosition() const
{
    return this->transform.position;
}

void blib::graphics::Sprite::setRotation(const Vector3f& _rotation)
{
    this->transform.rotation = _rotation;
}

blib::graphics::Vector3f blib::graphics::Sprite::getRotation() const
{
    return this->transform.rotation;
}

void blib::graphics::Sprite::setScale(const Vector3f& _scale)
{
    this->transform.scale.x = _scale.x;
    this->transform.scale.x = _scale.y;
    this->transform.scale.x = _scale.z;
}

blib::graphics::Vector3f blib::graphics::Sprite::getScale() const
{
    return this->transform.scale;
}

void blib::graphics::Sprite::setOrigin(const Vector3f& _origin)
{
    this->transform.origin.x = _origin.x;
    this->transform.origin.y = _origin.y;
    this->transform.origin.z = _origin.z;
}

blib::graphics::Vector3f blib::graphics::Sprite::getOrigin() const
{
    return this->transform.origin;
}

void blib::graphics::Sprite::Move(const Vector3f& _position)
{
    this->transform.position += _position;
}

void blib::graphics::Sprite::Rotate(const Vector3f& _rotatation)
{
    this->transform.rotation += _rotatation;
}

void blib::graphics::Sprite::draw(RenderWindow& window)
{
    glEnable(GL_TEXTURE_2D);

    glPushMatrix();
    {
        //glLoadIdentity();

        glTranslatef(
            this->transform.position.x,
            this->transform.position.y,
            this->transform.position.z
        );

        glRotatef(this->transform.rotation.x, 1, 0, 0);
        glRotatef(this->transform.rotation.y, 0, 1, 0);
        glRotatef(this->transform.rotation.z, 0, 0, 1);
        
        glScalef(this->transform.scale.x, this->transform.scale.y, this->transform.scale.z);

        glBegin(GL_TRIANGLE_STRIP);
        {
            float x0 = this->transform.position.x - this->transform.origin.x;
            float x1 = this->transform.position.x - this->transform.origin.x + this->pTexture->width;
            float y0 = this->transform.position.y - this->transform.origin.y;
            float y1 = this->transform.position.y - this->transform.origin.y + this->pTexture->height;

            glTexCoord3f(0, 0, 0); glVertex3f(x0, y0, 0);
            glTexCoord3f(0, 1, 0); glVertex3f(x0, y1, 0);
            glTexCoord3f(1, 0, 0); glVertex3f(x1, y0, 0);
            glTexCoord3f(1, 1, 0); glVertex3f(x1, y1, 0);
        }
        glEnd();

        GLfloat m[16];
        glGetFloatv(GL_PROJECTION_MATRIX, m);
        printf("spritePROJECTION\n");
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                printf("%f ", m[i * 4 + j]);
            }
            printf("\n");
        }
        printf("\n");
        printf("\n");
        printf("\n");
    }
    glPopMatrix();

}