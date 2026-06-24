#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include <blib/graphics/image.h>


namespace parsers
{
    class GM1File
    {
    public:
        enum class DataType : uint32_t
        {
            Interface = 0x00000001,
            Animation = 0x00000002,
            Building = 0x00000003,
            Font = 0x00000004,
            BitMap = 0x00000005,
            ConstSize = 0x00000006,
            BitMap1 = 0x00000007
        };

        struct Header
        {
            uint32_t quantity;
            DataType dataType;
            uint32_t dataSize;
        };

        struct ImageHeader
        {
            uint16_t width;
            uint16_t height;
            uint16_t widthOffset;
            uint16_t heightOffset;
            uint8_t part;
            uint8_t subparts;
            uint16_t tileOffset;
            uint8_t dir;
            uint8_t initialHorizontalOffset;
            uint8_t buildingWidth;
            uint8_t color;
        };

        std::vector<blib::graphics::Image> loadFromFile(const std::string& path);
        std::vector<blib::graphics::Image> parseFromMemory(const std::vector<uint8_t>& data);

    private:
        static const size_t PALETTE_SIZE = 5120;
        std::vector<uint8_t> palette;
        Header header;
        std::vector<ImageHeader> imageHeaders;

        blib::graphics::Image parseTGXWithPalette(const uint8_t* data, size_t size,
            const ImageHeader& imgHeader, uint8_t playerColor = 0);
    };
}