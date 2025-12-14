 #pragma once

#include <blib/config.h>
#include <blib/inline.h>

#include <blib/graphics/vector.h>
#include <blib/graphics/transform.h>
#include <blib/graphics/transformMatrix.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api ITransformable
        {
        private:
            blib::graphics::Transform transformData;
            mutable blib::graphics::TransformMatrix transformMatrix;
            mutable bool matrixNeedUpdate = false;
        public:

            ITransformable() {}
            virtual ~ITransformable() {}

            __blib_force_inline void setPosition(float x, float y, float z)
            {
                this->matrixNeedUpdate = true;
                this->transformData.setPosition(x, y, z);
            }
            __blib_force_inline void setPosition(const Vector3f& position);

            __blib_force_inline void setScale(float factorX, float factorY, float factorZ);
            __blib_force_inline void setScale(const Vector3f& factors);

            __blib_force_inline void setOrigin(float x, float y, float z);
            __blib_force_inline void setOrigin(const Vector3f& origin);

            __blib_force_inline void setRotation(float x, float y, float z)
            {
                this->matrixNeedUpdate = true;
                this->transformData.setRotation(x, y, z);
            }
            __blib_force_inline void setRotation(const Vector3f& origin);

            __blib_force_inline void rotateX(float angle);
            __blib_force_inline void rotateY(float angle);
            __blib_force_inline void rotateZ(float angle);

            __blib_force_inline const Vector3f& getPosition() const;
            __blib_force_inline Vector3f& getPosition();

            __blib_force_inline const Vector3f& getRotation() const;
            __blib_force_inline Vector3f& getRotation();

            __blib_force_inline const Vector3f& getScale() const;
            __blib_force_inline Vector3f& getScale();

            __blib_force_inline const Vector3f& getOrigin() const;
            __blib_force_inline Vector3f& getOrigin();

            __blib_force_inline void move(float offsetX, float offsetY, float offsetZ)
            {
                this->matrixNeedUpdate = true;
                this->transformData.move(offsetX, offsetY, offsetZ);
            }
            __blib_force_inline void move(const Vector3f& offset);

            virtual const TransformMatrix& getTransform() const;
            void setTransform(const TransformMatrix& transform);

            bool isNeedToRecalculate() const
            {
                return this->matrixNeedUpdate;
            }

            Vector3f transform(const Vector3f& point) const;
        };
    }
}

