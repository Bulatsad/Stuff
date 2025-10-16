#pragma once

#include <blib/config.h>

#include <blib/math/matrix.h>
#include <blib/graphics/rect.h>
#include <blib/graphics/vector.h>

namespace blib
{
    namespace graphics
    {
        typedef blib::math::Matrix<float, 4, 4> Transform;
        static const Transform Identity = blib::graphics::Transform({ 1, 0, 0, 0,
                                                                      0, 1, 0, 0,
                                                                      0, 0, 1, 0,
                                                                      0, 0, 0, 1 });

        //Transform getInverse() const;
        
        //Vector2f transformPoint(float x, float y) const;
        //Vector2f transformPoint(const Vector2f& point) const;
        
        //FloatRect transformRect(const FloatRect& rectangle) const;

        Transform rotateX(const Transform& matrix, float angle);
        Transform rotateY(const Transform& matrix, float angle);
        Transform rotateZ(const Transform& matrix, float angle);

        Transform lookAt(const blib::graphics::Vector3f& camera, const blib::graphics::Vector3f& target, const blib::graphics::Vector3f& worldUp);
        
        void decomposeMatrix(const Transform& matrix, Vector3f& position, Vector3f& rotation, Vector3f& scale);

    }
}
