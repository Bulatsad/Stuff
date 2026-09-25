#include <blib/test/src/test.h>

#include <blib/graphics/transformMatrix.h>
#include <blib/graphics/transform.h>
#include <blib/graphics/vector.h>
#include <blib/graphics/isometricCamera.h>

#include <blib/core/math/quaternion.h>
#include <blib/core/math/angle.h>
#include <blib/core/math/consts.h>

// ============================================================
// Конвенция матриц blib (см. CORE.md, «Конвенция матриц»):
// column-major хранилище (data[столбец][строка]), column-vector
// математика (v' = M * v), трансляция 4x4 — в последней колонке
// (data[3][0..2]), загрузка в OpenGL — transpose = GL_FALSE.
// Эти тесты блокируют регресс «транспонированных» матриц:
// раньше graphics-слой хранил матрицы перевёрнутыми и компенсировал
// это GL_TRUE при загрузке в шейдер.
// ============================================================

namespace
{
    constexpr float epsilon = 1e-4f;
}

// ============================================================
// composeMatrix
// ============================================================

BLIB_TEST_CASE("composeMatrix: identity rotation and unit scale puts translation in last column")
{
    const blib::graphics::Vector3f position(5.0f, 6.0f, 7.0f);
    const blib::math::Quaternion<float> rotation(
        blib::math::AngleDegree<float>(0.0f),
        blib::graphics::Vector3f(0.0f, 1.0f, 0.0f));
    const blib::graphics::Vector3f scale(1.0f, 1.0f, 1.0f);

    const blib::graphics::TransformMatrix m =
        blib::graphics::composeMatrix(position, rotation, scale);

    // Трансляция — последняя колонка
    BLIB_TEST_CHECK_CLOSE(m.data[3][0], 5.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[3][1], 6.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[3][2], 7.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[3][3], 1.0f, epsilon);
    // Единичная ротация: диагональ 3x3
    BLIB_TEST_CHECK_CLOSE(m.data[0][0], 1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[1][1], 1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[2][2], 1.0f, epsilon);
}

BLIB_TEST_CASE("composeMatrix: 90 degrees around Z maps +X to +Y")
{
    // Поворот +90° вокруг Z: +X → +Y
    const blib::graphics::Vector3f position(0.0f, 0.0f, 0.0f);
    const blib::math::Quaternion<float> rotation(
        blib::math::AngleDegree<float>(90.0f),
        blib::graphics::Vector3f(0.0f, 0.0f, 1.0f));
    const blib::graphics::Vector3f scale(1.0f, 1.0f, 1.0f);

    const blib::graphics::TransformMatrix m =
        blib::graphics::composeMatrix(position, rotation, scale);

    // R = [[0, -1, 0], [1, 0, 0], [0, 0, 1]]; column-major:
    // col0 = (0, 1, 0), col1 = (-1, 0, 0)
    BLIB_TEST_CHECK_CLOSE(m.data[0][0], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[0][1], 1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[1][0], -1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[1][1], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[2][2], 1.0f, epsilon);
}

BLIB_TEST_CASE("composeMatrix: scale multiplies rotation columns")
{
    const blib::graphics::Vector3f position(1.0f, 2.0f, 3.0f);
    const blib::math::Quaternion<float> rotation(
        blib::math::AngleDegree<float>(90.0f),
        blib::graphics::Vector3f(0.0f, 0.0f, 1.0f));
    const blib::graphics::Vector3f scale(2.0f, 3.0f, 4.0f);

    const blib::graphics::TransformMatrix m =
        blib::graphics::composeMatrix(position, rotation, scale);

    // col0 = R*S колонка 0 = (r00*sx, r10*sx, r20*sx) = (0, 2, 0)
    BLIB_TEST_CHECK_CLOSE(m.data[0][0], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[0][1], 2.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[0][2], 0.0f, epsilon);
    // col1 = (r01*sy, r11*sy, r21*sy) = (-3, 0, 0)
    BLIB_TEST_CHECK_CLOSE(m.data[1][0], -3.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[1][1], 0.0f, epsilon);
    // col2 = (0, 0, r22*sz) = (0, 0, 4)
    BLIB_TEST_CHECK_CLOSE(m.data[2][2], 4.0f, epsilon);
    // Трансляция не затронута масштабом
    BLIB_TEST_CHECK_CLOSE(m.data[3][0], 1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[3][1], 2.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(m.data[3][2], 3.0f, epsilon);
}

// ============================================================
// decomposeMatrix
// ============================================================

BLIB_TEST_CASE("decomposeMatrix: extracts position and scale from composeMatrix")
{
    const blib::graphics::Vector3f position(5.0f, -2.0f, 9.0f);
    const blib::math::Quaternion<float> rotation(
        blib::math::AngleDegree<float>(0.0f),
        blib::graphics::Vector3f(0.0f, 1.0f, 0.0f));
    const blib::graphics::Vector3f scale(2.0f, 3.0f, 4.0f);

    const blib::graphics::TransformMatrix m =
        blib::graphics::composeMatrix(position, rotation, scale);

    blib::graphics::Transform transform;
    blib::graphics::decomposeMatrix(m, transform);

    BLIB_TEST_CHECK_CLOSE(transform.getPosition().x, 5.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getPosition().y, -2.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getPosition().z, 9.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getScale().x, 2.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getScale().y, 3.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getScale().z, 4.0f, epsilon);
    // Идентичная ротация → нулевые углы
    BLIB_TEST_CHECK_CLOSE(transform.getRotation().x, 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getRotation().y, 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getRotation().z, 0.0f, epsilon);
}

BLIB_TEST_CASE("decomposeMatrix: XYZ euler roundtrip (rotateX/Y/Z convention)")
{
    // Сборка как в ITransformable::getTransform: M = X·Y·Z·T с углами
    // в ГРАДУСАХ. decomposeMatrix должен вернуть те же углы (X·Y·Z).
    const float angleX = 30.0f;
    const float angleY = -45.0f;
    const float angleZ = 60.0f;

    blib::graphics::TransformMatrix m = blib::graphics::Identity;
    m = blib::graphics::rotateX(m, angleX);
    m = blib::graphics::rotateY(m, angleY);
    m = blib::graphics::rotateZ(m, angleZ);
    // Трансляция — в последней колонке
    m.data[3][0] = 7.0f;
    m.data[3][1] = -3.0f;
    m.data[3][2] = 4.0f;

    blib::graphics::Transform transform;
    blib::graphics::decomposeMatrix(m, transform);

    BLIB_TEST_CHECK_CLOSE(transform.getRotation().x, angleX, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getRotation().y, angleY, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getRotation().z, angleZ, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getPosition().x, 7.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getPosition().y, -3.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getPosition().z, 4.0f, epsilon);
    // Масштаб единичный (rotateX/Y/Z не масштабируют)
    BLIB_TEST_CHECK_CLOSE(transform.getScale().x, 1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getScale().y, 1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getScale().z, 1.0f, epsilon);
}

BLIB_TEST_CASE("decomposeMatrix: 90 degrees Z from composeMatrix gives z = 90")
{
    const blib::graphics::Vector3f position(0.0f, 0.0f, 0.0f);
    const blib::math::Quaternion<float> rotation(
        blib::math::AngleDegree<float>(90.0f),
        blib::graphics::Vector3f(0.0f, 0.0f, 1.0f));
    const blib::graphics::Vector3f scale(1.0f, 1.0f, 1.0f);

    const blib::graphics::TransformMatrix m =
        blib::graphics::composeMatrix(position, rotation, scale);

    blib::graphics::Transform transform;
    blib::graphics::decomposeMatrix(m, transform);

    BLIB_TEST_CHECK_CLOSE(transform.getRotation().x, 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getRotation().y, 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(transform.getRotation().z, 90.0f, epsilon);
}

// ============================================================
// lookAt
// ============================================================

BLIB_TEST_CASE("lookAt: right-handed basis, eye maps to origin")
{
    // Камера в (0, 0, 5) смотрит в начало координат, up = +Y:
    // f = (0, 0, -1), s = (1, 0, 0), u = (0, 1, 0)
    const blib::graphics::Vector3f eye(0.0f, 0.0f, 5.0f);
    const blib::graphics::Vector3f target(0.0f, 0.0f, 0.0f);
    const blib::graphics::Vector3f up(0.0f, 1.0f, 0.0f);

    const blib::graphics::TransformMatrix v =
        blib::graphics::lookAt(eye, target, up);

    // Столбцы — образы мировых осей: col0 = (s.x, u.x, -f.x), ...
    BLIB_TEST_CHECK_CLOSE(v.data[0][0], 1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(v.data[0][1], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(v.data[0][2], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(v.data[1][0], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(v.data[1][1], 1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(v.data[1][2], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(v.data[2][0], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(v.data[2][1], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(v.data[2][2], 1.0f, epsilon);
    // Трансляция — в последней колонке: -dot(s,eye), -dot(u,eye), dot(f,eye)
    BLIB_TEST_CHECK_CLOSE(v.data[3][0], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(v.data[3][1], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(v.data[3][2], -5.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(v.data[3][3], 1.0f, epsilon);

    // Позиция камеры переходит в origin
    blib::math::Matrix<float, 1, 4> eyePoint;
    eyePoint.data[0][0] = eye.x;
    eyePoint.data[0][1] = eye.y;
    eyePoint.data[0][2] = eye.z;
    eyePoint.data[0][3] = 1.0f;
    const blib::math::Matrix<float, 1, 4> res = v * eyePoint;
    BLIB_TEST_CHECK_CLOSE(res.data[0][0], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(res.data[0][1], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(res.data[0][2], 0.0f, epsilon);
}

// ============================================================
// mul
// ============================================================

BLIB_TEST_CASE("mul is an alias of standard matrix product")
{
    const blib::math::Quaternion<float> rotationA(
        blib::math::AngleDegree<float>(30.0f),
        blib::graphics::Vector3f(0.0f, 1.0f, 0.0f));
    const blib::math::Quaternion<float> rotationB(
        blib::math::AngleDegree<float>(-20.0f),
        blib::graphics::Vector3f(1.0f, 0.0f, 0.0f));

    const blib::graphics::TransformMatrix a =
        blib::graphics::composeMatrix(
            blib::graphics::Vector3f(1.0f, 2.0f, 3.0f),
            rotationA,
            blib::graphics::Vector3f(1.0f, 1.0f, 1.0f));
    const blib::graphics::TransformMatrix b =
        blib::graphics::composeMatrix(
            blib::graphics::Vector3f(4.0f, 5.0f, 6.0f),
            rotationB,
            blib::graphics::Vector3f(1.0f, 1.0f, 1.0f));

    const blib::graphics::TransformMatrix viaMul =
        blib::graphics::mul(a, b);
    const blib::graphics::TransformMatrix viaOperator = a * b;

    for (blib::math::matrixSizeT i = 0; i < 4; ++i)
    {
        for (blib::math::matrixSizeT j = 0; j < 4; ++j)
        {
            BLIB_TEST_CHECK_CLOSE(viaMul.data[i][j], viaOperator.data[i][j], epsilon);
        }
    }
}

BLIB_TEST_CASE("mul: parent * local composition preserves child translation")
{
    // Мировая позиция ребёнка = parent * local: трансляция родителя
    // переносится на ребёнка (стандартное column-vector произведение)
    const blib::math::Quaternion<float> identityRotation(
        blib::math::AngleDegree<float>(0.0f),
        blib::graphics::Vector3f(0.0f, 1.0f, 0.0f));
    const blib::graphics::Vector3f unitScale(1.0f, 1.0f, 1.0f);

    const blib::graphics::TransformMatrix parent =
        blib::graphics::composeMatrix(
            blib::graphics::Vector3f(10.0f, 0.0f, 0.0f),
            identityRotation,
            unitScale);
    const blib::graphics::TransformMatrix local =
        blib::graphics::composeMatrix(
            blib::graphics::Vector3f(5.0f, 0.0f, 0.0f),
            identityRotation,
            unitScale);

    const blib::graphics::TransformMatrix world =
        blib::graphics::mul(parent, local);

    BLIB_TEST_CHECK_CLOSE(world.data[3][0], 15.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(world.data[3][1], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(world.data[3][2], 0.0f, epsilon);
}

// ============================================================
// Перспективная проекция (стандартная column-major раскладка)
// ============================================================

BLIB_TEST_CASE("IsometricCamera perspective: standard column-major layout")
{
    // fov 90° → f = 1; aspect 2, near 1, far 100
    blib::graphics::IsometricCamera camera;
    camera.setPerspective(
        blib::math::AngleDegreef(90.0f), 2.0f, 1.0f, 100.0f);

    const blib::graphics::TransformMatrix& p = camera.getProjectionMatrix();

    BLIB_TEST_CHECK_CLOSE(p.data[0][0], 0.5f, epsilon);
    BLIB_TEST_CHECK_CLOSE(p.data[1][1], 1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(p.data[2][2], (100.0f + 1.0f) / (1.0f - 100.0f), epsilon);
    // Перспективное деление: -1 в колонке 2, строке 3 (data[2][3])
    BLIB_TEST_CHECK_CLOSE(p.data[2][3], -1.0f, epsilon);
    // Сдвиг по z: 2fn/(n-f) в колонке 3, строке 2 (data[3][2])
    BLIB_TEST_CHECK_CLOSE(p.data[3][2], (2.0f * 100.0f * 1.0f) / (1.0f - 100.0f), epsilon);
    BLIB_TEST_CHECK_CLOSE(p.data[3][3], 0.0f, epsilon);
}
