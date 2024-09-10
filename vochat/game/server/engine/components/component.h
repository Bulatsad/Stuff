#pragma once

#include <engine/events/event.h>

#include <blib/blibint.h>

namespace eng
{
    namespace components
    {
        typedef buint8 ComponentType;


        
        struct ComponentRegisterContext
        {
            ComponentController componentController;
            EventController eventContoller;
        };

        class ComponentController
        {
        private:

        public:
            template<class ComponentT>
            ComponentType registerClass(ComponentT component);
        };
    }
}
