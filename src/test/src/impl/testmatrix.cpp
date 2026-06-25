#include <test/src/test.h>
#include <blib/math/matrix.h>

using namespace blib::math;

typedef Matrix<float, 4, 4> Mat4;
typedef Matrix<float, 3, 3> Mat3;
typedef Matrix<float, 2, 3> Mat2x3;
typedef Matrix<float, 3, 2> Mat3x2;
typedef Matrix<float, 2, 2> Mat2;

// ============================================================
// Default constructor (loads identity)
// ============================================================

BLIB_TEST_CASE("Matrix4x4 default constructor loads identity")
{
	Mat4 m;
	for (matrixSizeT i = 0; i < 4; ++i)
	{
		for (matrixSizeT j = 0; j < 4; ++j)
		{
			if (i == j)
				BLIB_TEST_CHECK(m.data[i][j] == 1.0f);
			else
				BLIB_TEST_CHECK(m.data[i][j] == 0.0f);
		}
	}
}

BLIB_TEST_CASE("Matrix3x3 default constructor loads identity")
{
	Mat3 m;
	for (matrixSizeT i = 0; i < 3; ++i)
	{
		for (matrixSizeT j = 0; j < 3; ++j)
		{
			if (i == j)
				BLIB_TEST_CHECK(m.data[i][j] == 1.0f);
			else
				BLIB_TEST_CHECK(m.data[i][j] == 0.0f);
		}
	}
}

BLIB_TEST_CASE("Matrix2x2 default constructor loads identity")
{
	Mat2 m;
	BLIB_TEST_CHECK(m.data[0][0] == 1.0f);
	BLIB_TEST_CHECK(m.data[0][1] == 0.0f);
	BLIB_TEST_CHECK(m.data[1][0] == 0.0f);
	BLIB_TEST_CHECK(m.data[1][1] == 1.0f);
}

// ============================================================
// loadIdentity
// ============================================================

BLIB_TEST_CASE("Matrix4x4 loadIdentity")
{
	Mat4 m;
	m.loadIdentity();
	for (matrixSizeT i = 0; i < 4; ++i)
	{
		for (matrixSizeT j = 0; j < 4; ++j)
		{
			if (i == j)
				BLIB_TEST_CHECK(m.data[i][j] == 1.0f);
			else
				BLIB_TEST_CHECK(m.data[i][j] == 0.0f);
		}
	}
}

BLIB_TEST_CASE("Matrix4x4 loadIdentity overwrites previous data")
{
	Mat4 m;
	m.data[0][0] = 42.0f;
	m.data[1][2] = 13.0f;
	m.loadIdentity();
	BLIB_TEST_CHECK(m.data[0][0] == 1.0f);
	BLIB_TEST_CHECK(m.data[1][2] == 0.0f);
}

// ============================================================
// Constructor from initializer_list
// ============================================================

BLIB_TEST_CASE("Matrix2x2 from initializer_list")
{
	Mat2 m({
		1.0f, 2.0f,
		3.0f, 4.0f
	});
	BLIB_TEST_CHECK(m.data[0][0] == 1.0f);
	BLIB_TEST_CHECK(m.data[1][0] == 2.0f);
	BLIB_TEST_CHECK(m.data[0][1] == 3.0f);
	BLIB_TEST_CHECK(m.data[1][1] == 4.0f);
}

BLIB_TEST_CASE("Matrix3x3 from initializer_list")
{
	Mat3 m({
		1.0f, 2.0f, 3.0f,
		4.0f, 5.0f, 6.0f,
		7.0f, 8.0f, 9.0f
	});
	BLIB_TEST_CHECK(m.data[0][0] == 1.0f);
	BLIB_TEST_CHECK(m.data[1][0] == 2.0f);
	BLIB_TEST_CHECK(m.data[2][0] == 3.0f);
	BLIB_TEST_CHECK(m.data[0][1] == 4.0f);
	BLIB_TEST_CHECK(m.data[1][1] == 5.0f);
	BLIB_TEST_CHECK(m.data[2][1] == 6.0f);
	BLIB_TEST_CHECK(m.data[0][2] == 7.0f);
	BLIB_TEST_CHECK(m.data[1][2] == 8.0f);
	BLIB_TEST_CHECK(m.data[2][2] == 9.0f);
}

BLIB_TEST_CASE("Matrix4x4 initializer_list throws on wrong count")
{
	BLIB_TEST_REQUIRE_THROWS(
		Mat4({ 1.0f, 2.0f, 3.0f }),
		std::runtime_error
	);
}

BLIB_TEST_CASE("Matrix4x4 initializer_list throws on too many")
{
	BLIB_TEST_REQUIRE_THROWS(
		Mat4({
			1,2,3,4, 5,6,7,8,
			9,10,11,12, 13,14,15,16,
			17
		}),
		std::runtime_error
	);
}

// ============================================================
// Transpose
// ============================================================

BLIB_TEST_CASE("Matrix3x3 Transpose")
{
	Mat3 m({
		1.0f, 2.0f, 3.0f,
		4.0f, 5.0f, 6.0f,
		7.0f, 8.0f, 9.0f
	});
	Mat3 t = m.Transpose();
	BLIB_TEST_CHECK(t.data[0][0] == 1.0f);
	BLIB_TEST_CHECK(t.data[0][1] == 2.0f);
	BLIB_TEST_CHECK(t.data[0][2] == 3.0f);
	BLIB_TEST_CHECK(t.data[1][0] == 4.0f);
	BLIB_TEST_CHECK(t.data[1][1] == 5.0f);
	BLIB_TEST_CHECK(t.data[1][2] == 6.0f);
	BLIB_TEST_CHECK(t.data[2][0] == 7.0f);
	BLIB_TEST_CHECK(t.data[2][1] == 8.0f);
	BLIB_TEST_CHECK(t.data[2][2] == 9.0f);
}

BLIB_TEST_CASE("Matrix2x3 Transpose produces 3x2")
{
	Mat2x3 m({
		1.0f, 2.0f,
		3.0f, 4.0f,
		5.0f, 6.0f
	});
	Mat3x2 t = m.Transpose();
	BLIB_TEST_CHECK(t.data[0][0] == 1.0f);
	BLIB_TEST_CHECK(t.data[0][1] == 2.0f);
	BLIB_TEST_CHECK(t.data[1][0] == 3.0f);
	BLIB_TEST_CHECK(t.data[1][1] == 4.0f);
	BLIB_TEST_CHECK(t.data[2][0] == 5.0f);
	BLIB_TEST_CHECK(t.data[2][1] == 6.0f);
}

BLIB_TEST_CASE("Matrix identity Transpose = identity")
{
	Mat4 m;
	Mat4 t = m.Transpose();
	for (matrixSizeT i = 0; i < 4; ++i)
	{
		for (matrixSizeT j = 0; j < 4; ++j)
		{
			if (i == j)
				BLIB_TEST_CHECK(t.data[i][j] == 1.0f);
			else
				BLIB_TEST_CHECK(t.data[i][j] == 0.0f);
		}
	}
}

// ============================================================
// operator=
// ============================================================

BLIB_TEST_CASE("Matrix4x4 operator=")
{
	Mat4 a;
	Mat4 b;
	a.data[0][0] = 42.0f;
	a.data[1][2] = 13.0f;
	b = a;
	BLIB_TEST_CHECK(b.data[0][0] == 42.0f);
	BLIB_TEST_CHECK(b.data[1][2] == 13.0f);
}

BLIB_TEST_CASE("Matrix4x4 operator= self")
{
	Mat4 a;
	a.data[0][0] = 42.0f;
	a = a;
	BLIB_TEST_CHECK(a.data[0][0] == 42.0f);
}

// ============================================================
// Future: operator+
// BUG: throw new (pointer throw), rhs.columnsCount undefined
// Uncomment when fixed
// ============================================================

BLIB_TEST_CASE("Matrix2x2 operator+")
{
	Mat2 a({ 1.0f, 2.0f, 3.0f, 4.0f });
	Mat2 b({ 5.0f, 6.0f, 7.0f, 8.0f });
	Mat2 c = a + b;
	BLIB_TEST_CHECK(c.data[0][0] == 6.0f);
	BLIB_TEST_CHECK(c.data[1][0] == 8.0f);
	BLIB_TEST_CHECK(c.data[0][1] == 10.0f);
	BLIB_TEST_CHECK(c.data[1][1] == 12.0f);
}

BLIB_TEST_CASE("Matrix2x2 operator+ zero")
{
	Mat2 a({ 1.0f, 2.0f, 3.0f, 4.0f });
	Mat2 zero({ 0.0f, 0.0f, 0.0f, 0.0f });
	Mat2 c = a + zero;
	BLIB_TEST_CHECK(c.data[0][0] == a.data[0][0]);
	BLIB_TEST_CHECK(c.data[1][0] == a.data[1][0]);
	BLIB_TEST_CHECK(c.data[0][1] == a.data[0][1]);
	BLIB_TEST_CHECK(c.data[1][1] == a.data[1][1]);
}

BLIB_TEST_CASE("Matrix2x2 operator+ commutativity")
{
	Mat2 a({ 1.0f, 2.0f, 3.0f, 4.0f });
	Mat2 b({ 5.0f, 6.0f, 7.0f, 8.0f });
	Mat2 c1 = a + b;
	Mat2 c2 = b + a;
	BLIB_TEST_CHECK(c1.data[0][0] == c2.data[0][0]);
	BLIB_TEST_CHECK(c1.data[1][0] == c2.data[1][0]);
	BLIB_TEST_CHECK(c1.data[0][1] == c2.data[0][1]);
	BLIB_TEST_CHECK(c1.data[1][1] == c2.data[1][1]);
}

// ============================================================
// Future: operator-
// BUG: same as operator+ (throw new, columnsCount)
// Uncomment when fixed
// ============================================================

BLIB_TEST_CASE("Matrix2x2 operator-")
{
	Mat2 a({ 9.0f, 8.0f, 7.0f, 6.0f });
	Mat2 b({ 1.0f, 2.0f, 3.0f, 4.0f });
	Mat2 c = a - b;
	BLIB_TEST_CHECK(c.data[0][0] == 8.0f);
	BLIB_TEST_CHECK(c.data[1][0] == 6.0f);
	BLIB_TEST_CHECK(c.data[0][1] == 4.0f);
	BLIB_TEST_CHECK(c.data[1][1] == 2.0f);
}

BLIB_TEST_CASE("Matrix2x2 operator- self = zero")
{
	Mat2 a({ 1.0f, 2.0f, 3.0f, 4.0f });
	Mat2 c = a - a;
	BLIB_TEST_CHECK(c.data[0][0] == 0.0f);
	BLIB_TEST_CHECK(c.data[1][0] == 0.0f);
	BLIB_TEST_CHECK(c.data[0][1] == 0.0f);
	BLIB_TEST_CHECK(c.data[1][1] == 0.0f);
}

// ============================================================
// Future: operator* (matrix multiplication)
// BUG: throw new (pointer throw)
// Uncomment when fixed
// ============================================================

BLIB_TEST_CASE("Matrix2x2 operator*")
{
	Mat2 a({ 1.0f, 2.0f, 3.0f, 4.0f });
	Mat2 b({ 5.0f, 6.0f, 7.0f, 8.0f });
	Mat2 c = a * b;
	BLIB_TEST_CHECK(c.data[0][0] == 19.0f);
	BLIB_TEST_CHECK(c.data[1][0] == 22.0f);
	BLIB_TEST_CHECK(c.data[0][1] == 43.0f);
	BLIB_TEST_CHECK(c.data[1][1] == 50.0f);
}

BLIB_TEST_CASE("Matrix4x4 operator* identity left")
{
	Mat4 id;
	Mat4 m;
	m.data[0][0] = 5.0f;
	m.data[1][2] = 3.0f;
	Mat4 c = id * m;
	BLIB_TEST_CHECK(c.data[0][0] == 5.0f);
	BLIB_TEST_CHECK(c.data[1][2] == 3.0f);
}

BLIB_TEST_CASE("Matrix4x4 operator* identity right")
{
	Mat4 id;
	Mat4 m;
	m.data[0][0] = 5.0f;
	m.data[1][2] = 3.0f;
	Mat4 c = m * id;
	BLIB_TEST_CHECK(c.data[0][0] == 5.0f);
	BLIB_TEST_CHECK(c.data[1][2] == 3.0f);
}

BLIB_TEST_CASE("Matrix2x3 times Matrix3x2 gives 2x2")
{
	Mat2x3 a({ 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f });
	Mat3x2 b({ 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f });
	auto c = a * b;
	BLIB_TEST_CHECK(c.data[0][0] == 27.0f);
	BLIB_TEST_CHECK(c.data[0][1] == 61.0f);
	BLIB_TEST_CHECK(c.data[1][0] == 30.0f);
	BLIB_TEST_CHECK(c.data[1][1] == 68.0f);
}

BLIB_TEST_CASE("Matrix multiply associativity")
{
	Mat2 a({ 1.0f, 2.0f, 3.0f, 4.0f });
	Mat2 b({ 5.0f, 6.0f, 7.0f, 8.0f });
	Mat2 c({ 9.0f, 10.0f, 11.0f, 12.0f });
	auto ab_c = (a * b) * c;
	auto a_bc = a * (b * c);
	BLIB_TEST_CHECK(ab_c.data[0][0] == a_bc.data[0][0]);
	BLIB_TEST_CHECK(ab_c.data[1][0] == a_bc.data[1][0]);
	BLIB_TEST_CHECK(ab_c.data[0][1] == a_bc.data[0][1]);
	BLIB_TEST_CHECK(ab_c.data[1][1] == a_bc.data[1][1]);
}

BLIB_TEST_CASE("Matrix multiply distributivity")
{
	Mat2 a({ 1.0f, 2.0f, 3.0f, 4.0f });
	Mat2 b({ 5.0f, 6.0f, 7.0f, 8.0f });
	Mat2 c({ 9.0f, 10.0f, 11.0f, 12.0f });
	auto a_bc = a * (b + c);
	auto ab_ac = a * b + a * c;
	BLIB_TEST_CHECK(a_bc.data[0][0] == ab_ac.data[0][0]);
	BLIB_TEST_CHECK(a_bc.data[1][0] == ab_ac.data[1][0]);
	BLIB_TEST_CHECK(a_bc.data[0][1] == ab_ac.data[0][1]);
	BLIB_TEST_CHECK(a_bc.data[1][1] == ab_ac.data[1][1]);
}

// ============================================================
// Future: Transpose(Transpose(M)) == M
// ============================================================

BLIB_TEST_CASE("Matrix3x3 Transpose roundtrip")
{
	Mat3 m({
		1.0f, 2.0f, 3.0f,
		4.0f, 5.0f, 6.0f,
		7.0f, 8.0f, 9.0f
	});
	Mat3 m2 = m.Transpose().Transpose();
	for (matrixSizeT i = 0; i < 3; ++i)
	{
		for (matrixSizeT j = 0; j < 3; ++j)
		{
			BLIB_TEST_CHECK(m2.data[i][j] == m.data[i][j]);
		}
	}
}
