#pragma once

#include <blib/config.h>

#include <blib/math/matrix.h>
#include <blib/math/quaternion.h>
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

        // standard matrix multiplication (lhs * rhs)
        TransformMatrix mul(const TransformMatrix& lhs, const TransformMatrix& rhs);

        template<class Type>
        TransformMatrix composeMatrix(const blib::graphics::Vector3f& position, const blib::math::Quaternion<Type>& rotation, const blib::graphics::Vector3f& scale)
        {
            blib::math::Quaternion<Type> q = rotation.normalize();
            Type qw = q.w, qx = q.x, qy = q.y, qz = q.z;

            Type r00 = Type(1) - Type(2) * (qy * qy + qz * qz);
            Type r01 = Type(2) * (qx * qy - qw * qz);
            Type r02 = Type(2) * (qx * qz + qw * qy);
            Type r10 = Type(2) * (qx * qy + qw * qz);
            Type r11 = Type(1) - Type(2) * (qx * qx + qz * qz);
            Type r12 = Type(2) * (qy * qz - qw * qx);
            Type r20 = Type(2) * (qx * qz - qw * qy);
            Type r21 = Type(2) * (qy * qz + qw * qx);
            Type r22 = Type(1) - Type(2) * (qx * qx + qy * qy);

            TransformMatrix res = blib::graphics::Identity;
            res.data[0][0] = static_cast<float>(r00 * scale.x);
            res.data[0][1] = static_cast<float>(r01 * scale.y);
            res.data[0][2] = static_cast<float>(r02 * scale.z);
            res.data[0][3] = position.x;
            res.data[1][0] = static_cast<float>(r10 * scale.x);
            res.data[1][1] = static_cast<float>(r11 * scale.y);
            res.data[1][2] = static_cast<float>(r12 * scale.z);
            res.data[1][3] = position.y;
            res.data[2][0] = static_cast<float>(r20 * scale.x);
            res.data[2][1] = static_cast<float>(r21 * scale.y);
            res.data[2][2] = static_cast<float>(r22 * scale.z);
            res.data[2][3] = position.z;
            return res;
        }
    } 
}