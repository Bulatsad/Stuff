#include <test/src/test.h>
#include <blib/math/quaternion.h>
#include <blib/math/angle.h>
#include <blib/math/consts.h>

using namespace blib::math;

BLIB_TEST_CASE("Quaternion 4-component constructor")
{
	Quaternion<float> q(1.0f, 0.0f, 0.0f, 0.0f);
	BLIB_TEST_CHECK(q.w == 1.0f);
	BLIB_TEST_CHECK(q.x == 0.0f);
	BLIB_TEST_CHECK(q.y == 0.0f);
	BLIB_TEST_CHECK(q.z == 0.0f);
}

BLIB_TEST_CASE("Quaternion 4-component arbitrary values")
{
	Quaternion<float> q(1.0f, 2.0f, 3.0f, 4.0f);
	BLIB_TEST_CHECK(q.w == 1.0f);
	BLIB_TEST_CHECK(q.x == 2.0f);
	BLIB_TEST_CHECK(q.y == 3.0f);
	BLIB_TEST_CHECK(q.z == 4.0f);
}

BLIB_TEST_CASE("Quaternion identity")
{
	Quaternion<float> q(1.0f, 0.0f, 0.0f, 0.0f);
	BLIB_TEST_CHECK(q.w == 1.0f);
	BLIB_TEST_CHECK(q.x == 0.0f);
	BLIB_TEST_CHECK(q.y == 0.0f);
	BLIB_TEST_CHECK(q.z == 0.0f);
}

BLIB_TEST_CASE("Quaternion scalar + axis constructor")
{
	Vector<float, 3> axis(0.0f, 0.0f, 1.0f);
	Quaternion<float> q(1.0f, axis);
	BLIB_TEST_CHECK(q.w == 1.0f);
	BLIB_TEST_CHECK(q.x == 0.0f);
	BLIB_TEST_CHECK(q.y == 0.0f);
	BLIB_TEST_CHECK(q.z == 1.0f);
}

BLIB_TEST_CASE("Quaternion scalar + axis, zero scalar")
{
	Vector<float, 3> axis(1.0f, 2.0f, 3.0f);
	Quaternion<float> q(0.0f, axis);
	BLIB_TEST_CHECK(q.w == 0.0f);
	BLIB_TEST_CHECK(q.x == 1.0f);
	BLIB_TEST_CHECK(q.y == 2.0f);
	BLIB_TEST_CHECK(q.z == 3.0f);
}

BLIB_TEST_CASE("Quaternion axis-angle 180 around X")
{
	Vector<float, 3> axis(1.0f, 0.0f, 0.0f);
	AngleRadian<float> angle(static_cast<float>(pi));
	Quaternion<float> q(angle, axis);
	BLIB_TEST_CHECK_CLOSE(q.w, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.x, 1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.z, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion axis-angle 90 around Z")
{
	Vector<float, 3> axis(0.0f, 0.0f, 1.0f);
	AngleRadian<float> angle(static_cast<float>(pi / 2.0));
	Quaternion<float> q(angle, axis);
	float sin45 = static_cast<float>(std::sin(pi / 4.0));
	BLIB_TEST_CHECK_CLOSE(q.w, sin45, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.x, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.z, sin45, 0.0001f);
}

BLIB_TEST_CASE("Quaternion axis-angle zero rotation = identity")
{
	Vector<float, 3> axis(1.0f, 0.0f, 0.0f);
	AngleRadian<float> angle(0.0f);
	Quaternion<float> q(angle, axis);
	BLIB_TEST_CHECK_CLOSE(q.w, 1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.x, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.z, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion axis-angle degrees 180 around Y")
{
	Vector<float, 3> axis(0.0f, 1.0f, 0.0f);
	AngleDegree<float> angle(180.0f);
	Quaternion<float> q(angle, axis);
	BLIB_TEST_CHECK_CLOSE(q.w, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.x, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.y, 1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.z, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion axis-angle degrees 0 = identity")
{
	Vector<float, 3> axis(0.0f, 0.0f, 1.0f);
	AngleDegree<float> angle(0.0f);
	Quaternion<float> q(angle, axis);
	BLIB_TEST_CHECK_CLOSE(q.w, 1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.x, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q.z, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion normalize identity stays identity")
{
	Quaternion<float> q(1.0f, 0.0f, 0.0f, 0.0f);
	Quaternion<float> n = q.normalize();
	BLIB_TEST_CHECK_CLOSE(n.w, 1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(n.x, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(n.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(n.z, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion normalize scales to unit length")
{
	Quaternion<float> q(2.0f, 4.0f, 4.0f, 4.0f);
	Quaternion<float> n = q.normalize();
	float len = std::sqrt(n.w * n.w + n.x * n.x + n.y * n.y + n.z * n.z);
	BLIB_TEST_CHECK_CLOSE(len, 1.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion conjugate")
{
	Quaternion<float> q(1.0f, 2.0f, 3.0f, 4.0f);
	Quaternion<float> c = q.conjugate();
	BLIB_TEST_CHECK(c.w == q.w);
	BLIB_TEST_CHECK(c.x == -q.x);
	BLIB_TEST_CHECK(c.y == -q.y);
	BLIB_TEST_CHECK(c.z == -q.z);
}

BLIB_TEST_CASE("Quaternion conjugate identity = identity")
{
	Quaternion<float> q(1.0f, 0.0f, 0.0f, 0.0f);
	Quaternion<float> c = q.conjugate();
	BLIB_TEST_CHECK(c.w == 1.0f);
	BLIB_TEST_CHECK(c.x == 0.0f);
	BLIB_TEST_CHECK(c.y == 0.0f);
	BLIB_TEST_CHECK(c.z == 0.0f);
}

BLIB_TEST_CASE("Quaternion multiply identity left")
{
	Quaternion<float> id(1.0f, 0.0f, 0.0f, 0.0f);
	Quaternion<float> q(2.0f, 3.0f, 4.0f, 5.0f);
	Quaternion<float> r = id * q;
	BLIB_TEST_CHECK(r.w == q.w);
	BLIB_TEST_CHECK(r.x == q.x);
	BLIB_TEST_CHECK(r.y == q.y);
	BLIB_TEST_CHECK(r.z == q.z);
}

BLIB_TEST_CASE("Quaternion multiply identity right")
{
	Quaternion<float> id(1.0f, 0.0f, 0.0f, 0.0f);
	Quaternion<float> q(2.0f, 3.0f, 4.0f, 5.0f);
	Quaternion<float> r = q * id;
	BLIB_TEST_CHECK(r.w == q.w);
	BLIB_TEST_CHECK(r.x == q.x);
	BLIB_TEST_CHECK(r.y == q.y);
	BLIB_TEST_CHECK(r.z == q.z);
}

BLIB_TEST_CASE("Quaternion multiply two 90-degree rotations about X")
{
	Vector<float, 3> axis(1.0f, 0.0f, 0.0f);
	AngleRadian<float> halfPi(static_cast<float>(pi / 2.0));
	Quaternion<float> q1(halfPi, axis);
	Quaternion<float> q2 = q1 * q1;
	BLIB_TEST_CHECK_CLOSE(q2.w, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q2.x, 1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q2.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(q2.z, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion inverse times self = identity")
{
	Quaternion<float> q(1.0f, 2.0f, 3.0f, 4.0f);
	q = q.normalize();
	Quaternion<float> inv = q.inverse();
	Quaternion<float> r = q * inv;
	BLIB_TEST_CHECK_CLOSE(r.w, 1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.x, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.y, 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.z, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion rotate vector 90 around Z")
{
	Vector<float, 3> axis(0.0f, 0.0f, 1.0f);
	AngleRadian<float> angle(static_cast<float>(pi / 2.0));
	Quaternion<float> q(angle, axis);
	Vector<float, 3> v(1.0f, 0.0f, 0.0f);
	Vector<float, 3> r = rotate(v, q);
	BLIB_TEST_CHECK_CLOSE(r.data[0], 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.data[1], 1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.data[2], 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion rotate vector 180 around Y")
{
	Vector<float, 3> axis(0.0f, 1.0f, 0.0f);
	AngleRadian<float> angle(static_cast<float>(pi));
	Quaternion<float> q(angle, axis);
	Vector<float, 3> v(1.0f, 0.0f, 0.0f);
	Vector<float, 3> r = rotate(v, q);
	BLIB_TEST_CHECK_CLOSE(r.data[0], -1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.data[1], 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.data[2], 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion rotate zero vector")
{
	Vector<float, 3> axis(0.0f, 0.0f, 1.0f);
	AngleRadian<float> angle(static_cast<float>(pi / 2.0));
	Quaternion<float> q(angle, axis);
	Vector<float, 3> v(0.0f, 0.0f, 0.0f);
	Vector<float, 3> r = rotate(v, q);
	BLIB_TEST_CHECK_CLOSE(r.data[0], 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.data[1], 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.data[2], 0.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion rotate identity preserves vector")
{
	Quaternion<float> q(1.0f, 0.0f, 0.0f, 0.0f);
	Vector<float, 3> v(3.0f, 5.0f, 7.0f);
	Vector<float, 3> r = rotate(v, q);
	BLIB_TEST_CHECK_CLOSE(r.data[0], 3.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.data[1], 5.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.data[2], 7.0f, 0.0001f);
}

BLIB_TEST_CASE("Quaternion rotate roundtrip 90 around Z")
{
	Vector<float, 3> axis(0.0f, 0.0f, 1.0f);
	AngleRadian<float> angle(static_cast<float>(pi / 2.0));
	Quaternion<float> q(angle, axis);
	Vector<float, 3> v(1.0f, 0.0f, 0.0f);
	Vector<float, 3> r = rotate(v, q);
	r = rotate(r, q.conjugate());
	BLIB_TEST_CHECK_CLOSE(r.data[0], 1.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.data[1], 0.0f, 0.0001f);
	BLIB_TEST_CHECK_CLOSE(r.data[2], 0.0f, 0.0001f);
}
