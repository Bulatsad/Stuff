#include <blib/graphics/hierarchal.h>

blib::graphics::IHierarchal::IHierarchal()
{
    this->parent = nullptr;
}

blib::graphics::IHierarchal::~IHierarchal()
{
}

void blib::graphics::IHierarchal::setParent(blib::graphics::IHierarchal* pH)
{
    this->parent = pH;
}

const blib::graphics::IHierarchal* blib::graphics::IHierarchal::getParent() const
{
    return this->parent;
}

blib::graphics::IHierarchal* blib::graphics::IHierarchal::getParent()
{
    return this->parent;
}

bool blib::graphics::IHierarchal::addChild(blib::graphics::IHierarchal* ph)
{
    ph->setParent(this);
    this->childs.push_back(ph);
    return true;
}

//const std::vector<const blib::graphics::IHierarchal*>& blib::graphics::IHierarchal::getChilds() const
//{
//    return static_cast<const std::vector<const blib::graphics::IHierarchal*> >(this->childs);
//}

const std::vector<blib::graphics::IHierarchal*>& blib::graphics::IHierarchal::getChilds()
{
    return this->childs;
}
