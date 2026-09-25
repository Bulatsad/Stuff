#include <blib/graphics/transformMatrix.h>

#include <blib/inline.h>

#include <blib/core/math/trigonometry.h>
#include <blib/core/math/consts.h>


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
    // Стандартная column-major view-матрица (column-vector конвенция,
    // как в glm::lookAt): столбцы — образы мировых осей, трансляция —
    // в последней колонке (data[3][0..2]). В шейдере: gl_Position =
    // projection * view * model * vertex.
    blib::graphics::Vector3f const f(blib::math::normalize(target - camera));
    blib::graphics::Vector3f const s(blib::math::normalize(cross(f, worldUp)));
    blib::graphics::Vector3f const u(blib::math::cross(s, f));

    blib::graphics::TransformMatrix res = blib::graphics::Identity;
    res.data[0][0] = s.x;
    res.data[0][1] = u.x;
    res.data[0][2] = -f.x;
    res.data[1][0] = s.y;
    res.data[1][1] = u.y;
    res.data[1][2] = -f.y;
    res.data[2][0] = s.z;
    res.data[2][1] = u.z;
    res.data[2][2] = -f.z;
    res.data[3][0] = -blib::math::dot(s, camera);
    res.data[3][1] = -blib::math::dot(u, camera);
    res.data[3][2] = blib::math::dot(f, camera);
    return res;
}

blib::graphics::TransformMatrix blib::graphics::lookAt(const blib::graphics::Vector3f& camera, const blib::graphics::Vector3f& target, const blib::graphics::Vector3f& worldUp)
{
    return lookAtRightHand(camera, target, worldUp);
}

blib::graphics::TransformMatrix blib::graphics::mul(const TransformMatrix& lhs, const TransformMatrix& rhs)
{
    // Алиас стандартного произведения Matrix::operator* (column-major,
    // column-vector конвенция): v' = lhs * rhs * v. Трансляция при
    // преобразовании точки не теряется (см. CORE.md, «Конвенция матриц»).
    return lhs * rhs;
}

void blib::graphics::decomposeMatrix(const TransformMatrix& matrix, Transform& transform)
{
    // Раскладка стандартной column-major TRS-матрицы на TRS-компоненты.
    // Конвенция согласована с ITransformable::getTransform: повороты в
    // порядке X·Y·Z, углы в ГРАДУСАХ (как Transform::getRotation()).
    constexpr float epsilon = 1e-6f;

    // Трансляция — последняя колонка (data[3][0..2])
    blib::graphics::Vector3f& position = transform.getPosition();
    position = blib::graphics::Vector3f(matrix.data[3]);

    // Масштаб — длины столбцов 0..2 (столбцы содержат базис R*S)
    blib::graphics::Vector3f& scale = transform.getScale();
    scale.x = blib::math::length(blib::graphics::Vector3f(matrix.data[0]));
    scale.y = blib::math::length(blib::graphics::Vector3f(matrix.data[1]));
    scale.z = blib::math::length(blib::graphics::Vector3f(matrix.data[2]));

    blib::graphics::Vector3f& rotation = transform.getRotation();

    // Нулевой масштаб — вращение невосстановимо; оставляем нули
    if (__blib_unlikely(scale.x < epsilon || scale.y < epsilon || scale.z < epsilon))
    {
        rotation = blib::graphics::Vector3f(0.0f, 0.0f, 0.0f);
        return;
    }

    // Нормализованная ротационная часть R = Rx(θx)·Ry(θy)·Rz(θz):
    // r[row][col] = data[col][row] / scale[col]
    const float r00 = matrix.data[0][0] / scale.x;
    const float r10 = matrix.data[0][1] / scale.x;
    const float r20 = matrix.data[0][2] / scale.x;
    const float r01 = matrix.data[1][0] / scale.y;
    const float r11 = matrix.data[1][1] / scale.y;
    const float r21 = matrix.data[1][2] / scale.y;
    const float r02 = matrix.data[2][0] / scale.z;
    const float r12 = matrix.data[2][1] / scale.z;
    const float r22 = matrix.data[2][2] / scale.z;

    // Конвертация радианы → градусы (Transform хранит градусы)
    const float radiansToDegrees = static_cast<float>(blib::math::c180DivPi);

    // θy = asin(r02); asin(x) = atan2(x, sqrt(1-x²)) — через имеющийся
    // blib::math::atan2, без дополнительного API
    const float clampedR02 = r02 > 1.0f ? 1.0f : (r02 < -1.0f ? -1.0f : r02);
    const float cosY = blib::math::sqrt(1.0f - clampedR02 * clampedR02);

    float x;
    float y;
    float z;
    if (cosY > epsilon)
    {
        // Невырожденный случай: R[1][2] = -sinX·cosY, R[2][2] = cosX·cosY;
        // R[0][1] = -cosY·sinZ, R[0][0] = cosY·cosZ
        x = blib::math::atan2(-r12, r22);
        y = blib::math::atan2(clampedR02, cosY);
        z = blib::math::atan2(-r01, r00);
    }
    else
    {
        // Gimbal lock (cosY ≈ 0): θz неоднозначен, фиксируем 0
        x = blib::math::atan2(r21 * clampedR02, r11);
        y = blib::math::atan2(clampedR02, cosY);
        z = 0.0f;
    }

    rotation = blib::graphics::Vector3f(
        x * radiansToDegrees,
        y * radiansToDegrees,
        z * radiansToDegrees);
}
