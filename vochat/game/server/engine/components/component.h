#pragma once

#include <blib/blibint.h>

namespace eng
{
    namespace components
    {
        typedef buint8 ComponentType;
        class ComponentController
        {
        private:

        public:
            template<class Component>
            ComponentType registerClass(Component component);
        };
    }
}
