#include <beng/client/componentCameraAdapter.h>

#include <blib/core/math/angle.h>
#include <blib/core/math/consts.h>
#include <blib/core/math/trigonometry.h>

namespace beng
{
    namespace
    {
        // Локальные оси сущности-камеры (конвенция Unity: взгляд — +Z)
        const blib::graphics::Vector3f localForward(0.0f, 0.0f, 1.0f);
        const blib::graphics::Vector3f localRight(1.0f, 0.0f, 0.0f);
        const blib::graphics::Vector3f localUp(0.0f, 1.0f, 0.0f);

        // Страховка от деления на ноль в проекции (вырожденный ввод)
        constexpr float minFovDegrees = 1.0f;
        constexpr float minAspect = 0.001f;
    }

    ComponentCameraAdapter::ComponentCameraAdapter()
        : position(0.0f, 0.0f, 0.0f)
        , forward(0.0f, 0.0f, 1.0f)
    {
        this->projectionMatrix.loadIdentity();
        this->viewMatrix.loadIdentity();
    }

    void ComponentCameraAdapter::computeBasis(
        _In const blib::math::Quaternion<float>& worldRotation,
        _Out blib::graphics::Vector3f& outForward,
        _Out blib::graphics::Vector3f& outRight,
        _Out blib::graphics::Vector3f& outUp)
    {
        // Поворот локальных осей мировым кватернионом (см. CORE.md
        // «Конвенция матриц»: rotate — стандартная операция quaternion)
        outForward = blib::math::normalize(blib::math::rotate(localForward, worldRotation));
        outRight = blib::math::normalize(blib::math::rotate(localRight, worldRotation));
        outUp = blib::math::normalize(blib::math::rotate(localUp, worldRotation));
    }

    void ComponentCameraAdapter::sync(
        _In const CameraComponent& camera,
        _In const TransformComponent& transform,
        float aspect)
    {
        // Базис из мирового поворота сущности
        blib::graphics::Vector3f right;
        blib::graphics::Vector3f up;
        computeBasis(transform.getWorldRotation(), this->forward, right, up);

        this->position = transform.getWorldPosition();

        // View: камера в позиции сущности, взгляд вдоль forward
        // (правосторонний lookAt — см. GRAPHICS.md)
        this->viewMatrix = blib::graphics::lookAt(
            this->position, this->position + this->forward, up);

        // Проекция: та же формула, что у Camera/OrbitCamera/
        // IsometricCamera (см. camera.cpp) — column-major
        const float fov = (camera.getFovDegrees() < minFovDegrees)
            ? minFovDegrees : camera.getFovDegrees();
        const float safeAspect = (aspect < minAspect) ? minAspect : aspect;
        const float f = 1.0f / blib::math::tan(
            fov * static_cast<float>(blib::math::piDiv360));

        const float nearDist = camera.getNearDistance();
        const float farDist = camera.getFarDistance();

        this->projectionMatrix.loadIdentity();
        this->projectionMatrix.data[0][0] = f / safeAspect;
        this->projectionMatrix.data[1][1] = f;
        this->projectionMatrix.data[2][2] = (farDist + nearDist) / (nearDist - farDist);
        this->projectionMatrix.data[2][3] = -1;
        this->projectionMatrix.data[3][2] = (2.0f * farDist * nearDist) / (nearDist - farDist);
        this->projectionMatrix.data[3][3] = 0;
    }

    const blib::graphics::TransformMatrix& ComponentCameraAdapter::getViewMatrix() const
    {
        return this->viewMatrix;
    }

    const blib::graphics::TransformMatrix& ComponentCameraAdapter::getProjectionMatrix() const
    {
        return this->projectionMatrix;
    }

    const blib::graphics::Vector3f& ComponentCameraAdapter::getPosition() const
    {
        return this->position;
    }

} // namespace beng
