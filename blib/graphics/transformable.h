#pragma once

#include <blib/config.h>

#include <blib/graphics/vector.h>
#include <blib/graphics/transform.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Transformable
        {
        public:

            Transformable();

            virtual ~Transformable();

            void setPosition(float x, float y);
            void setPosition(float x, float y, float z);
            void setPosition(const Vector2f& position);

            void setScale(float factorX, float factorY);
            void setScale(float factorX, float factorY, float factorZ);
            void setScale(const Vector2f& factors);

            void setOrigin(float x, float y);
            void setOrigin(const Vector2f& origin);

            void setRotation(float x, float y, float z);

            void rotateX(float angle);
            void rotateY(float angle);
            void rotateZ(float angle);


            const Vector3f& getPosition() const;

            const Vector3f& getRotation() const;

            const Vector3f& getScale() const;

            const Vector3f& getOrigin() const;

            void move(float offsetX, float offsetY, float offsetZ);
            void move(const Vector3f& offset);

            void scale(float factorX, float factorY);
            void scale(const Vector2f& factor);

            const Transform& getTransform() const;
            void setTransform(const Transform& transform);

            const Transform& getInverseTransform() const;

            bool isNeedToRecalculate() const
            {
                return this->m_transformNeedUpdate;
            }

        private:

            Vector3f          m_origin;                    
            Vector3f          m_position;                  
            Vector3f          m_rotation;
            Vector3f          m_scale;                     
            mutable Transform m_transform;                 
            mutable bool      m_transformNeedUpdate;       
            mutable Transform m_inverseTransform;          
            mutable bool      m_inverseTransformNeedUpdate;
        };
    }
}

