#pragma once

#include <blib/config.h>

#include <blib/graphics/vector.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Transform
        {
        private:
            Vector3f          origin;
            Vector3f          position;
            Vector3f          rotation;
            Vector3f          scale;
        
        public:
            Transform();

            void setPosition(float x, float y, float z);
            void setPosition(const Vector3f& position);

            void scaleX(float factorX);
            void scaleY(float factorY);
            void scaleZ(float factorZ);
            void setScale(float factorX, float factorY, float factorZ);
            void setScale(const Vector3f& factors);

            void setOrigin(float x, float y, float z);
            void setOrigin(const Vector3f& origin);

            void setRotation(float x, float y, float z);

            void rotateX(float angle);
            void rotateY(float angle);
            void rotateZ(float angle);

            const Vector3f& getPosition() const;
            Vector3f& getPosition();

            const Vector3f& getRotation() const;
            Vector3f& getRotation();

            const Vector3f& getScale() const;
            Vector3f& getScale();

            const Vector3f& getOrigin() const;
            Vector3f& getOrigin();

            void move(float offsetX, float offsetY, float offsetZ);
            void move(const Vector3f& offset);

            Vector3f transform(const Vector3f& point) const;
        };
    }
}
