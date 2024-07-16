#pragma once

#include <string>

namespace blib
{
    namespace graphics
    {
        enum class ModelParsingStatus
        {
            OK,
            CannotOpenFile,
            UnsupportedFormat,

            END_OF_ENUM
        };
        class ObjModel
        {
        private:
            void* ctx;
        public:
            ObjModel();
            ~ObjModel();
            ModelParsingStatus loadFromFile(const std::string& file);
            void testDraw();
        };
    }
}
