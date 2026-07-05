#include "..\..\animator.h"

bool blib::graphics::Animator::loadFromAssimp(const aiScene* paiscene)
{
    this->animationList.resize(paiscene->mNumAnimations);

    for (size_t i = 0; i < this->animationList.size(); ++i)
    {
        if(!(this->animationList[i].loadFromAssimp(paiscene->mAnimations[i])))
        {
            // TODO : Logging
            return false;
        }
    }

    return true;
}
