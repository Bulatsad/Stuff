#pragma once

#include <blib/config.h>
#include <blib/inline.h>

#include <blib/graphics/color.h>
#include <blib/blibint.h>

namespace blib
{
    namespace graphics
    {
        struct __blib_api vector2f
        {
            float x = 0.f;
            float y = 0.f;
        };

        struct __blib_api vector2i
        {
            bint32 x = 0;
            bint32 y = 0;
        };

        struct __blib_api transform_t2f
        {
            vector2f position;
            vector2f origin;
            float rotation = 0.f;
            vector2f scale;
        };

        struct __blib_api verties_2f_t
        {
            vector2f screenCoord;
            blib::graphics::Color color;
            vector2f textureCoord;
        };

        struct __blib_api Vector3f
        {
            Vector3f()
            {
                float x = 0;
                float y = 0;
                float z = 0;
            }
            Vector3f(float _x, float _y, float _z)
            {
                this->x = _x;
                this->y = _y;
                this->z = _z;
            }
            float x = 0;
            float y = 0;
            float z = 0;
        };

        struct __blib_api Vertex4f
        {
            float x = 0.f;
            float y = 0.f;
            float z = 0.f;
            float w = 0.f;
        };

        struct __blib_api Transform3f
        {
            Vector3f position;
            Vector3f rotation; //direction
            Vector3f scale = { 1,1,1 };
            Vector3f origin;
        };

        __blib_inline void operator+=(Vector3f& lhs, const Vector3f& rhs)
        {
            lhs.x += rhs.x;
            lhs.y += rhs.y;
            lhs.z += rhs.z;
        }
    }
}