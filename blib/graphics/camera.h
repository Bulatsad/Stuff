#pragma once

#include <blib/config.h>
#include <blib/utilmacro.h>

#include <blib/graphics/vertex.h>
#include <blib/graphics/drawable.h>
#include <blib/graphics/transformable.h>
#include <blib/graphics/renderWindow.h>

#include <blib/math/angle.h>

namespace blib
{
    namespace graphics
    {
        /*!
        * \brief Camera
        * 
        * implement Transormable
        */
        class __blib_api Camera : public Transform
        {
        private:
            blib::graphics::TransformMatrix projectionMatrix;
            blib::graphics::TransformMatrix viewMatrix;
            blib::math::Vector<float, 3> right;
            blib::math::Vector<float, 3> up = { 0,1,0 };
            blib::math::Vector<float, 3> front;
            blib::math::Vector<float, 3> worldUp = { 0,1,0 };
            blib::math::AngleDegreef yaw = blib::math::AngleDegreef(0.f);
            blib::math::AngleDegreef pitch = blib::math::AngleDegreef(0.f);
            blib::math::Matrix<float, 4, 4> perspective(const blib::math::AngleDegreef& fov, float aspect, float nearDist, float farDist);

            void updateVectors();

        public:
            float movespeed = 1;
            float rotatespeed = 10;

            Camera();
            ~Camera();

            // fov - angle of vision
            // aspect - width / height
            // nearDist - near distance of cutting off
            // farDist - far distance of cutting off
            void setPerpective(const blib::math::AngleDegreef& fov, float aspect, float nearDist, float farDist);

            const blib::graphics::TransformMatrix& getProjectionMatrix() const;
            const blib::graphics::TransformMatrix& getViewMatrix() const;

            void controlUpdate(float deltaTime,  RenderWindow& wnd);

        };
    }
}
