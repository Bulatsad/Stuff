#pragma once

#include <string>
#include <vector>

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/utilmacro.h>

namespace blib
{
    namespace core
    {
        typedef std::vector<std::string>StringList;

        StringList __blib_api split(const std::string& str, const std::string& delimeter);
        buint64 __blib_api replace(_In _Out std::string& str, _In const std::string& from, _In const std::string& to);
        
        template<class T>
        bool contains(const std::vector<T>& arr, const T& elem)
        {
            for (const auto& a : arr)
            {
                if (a == elem)
                    return true;
            }
            return false;
        }
    }
}
