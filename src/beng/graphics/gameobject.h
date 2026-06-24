#pragma once

#include <vector>

#include <blib/graphics/transformable.h>

#include <beng/config.h>
#include <beng/graphics/scene.h>
#include <beng/graphics/hierarchal.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api GameObject : public blib::graphics::ITransformable, public beng::graphics::IHierarchal
        {
        public:
            virtual ~GameObject();



            //const std::vector<beng::graphics::IComponent*> getComponents();
            //const std::vector<const beng::graphics::IComponent *const>& getComponents() const;
            //const IComponent* getComponentByName(const std::string& name);
            //
            //template<class ComponentType>
            //IComponent* addComponent();

        protected:

            //std::vector<beng::graphics::IComponent*>components;

        };
        

    }
}