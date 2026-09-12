#include <blib/graphics/isometricCamera.h>

#include <blib/core/math/consts.h>
#include <blib/core/math/trigonometry.h>

namespace blib
{
    namespace graphics
    {
        namespace
        {
            // Границы дистанции камеры (ближняя — не даём провалиться
            // в землю, дальняя — страховка от переполнения)
            constexpr float minCameraDistance = 5.0f;
            constexpr float maxCameraDistance = 100000.0f;

            // Ракурс по умолчанию: наклон сверху как в Hades
            constexpr float defaultPitchDegrees = 55.0f;

            // Азимут по умолчанию: 45° дают «ромбическую» изометрию —
            // оси мира X и Z идут по диагоналям экрана
            constexpr float defaultYawDegrees = 45.0f;

            // Дистанция по умолчанию
            constexpr float defaultDistance = 200.0f;

            // Наклон не доводим до полюсов: lookAt вырождается при
            // pitch = 90 (взгляд строго вниз, up совпадает с front)
            constexpr float maxPitchDegrees = 89.0f;
            constexpr float minPitchDegrees = 1.0f;
        }

        IsometricCamera::IsometricCamera()
            : target(0.0f, 0.0f, 0.0f)
            , distance(defaultDistance)
            , yaw(blib::math::AngleDegreef(defaultYawDegrees))
            , pitch(blib::math::AngleDegreef(defaultPitchDegrees))
            , worldUp(0.0f, 1.0f, 0.0f)
            , position(0.0f, 0.0f, 0.0f)
        {
            this->projectionMatrix.loadIdentity();
            this->viewMatrix.loadIdentity();
            this->update();
        }

        void IsometricCamera::setTarget(_In const blib::graphics::Vector3f& newTarget)
        {
            this->target = newTarget;
        }

        const blib::graphics::Vector3f& IsometricCamera::getTarget() const
        {
            return this->target;
        }

        void IsometricCamera::moveTarget(_In const blib::graphics::Vector3f& delta)
        {
            this->target = this->target + delta;
        }

        void IsometricCamera::setDistance(float newDistance)
        {
            this->distance = newDistance;
        }

        float IsometricCamera::getDistance() const
        {
            return this->distance;
        }

        void IsometricCamera::setPitch(_In const blib::math::AngleDegreef& pitchAngle)
        {
            this->pitch = pitchAngle;
            if (this->pitch.data > maxPitchDegrees)
            {
                this->pitch.data = maxPitchDegrees;
            }
            if (this->pitch.data < minPitchDegrees)
            {
                this->pitch.data = minPitchDegrees;
            }
        }

        void IsometricCamera::setYaw(_In const blib::math::AngleDegreef& yawAngle)
        {
            this->yaw.data = blib::math::fmod(yawAngle.data, 360.0f);
        }

        void IsometricCamera::zoom(float deltaDistance)
        {
            this->distance += deltaDistance;
            if (this->distance < minCameraDistance)
            {
                this->distance = minCameraDistance;
            }
            if (this->distance > maxCameraDistance)
            {
                this->distance = maxCameraDistance;
            }
        }

        void IsometricCamera::setPerspective(_In const blib::math::AngleDegreef& fov, float aspect, float nearDist, float farDist)
        {
            // Тот же расчёт, что в Camera::perspective (camera.cpp)
            // и OrbitCamera::setPerspective (orbitCamera.cpp)
            float f = 1.0f / blib::math::tan(fov.data * static_cast<float>(blib::math::piDiv360));

            this->projectionMatrix.loadIdentity();
            this->projectionMatrix.data[0][0] = f / aspect;      // x scale
            this->projectionMatrix.data[1][1] = f;               // y scale
            this->projectionMatrix.data[2][2] = (farDist + nearDist) / (nearDist - farDist);   // z scale
            this->projectionMatrix.data[3][2] = -1;              // perspective div
            this->projectionMatrix.data[2][3] = (2 * farDist * nearDist) / (nearDist - farDist); // z shift
            this->projectionMatrix.data[3][3] = 0;
        }

        void IsometricCamera::update()
        {
            // Сферические координаты → позиция камеры. Согласовано
            // с Camera::updateVectors (camera.cpp) и OrbitCamera::update
            // (orbitCamera.cpp): при yaw=0, pitch=0 камера находится
            // за целью по +Z и смотрит на неё по -Z
            const float yawRad = this->yaw.toRadian().data;
            const float pitchRad = this->pitch.toRadian().data;

            blib::graphics::Vector3f direction;
            direction.x = blib::math::cos(pitchRad) * blib::math::sin(yawRad);
            direction.y = blib::math::sin(pitchRad);
            direction.z = blib::math::cos(pitchRad) * blib::math::cos(yawRad);

            this->position = this->target + direction * this->distance;

            this->viewMatrix = blib::graphics::lookAt(this->position, this->target, this->worldUp);
        }

        const blib::graphics::TransformMatrix& IsometricCamera::getProjectionMatrix() const
        {
            return this->projectionMatrix;
        }

        const blib::graphics::TransformMatrix& IsometricCamera::getViewMatrix() const
        {
            return this->viewMatrix;
        }

        const blib::graphics::Vector3f& IsometricCamera::getPosition() const
        {
            return this->position;
        }

    }
}
