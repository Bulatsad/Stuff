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


void blib::graphics::Camera::setProjectionMode(ProjectionMode mode, const RenderWindow& wnd)
{
    float height = wnd.getWight();
    float width = wnd.getHeight();
    height /= 2;
    width /= 2;
    
    //{
    //    GLfloat m[16];
    //    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    //    printf("cameraPROJECTION\n");
    //    for (int i = 0; i < 4; ++i)
    //    {
    //        for (int j = 0; j < 4; ++j)
    //        {
    //            printf("%f ", m[i * 4 + j]);
    //        }
    //        printf("\n");
    //    }
    //    printf("\n");
    //    printf("\n");
    //    printf("\n");
    //}

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

void blib::graphics::Camera::controlUpdate(float deltaTime,  RenderWindow& wnd)
{
    blib::graphics::Vector2i mousepos = blib::graphics::Mouse::getPosition(wnd);
    blib::graphics::Vector2i zeropos;
    blib::graphics::Vector2i deltapos;
    zeropos.y = wnd.getHeight() / 2;
    zeropos.x = wnd.getWight() / 2;
    deltapos.x = zeropos.x - mousepos.x;
    deltapos.y = zeropos.y - mousepos.y;
    blib::graphics::Mouse::setPosition(wnd, zeropos);
    auto radian = [](float x) -> float {
        return x * (3.1415f / 180.0f);
        };

    //printf("%d\n", deltapos.x);

    this->rotateZ(deltapos.x * this->rotatespeed * deltaTime);
    this->rotateX(deltapos.y * this->rotatespeed * deltaTime);
    if (this->getRotation().x > 180)
        this->setRotation(180, this->getRotation().y, this->getRotation().z);
    if (this->getRotation().x < 0)
        this->setRotation(0, this->getRotation().y, this->getRotation().z);

    constexpr float pi = 3.1415;
    float zangle = radian(-this->getRotation().z);
    float xangle = radian(-this->getRotation().x);


    auto posdelta = movespeed * deltaTime;

    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LControl))
    {
        posdelta *= 5;
    }

    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Space))
    {
        this->move(0, 0, posdelta);
    }
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LShift))
    {
        this->move(0, 0, -posdelta);
    }
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::W))
    {
        this->move(sin(zangle) * posdelta, cos(zangle) * posdelta, 0);
    }
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::S))
    {
        this->move(-sin(zangle) * posdelta, -cos(zangle) * posdelta, 0);
    }
    
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::A))
    {
        zangle -= pi * 0.5;
        this->move(sin(zangle) * posdelta, cos(zangle) * posdelta, 0);
    }
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::D))
    {
        zangle += pi * 0.5;
        this->move(sin(zangle) * posdelta, cos(zangle) * posdelta, 0);
    }

    //blib::graphics::Vector2i mousepos = blib::graphics::Mouse::getPosition(wnd);
    //blib::graphics::Vector2i zeropos;
    //blib::graphics::Vector2i deltapos;
    //zeropos.y = wnd.getHeight() / 2;
    //zeropos.x = wnd.getWight() / 2;
    //deltapos.x = zeropos.x - mousepos.x;
    //deltapos.y = zeropos.y - mousepos.y;
    ////blib::graphics::Mouse::setPosition(wnd, zeropos);
    //auto radian = [](float x) -> float {
    //    return x * (3.1415f / 180.0f);
    //    };

    //this->rotateZ(deltapos.x * this->rotatespeed * deltaTime);
    //this->rotateX(deltapos.y * this->rotatespeed * deltaTime);
    //if (this->getRotation().x > 180)
    //    this->setRotation(180, this->getRotation().y, this->getRotation().z);
    //if (this->getRotation().x < 0)
    //    this->setRotation(0, this->getRotation().y, this->getRotation().z);

    //this->getTransform()

    //constexpr float pi = 3.1415;
    //float zangle = radian(-this->getRotation().z);
    //float xangle = radian(-this->getRotation().x);

    //auto posdelta = movespeed * deltaTime;

}

void blib::graphics::Camera::lookAt(const blib::graphics::Transformable& target, const blib::graphics::Vector3f worldUp)
{
    this->setTransform(blib::graphics::lookAt(this->getPosition(), target.getPosition(), worldUp));
}

void blib::graphics::Camera::draw(RenderTarget& target, RenderContext& ctx) const
{
    ////glPushMatrix();
    //{
    //    //glMatrixMode(GL_PROJECTION);
    //    //glMatrixMode(GL_MODELVIEW);
    //
    //    //auto transform = this->getInverseTransform();
    //
    //    ctx.applyTransform(*this);
    //
    //    //glLoadIdentity();
    //
    //    //if(this->isNeedToRecalculate())
    //    //glLoadMatrixf(this->getTransform().getMatrix());
    //
    //    //glRotatef(-this->getRotation().x, 1, 0, 0);
    //    //glRotatef(-this->getRotation().y, 0, 1, 0);
    //    //glRotatef(-this->getRotation().z, 0, 0, 1);
    //    //
    //    //glTranslatef(-(this->getPosition().x), -(this->getPosition().y), -(this->getPosition().z));
    //    
    //    //glRotatef(-this->transform.rotation.x, 1, 0, 0);
    //    //glRotatef(-this->transform.rotation.y, 0, 1, 0);
    //    //glRotatef(-this->transform.rotation.z, 0, 0, 1);
    //    //glTranslatef(-(this->transform.position.x), -(this->transform.position.y), -(this->transform.position.z));
    //
    //    ////
    //    float position[] = { 0, 0, 1, 0 };
    //    glLightfv(GL_LIGHT0, GL_POSITION, position);
    //    //{
    //    //    GLfloat m[16];
    //    //    glGetFloatv(GL_PROJECTION_MATRIX, m);
    //    //    printf("cameraPROJECTION\n");
    //    //    for (int i = 0; i < 4; ++i)
    //    //    {
    //    //        for (int j = 0; j < 4; ++j)
    //    //        {
    //    //            printf("%f ", m[i * 4 + j]);
    //    //        }
    //    //        printf("\n");
    //    //    }
    //    //}
    //    //printf("\n");
    //    //auto m = this->getTransform().getMatrix();
    //    //for (int i = 0; i < 4; ++i)
    //    //{
    //    //    for (int j = 0; j < 4; ++j)
    //    //    {
    //    //        printf("%f ", m[i * 4 + j]);
    //    //    }
    //    //    printf("\n");
    //    //}
    //    //printf("\n");
    //    //printf("\n");
    //    //printf("\n");
    //
    //}
    ////glPopMatrix();
    //
    //
    ////wnd.display();

    //ctx.api.ogl.ext.__blib_gl_glGetUniformLocation();

}
