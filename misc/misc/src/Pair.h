#pragma once

namespace blib
{
    template<typename T1, typename T2>
    struct Pair
    {
        T1 first;
        T2 second;
    };

    template<typename T1, typename T2>
    Pair<typename T1, typename T2> make_pair(const T1& t1, const T2& t2);



    template<typename T1, typename T2>
    Pair<typename T1, typename T2> make_pair(const T1& t1, const T2& t2)
    {
        Pair<T1, T2> p;
        p.first = t1;
        p.second = t2;
        return p;
    }
}
