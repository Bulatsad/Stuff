#pragma once

#include <blib/config.h>

#include <blib/blibint.h>
#include <blib/core/unsafeslicer.h>
#include <blib/graphics/color.h>

#include <vector>

namespace blib
{
    namespace graphics
    {
        class __blib_api Image
        {
        private:
            std::vector<Color> bitmap;

        public:
            buint16 width;
            buint16 height;

            Image();

            /*
            Create own data in memory if pdata is null
            */
            Image(
                decltype(blib::graphics::Image::width) aWidth,
                decltype(blib::graphics::Image::height) aHeight,
                const Color* pdata = nullptr);
            ~Image();

            std::vector<Color>& data();
            void create(buint16 width, buint16 height, blib::graphics::Color color);

            // OLD C CODE
            bool loadFromTgx(const char* path);
            bool loadTGXPixelData(
                /*_In*/ const buint8* pdata,
                /*_In*/ const size_t pdatasize,
                /*_In*/ buint8* pallete,
                /*_In*/ buint8 color,
                /*_In*/ buint16 nWidth,
                /*_In*/ buint16 nHeight //,
                //std::vector<blib::graphics::Color>& pPixelData
            );

            const void* getData() const;


            void update(decltype(blib::graphics::Image::width) posX, decltype(blib::graphics::Image::height) posY, const blib::graphics::Image& img);
            blib::core::UnsafeSlicer<blib::graphics::Color>operator[](buint16 index) __blib_unsafe;
            const blib::core::UnsafeSlicer<blib::graphics::Color>operator[](buint16 index) const __blib_unsafe;
        };
    }
}
