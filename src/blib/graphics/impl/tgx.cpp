#include <fstream>
#include <memory>

#include <blib/graphics/tgx.h>

namespace parsers
{
    std::vector<blib::graphics::Image> TGXFile::readFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return {};
        }

        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(fileSize);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
            return {};
        }

        return parseFromMemory(buffer);
    }

    std::vector<blib::graphics::Image> TGXFile::parseFromMemory(const std::vector<uint8_t>& data)
    {
        if (data.size() < 8) {
            return {};
        }

        TGXHeader header = parseHeader(data.data());
        std::vector<blib::graphics::Image> images;
        images.push_back(parseImageData(data.data() + 8, data.size() - 8, header));

        return images;
    }

    TGXFile::TGXHeader TGXFile::parseHeader(const uint8_t* data)
    {
        TGXHeader header;
        const uint16_t* wordData = reinterpret_cast<const uint16_t*>(data);
        header.width = wordData[0];
        header.height = wordData[2];
        return header;
    }

    blib::graphics::Color TGXFile::parseTGXColor(uint16_t tgxPixel)
    {
        blib::graphics::Color color;
        color.red = ((tgxPixel >> 10) & 0x1F) << 3;
        color.green = ((tgxPixel >> 5) & 0x1F) << 3;
        color.blue = (tgxPixel & 0x1F) << 3;
        color.alpha = 255;
        return color;
    }

    blib::graphics::Image TGXFile::parseImageData(const uint8_t* data, size_t size, const TGXHeader& header)
    {
        blib::graphics::Image image;
        image.create(header.width, header.height, blib::graphics::Color::Transparent);

        const uint8_t* end = data + size;
        uint16_t x = 0, y = 0;

        while (data < end && y < header.height)
        {
            uint8_t len = (*data & 0x1F) + 1;
            TGXTokenType type = static_cast<TGXTokenType>(*data >> 5);
            data++;

            switch (type)
            {
            case TGXTokenType::PixelStream:
                for (uint8_t i = 0; i < len && x < header.width; i++, x++)
                {
                    uint16_t pixel = *reinterpret_cast<const uint16_t*>(data);
                    image[x][y] = parseTGXColor(pixel);
                    data += 2;
                }
                break;

            case TGXTokenType::TransparentPixelString:
                for (uint8_t i = 0; i < len && x < header.width; i++, x++)
                {
                    // ѕрозрачные пиксели - оставл€ем как есть (Transparent)
                }
                break;

            case TGXTokenType::RepeatingPixels:
                if (data + 2 <= end)
                {
                    uint16_t pixel = *reinterpret_cast<const uint16_t*>(data);
                    blib::graphics::Color color = parseTGXColor(pixel);
                    data += 2;

                    for (uint8_t i = 0; i < len && x < header.width; i++, x++)
                    {
                        image[x][y] = color;
                    }
                }
                break;

            case TGXTokenType::NewLine:
                x = 0;
                y++;
                break;

            default:
                break;
            }
        }

        return image;
    }
}
