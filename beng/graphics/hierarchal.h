#pragma once

#include <vector>

#include <beng/config.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api IHierarchal
        {
        public:
            IHierarchal();

            // dont call destructor for childs mb rewrite
            virtual ~IHierarchal();

            void setParent(beng::graphics::IHierarchal* pH);
            const beng::graphics::IHierarchal* getParent() const;
            beng::graphics::IHierarchal* getParent();

            bool addChild(beng::graphics::IHierarchal* ph);
            const std::vector<const beng::graphics::IHierarchal*>& getChilds() const;
            const std::vector<beng::graphics::IHierarchal*>& getChilds();

        protected:
            beng::graphics::IHierarchal* parent;
            std::vector<beng::graphics::IHierarchal*>childs;
        };
    }
}
