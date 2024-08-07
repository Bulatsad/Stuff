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
        * implement Transormable, Directional
        */
        class __blib_api Camera
        {
        private:
            Vector3f postion;
        public:
            enum class ProjectionMode
            {
                Perspective,
                Ortho,

                END_OF_ENUM
            };

            Camera();
            ~Camera();

            // Tramsormable
            void setPosition(const Vector3f& position);
            Vector3f getPosition() const;

            //Directional
            void setDirection(const Vector3f& direction);
            Vector3f getDirection() const;

            void setProjectionMode(ProjectionMode mode);

            template<class Tramsormable>
            void alignTo(const Tramsormable directToObject);

            void display(RenderWindow& wnd);

        };
    }
}
