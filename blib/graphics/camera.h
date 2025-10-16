#pragma once

#include <blib/config.h>
#include <blib/utilmacro.h>

#include <blib/graphics/vertex.h>

#include <blib/graphics/drawable.h>
#include <blib/graphics/transformable.h>

#include <blib/graphics/renderWindow.h>

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
        public:
            float movespeed = 10;
            float rotatespeed = 10;

            struct Transformtest
            {
                Vector3f position;
                Vector3f rotation;
            } transform;

            enum class ProjectionMode
            {
                Perspective,
                Ortho,

                END_OF_ENUM
            };

            Camera();
            ~Camera();

            void setProjectionMode(ProjectionMode mode, const RenderWindow& wnd);

            void controlUpdate(float deltaTime,  RenderWindow& wnd);

            void lookAt(const blib::graphics::Transformable& target, const blib::graphics::Vector3f worldUp);

            // Унаследовано через IDrawable
            virtual void draw(RenderTarget& target, RenderContext& ctx) const __blib_override;
        };
    }
}
