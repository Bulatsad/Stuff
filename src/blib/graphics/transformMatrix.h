#pragma once

#include <blib/config.h>

#include <blib/math/matrix.h>
#include <blib/graphics/vector.h>
#include <blib/graphics/transform.h>

namespace blib
{
    namespace graphics
    {
        typedef blib::math::Matrix<float, 4, 4> TransformMatrix;
        static const TransformMatrix Identity = blib::graphics::TransformMatrix({ 1, 0, 0, 0,
                                                                      0, 1, 0, 0,
                                                                      0, 0, 1, 0,
                                                                      0, 0, 0, 1 });

        //Transform getInverse() const;

        //Vector2f transformPoint(float x, float y) const;
        //Vector2f transformPoint(const Vector2f& point) const;

        //FloatRect transformRect(const FloatRect& rectangle) const;

        TransformMatrix rotateX(const TransformMatrix& matrix, float angle);
        TransformMatrix rotateY(const TransformMatrix& matrix, float angle);
        TransformMatrix rotateZ(const TransformMatrix& matrix, float angle);

        TransformMatrix lookAt(const blib::graphics::Vector3f& camera, const blib::graphics::Vector3f& target, const blib::graphics::Vector3f& worldUp);

        void decomposeMatrix(const TransformMatrix& matrix, Transform& transform);
    } 
}