#pragma once

#include <blib/config.h>

#include <blib/graphics/vertex.h>
#include <blib/graphics/rect.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Transform
        {
        public:

            Transform();

            Transform(float a00, float a01, float a02, float a03,
                      float a10, float a11, float a12, float a13,
                      float a20, float a21, float a22, float a23,
                      float a30, float a31, float a32, float a33
            );
            
            const float* getMatrix() const;
            
            Transform getInverse() const;
            
            Vector2f transformPoint(float x, float y) const;
            Vector2f transformPoint(const Vector2f& point) const;

            FloatRect transformRect(const FloatRect& rectangle) const;

            Transform& rotateX(float angle);
            Transform& rotateY(float angle);
            Transform& rotateZ(float angle);

            static const Transform Identity;

            float m_matrix[16];
        };
    }
}

__blib_api blib::graphics::Transform operator*(const blib::graphics::Transform& left, const blib::graphics::Transform& right);
__blib_api blib::graphics::Transform& operator *=(blib::graphics::Transform& left, const blib::graphics::Transform& right);
__blib_api blib::graphics::Vector2f operator *(const blib::graphics::Transform& left, const blib::graphics::Vector2f& right);
__blib_api bool operator ==(const blib::graphics::Transform& left, const blib::graphics::Transform& right);
__blib_api bool operator !=(const blib::graphics::Transform& left, const blib::graphics::Transform& right);
