#pragma once
#pragma once

#include <blib/blibint.h>

namespace eng
{
    typedef buint64 ID;
    typedef buint8 EntityType;


    class EntityController
    {
    public:
        bool registerEnity();
    };

    class Entity
    {
    private:
        ID id;
        EntityType type;
    public:

    };
}
