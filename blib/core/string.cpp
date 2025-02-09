#include <blib/core/string.h>

blib::core::StringList blib::core::split(const std::string& str, const std::string& delimeter)
{
    StringList res;

    size_t start = 0;
    size_t pos = str.find(delimeter, start);

    while (pos != std::string::npos)
    {
        res.emplace_back(str.substr(start, pos - start));
        start = pos + 1;
        pos = str.find(delimeter, start);
    }

    res.emplace_back(str.substr(start, str.size() - start));

    return res;
}
