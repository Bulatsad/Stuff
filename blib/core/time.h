#pragma once

#include <ctime>
#include <blib/blibint.h>

namespace blib
{
    namespace core
    {
        typedef 
        class Time
        {
            static bint64 delteTime;
        public:
            Time() = delete;
            ~Time() = delete;

            static void init()
            {
                blib::core::Time::delteTime = 0;
            }

            static void update()
            {
                blib::core::Time::delteTime = clock() - blib::core::Time::delteTime;
            }
            /*!
            *   \brief return time as ms from last update
            */
            static bint64 getDeltaTime()
            {
                return blib::core::Time::delteTime;
            }
        };
    }
}
