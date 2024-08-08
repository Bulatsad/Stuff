#pragma once

#include <blib/config.h>

#include <blib/graphics/texture.h>

#include <blib/graphics/vertex.h>

#include <blib/graphics/renderWindow.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Sprite
        {
        private:
            const Texture* pTexture;
            Transform3f transform;
            float matrixTransform[16];
            bool updateCache;
            verties_2f_t verties[4];

        public:
            Sprite();
            ~Sprite();

            void setTexture(const Texture& texture);
            void draw(RenderWindow& window);
            
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
