#pragma once

#include <blib/blibint.h>

namespace eng
{
    typedef buint8 eventCreator;
    //{
    //    Server = 0x00,
    //    Client = 0x01
    //};

    typedef buint8 eventType;
    //{
    //    Move = 0x00,
    //    Build = 0x01,
    //    Collision = 0x02,
    //    
    //};

    class EventController
    {
    public:
        eventType registerEvent();
    };

    struct Event
    {
        eventType type;
        eventCreator creator;
        size_t size;
    };

}
