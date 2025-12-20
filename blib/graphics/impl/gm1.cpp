#include <blib/graphics/gm1.h>

#include <fstream>
#include <algorithm>

#include <blib/graphics/tgx.h>

namespace parsers
{
    std::vector<blib::graphics::Image> GM1File::loadFromFile(const std::string& path)
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

    std::vector<blib::graphics::Image> GM1File::parseFromMemory(const std::vector<uint8_t>& data)
    {
        if (data.size() < 80) {
            return {};
        }

        const uint8_t* ptr = data.data();

        // Пропускаем 12 байт
        ptr += 12;

        // Читаем заголовок
        header.quantity = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += 4;

        header.dataType = static_cast<DataType>(*reinterpret_cast<const uint32_t*>(ptr));
        ptr += 4;

        // Пропускаем 56 байт
        ptr += 56;

        header.dataSize = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += 4;

        // Читаем палитру
        palette.assign(ptr, ptr + PALETTE_SIZE);
        ptr += PALETTE_SIZE;

        // Читаем смещения изображений
        std::vector<uint32_t> imageOffsets(header.quantity);
        for (size_t i = 0; i < header.quantity; i++) {
            imageOffsets[i] = *reinterpret_cast<const uint32_t*>(ptr);
            ptr += 4;
        }

        // Читаем размеры изображений
        std::vector<uint32_t> imageSizes(header.quantity);
        for (size_t i = 0; i < header.quantity; i++) {
            imageSizes[i] = *reinterpret_cast<const uint32_t*>(ptr);
            ptr += 4;
        }

        // Читаем заголовки изображений
        imageHeaders.resize(header.quantity);
        for (size_t i = 0; i < header.quantity; i++) {
            imageHeaders[i].width = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;
            imageHeaders[i].height = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;
            imageHeaders[i].widthOffset = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;
            imageHeaders[i].heightOffset = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;
            imageHeaders[i].part = *ptr++;
            imageHeaders[i].subparts = *ptr++;
            imageHeaders[i].tileOffset = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;
            imageHeaders[i].dir = *ptr++;
            imageHeaders[i].initialHorizontalOffset = *ptr++;
            imageHeaders[i].buildingWidth = *ptr++;
            imageHeaders[i].color = *ptr++;
        }

        // Парсим изображения
        std::vector<blib::graphics::Image> images;
        TGXFile tgxParser;

        for (size_t i = 0; i < header.quantity; i++) {
            if (imageOffsets[i] + imageSizes[i] > data.size()) {
                continue;
            }

            const uint8_t* imageData = data.data() + imageOffsets[i];

            switch (header.dataType) {
            case DataType::Interface:
            case DataType::Font:
            case DataType::ConstSize:
            case DataType::Animation:
                images.push_back(parseTGXWithPalette(imageData, imageSizes[i],
                    imageHeaders[i], imageHeaders[i].color));
                break;

            case DataType::BitMap:
            case DataType::BitMap1:
                // Для bitmap просто парсим как TGX
            {
                TGXFile::TGXHeader tgxHeader(imageHeaders[i].width, imageHeaders[i].height);
                auto tgxImages = tgxParser.parseFromMemory(
                    std::vector<uint8_t>(imageData, imageData + imageSizes[i])
                );
                if (!tgxImages.empty()) {
                    images.push_back(std::move(tgxImages[0]));
                }
            }
            break;

            default:
                // Для остальных типов создаем пустое изображение
                blib::graphics::Image emptyImage;
                emptyImage.create(imageHeaders[i].width, imageHeaders[i].height, blib::graphics::Color::Transparent);
                images.push_back(emptyImage);
                break;
            }
        }

        return images;
    }

    blib::graphics::Image GM1File::parseTGXWithPalette(const uint8_t* data, size_t size,
        const ImageHeader& imgHeader, uint8_t playerColor)
    {
        blib::graphics::Image image;
        image.create(imgHeader.width, imgHeader.height, blib::graphics::Color::Transparent);

        auto parseColor = [](uint16_t tgxPixel) -> blib::graphics::Color {
            blib::graphics::Color color;
            color.red = ((tgxPixel >> 10) & 0x1F) << 3;
            color.green = ((tgxPixel >> 5) & 0x1F) << 3;
            color.blue = (tgxPixel & 0x1F) << 3;
            color.alpha = 255;
            return color;
            };

        const uint8_t* end = data + size;
        uint16_t x = 0, y = 0;

        while (data < end && y < imgHeader.height) {
            uint8_t len = (*data & 0x1F) + 1;
            uint8_t type = *data >> 5;
            data++;

            switch (type) {
            case 0: // PixelStream
                for (uint8_t i = 0; i < len && x < imgHeader.width; i++, x++) {
                    uint8_t paletteIndex = *data++;
                    uint16_t paletteOffset = (256 * playerColor + paletteIndex) * 2;

                    if (paletteOffset + 2 <= palette.size()) {
                        uint16_t colorValue = *reinterpret_cast<const uint16_t*>(
                            palette.data() + paletteOffset
                            );
                        image[x][y] = parseColor(colorValue);
                    }
                }
                break;

            case 1: // TransparentPixelString
                for (uint8_t i = 0; i < len && x < imgHeader.width; i++, x++) {
                    // Прозрачные пиксели
                }
                break;

            case 2: // RepeatingPixels
                if (data < end) {
                    uint8_t paletteIndex = *data++;
                    uint16_t paletteOffset = (256 * playerColor + paletteIndex) * 2;

                    if (paletteOffset + 2 <= palette.size()) {
                        uint16_t colorValue = *reinterpret_cast<const uint16_t*>(
                            palette.data() + paletteOffset
                            );
                        blib::graphics::Color color = parseColor(colorValue);

                        for (uint8_t i = 0; i < len && x < imgHeader.width; i++, x++) {
                            image[x][y] = color;
                        }
                    }
                }
                break;

            case 4: // NewLine
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
