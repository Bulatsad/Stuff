#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include <blib/graphics/image.h>

namespace parsers
{
    class TGXFile
    {
    public:
        struct TGXHeader
        {
            uint16_t width;
            uint16_t height;

            TGXHeader() = default;
            TGXHeader(uint16_t w, uint16_t h) : width(w), height(h) {}
        };

        std::vector<blib::graphics::Image> readFile(const std::string& path);
        std::vector<blib::graphics::Image> parseFromMemory(const std::vector<uint8_t>& data);

    private:
        enum class TGXTokenType : uint8_t
        {
            PixelStream = 0x00,
            TransparentPixelString = 0x01,
            RepeatingPixels = 0x02,
            NewLine = 0x04
        };

        TGXHeader parseHeader(const uint8_t* data);
        blib::graphics::Color parseTGXColor(uint16_t tgxPixel);
        blib::graphics::Image parseImageData(const uint8_t* data, size_t size, const TGXHeader& header);
    };
}