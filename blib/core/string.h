#pragma once

#include <string>
#include <vector>

namespace blib
{
    namespace core
    {
        typedef std::vector<std::string>StringList;

        StringList split(const std::string& str, const std::string& delimeter);
    }
}
