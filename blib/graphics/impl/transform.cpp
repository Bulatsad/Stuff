#include <blib/graphics/transform.h>

#include <cmath>

const blib::graphics::Transform blib::graphics::Transform::Identity;

blib::graphics::Transform::Transform()
{
    m_matrix[0] = 1.f; m_matrix[4] = 0.f; m_matrix[8]  = 0.f; m_matrix[12] = 0.f;
    m_matrix[1] = 0.f; m_matrix[5] = 1.f; m_matrix[9]  = 0.f; m_matrix[13] = 0.f;
    m_matrix[2] = 0.f; m_matrix[6] = 0.f; m_matrix[10] = 1.f; m_matrix[14] = 0.f;
    m_matrix[3] = 0.f; m_matrix[7] = 0.f; m_matrix[11] = 0.f; m_matrix[15] = 1.f;
}

blib::graphics::Transform::Transform(float a00, float a01, float a02, float a03,
                                     float a10, float a11, float a12, float a13, 
                                     float a20, float a21, float a22, float a23,
                                     float a30, float a31, float a32, float a33)
{
    //m_matrix[0] = a00; m_matrix[4] = a01; m_matrix[8] = a02; m_matrix[12] = a03;
    //m_matrix[1] = a10; m_matrix[5] = a11; m_matrix[9] = a12; m_matrix[13] = a13;
    //m_matrix[2] = a20; m_matrix[6] = a21; m_matrix[10] = a22; m_matrix[14] = a23;
    //m_matrix[3] = a30; m_matrix[7] = a31; m_matrix[11] = a32; m_matrix[15] = a33;

    m_matrix[0] = a00; m_matrix[1] = a01; m_matrix[2] = a02; m_matrix[3] = a03;
    m_matrix[4] = a10; m_matrix[5] = a11; m_matrix[6] = a12; m_matrix[7] = a13;
    m_matrix[8] = a20; m_matrix[9] = a21; m_matrix[10] = a22; m_matrix[11] = a23;
    m_matrix[12] = a30; m_matrix[13] = a31; m_matrix[14] = a32; m_matrix[15] = a33;
}

const float* blib::graphics::Transform::getMatrix() const
{
    return m_matrix;
}

blib::graphics::Vector2f blib::graphics::Transform::transformPoint(float x, float y) const
{
    return Vector2f(m_matrix[0] * x + m_matrix[4] * y + m_matrix[12],
                    m_matrix[1] * x + m_matrix[5] * y + m_matrix[13]);
}


blib::graphics::Vector2f blib::graphics::Transform::transformPoint(const blib::graphics::Vector2f& point) const
{
    return transformPoint(point.x, point.y);
}


blib::graphics::FloatRect blib::graphics::Transform::transformRect(const blib::graphics::FloatRect& rectangle) const
{
    // Transform the 4 corners of the rectangle
    const blib::graphics::Vector2f points[] =
    {
        transformPoint(rectangle.left, rectangle.top),
        transformPoint(rectangle.left, rectangle.top + rectangle.height),
        transformPoint(rectangle.left + rectangle.width, rectangle.top),
        transformPoint(rectangle.left + rectangle.width, rectangle.top + rectangle.height)
    };

    // Compute the bounding rectangle of the transformed points
    float left = points[0].x;
    float top = points[0].y;
    float right = points[0].x;
    float bottom = points[0].y;
    for (int i = 1; i < 4; ++i)
    {
        if      (points[i].x < left)   left = points[i].x;
        else if (points[i].x > right)  right = points[i].x;
        if      (points[i].y < top)    top = points[i].y;
        else if (points[i].y > bottom) bottom = points[i].y;
    }

    return blib::graphics::FloatRect(left, top, right - left, bottom - top);
}

blib::graphics::Transform& blib::graphics::Transform::rotateX(float angle)
{
    float rad = angle * 3.141592654f / 180.f;
    float cos = std::cos(rad);
    float sin = std::sin(rad);

    Transform rotation( 1., 0.,   0.,  0.,
                        0., cos, -sin, 0.,
                        0., sin,  cos, 0.,
                        0., 0.,   0.,  1.
    );

    return (*this) *=rotation;
}

blib::graphics::Transform& blib::graphics::Transform::rotateY(float angle)
{
    float rad = angle * 3.141592654f / 180.f;
    float cos = std::cos(rad);
    float sin = std::sin(rad);

    Transform rotation( cos, 0., sin, 0.,
                        0.,  1., 0.,  0.,
                       -sin, 0,  cos, 0.,
                        0.,  0., 0.,  1.
    );

    return (*this) *= rotation;
}

blib::graphics::Transform& blib::graphics::Transform::rotateZ(float angle)
{
    float rad = angle * 3.141592654f / 180.f;
    float cos = std::cos(rad);
    float sin = std::sin(rad);

    Transform rotation( cos, -sin, 0., 0.,
                        sin,  cos, 0., 0.,
                        0.,   0.,  1., 0.,
                        0.,   0.,  0., 1.f 
    );

    return (*this) *= rotation;
}

blib::graphics::Transform operator*(const blib::graphics::Transform& left, const blib::graphics::Transform& right)
{
    const auto& b = left.m_matrix;
    const auto& a = right.m_matrix;

    return blib::graphics::Transform(
        a[0] * b[0]  + a[4] * b[1]  + a[8] * b[2]  + a[12] * b[3],
        a[0] * b[4]  + a[4] * b[5]  + a[8] * b[6]  + a[12] * b[7],
        a[0] * b[8]  + a[4] * b[9]  + a[8] * b[10] + a[12] * b[11],
        a[0] * b[12] + a[4] * b[13] + a[8] * b[14] + a[12] * b[15],

        a[1] * b[0]  + a[5] * b[1]  + a[9] * b[2]  + a[13] * b[3],
        a[1] * b[4]  + a[5] * b[5]  + a[9] * b[6]  + a[13] * b[7],
        a[1] * b[8]  + a[5] * b[9]  + a[9] * b[10] + a[13] * b[11],
        a[1] * b[12] + a[5] * b[13] + a[9] * b[14] + a[13] * b[15],

        a[2] * b[0]  + a[6] * b[1]  + a[10] * b[2]  + a[14] * b[3],
        a[2] * b[4]  + a[6] * b[5]  + a[10] * b[6]  + a[14] * b[7],
        a[2] * b[8]  + a[6] * b[9]  + a[10] * b[10] + a[14] * b[11],
        a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14] * b[15],

        a[3] * b[0]  + a[7] * b[1]  + a[11] * b[2]  + a[15] * b[3],
        a[3] * b[4]  + a[7] * b[5]  + a[11] * b[6]  + a[15] * b[7],
        a[3] * b[8]  + a[7] * b[9]  + a[11] * b[10] + a[15] * b[11],
        a[3] * b[12] + a[7] * b[13] + a[11] * b[14] + a[15] * b[15]
    );
}


blib::graphics::Transform& operator *=(blib::graphics::Transform& left, const blib::graphics::Transform& right)
{
    left = left * right;
    return left;
}

blib::graphics::Vector2f operator *(const blib::graphics::Transform& left, const blib::graphics::Vector2f& right)
{
    return left.transformPoint(right);
}

bool operator ==(const blib::graphics::Transform& left, const blib::graphics::Transform& right)
{
    const float* a = left.getMatrix();
    const float* b = right.getMatrix();

    return ((a[0]  == b[0])  && (a[1]  == b[1])  && (a[3]  == b[3]) &&
            (a[4]  == b[4])  && (a[5]  == b[5])  && (a[7]  == b[7]) &&
            (a[12] == b[12]) && (a[13] == b[13]) && (a[15] == b[15]));
}

bool operator !=(const blib::graphics::Transform& left, const blib::graphics::Transform& right)
{
    return !(left == right);
}
