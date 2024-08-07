#pragma once

#include <blib/core/bytearray.h>
#include <blib/core/flags.h>

#include <fstream>
#include <vector>

namespace blib
{
    namespace core
    {
        enum class FileStatus : buint8
        {
            CantOpen,

            OK
        };

        enum class OpenMode : buint8
        {
            Read = 0x01,
            Write = 0x02,
            Binary = 0x04,
            Truncate = 0x08,
            Append = 0x10
        };

        class File
        {
        private:
            std::ifstream fin;
        public:
            typedef Flags<OpenMode> OpenModeFlags;

            File();
            File(std::ifstream&& fin);
            FileStatus open(const char* path, OpenModeFlags mode);
            ByteArray readAll();
            ~File();
        };
    }
}
