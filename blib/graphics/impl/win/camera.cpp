#include <blib/graphics/camera.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/mouse.h>

#include <blib/math/trigonometry.h> 
#include <blib/math/consts.h>

#include <Windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>


blib::graphics::Camera::Camera()
{
    this->projectionMatrix.loadIdentity();
}

blib::graphics::Camera::~Camera()
{
}

void blib::graphics::Camera::setPerpective(const blib::math::AngleDegree<float>& fov, float aspect, float nearDist, float farDist)
{
    this->projectionMatrix = this->perspective(fov, aspect, nearDist, farDist);
}

const blib::math::Matrix<float, 4, 4>& blib::graphics::Camera::getProjectionMatrix() const
{
    return this->projectionMatrix;
}

void blib::graphics::Camera::controlUpdate(float deltaTime, blib::graphics::RenderWindow& wnd)
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

blib::math::Matrix<float, 4, 4> blib::graphics::Camera::perspective(const blib::math::AngleDegreef& fov, float aspect, float nearDist, float farDist)
{
     blib::math::Matrix<float, 4, 4> projMat;

    float f = 1.0f / blib::math::tan(fov.data * 3.14159f / 360.f);

    projMat.data[0][0] = f / aspect;    // x scale
    projMat.data[1][1] = f;             // y scale
    projMat.data[2][2] = (farDist + nearDist) / (nearDist - farDist);   // z scale
    projMat.data[2][3] = -1;            // perspective div
    projMat.data[3][2] = (2 * farDist * nearDist) / (nearDist - farDist); // z shift

    projMat.data[3][3] = 0;

    return projMat;
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
