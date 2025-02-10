#pragma once

#include <blib/config.h>

#include <blib/graphics/vertex.h>
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
        class __blib_api Camera
        {
        private:
            Transform3f transform;
        public:

            float movespeed = 10;
            float rotatespeed = 10;

            enum class ProjectionMode
            {
                Perspective,
                Ortho,

                END_OF_ENUM
            };

            Camera();
            ~Camera();

            // Tramsormable
            void setPosition(const Vector3f& _position);
            Vector3f getPosition() const;

            void setRotation(const Vector3f& _rotation);
            Vector3f getRotation() const;

            void setScale(const Vector3f& _scale);
            Vector3f getScale() const;
            
            void setOrigin(const Vector3f& _origin);
            Vector3f getOrigin() const;

            void move(const Vector3f& _position);
            void rotate(const Vector3f& _rotatation);


            void setProjectionMode(ProjectionMode mode, const RenderWindow& wnd);

            template<class Tramsormable>
            void alignTo(const Tramsormable directToObject);

            void display(RenderWindow& wnd);

            void LookAt(const Transform3f& transform);

            void controlUpdate(float deltaTime, RenderWindow& wnd);
        };
    }
}
