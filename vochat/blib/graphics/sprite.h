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
            void setPosition(float x, float y, float z);
            void setOrigin(float x, float y);
            void draw(RenderWindow& window);
        };
    }
}
