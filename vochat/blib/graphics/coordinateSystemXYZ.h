#pragma once

#include <blib/graphics/vertex.h>

namespace blib
{
    namespace graphics
    {
        class CoordinateSystemXYZ
        {
        private:

        public:
            /*!
            * \brief vector3f transform(const vector3f& position)
            * 
            *   transform coordinateSystem position to renderApi 
            *   coordinate system
            */
            Vector3f transform(const Vector3f& position);
        };
    }
}
