#pragma once

#include <blib/config.h>
#include <blib/graphics/vertex.h>

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
        class __blib_api ObjModel
        {
        private:
            void* ctx;
        public:
            Transform3f transform;
            ObjModel();
            ~ObjModel();
            ModelParsingStatus loadFromFile(const std::string& file);

            //if you want use origin on load you must recalculate all coords
            //using origin an then use it. Can`t change origin at run time
            void testDraw();
        };
    }
}
