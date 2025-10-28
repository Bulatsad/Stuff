#pragma once

#include <vector>
#include <string>

#include <blib/config.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api IComponent
        {
        public:
            IComponent();
            virtual ~IComponent();

            virtual std::string CompomemtName() const = 0;
        protected:
        };
    }
}
