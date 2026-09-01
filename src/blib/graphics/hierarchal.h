#pragma once

#include <vector>

#include <blib/config.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api IHierarchal
        {
        public:
            IHierarchal();

            // dont call destructor for childs mb rewrite
            virtual ~IHierarchal();

            void setParent(blib::graphics::IHierarchal* pH);
            const blib::graphics::IHierarchal* getParent() const;
            blib::graphics::IHierarchal* getParent();

            bool addChild(blib::graphics::IHierarchal* ph);
            const std::vector<const blib::graphics::IHierarchal*>& getChilds() const;
            const std::vector<blib::graphics::IHierarchal*>& getChilds();

        protected:
            blib::graphics::IHierarchal* parent;
            std::vector<blib::graphics::IHierarchal*>childs;
        };
    }
}
