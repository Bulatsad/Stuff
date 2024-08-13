#pragma once

#include <blib/config.h>

#include <blib/graphics/color.h>
#include <blib/graphics/vertex.h>
#include <blib/graphics/renderWindow.h>
#include <blib/blibint.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Romb
        {
        private:
            Transform3f transform;
            Color color;
        public:
            bint32 width;
            bint32 height;

            void setWidth(bint32 _width);
            void setHeight(bint32 _height);
            void setColor(const Color color);
            void draw(RenderWindow& wnd);

            // Tramsormable
            void setPosition(const Vector3f& _position);
            Vector3f getPosition() const;

            void setRotation(const Vector3f& _rotation);
            Vector3f getRotation() const;

            void setScale(const Vector3f& _scale);
            Vector3f getScale() const;

            void setOrigin(const Vector3f& _origin);
            Vector3f getOrigin() const;

            void Move(const Vector3f& _position);
            void Rotate(const Vector3f& _rotatation);

        };
    }
}