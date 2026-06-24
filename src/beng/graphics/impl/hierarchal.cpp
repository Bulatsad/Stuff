#include <beng/graphics/hierarchal.h>

beng::graphics::IHierarchal::IHierarchal()
{
    this->parent = nullptr;
}

beng::graphics::IHierarchal::~IHierarchal()
{
}

void beng::graphics::IHierarchal::setParent(beng::graphics::IHierarchal* pH)
{
    this->parent = pH;
}

const beng::graphics::IHierarchal* beng::graphics::IHierarchal::getParent() const
{
    return this->parent;
}

beng::graphics::IHierarchal* beng::graphics::IHierarchal::getParent()
{
    return this->parent;
}

bool beng::graphics::IHierarchal::addChild(beng::graphics::IHierarchal* ph)
{
    this->childs.push_back(ph);
    return true;
}

//const std::vector<const beng::graphics::IHierarchal*>& beng::graphics::IHierarchal::getChilds() const
//{
//    return static_cast<const std::vector<const beng::graphics::IHierarchal*> >(this->childs);
//}

const std::vector<beng::graphics::IHierarchal*>& beng::graphics::IHierarchal::getChilds()
{
    return this->childs;
}
