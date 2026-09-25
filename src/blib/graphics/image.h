#pragma once

#include <blib/config.h>

#include <blib/blibint.h>
#include <blib/core/unsafeslicer.h>
#include <blib/core/isaveloadable.h>
#include <blib/core/json/json.h>
#include <blib/system/memory/iallocatorAware.h>
#include <blib/graphics/color.h>

#include <vector>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Image : public blib::core::ISaveLoadable, public blib::memory::IAllocatorAware
        {
        private:
            std::vector<Color> bitmap;

        public:
            // Стабильное имя типа ресурса — тег кеша ресурсов
            // (ResourceManager; сравнение по содержимому, не по адресу)
            static constexpr const char* resourceTypeName = "blib.graphics.Image";

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
            ~Image() __blib_override;

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

            // ========== ISaveLoadable: сериализация и сравнение ==========
            //
            // Формат toJson: {width, height, pixels[]} — плоский массив
            // байт RGBA (width * height * 4 элементов, row-major).
            // fromJson валидирует размеры и длину массива ДО применения.
            // Не прятать 1-аргументную точку входа строгого сравнения.

            using blib::core::IStrongComparable::strongCompare;

            blib::core::json::JsonValue toJson() const;
            blib::core::LoadStatus fromJson(_In const blib::core::json::JsonValue& json);
            blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;
            blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;
            bool strongCompare(_In const blib::core::IStrongComparable& other,
                _In blib::core::CompareSession& session) const __blib_override;
            bool verify() const __blib_override;
        };
    }
}

