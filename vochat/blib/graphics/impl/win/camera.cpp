#include <blib/graphics/camera.h>

#include <Windows.h>
#include <gl/GL.h>

blib::graphics::Camera::Camera()
{
}

blib::graphics::Camera::~Camera()
{
}

void blib::graphics::Camera::setPosition(const Vector3f& position)
{
    this->postion = position;
    glTranslatef(-(this->postion).x, -(this->postion.y), -(this->postion.z));
}

blib::graphics::Vector3f blib::graphics::Camera::getPosition() const
{
    return this->postion;
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
