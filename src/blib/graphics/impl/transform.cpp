#include <blib/graphics/transform.h>

blib::graphics::Transform::Transform()
{
    this->origin = blib::graphics::Vector3f(0, 0, 0);
    this->position = blib::graphics::Vector3f(0, 0, 0);
    this->rotation = blib::graphics::Vector3f(0, 0, 0);
    this->scale = blib::graphics::Vector3f(1, 1, 1);
}

void blib::graphics::Transform::setPosition(float x, float y, float z)
{
    position.x = x;
    position.y = y;
    position.z = z;
}

void blib::graphics::Transform::setPosition(const blib::graphics::Vector3f& position)
{
    setPosition(position.x, position.y, position.z);
}

void blib::graphics::Transform::scaleX(float factorX)
{
    this->scale.x = this->scale.x * factorX;
}

void blib::graphics::Transform::scaleY(float factorY)
{
    this->scale.y = this->scale.y * factorY;
}

void blib::graphics::Transform::scaleZ(float factorZ)
{
    this->scale.z = this->scale.z * factorZ;
}

void blib::graphics::Transform::setScale(float factorX, float factorY, float factorZ)
{
    this->scale.x = factorX;
    this->scale.y = factorY;
    this->scale.z = factorZ;
}

void blib::graphics::Transform::setScale(const blib::graphics::Vector3f& factors)
{
    this->setScale(factors.x, factors.y, factors.z);
}

void blib::graphics::Transform::setOrigin(float x, float y, float z)
{
    this->origin.x = x;
    this->origin.y = y;
    this->origin.z = z;
}

void blib::graphics::Transform::setOrigin(const blib::graphics::Vector3f& origin)
{
    this->setOrigin(origin.x, origin.y, origin.z);
}

void blib::graphics::Transform::setRotation(float x, float y, float z)
{
    this->rotation.x = x;
    this->rotation.y = y;
    this->rotation.z = z;
}

void blib::graphics::Transform::rotateX(float angle)
{
    this->rotation.x = blib::math::fmod(this->rotation.x + angle, 360.f);
}

void blib::graphics::Transform::rotateY(float angle)
{
    this->rotation.y = blib::math::fmod(this->rotation.y + angle, 360.f);
}

void blib::graphics::Transform::rotateZ(float angle)
{
    this->rotation.z = blib::math::fmod(this->rotation.z + angle, 360.f);
}

const blib::graphics::Vector3f& blib::graphics::Transform::getPosition() const
{
    return position;
}

blib::graphics::Vector3f& blib::graphics::Transform::getPosition()
{
    return position;
}

const blib::graphics::Vector3f& blib::graphics::Transform::getRotation() const
{
    return rotation;
}

blib::graphics::Vector3f& blib::graphics::Transform::getRotation()
{
    return rotation;
}

const blib::graphics::Vector3f& blib::graphics::Transform::getScale() const
{
    return scale;
}

blib::graphics::Vector3f& blib::graphics::Transform::getScale()
{
    return scale;
}

const blib::graphics::Vector3f& blib::graphics::Transform::getOrigin() const
{
    return origin;
}

blib::graphics::Vector3f& blib::graphics::Transform::getOrigin()
{
    return origin;
}

void blib::graphics::Transform::move(float offsetX, float offsetY, float offsetZ)
{
    setPosition(position.x + offsetX, position.y + offsetY, position.z + offsetZ);
}

void blib::graphics::Transform::move(const blib::graphics::Vector3f& offset)
{
    setPosition(position.x + offset.x, position.y + offset.y, position.z + offset.z);
}

