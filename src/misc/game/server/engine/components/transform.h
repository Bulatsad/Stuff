#pragma once

#include <engine/components/component.h>

#include <engine/bridge/transform.h>

namespace eng
{
    namespace components
    {
        class TransformComponent
        {
        private:
            Transform transform;
        public:

            //component
            bool registerComponent(ComponentRegisterContext context);

        };
    }
}
