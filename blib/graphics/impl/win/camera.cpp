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
    this->updateVectors();
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

const blib::math::Matrix<float, 4, 4>& blib::graphics::Camera::getViewMatrix() const
{
    return this->viewMatrix;
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

    this->yaw.data += deltapos.x * -this->rotatespeed * deltaTime;
    this->pitch.data += deltapos.y * this->rotatespeed * deltaTime;
    if (this->pitch.data > 89.0f)
        this->pitch.data = 89.0f;
    if (this->pitch.data < -89.0f)
        this->pitch.data = -89.0f;
    this->yaw.data = blib::math::fmod(yaw.data, 360.0f);

    auto posdelta = movespeed * deltaTime;

    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LControl))
    {
        posdelta *= 5;
    }

    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Space))
    {
        //this->move(0, 0, posdelta);
        this->move(this->up * movespeed * deltaTime);
    }
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LShift))
    {
        //this->move(0, 0, -posdelta);
        this->move(this->up * -movespeed * deltaTime);
    }
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::W))
    {
        //this->move(sin(zangle) * posdelta, cos(zangle) * posdelta, 0);
        this->move(this->front * movespeed * deltaTime);
    }
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::S))
    {
        //this->move(-sin(zangle) * posdelta, -cos(zangle) * posdelta, 0);
        this->move(this->front * -movespeed * deltaTime);
    }
    
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::A))
    {
        //zangle -= pi * 0.5;
        //this->move(sin(zangle) * posdelta, cos(zangle) * posdelta, 0);
        this->move(this->right * -movespeed * deltaTime);
    }
    if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::D))
    {
        //zangle += pi * 0.5;
        //this->move(sin(zangle) * posdelta, cos(zangle) * posdelta, 0);
        this->move(this->right * movespeed * deltaTime);
    }
    this->updateVectors();
    this->viewMatrix = blib::graphics::lookAt(this->getPosition(), this->getPosition() + this->front, this->up);
}

blib::math::Matrix<float, 4, 4> blib::graphics::Camera::perspective(const blib::math::AngleDegreef& fov, float aspect, float nearDist, float farDist)
{
     blib::math::Matrix<float, 4, 4> projMat;

    float f = 1.0f / blib::math::tan(fov.data * 3.14159f / 360.f);

    projMat.data[0][0] = f / aspect;    // x scale
    projMat.data[1][1] = f;             // y scale
    projMat.data[2][2] = (farDist + nearDist) / (nearDist - farDist);   // z scale
    projMat.data[3][2] = -1;            // perspective div
    projMat.data[2][3] = (2 * farDist * nearDist) / (nearDist - farDist); // z shift

    projMat.data[3][3] = 0;

    return projMat;
}

void blib::graphics::Camera::updateVectors()
{
    blib::math::Vector<float, 3> newFront;
    newFront.x = blib::math::sin(this->yaw.toRadian().data) * blib::math::cos(this->pitch.toRadian().data);
    newFront.y = blib::math::sin(this->pitch.toRadian().data);
    newFront.z = -blib::math::cos(this->yaw.toRadian().data) * blib::math::cos(this->pitch.toRadian().data);

    this->front = blib::math::normalize(newFront);

    // Пересчитываем right и up векторы
    this->right = blib::math::normalize(blib::math::cross(front, worldUp));
    this->up = blib::math::normalize(blib::math::cross(right, front));
}
