#include <blib/graphics/camera.h>

#include <Windows.h>
#include <gl/GL.h>

blib::graphics::Camera::Camera()
{
}

blib::graphics::Camera::~Camera()
{
}

void blib::graphics::Camera::setPosition(const Vector3f& _position)
{
    this->transform.position = _position;
    glTranslatef(-(this->transform.position.x), -(this->transform.position.y), -(this->transform.position.z));
}

blib::graphics::Vector3f blib::graphics::Camera::getPosition() const
{
    return this->transform.position;
}

void blib::graphics::Camera::setRotation(const Vector3f& _rotation)
{
    this->transform.rotation = _rotation;
    glRotatef(-this->transform.rotation.x, 1, 0, 0);
    glRotatef(-this->transform.rotation.y, 0, 1, 0);
    glRotatef(-this->transform.rotation.z, 0, 0, 1);
}

blib::graphics::Vector3f blib::graphics::Camera::getRotation() const
{
    return this->transform.rotation;
}

void blib::graphics::Camera::setScale(const Vector3f& _scale)
{
    this->transform.scale.x = _scale.x;
    this->transform.scale.x = _scale.y;
    this->transform.scale.x = _scale.z;
}

blib::graphics::Vector3f blib::graphics::Camera::getScale() const
{
    return this->transform.scale;
}

void blib::graphics::Camera::setOrigin(const Vector3f& _origin)
{
    this->transform.origin.x = _origin.x;
    this->transform.origin.y = _origin.y;
    this->transform.origin.z = _origin.z;
}

blib::graphics::Vector3f blib::graphics::Camera::getOrigin() const
{
    return this->transform.origin;
}

void blib::graphics::Camera::display(RenderWindow& wnd)
{
    //glPushMatrix();
    {
        GLfloat m[16];
        glGetFloatv(GL_PROJECTION_MATRIX, m);
        printf("cameraPROJECTION\n");
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
    //glPopMatrix();
    wnd.display();
}
