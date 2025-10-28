#pragma once

#include <vector>

#include <blib/config.h>
#include <blib/graphics/transformable.h>
#include <beng/graphics/scene.h>

namespace beng
{
    namespace graphics
    {
        class __blib_api GameObject
        {
        public:
            virtual ~GameObject();

            void setParent(beng::graphics::GameObject* pGO);
            const beng::graphics::GameObject* getParent() const;
            beng::graphics::GameObject* getParent();

            bool addChild(beng::graphics::GameObject* pGO);
            const std::vector<const beng::graphics::GameObject*>getChilds() const;
            const std::vector<beng::graphics::GameObject*>getChilds();

            //const std::vector<beng::graphics::IComponent*> getComponents();
            //const std::vector<const beng::graphics::IComponent *const>& getComponents() const;
            //const IComponent* getComponentByName(const std::string& name);
            //
            //template<class ComponentType>
            //IComponent* addComponent();

        protected:
            beng::graphics::GameObject* parent = nullptr;
            std::vector<beng::graphics::GameObject*>childs;
            //std::vector<beng::graphics::IComponent*>components;

        };
        

    }
}