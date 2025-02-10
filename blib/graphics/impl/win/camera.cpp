#include <blib/graphics/camera.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/mouse.h>

#include <Windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>


blib::graphics::Camera::Camera()
{
}

blib::graphics::Camera::~Camera()
{
}

void blib::graphics::Camera::setPosition(const Vector3f& _position)
{
    this->transform.position = _position;
}

blib::graphics::Vector3f blib::graphics::Camera::getPosition() const
{
    return this->transform.position;
}

void blib::graphics::Camera::setRotation(const Vector3f& _rotation)
{
    this->transform.rotation = _rotation;
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

void blib::graphics::Camera::move(const Vector3f& _position)
{
    this->transform.position += _position;
}

void blib::graphics::Camera::rotate(const Vector3f& _rotatation)
{
    this->transform.rotation += _rotatation;
}

void blib::graphics::Camera::setProjectionMode(ProjectionMode mode, const RenderWindow& wnd)
{
    float height = wnd.getWight();
    float width = wnd.getHeight();
    height /= 2;
    width /= 2;
    
    {
        GLfloat m[16];
        glGetFloatv(GL_MODELVIEW_MATRIX, m);
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

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glViewport(0, 0, wnd.getWight(), wnd.getHeight());
    
    GLenum a;

    switch (mode)
    {
    case blib::graphics::Camera::ProjectionMode::Perspective:
        height /= 1000;
        width /= 1000;
        glFrustum(-height, height, -width, width, 1, 2000);
        a = glGetError();
        break;
    case blib::graphics::Camera::ProjectionMode::Ortho:
        glOrtho(-height, height, -width, width, 1, 2000);
        break;
    case blib::graphics::Camera::ProjectionMode::END_OF_ENUM:
        break;
    default:
        break;
    }
    {
        //GLfloat m[16];
        //glGetFloatv(GL_PROJECTION_MATRIX, m);
        //printf("cameraPROJECTION\n");
        //for (int i = 0; i < 4; ++i)
        //{
        //    for (int j = 0; j < 4; ++j)
        //    {
        //        printf("%f ", m[i * 4 + j]);
        //    }
        //    printf("\n");
        //}
        //printf("\n");
        //printf("\n");
        //printf("\n");
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
}

void blib::graphics::Camera::display(RenderWindow& wnd)
{
    //glPushMatrix();
    {
        //glMatrixMode(GL_PROJECTION);
        //glMatrixMode(GL_MODELVIEW);

        //glLoadIdentity();
    

        glRotatef(-this->transform.rotation.x, 1, 0, 0);
        glRotatef(-this->transform.rotation.y, 0, 1, 0);
        glRotatef(-this->transform.rotation.z, 0, 0, 1);
        glTranslatef(-(this->transform.position.x), -(this->transform.position.y), -(this->transform.position.z));

        float position[] = { 0, 0, 1, 0 };
        glLightfv(GL_LIGHT0, GL_POSITION, position);

        //GLfloat m[16];
        //glGetFloatv(GL_PROJECTION_MATRIX, m);
        //printf("cameraPROJECTION\n");
        //for (int i = 0; i < 4; ++i)
        //{
        //    for (int j = 0; j < 4; ++j)
        //    {
        //        printf("%f ", m[i * 4 + j]);
        //    }
        //    printf("\n");
        //}
        //printf("\n");
        //printf("\n");
        //printf("\n");
    }
    //glPopMatrix();


    //wnd.display();
}

void blib::graphics::Camera::controlUpdate(float deltaTime, RenderWindow& wnd)
{
    blib::graphics::vector2i mousepos = blib::graphics::Mouse::getPosition(wnd);
    blib::graphics::vector2i zeropos;
    blib::graphics::vector2i deltapos;
    zeropos.y = wnd.getHeight() / 2;
    zeropos.x = wnd.getWight() / 2;
    deltapos.x = zeropos.x - mousepos.x;
    deltapos.y = zeropos.y - mousepos.y;
    blib::graphics::Mouse::setPosition(wnd, zeropos);
    auto radian = [](float x) -> float {
        return x * (3.1415f / 180.0f);
        };

    
    this->transform.rotation.z += deltapos.x * this->rotatespeed * deltaTime;
    this->transform.rotation.x += deltapos.y * this->rotatespeed * deltaTime;

    if (this->transform.rotation.x >= 120)
        this->transform.rotation.x = 120;
    printf("%f rotate\n", this->transform.rotation.x);

    constexpr float pi = 3.1415;

    float zangle = radian(-this->transform.rotation.z);
    float xangle = radian(-this->transform.rotation.x);
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::W))
    {
        this->transform.position.x += sin(zangle) * movespeed * deltaTime;
        this->transform.position.y += cos(zangle) * movespeed * deltaTime;
    }
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::S))
    {
        this->transform.position.x -= sin(zangle) * movespeed * deltaTime;
        this->transform.position.y -= cos(zangle) * movespeed * deltaTime;
    }
    
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::A))
    {
        zangle -= pi * 0.5;
        this->transform.position.x += sin(zangle) * movespeed * deltaTime;
        this->transform.position.y += cos(zangle) * movespeed * deltaTime;
    }
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::D))
    {
        zangle += pi * 0.5;
        this->transform.position.x += sin(zangle) * movespeed * deltaTime;
        this->transform.position.y += cos(zangle) * movespeed * deltaTime;
    }
    
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Space))
    {
        this->transform.position.z += movespeed * deltaTime;
    }
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LShift))
    {
        zangle -= pi * 0.5;
        this->transform.position.z -= movespeed * deltaTime;
    }


    
    //this->transform.rotation.x += deltapos.y * this->rotatespeed * deltaTime;
    
}
