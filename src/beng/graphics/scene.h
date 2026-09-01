//#pragma once
//
//#include <vector>
//
//#include <blib/config.h>
//#include <beng/graphics/gameobject.h>
//#include <blib/graphics/drawable.h>
//
//
//namespace blib
//{
//    namespace graphics
//    {
//        class __blib_graphics_api GameObject;
//
//        class __blib_graphics_api Scene : public blib::graphics::IDrawable
//        {
//        public:
//            GameObject* instantiate();
//            template<class ComponentType>
//           // IComponent* createComponent();
//
//            // IDrawable
//            //virtual void draw(RenderTarget& target, RenderContext& ctx) const override;
//
//
//        private:
//            std::vector<GameObject*>gameObjects;
//            //std::vector<std::vector<IComponent*> > components;
//            //std::vector<IComponent*>components;
//        };
//        //template<class ComponentType>
//        //inline IComponent* Scene::createComponent()
//        //{
//        //    IComponent res = new ComponentType();
//        //    this->components.push_back(res);
//        //    return res;
//        //}
//    }
//}
