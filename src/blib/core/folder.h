#pragma once

#include <string>
#include <vector>

#include <blib/config.h>

namespace blib
{
    namespace core
    {
        class __blib_api Folder
        {
        public:
            Folder(const std::string& path);

            std::string getCurrentPath() const;
            std::vector<std::string> getAllEntries() const;

            bool isFolder();
            bool up();
            bool down(const std::string& foldername);

        private:
            std::string currentPath;
        };
    }
}
