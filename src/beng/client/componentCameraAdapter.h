#pragma once

#include <beng/config.h>

#include <beng/client/components/cameraComponent.h>
#include <beng/components/transform.h>

#include <blib/graphics/iCamera.h>
#include <blib/graphics/transformMatrix.h>
#include <blib/graphics/vector.h>

#include <blib/core/math/quaternion.h>

namespace beng
{
    /**
     * ComponentCameraAdapter — реализация ICamera, читающая камеру
     * из ECS: позиция/поворот — из TransformComponent сущности,
     * параметры проекции — из CameraComponent (см. cameraComponent.h).
     *
     * Назначение:
     * - Позволяет отрисовать сцену «из глаз» камеры-сущности (Game-
     *   превью эдитора, будущие игровые скрипты): рендер-контексту
     *   нужен только ICamera (view/projection/position);
     * - Локальная ось +Z сущности — направление взгляда (как у Unity);
     *   up = локальная +Y, повёрнутая мировым поворотом;
     * - Матрицы кешируются до следующего sync() — адаптер дёшев на
     *   кадр, sync зовёт потребитель при изменении камеры/аспекта.
     *
     * Использование:
     *   ComponentCameraAdapter adapter;
     *   adapter.sync(cameraComponent, transformComponent, aspect);
     *   renderTarget.rc.setCamera(&adapter);
     *
     * Ограничения:
     * - Валидность параметров (fov > 0, near < far) — забота
     *   потребителя; sync() дополнительно защищается от деления
     *   на ноль по fov/aspect;
     * - Поворот «вверх ногами» (up коллинеарен forward) не
     *   проверяется — lookAt вырождается (см. GRAPHICS.md).
     */
    class __beng_api ComponentCameraAdapter : public blib::graphics::ICamera
    {
    private:
        blib::graphics::TransformMatrix projectionMatrix;
        blib::graphics::TransformMatrix viewMatrix;
        blib::graphics::Vector3f position;
        blib::graphics::Vector3f forward;

    public:
        ComponentCameraAdapter();

        /**
         * Базис камеры-сущности из МИРОВОГО поворота (кватернион):
         * локальная +Z → forward, +X → right, +Y → up. Векторы
         * нормируются. Нужен и адаптеру, и гизмо фрустума — единая
         * математика в одном месте.
         */
        static void computeBasis(
            _In const blib::math::Quaternion<float>& worldRotation,
            _Out blib::graphics::Vector3f& outForward,
            _Out blib::graphics::Vector3f& outRight,
            _Out blib::graphics::Vector3f& outUp);

        /**
         * Пересчитать матрицы камеры из компонентов сцены.
         *
         * @param camera Камера-сущность (FOV/near/far)
         * @param transform Трансформ той же сущности (позиция/поворот)
         * @param aspect Аспект кадра (ширина/высота FBO потребителя)
         */
        void sync(
            _In const CameraComponent& camera,
            _In const TransformComponent& transform,
            float aspect);

        /**
         * Мировое направление взгляда после sync() (для гизмо/скриптов).
         */
        const blib::graphics::Vector3f& getForward() const { return forward; }

        // ICamera
        const blib::graphics::TransformMatrix& getViewMatrix() const override;
        const blib::graphics::TransformMatrix& getProjectionMatrix() const override;
        const blib::graphics::Vector3f& getPosition() const override;
    };

} // namespace beng
