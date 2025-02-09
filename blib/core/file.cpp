#include "file.h"

#include <string>

blib::core::File::File(std::ifstream&& fin)
{
    this->fin = std::move(fin);
}

blib::core::ByteArray blib::core::File::readAll()
{
    std::vector<buint8> res;

    this->fin.seekg(0, std::ios::end);
    size_t size = this->fin.tellg();
    this->fin.seekg(0);

    if (size == 0)
        return res;

    res.resize(size);

    this->fin.read(reinterpret_cast<char*>(&res[0]), size);

    return res;
}

blib::core::File::~File()
{
    if (this->fin.is_open())
        this->fin.close();
}
