#include <blib/graphics/transformMatrix.h>

#include <blib/inline.h>

#include <blib/math/trigonometry.h>


//blib::graphics::Vector2f blib::graphics::Transform::transformPoint(float x, float y) const
//{
//    return Vector2f(m_matrix[0] * x + m_matrix[4] * y + m_matrix[12],
//                    m_matrix[1] * x + m_matrix[5] * y + m_matrix[13]);
//}
//
//
//blib::graphics::Vector2f blib::graphics::Transform::transformPoint(const blib::graphics::Vector2f& point) const
//{
//    return transformPoint(point.x, point.y);
//}
//
//
//blib::graphics::FloatRect blib::graphics::Transform::transformRect(const blib::graphics::FloatRect& rectangle) const
//{
//    // Transform the 4 corners of the rectangle
//    const blib::graphics::Vector2f points[] =
//    {
//        transformPoint(rectangle.left, rectangle.top),
//        transformPoint(rectangle.left, rectangle.top + rectangle.height),
//        transformPoint(rectangle.left + rectangle.width, rectangle.top),
//        transformPoint(rectangle.left + rectangle.width, rectangle.top + rectangle.height)
//    };
//
//    // Compute the bounding rectangle of the transformed points
//    float left = points[0].x;
//    float top = points[0].y;
//    float right = points[0].x;
//    float bottom = points[0].y;
//    for (int i = 1; i < 4; ++i)
//    {
//        if      (points[i].x < left)   left = points[i].x;
//        else if (points[i].x > right)  right = points[i].x;
//        if      (points[i].y < top)    top = points[i].y;
//        else if (points[i].y > bottom) bottom = points[i].y;
//    }
//
//    return blib::graphics::FloatRect(left, top, right - left, bottom - top);
//}

blib::graphics::TransformMatrix blib::graphics::rotateX(const blib::graphics::TransformMatrix& martix, float angle)
{
    float rad = angle * 3.141592654f / 180.f;
    float cos;
    float sin;
    blib::math::sincos(rad, sin, cos);


    blib::graphics::TransformMatrix rotation({ 1.f, 0.f,   0.f, 0.f,
                                         0.f, cos, -sin,  0.f,
                                         0.f, sin,  cos,  0.f,
                                         0.f, 0.f,   0.f, 1.f }
    );

    return martix * rotation;
}

blib::graphics::TransformMatrix blib::graphics::rotateY(const blib::graphics::TransformMatrix& martix, float angle)
{
    float rad = angle * 3.141592654f / 180.f;
    float cos;
    float sin;
    blib::math::sincos(rad, sin, cos);

    blib::graphics::TransformMatrix rotation({ cos,  0.f, sin, 0.f,
                                         0.f,  1.f, 0.f, 0.f,
                                        -sin,  0.f, cos, 0.f,
                                         0.f,  0.f, 0.f, 1.f }
    );

    return martix * rotation;
}

blib::graphics::TransformMatrix blib::graphics::rotateZ(const blib::graphics::TransformMatrix& martix, float angle)
{
    float rad = angle * 3.141592654f / 180.f;
    float cos;
    float sin;
    blib::math::sincos(rad, sin, cos);

    blib::graphics::TransformMatrix rotation({ cos, -sin, 0.f, 0.f,
                                         sin,  cos, 0.f, 0.f,
                                         0.f,  0.f, 1.f, 0.f,
                                         0.f,  0.f, 0.f, 1.f }
    );

    return martix * rotation;
}

__blib_private_func blib::graphics::TransformMatrix lookAtRightHand(const blib::graphics::Vector3f& camera, const blib::graphics::Vector3f& target, const blib::graphics::Vector3f& worldUp)
{
    blib::graphics::Vector3f const f(blib::math::normalize(target - camera));
    blib::graphics::Vector3f const s(blib::math::normalize(cross(f, worldUp)));
    blib::graphics::Vector3f const u(blib::math::cross(s, f));

    blib::graphics::TransformMatrix res = blib::graphics::Identity;
    res.data[0][0] = s.x;
    res.data[0][1] = s.y;
    res.data[0][2] = s.z;
    res.data[1][0] = u.x;
    res.data[1][1] = u.y;
    res.data[1][2] = u.z;
    res.data[2][0] = -f.x;
    res.data[2][1] = -f.y;
    res.data[2][2] = -f.z;
    res.data[0][3] = -blib::math::dot(s, camera);
    res.data[1][3] = -blib::math::dot(u, camera);
    res.data[2][3] = blib::math::dot(f, camera);
    return res;
}

blib::graphics::TransformMatrix blib::graphics::lookAt(const blib::graphics::Vector3f& camera, const blib::graphics::Vector3f& target, const blib::graphics::Vector3f& worldUp)
{
    return lookAtRightHand(camera, target, worldUp);
}

blib::graphics::TransformMatrix blib::graphics::mul(const TransformMatrix& lhs, const TransformMatrix& rhs)
{
    TransformMatrix res = blib::graphics::Identity;
    for (size_t i = 0; i < 4; ++i)
    {
        for (size_t j = 0; j < 4; ++j)
        {
            res.data[i][j] = 0;
            for (size_t k = 0; k < 4; ++k)
                res.data[i][j] += lhs.data[i][k] * rhs.data[k][j];
        }
    }
    return res;
}

void blib::graphics::decomposeMatrix(const TransformMatrix& matrix, Transform& transform)
{    // �������
    blib::graphics::Vector3f& position = transform.getPosition();
    position = blib::graphics::Vector3f(matrix.data[3]);

    // �������
    blib::graphics::Vector3f& scale = transform.getScale();
    scale.x = blib::math::length(blib::graphics::Vector3f(matrix.data[0]));
    scale.y = blib::math::length(blib::graphics::Vector3f(matrix.data[1]));
    scale.z = blib::math::length(blib::graphics::Vector3f(matrix.data[2]));

    //// �������
    //glm::mat3 rotMat;
    //rotMat[0] = glm::vec3(matrix[0]) / scale.x;
    //rotMat[1] = glm::vec3(matrix[1]) / scale.y;
    //rotMat[2] = glm::vec3(matrix[2]) / scale.z;
    //
    //rotation = glm::quat_cast(rotMat);
    blib::graphics::Vector3f& rotation = transform.getRotation();

    float sy = blib::math::sqrt(matrix.data[0][0] * matrix.data[0][0] + matrix.data[1][0] * matrix.data[1][0]);

    bool singular = sy < 1e-6;

    float x, y, z;

    if (!singular) {
        x = atan2(matrix.data[2][1], matrix.data[2][2]);
        y = atan2(-matrix.data[2][0], sy);
        z = atan2(matrix.data[1][0], matrix.data[0][0]);
    }
    else {
        x = atan2(-matrix.data[1][2], matrix.data[1][1]);
        y = atan2(-matrix.data[2][0], sy);
        z = 0;
    }

    rotation = blib::graphics::Vector3f(x, y, z);
}
