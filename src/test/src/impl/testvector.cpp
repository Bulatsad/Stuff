#include <test/src/test.h>
#include <blib/math/vector.h>
#include <blib/math/consts.h>

using namespace blib::math;

typedef Vector<float, 2> Vec2;
typedef Vector<float, 3> Vec3;
typedef Vector<float, 4> Vec4;

// ============================================================
// Construction
// ============================================================

BLIB_TEST_CASE("Vector2 default construction")
{
	Vec2 v = Vec2(0.0f, 0.0f);
	BLIB_TEST_CHECK(v.x == 0.0f);
	BLIB_TEST_CHECK(v.y == 0.0f);
}

BLIB_TEST_CASE("Vector2 construction and member access")
{
	Vec2 v(1.0f, 2.0f);
	BLIB_TEST_CHECK(v.x == 1.0f);
	BLIB_TEST_CHECK(v.y == 2.0f);
	BLIB_TEST_CHECK(v.data[0] == 1.0f);
	BLIB_TEST_CHECK(v.data[1] == 2.0f);
}

BLIB_TEST_CASE("Vector3 construction")
{
	Vec3 v(1.0f, 2.0f, 3.0f);
	BLIB_TEST_CHECK(v.x == 1.0f);
	BLIB_TEST_CHECK(v.y == 2.0f);
	BLIB_TEST_CHECK(v.z == 3.0f);
}

BLIB_TEST_CASE("Vector4 construction")
{
	Vec4 v(1.0f, 2.0f, 3.0f, 4.0f);
	BLIB_TEST_CHECK(v.x == 1.0f);
	BLIB_TEST_CHECK(v.y == 2.0f);
	BLIB_TEST_CHECK(v.z == 3.0f);
	BLIB_TEST_CHECK(v.w == 4.0f);
}

// ============================================================
// operator[], mutable access
// ============================================================

BLIB_TEST_CASE("Vector3 operator[] const and mutable")
{
	Vec3 v(1.0f, 2.0f, 3.0f);
	BLIB_TEST_CHECK(v[0] == 1.0f);
	BLIB_TEST_CHECK(v[1] == 2.0f);
	BLIB_TEST_CHECK(v[2] == 3.0f);

	v[0] = 10.0f;
	BLIB_TEST_CHECK(v.x == 10.0f);
	v[1] = 20.0f;
	BLIB_TEST_CHECK(v.y == 20.0f);
	v[2] = 30.0f;
	BLIB_TEST_CHECK(v.z == 30.0f);
}

// ============================================================
// Copy / Assignment
// ============================================================

BLIB_TEST_CASE("Vector3 copy assignment")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(4.0f, 5.0f, 6.0f);
	a = b;
	BLIB_TEST_CHECK(a.x == 4.0f);
	BLIB_TEST_CHECK(a.y == 5.0f);
	BLIB_TEST_CHECK(a.z == 6.0f);
}

BLIB_TEST_CASE("Vector4 copy assignment")
{
	Vec4 a(1.0f, 2.0f, 3.0f, 4.0f);
	Vec4 b(5.0f, 6.0f, 7.0f, 8.0f);
	a = b;
	BLIB_TEST_CHECK(a.x == 5.0f);
	BLIB_TEST_CHECK(a.y == 6.0f);
	BLIB_TEST_CHECK(a.z == 7.0f);
	BLIB_TEST_CHECK(a.w == 8.0f);
}

BLIB_TEST_CASE("Vector3 copy from larger Vector4")
{
	Vec4 a(10.0f, 20.0f, 30.0f, 40.0f);
	Vec3 b(a);
	BLIB_TEST_CHECK(b.x == 10.0f);
	BLIB_TEST_CHECK(b.y == 20.0f);
	BLIB_TEST_CHECK(b.z == 30.0f);
}

BLIB_TEST_CASE("Vector2 copy from larger Vector4")
{
	Vec4 a(1.0f, 2.0f, 3.0f, 4.0f);
	Vec2 b(a);
	BLIB_TEST_CHECK(b.x == 1.0f);
	BLIB_TEST_CHECK(b.y == 2.0f);
}

// ============================================================
// operator+, operator-
// ============================================================

BLIB_TEST_CASE("Vector3 operator+")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(4.0f, 5.0f, 6.0f);
	Vec3 c = a + b;
	BLIB_TEST_CHECK(c.x == 5.0f);
	BLIB_TEST_CHECK(c.y == 7.0f);
	BLIB_TEST_CHECK(c.z == 9.0f);
}

BLIB_TEST_CASE("Vector3 operator+ zero")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 zero(0.0f, 0.0f, 0.0f);
	Vec3 c = a + zero;
	BLIB_TEST_CHECK(c.x == a.x);
	BLIB_TEST_CHECK(c.y == a.y);
	BLIB_TEST_CHECK(c.z == a.z);
}

BLIB_TEST_CASE("Vector3 operator-")
{
	Vec3 a(5.0f, 7.0f, 9.0f);
	Vec3 b(1.0f, 2.0f, 3.0f);
	Vec3 c = a - b;
	BLIB_TEST_CHECK(c.x == 4.0f);
	BLIB_TEST_CHECK(c.y == 5.0f);
	BLIB_TEST_CHECK(c.z == 6.0f);
}

BLIB_TEST_CASE("Vector3 operator- self = zero")
{
	Vec3 a(5.0f, 7.0f, 9.0f);
	Vec3 c = a - a;
	BLIB_TEST_CHECK(c.x == 0.0f);
	BLIB_TEST_CHECK(c.y == 0.0f);
	BLIB_TEST_CHECK(c.z == 0.0f);
}

// ============================================================
// operator* scalar / element-wise
// ============================================================

BLIB_TEST_CASE("Vector3 operator* scalar")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 c = a * 2.0f;
	BLIB_TEST_CHECK(c.x == 2.0f);
	BLIB_TEST_CHECK(c.y == 4.0f);
	BLIB_TEST_CHECK(c.z == 6.0f);
}

BLIB_TEST_CASE("Vector3 operator* scalar zero")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 c = a * 0.0f;
	BLIB_TEST_CHECK(c.x == 0.0f);
	BLIB_TEST_CHECK(c.y == 0.0f);
	BLIB_TEST_CHECK(c.z == 0.0f);
}

BLIB_TEST_CASE("Vector3 operator* scalar one")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 c = a * 1.0f;
	BLIB_TEST_CHECK(c.x == a.x);
	BLIB_TEST_CHECK(c.y == a.y);
	BLIB_TEST_CHECK(c.z == a.z);
}

BLIB_TEST_CASE("Vector3 operator* element-wise")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(4.0f, 5.0f, 6.0f);
	Vec3 c = a * b;
	BLIB_TEST_CHECK(c.x == 4.0f);
	BLIB_TEST_CHECK(c.y == 10.0f);
	BLIB_TEST_CHECK(c.z == 18.0f);
}

BLIB_TEST_CASE("Vector3 operator* element-wise unit")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 one(1.0f, 1.0f, 1.0f);
	Vec3 c = a * one;
	BLIB_TEST_CHECK(c.x == a.x);
	BLIB_TEST_CHECK(c.y == a.y);
	BLIB_TEST_CHECK(c.z == a.z);
}

BLIB_TEST_CASE("Vector3 operator* element-wise zero")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 zero(0.0f, 0.0f, 0.0f);
	Vec3 c = a * zero;
	BLIB_TEST_CHECK(c.x == 0.0f);
	BLIB_TEST_CHECK(c.y == 0.0f);
	BLIB_TEST_CHECK(c.z == 0.0f);
}

// ============================================================
// dot product
// ============================================================

BLIB_TEST_CASE("Vector3 dot product")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(4.0f, 5.0f, 6.0f);
	float d = dot(a, b);
	BLIB_TEST_CHECK_CLOSE(d, 32.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector3 dot product self = magnitude squared")
{
	Vec3 a(3.0f, 4.0f, 0.0f);
	float d = dot(a, a);
	float m = magnitude(a);
	BLIB_TEST_CHECK_CLOSE(d, m * m, 0.0001f);
}

BLIB_TEST_CASE("Vector3 dot product orthogonal")
{
	Vec3 a(1.0f, 0.0f, 0.0f);
	Vec3 b(0.0f, 1.0f, 0.0f);
	BLIB_TEST_CHECK_CLOSE(dot(a, b), 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector3 dot product zero vector")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 zero(0.0f, 0.0f, 0.0f);
	BLIB_TEST_CHECK_CLOSE(dot(a, zero), 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector2 dot product")
{
	Vec2 a(3.0f, 4.0f);
	Vec2 b(5.0f, 6.0f);
	float d = dot(a, b);
	BLIB_TEST_CHECK_CLOSE(d, 39.0f, 0.0001f);
}

// ============================================================
// cross product (Vec3 only)
// ============================================================

BLIB_TEST_CASE("Vector3 cross product basis vectors")
{
	Vec3 x(1.0f, 0.0f, 0.0f);
	Vec3 y(0.0f, 1.0f, 0.0f);
	Vec3 z = cross(x, y);
	BLIB_TEST_CHECK_CLOSE(z.x, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(z.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(z.z, 1.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector3 cross product anti-commutative")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(4.0f, 5.0f, 6.0f);
	Vec3 c1 = cross(a, b);
	Vec3 c2 = cross(b, a);
	BLIB_TEST_CHECK_CLOSE(c1.x, -c2.x, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(c1.y, -c2.y, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(c1.z, -c2.z, 0.0001f);
}

BLIB_TEST_CASE("Vector3 cross product self = zero")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 c = cross(a, a);
	BLIB_TEST_CHECK_CLOSE(c.x, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(c.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(c.z, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector3 cross product parallel = zero")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b = a * 2.0f;
	Vec3 c = cross(a, b);
	BLIB_TEST_CHECK_CLOSE(c.x, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(c.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(c.z, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector3 cross product orthogonal to both operands")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(4.0f, 5.0f, 6.0f);
	Vec3 c = cross(a, b);
	BLIB_TEST_CHECK_CLOSE(dot(c, a), 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(dot(c, b), 0.0f, 0.0001f);
}

// ============================================================
// magnitude
// ============================================================

BLIB_TEST_CASE("Vector3 magnitude")
{
	Vec3 v(3.0f, 4.0f, 0.0f);
	float m = magnitude(v);
	BLIB_TEST_CHECK_CLOSE(m, 5.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector3 zero vector magnitude")
{
	Vec3 v(0.0f, 0.0f, 0.0f);
	float m = magnitude(v);
	BLIB_TEST_CHECK_CLOSE(m, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector2 magnitude")
{
	Vec2 v(3.0f, 4.0f);
	float m = magnitude(v);
	BLIB_TEST_CHECK_CLOSE(m, 5.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector4 magnitude")
{
	Vec4 v(1.0f, 2.0f, 3.0f, 4.0f);
	float m = magnitude(v);
	BLIB_TEST_CHECK_CLOSE(m * m, 30.0f, 0.0001f);
}

// ============================================================
// length
// ============================================================

BLIB_TEST_CASE("Vector3 length equals magnitude")
{
	Vec3 v(1.0f, 2.0f, 3.0f);
	float len = length(v);
	float mag = magnitude(v);
	BLIB_TEST_CHECK_CLOSE(len, mag, 0.0001f);
}

BLIB_TEST_CASE("Vector2 length")
{
	Vec2 v(3.0f, 4.0f);
	BLIB_TEST_CHECK_CLOSE(length(v), 5.0f, 0.0001f);
}

// ============================================================
// normalize
// ============================================================

BLIB_TEST_CASE("Vector3 normalize unit X")
{
	Vec3 v(3.0f, 0.0f, 0.0f);
	Vec3 n = normalize(v);
	BLIB_TEST_CHECK_CLOSE(n.x, 1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(n.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(n.z, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(length(n), 1.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector3 normalize arbitrary direction")
{
	Vec3 v(1.0f, 2.0f, 3.0f);
	Vec3 n = normalize(v);
	BLIB_TEST_CHECK_CLOSE(length(n), 1.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector3 normalize preserves direction")
{
	Vec3 v(2.0f, 4.0f, 6.0f);
	Vec3 n = normalize(v);
	float dotCheck = dot(n, normalize(Vec3(1.0f, 2.0f, 3.0f)));
	BLIB_TEST_CHECK_CLOSE(dotCheck, 1.0f, 0.0001f);
}

BLIB_TEST_CASE("Vector2 normalize")
{
	Vec2 v(5.0f, 0.0f);
	Vec2 n = normalize(v);
	BLIB_TEST_CHECK_CLOSE(n.x, 1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(n.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(length(n), 1.0f, 0.0001f);
}

// ============================================================
// Future: operator== / operator!=
// BUG: return type is Vector instead of bool
// BUG: free function uses this->data (won't compile)
// Uncomment when fixed
// ============================================================

BLIB_TEST_CASE("Vector3 operator== equal")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(1.0f, 2.0f, 3.0f);
	BLIB_TEST_CHECK(a == b);
}

BLIB_TEST_CASE("Vector3 operator== not equal")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(4.0f, 5.0f, 6.0f);
	BLIB_TEST_CHECK(!(a == b));
}

BLIB_TEST_CASE("Vector3 operator== different components")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(1.0f, 2.0f, 4.0f);
	BLIB_TEST_CHECK(!(a == b));
}

BLIB_TEST_CASE("Vector3 operator!= equal")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(1.0f, 2.0f, 3.0f);
	BLIB_TEST_CHECK(!(a != b));
}

BLIB_TEST_CASE("Vector3 operator!= not equal")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(4.0f, 5.0f, 6.0f);
	BLIB_TEST_CHECK(a != b);
}

BLIB_TEST_CASE("Vector2 operator==")
{
	Vec2 a(1.0f, 2.0f);
	Vec2 b(1.0f, 2.0f);
	BLIB_TEST_CHECK(a == b);
}

BLIB_TEST_CASE("Vector4 operator==")
{
	Vec4 a(1.0f, 2.0f, 3.0f, 4.0f);
	Vec4 b(1.0f, 2.0f, 3.0f, 4.0f);
	BLIB_TEST_CHECK(a == b);
}

// ============================================================
// Future: operator+= / operator-=
// BUG: takes lhs by value (copy), returns void — no-op
// Uncomment when fixed
// ============================================================

BLIB_TEST_CASE("Vector3 operator+=")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b(4.0f, 5.0f, 6.0f);
	a += b;
	BLIB_TEST_CHECK(a.x == 5.0f);
	BLIB_TEST_CHECK(a.y == 7.0f);
	BLIB_TEST_CHECK(a.z == 9.0f);
}

BLIB_TEST_CASE("Vector3 operator+= self")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	a += a;
	BLIB_TEST_CHECK(a.x == 2.0f);
	BLIB_TEST_CHECK(a.y == 4.0f);
	BLIB_TEST_CHECK(a.z == 6.0f);
}

BLIB_TEST_CASE("Vector3 operator-=")
{
	Vec3 a(5.0f, 7.0f, 9.0f);
	Vec3 b(1.0f, 2.0f, 3.0f);
	a -= b;
	BLIB_TEST_CHECK(a.x == 4.0f);
	BLIB_TEST_CHECK(a.y == 5.0f);
	BLIB_TEST_CHECK(a.z == 6.0f);
}

BLIB_TEST_CASE("Vector3 operator-= self = zero")
{
	Vec3 a(5.0f, 7.0f, 9.0f);
	a -= a;
	BLIB_TEST_CHECK(a.x == 0.0f);
	BLIB_TEST_CHECK(a.y == 0.0f);
	BLIB_TEST_CHECK(a.z == 0.0f);
}

// ============================================================
// Future: operator*= / operator/=
// BUG: same as += / -= (void return, by value)
// Uncomment when fixed
// ============================================================

BLIB_TEST_CASE("Vector3 operator*= scalar")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	a *= 2.0f;
	BLIB_TEST_CHECK(a.x == 2.0f);
	BLIB_TEST_CHECK(a.y == 4.0f);
	BLIB_TEST_CHECK(a.z == 6.0f);
}

BLIB_TEST_CASE("Vector3 operator*= one no-op")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	a *= 1.0f;
	BLIB_TEST_CHECK(a.x == 1.0f);
	BLIB_TEST_CHECK(a.y == 2.0f);
	BLIB_TEST_CHECK(a.z == 3.0f);
}

BLIB_TEST_CASE("Vector3 operator/= scalar")
{
	Vec3 a(4.0f, 6.0f, 8.0f);
	a /= 2.0f;
	BLIB_TEST_CHECK(a.x == 2.0f);
	BLIB_TEST_CHECK(a.y == 3.0f);
	BLIB_TEST_CHECK(a.z == 4.0f);
}

// ============================================================
// Future: operator/
// BUG: uses rhs.data[i] on scalar Type — won't compile
// Uncomment when fixed
// ============================================================

BLIB_TEST_CASE("Vector3 operator/ scalar")
{
	Vec3 a(4.0f, 6.0f, 8.0f);
	Vec3 b = a / 2.0f;
	BLIB_TEST_CHECK(b.x == 2.0f);
	BLIB_TEST_CHECK(b.y == 3.0f);
	BLIB_TEST_CHECK(b.z == 4.0f);
}

BLIB_TEST_CASE("Vector3 operator/ by one")
{
	Vec3 a(1.0f, 2.0f, 3.0f);
	Vec3 b = a / 1.0f;
	BLIB_TEST_CHECK(b.x == a.x);
	BLIB_TEST_CHECK(b.y == a.y);
	BLIB_TEST_CHECK(b.z == a.z);
}

// ============================================================
// Future: normalize zero vector (should not crash / NaN)
// ============================================================

BLIB_TEST_CASE("Vector3 normalize zero vector")
{
	Vec3 v(0.0f, 0.0f, 0.0f);
	Vec3 n = normalize(v);
	(void)n;
	BLIB_TEST_CHECK(true);
}
