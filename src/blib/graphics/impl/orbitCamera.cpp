#include <blib/graphics/orbitCamera.h>

#include <blib/core/math/consts.h>
#include <blib/core/math/trigonometry.h>
#include <blib/core/math/utilfuncs.h>

namespace blib
{
    namespace graphics
    {
        namespace
        {
            // Пределы вертикального угла: не даём камере перевернуться
            // через полюса (lookAt вырождается при углах ±90 градусов)
            constexpr float maxElevationDegrees = 89.0f;

            // Границы дистанции камеры
            constexpr float minCameraDistance = 0.5f;
            constexpr float maxCameraDistance = 100000.0f;
        }

        OrbitCamera::OrbitCamera()
            : target(0.0f, 0.0f, 0.0f)
            , distance(100.0f)
            , azimuth(blib::math::AngleDegreef(0.0f))
            , elevation(blib::math::AngleDegreef(0.0f))
            , worldUp(0.0f, 1.0f, 0.0f)
            , position(0.0f, 0.0f, 0.0f)
        {
            this->projectionMatrix.loadIdentity();
            this->viewMatrix.loadIdentity();
            this->update();
        }

        void OrbitCamera::setTarget(_In const blib::graphics::Vector3f& newTarget)
        {
            this->target = newTarget;
        }

        const blib::graphics::Vector3f& OrbitCamera::getTarget() const
        {
            return this->target;
        }

        void OrbitCamera::setDistance(float newDistance)
        {
            this->distance = newDistance;
        }

        float OrbitCamera::getDistance() const
        {
            return this->distance;
        }

        void OrbitCamera::rotate(float deltaAzimuth, float deltaElevation)
        {
            this->azimuth.data = blib::math::fmod(this->azimuth.data + deltaAzimuth, 360.0f);

            this->elevation.data += deltaElevation;
            if (this->elevation.data > maxElevationDegrees)
            {
                this->elevation.data = maxElevationDegrees;
            }
            if (this->elevation.data < -maxElevationDegrees)
            {
                this->elevation.data = -maxElevationDegrees;
            }
        }

        void OrbitCamera::pan(float deltaX, float deltaY)
        {
            // Оси камеры извлекаем из уже посчитанной view-матрицы:
            // первый столбец — right, второй — up. Предполагается,
            // что update() вызывался хотя бы раз (конструктор делает это)
            blib::graphics::Vector3f right(
                this->viewMatrix.data[0][0],
                this->viewMatrix.data[1][0],
                this->viewMatrix.data[2][0]);
            blib::graphics::Vector3f up(
                this->viewMatrix.data[0][1],
                this->viewMatrix.data[1][1],
                this->viewMatrix.data[2][1]);

            // Сдвиг цели в плоскости камеры: движение мыши вправо
            // сдвигает цель влево (мир едет за курсором)
            this->target = this->target - right * deltaX + up * deltaY;
        }

        void OrbitCamera::zoom(float deltaDistance)
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

        void OrbitCamera::setPerspective(_In const blib::math::AngleDegreef& fov, float aspect, float nearDist, float farDist)
        {
            // Тот же расчёт, что в Camera::perspective (camera.cpp):
            // перспективная матрица из FOV/aspect/ближней/дальней
            float f = 1.0f / blib::math::tan(fov.data * static_cast<float>(blib::math::piDiv360));

            this->projectionMatrix.loadIdentity();
            this->projectionMatrix.data[0][0] = f / aspect;      // x scale
            this->projectionMatrix.data[1][1] = f;               // y scale
            this->projectionMatrix.data[2][2] = (farDist + nearDist) / (nearDist - farDist);   // z scale
            this->projectionMatrix.data[3][2] = -1;              // perspective div
            this->projectionMatrix.data[2][3] = (2 * farDist * nearDist) / (nearDist - farDist); // z shift
            this->projectionMatrix.data[3][3] = 0;
        }

        void OrbitCamera::update()
        {
            // Сферические координаты → позиция камеры.
            // Согласовано с Camera::updateVectors (camera.cpp):
            // при azimuth=0, elevation=0 камера смотрит на цель по -Z
            const float azimuthRad = this->azimuth.toRadian().data;
            const float elevationRad = this->elevation.toRadian().data;

            blib::graphics::Vector3f offset;
            offset.x = this->distance * blib::math::cos(elevationRad) * blib::math::sin(azimuthRad);
            offset.y = this->distance * blib::math::sin(elevationRad);
            offset.z = this->distance * blib::math::cos(elevationRad) * blib::math::cos(azimuthRad);

            this->position = this->target + offset;

            this->viewMatrix = blib::graphics::lookAt(this->position, this->target, this->worldUp);
        }

        const blib::graphics::TransformMatrix& OrbitCamera::getProjectionMatrix() const
        {
            return this->projectionMatrix;
        }

        const blib::graphics::TransformMatrix& OrbitCamera::getViewMatrix() const
        {
            return this->viewMatrix;
        }

        const blib::graphics::Vector3f& OrbitCamera::getPosition() const
        {
            return this->position;
        }

    }
}
