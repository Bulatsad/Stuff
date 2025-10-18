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
        class __blib_api Camera : public Transformable, public IDrawable
        {
        private:
            blib::math::Matrix<float, 4, 4> projectionMatrix;

            blib::math::Matrix<float, 4, 4> perspective(const blib::math::AngleDegreef& fov, float aspect, float nearDist, float farDist);

        public:
            float movespeed = 10;
            float rotatespeed = 10;

            Camera();
            ~Camera();

            // fov - angle of vision
            // aspect - width / height
            // nearDist - near distance of cutting off
            // farDist - far distance of cutting off
            void setPerpective(const blib::math::AngleDegreef& fov, float aspect, float nearDist, float farDist);

            const blib::math::Matrix<float, 4, 4>& getProjectionMatrix() const;

            void controlUpdate(float deltaTime,  RenderWindow& wnd);

            void lookAt(const blib::graphics::Transformable& target, const blib::graphics::Vector3f worldUp);

            // Унаследовано через IDrawable
            virtual void draw(RenderTarget& target, RenderContext& ctx) const __blib_override;
        };
    }
}
