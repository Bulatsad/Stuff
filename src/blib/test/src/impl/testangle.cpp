#include <blib/test/src/test.h>
#include <blib/math/angle.h>
#include <blib/math/consts.h>

using namespace blib::math;

// ============================================================
// Default construction
// ============================================================

BLIB_TEST_CASE("AngleDegree default construction")
{
	AngleDegreef deg;
	BLIB_TEST_CHECK_CLOSE(deg.data, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian default construction")
{
	AngleRadianf rad;
	BLIB_TEST_CHECK_CLOSE(rad.data, 0.0f, 0.0001f);
}

// ============================================================
// Value construction — positive overflow wraps
// ============================================================

BLIB_TEST_CASE("AngleDegree wraps positive overflow")
{
	AngleDegreef deg(450.0f);
	BLIB_TEST_CHECK_CLOSE(deg.data, 90.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleDegree wraps multiple full rotations")
{
	AngleDegreef deg(720.0f + 45.0f);
	BLIB_TEST_CHECK_CLOSE(deg.data, 45.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian wraps to [0, 2*PI)")
{
	AngleRadianf rad(static_cast<float>(4.0 * pi));
	BLIB_TEST_CHECK_CLOSE(rad.data, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian wraps positive overflow")
{
	AngleRadianf rad(static_cast<float>(3.0 * pi));
	BLIB_TEST_CHECK_CLOSE(rad.data, static_cast<float>(pi), 0.0001f);
}

// stdfmod preserves sign: fmod(-90, 360) = -90, NOT 270
BLIB_TEST_CASE("AngleDegree negative value wraps to [0,360)")
{
	AngleDegreef deg(-90.0f);
	BLIB_TEST_CHECK_CLOSE(deg.data, 270.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian negative value wraps to [0,2*PI)")
{
	AngleRadianf rad(static_cast<float>(-pi));
	BLIB_TEST_CHECK_CLOSE(rad.data, static_cast<float>(pi), 0.0001f);
}

// ============================================================
// Boundary values
// ============================================================

BLIB_TEST_CASE("AngleDegree 0 stays 0")
{
	AngleDegreef deg(0.0f);
	BLIB_TEST_CHECK_CLOSE(deg.data, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleDegree 360 wraps to 0")
{
	AngleDegreef deg(360.0f);
	BLIB_TEST_CHECK_CLOSE(deg.data, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian 2*PI wraps to 0")
{
	AngleRadianf rad(static_cast<float>(2.0 * pi));
	BLIB_TEST_CHECK_CLOSE(rad.data, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian 0 stays 0")
{
	AngleRadianf rad(0.0f);
	BLIB_TEST_CHECK_CLOSE(rad.data, 0.0f, 0.0001f);
}

// ============================================================
// Conversion: Degree <-> Radian
// ============================================================

BLIB_TEST_CASE("AngleDegree toRadian conversion: 180 -> PI")
{
	AngleDegreef deg(180.0f);
	AngleRadianf rad = deg.toRadian();
	BLIB_TEST_CHECK_CLOSE(rad.data, static_cast<float>(pi), 0.0001f);
}

BLIB_TEST_CASE("AngleDegree toRadian conversion: 0 -> 0")
{
	AngleDegreef deg(0.0f);
	AngleRadianf rad = deg.toRadian();
	BLIB_TEST_CHECK_CLOSE(rad.data, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleDegree toRadian conversion: 90 -> PI/2")
{
	AngleDegreef deg(90.0f);
	AngleRadianf rad = deg.toRadian();
	BLIB_TEST_CHECK_CLOSE(rad.data, static_cast<float>(pi / 2.0), 0.0001f);
}

BLIB_TEST_CASE("AngleDegree toRadian conversion: 360 -> 0 (2*PI)")
{
	AngleDegreef deg(360.0f);
	AngleRadianf rad = deg.toRadian();
	BLIB_TEST_CHECK_CLOSE(rad.data, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian toDergee conversion: PI -> 180")
{
	AngleRadianf rad(static_cast<float>(pi));
	AngleDegreef deg = rad.toDergee();
	BLIB_TEST_CHECK_CLOSE(deg.data, 180.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian toDergee conversion: 0 -> 0")
{
	AngleRadianf rad(0.0f);
	AngleDegreef deg = rad.toDergee();
	BLIB_TEST_CHECK_CLOSE(deg.data, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian toDergee conversion: PI/2 -> 90")
{
	AngleRadianf rad(static_cast<float>(pi / 2.0));
	AngleDegreef deg = rad.toDergee();
	BLIB_TEST_CHECK_CLOSE(deg.data, 90.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian toDergee conversion: 2*PI -> 0 (360)")
{
	AngleRadianf rad(static_cast<float>(2.0 * pi));
	AngleDegreef deg = rad.toDergee();
	BLIB_TEST_CHECK_CLOSE(deg.data, 0.0f, 0.0001f);
}

// ============================================================
// Roundtrip conversions
// ============================================================

BLIB_TEST_CASE("Angle roundtrip degree -> radian -> degree")
{
	AngleDegreef deg(42.0f);
	AngleRadianf rad = deg.toRadian();
	AngleDegreef deg2 = rad.toDergee();
	BLIB_TEST_CHECK_CLOSE(deg2.data, 42.0f, 0.0001f);
}

BLIB_TEST_CASE("Angle roundtrip radian -> degree -> radian")
{
	AngleRadianf rad(1.0f);
	AngleDegreef deg = rad.toDergee();
	AngleRadianf rad2 = deg.toRadian();
	BLIB_TEST_CHECK_CLOSE(rad2.data, 1.0f, 0.0001f);
}

BLIB_TEST_CASE("Angle roundtrip: 720 degrees through radian")
{
	AngleDegreef deg(720.0f);
	AngleRadianf rad = deg.toRadian();
	AngleDegreef deg2 = rad.toDergee();
	BLIB_TEST_CHECK_CLOSE(deg2.data, 0.0f, 0.0001f);
}

// ============================================================
// Future: negative wrap to [0, 360) / [0, 2*PI)
// Current: stdfmod preserves sign — negative stays negative
// Fix: add 360 (or 2*PI) when result < 0
// ============================================================

BLIB_TEST_CASE("AngleDegree negative wrap to [0, 360)")
{
	AngleDegreef deg(-90.0f);
	BLIB_TEST_CHECK_CLOSE(deg.data, 270.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleDegree large negative wrap")
{
	AngleDegreef deg(-450.0f);
	BLIB_TEST_CHECK_CLOSE(deg.data, 270.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian negative wrap to [0, 2*PI)")
{
	AngleRadianf rad(static_cast<float>(-pi));
	BLIB_TEST_CHECK_CLOSE(rad.data, static_cast<float>(pi), 0.0001f);
}

BLIB_TEST_CASE("AngleRadian large negative wrap")
{
	AngleRadianf rad(static_cast<float>(-3.0 * pi));
	BLIB_TEST_CHECK_CLOSE(rad.data, static_cast<float>(pi), 0.0001f);
}

// ============================================================
// Future: Angle arithmetic (+, -, scalar multiply)
// ============================================================

BLIB_TEST_CASE("AngleDegree operator+")
{
	AngleDegreef a(30.0f);
	AngleDegreef b(40.0f);
	AngleDegreef c = a + b;
	BLIB_TEST_CHECK_CLOSE(c.data, 70.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleDegree operator+ wrap")
{
	AngleDegreef a(300.0f);
	AngleDegreef b(100.0f);
	AngleDegreef c = a + b;
	BLIB_TEST_CHECK_CLOSE(c.data, 40.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleDegree operator-")
{
	AngleDegreef a(100.0f);
	AngleDegreef b(30.0f);
	AngleDegreef c = a - b;
	BLIB_TEST_CHECK_CLOSE(c.data, 70.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleDegree operator* scalar")
{
	AngleDegreef a(90.0f);
	AngleDegreef c = a * 2.0f;
	BLIB_TEST_CHECK_CLOSE(c.data, 180.0f, 0.0001f);
}

BLIB_TEST_CASE("AngleRadian operator+")
{
	AngleRadianf a(static_cast<float>(pi / 2.0));
	AngleRadianf b(static_cast<float>(pi / 2.0));
	AngleRadianf c = a + b;
	BLIB_TEST_CHECK_CLOSE(c.data, static_cast<float>(pi), 0.0001f);
}
