#include <blib/graphics/transformable.h>
#include <cmath>

blib::graphics::Transformable::Transformable() :
    m_origin(0, 0, 0),
    m_position(0, 0 , 0),
    m_rotation(0, 0, 0),
    m_scale(1, 1, 1),
    m_transform(),
    m_transformNeedUpdate(true),
    m_inverseTransform(),
    m_inverseTransformNeedUpdate(true)
{
}

blib::graphics::Transformable::~Transformable()
{
}

void blib::graphics::Transformable::setPosition(float x, float y)
{
    m_position.x = x;
    m_position.y = y;
    m_transformNeedUpdate = true;
    m_inverseTransformNeedUpdate = true;
}

void blib::graphics::Transformable::setPosition(float x, float y, float z)
{
    m_position.x = x;
    m_position.y = y;
    m_position.z = z;
    m_transformNeedUpdate = true;
    m_inverseTransformNeedUpdate = true;
}

void blib::graphics::Transformable::setPosition(const blib::graphics::Vector2f& position)
{
    setPosition(position.x, position.y);
}

void blib::graphics::Transformable::setScale(float factorX, float factorY)
{
    m_scale.x = factorX;
    m_scale.y = factorY;
    m_transformNeedUpdate = true;
    m_inverseTransformNeedUpdate = true;
}

void blib::graphics::Transformable::setScale(const blib::graphics::Vector2f& factors)
{
    setScale(factors.x, factors.y);
}

void blib::graphics::Transformable::setOrigin(float x, float y)
{
    m_origin.x = x;
    m_origin.y = y;
    m_transformNeedUpdate = true;
    m_inverseTransformNeedUpdate = true;
}

void blib::graphics::Transformable::setOrigin(const blib::graphics::Vector2f& origin)
{
    setOrigin(origin.x, origin.y);
}

void blib::graphics::Transformable::setRotation(float x, float y, float z)
{
    m_rotation.x = x;
    m_rotation.y = y;
    m_rotation.z = z;

    m_transformNeedUpdate = true;
    m_inverseTransformNeedUpdate = true;
}

void blib::graphics::Transformable::rotateX(float angle)
{
    this->m_rotation.x = fmod(this->m_rotation.x + angle, 360);

    m_transformNeedUpdate = true;
    m_inverseTransformNeedUpdate = true;
}

void blib::graphics::Transformable::rotateY(float angle)
{
    this->m_rotation.y = fmod(this->m_rotation.y + angle, 360);

    m_transformNeedUpdate = true;
    m_inverseTransformNeedUpdate = true;
}

void blib::graphics::Transformable::rotateZ(float angle)
{
    this->m_rotation.z = fmod(this->m_rotation.z + angle, 360);

    m_transformNeedUpdate = true;
    m_inverseTransformNeedUpdate = true;
}

const blib::graphics::Vector3f& blib::graphics::Transformable::getPosition() const
{
    return m_position;
}

const blib::graphics::Vector3f& blib::graphics::Transformable::getRotation() const
{
    return m_rotation;
}

const blib::graphics::Vector3f& blib::graphics::Transformable::getScale() const
{
    return m_scale;
}

const blib::graphics::Vector3f& blib::graphics::Transformable::getOrigin() const
{
    return m_origin;
}

void blib::graphics::Transformable::move(float offsetX, float offsetY, float offsetZ)
{
    setPosition(m_position.x + offsetX, m_position.y + offsetY, m_position.z + offsetZ);
}

void blib::graphics::Transformable::move(const blib::graphics::Vector3f& offset)
{
    setPosition(m_position.x + offset.x, m_position.y + offset.y, m_position.z + offset.z);
}

void blib::graphics::Transformable::scale(float factorX, float factorY)
{
    setScale(m_scale.x * factorX, m_scale.y * factorY);
}

void blib::graphics::Transformable::scale(const blib::graphics::Vector2f& factor)
{
    setScale(m_scale.x * factor.x, m_scale.y * factor.y);
}


const blib::graphics::Transform& blib::graphics::Transformable::getTransform() const
{
    if (m_transformNeedUpdate)
    {
        //float angle = -m_rotation.z * 3.141592654f / 180.f;
        //float cosine = std::cos(angle);
        //float sine = std::sin(angle);
        //float sxc = m_scale.x * cosine;
        //float syc = m_scale.y * cosine;
        //float sxs = m_scale.x * sine;
        //float sys = m_scale.y * sine;
        //float tx = -m_origin.x * sxc - m_origin.y * sys + m_position.x;
        //float ty = m_origin.x * sxs - m_origin.y * syc + m_position.y;
        //
        //m_transform = Transform(sxc,  sys, tx,
        //                        -sxs, syc, ty,
        //                        0.f,  0.f, 1.f);
        //m_transformNeedUpdate = false;

        Transform xrotate;
        Transform yrotate;
        Transform zrotate;

        Transform translate;
        translate.m_matrix[12] = this->m_position.x;
        translate.m_matrix[13] = this->m_position.y;
        translate.m_matrix[14] = this->m_position.z;

        if (this->m_rotation.x != 0)
        {
            Transform dfwerotate;
        }
        xrotate.rotateX(this->m_rotation.x);
        yrotate.rotateY(this->m_rotation.y);
        zrotate.rotateZ(this->m_rotation.z);

        this->m_transform = xrotate * yrotate * zrotate *translate;
        //this->m_transform.m_matrix[12] = this->m_position.x;
        //this->m_transform.m_matrix[13] = this->m_position.y;
        //this->m_transform.m_matrix[14] = this->m_position.z;

        m_transformNeedUpdate = false;
    }

    return m_transform;
}

