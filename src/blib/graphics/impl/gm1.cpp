#include <blib/graphics/gm1.h>

#include <fstream>
#include <algorithm>

#include <blib/core/console/console.h>

namespace parsers
{
    namespace
    {
        // ---------------------------------------------------------------
        // Константы формата GM1 (по референс-декомпилу Stronghold Image
        // Toolbox, см. GRAPHICS.md «Форматы»)
        // ---------------------------------------------------------------

        // Смещения заголовка файла
        constexpr size_t headerBaseSize = 88;      // 22 x uint32 (0..87)
        constexpr size_t imageOffsetField = 12;    // Image_Count
        constexpr size_t dataTypeField = 20;       // Data_Type
        constexpr size_t dataSizeField = 80;       // Data_Size
        constexpr size_t paletteField = 88;        // палитра: 2560 x uint16
        constexpr size_t offsetsField = 5208;      // offsets/sizes/headers

        constexpr size_t imageHeaderSize = 16;     // размер заголовка кадра

        // Лимиты валидации (защита от битых файлов: исключения запрещены
        // правилами проекта, мусорные размеры уронили бы vector::resize)
        constexpr uint32_t maxImages = 4096;
        constexpr buint32 maxImageSide = 4096;

        // Прозрачный пиксель палитровых кадров: индекс 0 — всегда
        // прозрачный (первый цвет каждой палитры игрока — заглушка)
        constexpr uint8_t transparentPaletteIndex = 0;

        // Цвет RGB555 (u16 LE) -> Color (5 бит -> 8 бит сдвигом <<3)
        blib::graphics::Color colorFromTgx(uint16_t tgxPixel)
        {
            blib::graphics::Color color;
            color.red = static_cast<buint8>((tgxPixel >> 10) & 0x1F) << 3;
            color.green = static_cast<buint8>((tgxPixel >> 5) & 0x1F) << 3;
            color.blue = static_cast<buint8>(tgxPixel & 0x1F) << 3;
            color.alpha = 255;
            return color;
        }

        /**
         * Декодер TGX-токенов с 16-битными пикселями (RGB555) —
         * типы Interface/Font/ConstSize и TGX-часть Building.
         * Токен: старшие 3 бита — тип (0=pixelstream, 1=transparent,
         * 2=repeating, 4=newline), младшие 5 — длина-1.
         * Чтение не выходит за size; картинка заполняется до h строк.
         */
        blib::graphics::Image decodeTgx16(
            const uint8_t* data, size_t size, buint32 width, buint32 height)
        {
            blib::graphics::Image image;
            image.create(static_cast<buint16>(width), static_cast<buint16>(height),
                blib::graphics::Color::Transparent);

            const uint8_t* end = data + size;
            buint32 x = 0, y = 0;

            while (data < end && y < height)
            {
                const uint8_t token = *data++;
                const uint8_t type = token >> 5;
                const buint32 length = static_cast<buint32>(token & 0x1F) + 1;

                switch (type)
                {
                case 0: // PixelStream: length x 16-битных пикселей
                    for (buint32 i = 0; i < length && x < width; ++i, ++x)
                    {
                        if (data + 2 > end)
                        {
                            return image;
                        }
                        const uint16_t pixel = static_cast<uint16_t>(data[0]) |
                            (static_cast<uint16_t>(data[1]) << 8);
                        data += 2;
                        image[x][y] = colorFromTgx(pixel);
                    }
                    break;

                case 1: // TransparentPixelString: length прозрачных
                    for (buint32 i = 0; i < length && x < width; ++i, ++x)
                    {
                        // пиксели остаются прозрачными (заливка create)
                    }
                    break;

                case 2: // RepeatingPixels: один 16-битный пиксель x length
                    if (data + 2 > end)
                    {
                        return image;
                    }
                    {
                        const uint16_t pixel = static_cast<uint16_t>(data[0]) |
                            (static_cast<uint16_t>(data[1]) << 8);
                        data += 2;
                        const blib::graphics::Color color = colorFromTgx(pixel);
                        for (buint32 i = 0; i < length && x < width; ++i, ++x)
                        {
                            image[x][y] = color;
                        }
                    }
                    break;

                case 4: // NewLine
                    x = 0;
                    ++y;
                    break;

                default:
                    // неизвестный токен — битые данные, останавливаемся
                    return image;
                }
            }

            return image;
        }

        /**
         * Декодер TGX-токенов с палитровыми индексами (1 байт/пиксель) —
         * тип Animation. Цвет = palette[(paletteBase + index) * 2]
         * (палитра сегментирована по игрокам, 256 цветов на игрока).
         * Индекс 0 — прозрачный (первый цвет палитры — заглушка).
         */
        blib::graphics::Image decodeTgxIndexed(
            const uint8_t* data, size_t size, buint32 width, buint32 height,
            const std::vector<uint8_t>& palette, buint32 paletteBase)
        {
            blib::graphics::Image image;
            image.create(static_cast<buint16>(width), static_cast<buint16>(height),
                blib::graphics::Color::Transparent);

            const uint8_t* end = data + size;
            buint32 x = 0, y = 0;

            while (data < end && y < height)
            {
                const uint8_t token = *data++;
                const uint8_t type = token >> 5;
                const buint32 length = static_cast<buint32>(token & 0x1F) + 1;

                switch (type)
                {
                case 0: // PixelStream: length x индекс палитры
                    for (buint32 i = 0; i < length && x < width; ++i, ++x)
                    {
                        const uint8_t paletteIndex = *data++;
                        if (paletteIndex == transparentPaletteIndex)
                        {
                            continue;
                        }
                        const size_t paletteOffset = (paletteBase + paletteIndex) * 2;
                        if (paletteOffset + 2 <= palette.size())
                        {
                            const uint16_t colorValue = static_cast<uint16_t>(palette[paletteOffset]) |
                                (static_cast<uint16_t>(palette[paletteOffset + 1]) << 8);
                            image[x][y] = colorFromTgx(colorValue);
                        }
                    }
                    break;

                case 1: // TransparentPixelString
                    for (buint32 i = 0; i < length && x < width; ++i, ++x)
                    {
                        // прозрачные
                    }
                    break;

                case 2: // RepeatingPixels: один индекс x length
                    {
                        const uint8_t paletteIndex = *data++;
                        if (paletteIndex != transparentPaletteIndex)
                        {
                            const size_t paletteOffset = (paletteBase + paletteIndex) * 2;
                            if (paletteOffset + 2 <= palette.size())
                            {
                                const uint16_t colorValue = static_cast<uint16_t>(palette[paletteOffset]) |
                                    (static_cast<uint16_t>(palette[paletteOffset + 1]) << 8);
                                const blib::graphics::Color color = colorFromTgx(colorValue);
                                for (buint32 i = 0; i < length && x < width; ++i, ++x)
                                {
                                    image[x][y] = color;
                                }
                                break;
                            }
                        }
                        for (buint32 i = 0; i < length && x < width; ++i, ++x)
                        {
                            // прозрачные
                        }
                    }
                    break;

                case 4: // NewLine
                    x = 0;
                    ++y;
                    break;

                default:
                    return image;
                }
            }

            return image;
        }

        /**
         * Сырой 16-битный битмап (BitMap/BitMap1): w x h пикселей RGB555.
         */
        blib::graphics::Image decodeRaw16(
            const uint8_t* data, size_t size, buint32 width, buint32 height)
        {
            blib::graphics::Image image;
            image.create(static_cast<buint16>(width), static_cast<buint16>(height),
                blib::graphics::Color::Transparent);

            const size_t requiredSize = static_cast<size_t>(width) * height * 2;
            if (requiredSize > size)
            {
                return image;
            }

            for (buint32 y = 0; y < height; ++y)
            {
                for (buint32 x = 0; x < width; ++x)
                {
                    const size_t offset = (static_cast<size_t>(y) * width + x) * 2;
                    const uint16_t pixel = static_cast<uint16_t>(data[offset]) |
                        (static_cast<uint16_t>(data[offset + 1]) << 8);
                    image[x][y] = colorFromTgx(pixel);
                }
            }
            return image;
        }

        /**
         * Ромб-тайл 30x16 (тип Building, часть кадра 512 байт): данные —
         * 256 x uint16 пикселей, строки ромба длинами
         * 2,6,10,14,18,22,26,30,30,26,22,18,14,10,6,2; каждая строка
         * центрируется в 30-пиксельную строку, края — прозрачные
         * (референс: GM1ImageTile в Stronghold Image Toolbox).
         */
        blib::graphics::Image decodeTile16(const uint8_t* data, size_t size)
        {
            constexpr buint32 tileWidth = 30;
            constexpr buint32 tileHeight = 16;
            constexpr size_t tilePixelCount = 256; // сумма длин строк ромба
            constexpr size_t tileDataSize = tilePixelCount * 2;

            static const buint32 rowLengths[tileHeight] =
            {
                2, 6, 10, 14, 18, 22, 26, 30, 30, 26, 22, 18, 14, 10, 6, 2
            };

            blib::graphics::Image image;
            image.create(tileWidth, tileHeight, blib::graphics::Color::Transparent);

            if (size < tileDataSize)
            {
                return image;
            }

            const uint8_t* pixelData = data;
            for (buint32 y = 0; y < tileHeight; ++y)
            {
                const buint32 rowLength = rowLengths[y];
                const buint32 rowStart = (tileWidth - rowLength) / 2;

                for (buint32 i = 0; i < rowLength; ++i)
                {
                    const uint16_t pixel = static_cast<uint16_t>(pixelData[0]) |
                        (static_cast<uint16_t>(pixelData[1]) << 8);
                    pixelData += 2;
                    image[rowStart + i][y] = colorFromTgx(pixel);
                }
            }
            return image;
        }

        /**
         * Пустой прозрачный кадр для битого/нечитаемого заголовка —
         * сохраняет индексацию (images[i] соответствует imageHeaders[i])
         * и не позволяет мусорным размерам дойти до vector::resize
         * (исключения в проекте запрещены).
         */
        blib::graphics::Image makeEmptyImage()
        {
            blib::graphics::Image image;
            image.create(1, 1, blib::graphics::Color::Transparent);
            return image;
        }
    }

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
        // ---------------------------------------------------------------
        // Заголовок (22 x uint32) + палитра
        // ---------------------------------------------------------------

        if (data.size() < headerBaseSize + PALETTE_SIZE) {
            return {};
        }

        const uint8_t* ptr = data.data();
        ptr += imageOffsetField;
        header.quantity = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += 4;

        ptr += 4; // Unknown4

        header.dataType = static_cast<DataType>(*reinterpret_cast<const uint32_t*>(ptr));
        ptr += 4;

        ptr += 56; // Unknown5..17 + SizeType (14 x uint32)

        header.dataSize = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += 4;

        ptr += 4; // Unknown18

        palette.assign(ptr, ptr + PALETTE_SIZE);
        ptr += PALETTE_SIZE;

        // ---------------------------------------------------------------
        // Валидация количества кадров (мусорный quantity -> гигантские
        // векторы/allocations; исключения запрещены)
        // ---------------------------------------------------------------

        if (header.quantity == 0 || header.quantity > maxImages)
        {
            __blib_log_warning("gm1: invalid image count %u, file rejected",
                header.quantity);
            return {};
        }

        const size_t headerArea = static_cast<size_t>(offsetsField) +
            static_cast<size_t>(header.quantity) * 8 +
            static_cast<size_t>(header.quantity) * imageHeaderSize;
        if (headerArea > data.size())
        {
            __blib_log_warning("gm1: header area exceeds file size, file rejected");
            return {};
        }

        // ---------------------------------------------------------------
        // Таблицы offsets (относительные от начала данных!), sizes, headers
        // ---------------------------------------------------------------

        const uint32_t imageCount = header.quantity;

        std::vector<uint32_t> imageOffsets(imageCount);
        std::vector<uint32_t> imageSizes(imageCount);
        imageHeaders.resize(imageCount);
        imageTiles.resize(imageCount);

        ptr = data.data() + offsetsField;
        for (size_t i = 0; i < imageCount; i++) {
            imageOffsets[i] = *reinterpret_cast<const uint32_t*>(ptr);
            ptr += 4;
        }
        for (size_t i = 0; i < imageCount; i++) {
            imageSizes[i] = *reinterpret_cast<const uint32_t*>(ptr);
            ptr += 4;
        }
        for (size_t i = 0; i < imageCount; i++) {
            imageHeaders[i].width = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;
            imageHeaders[i].height = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;
            imageHeaders[i].horizontalOffset = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;
            imageHeaders[i].verticalOffset = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;
            imageHeaders[i].part = *ptr++;
            imageHeaders[i].subparts = *ptr++;
            imageHeaders[i].baseHeight = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;
            imageHeaders[i].direction = *ptr++;
            imageHeaders[i].horizontalStartOffset = *ptr++;
            imageHeaders[i].widthInGame = *ptr++;
            imageHeaders[i].performanceId = *ptr++;
        }

        const size_t dataStart = headerArea;
        const buint32 paletteBase = playerColorOverridden
            ? static_cast<buint32>(playerColorOverride) * 256
            : 0;

        // ---------------------------------------------------------------
        // Декодирование кадров (по Data_Type, см. gm1.h)
        // ---------------------------------------------------------------

        std::vector<blib::graphics::Image> images;
        images.reserve(imageCount);

        for (uint32_t i = 0; i < imageCount; i++)
        {
            const ImageHeader& imageHeader = imageHeaders[i];
            const uint32_t offset = imageOffsets[i];
            const uint32_t size = imageSizes[i];

            // Кадр обязан целиком лежать в блоке данных (dataSize — сумма
            // sizes всех кадров) и в файле
            const bool frameInRange =
                static_cast<uint64_t>(offset) + size <= header.dataSize &&
                dataStart + static_cast<uint64_t>(offset) + size <= data.size();

            const buint32 width = imageHeader.width;
            const buint32 height = imageHeader.height;
            const bool validSize = width > 0 && height > 0 &&
                width <= maxImageSide && height <= maxImageSide;

            if (!frameInRange || !validSize)
            {
                __blib_log_warning("gm1: frame %u skipped (broken offset/size or header)", i);
                images.push_back(makeEmptyImage());
                continue;
            }

            const uint8_t* imageData = data.data() + dataStart + offset;

            switch (header.dataType)
            {
            case DataType::Interface:   // 16-битный TGX
            case DataType::Font:
            case DataType::ConstSize:
                images.push_back(decodeTgx16(imageData, size, width, height));
                break;

            case DataType::Animation:   // палитровый TGX (индексы)
                images.push_back(decodeTgxIndexed(imageData, size, width, height,
                    palette, paletteBase));
                break;

            case DataType::Building:
            {
                // Кадр = 512 байт ромб-тайла (30x16) + TGX-часть здания
                // размером widthInGame x (baseHeight + 7). Тайл отдаём
                // всегда (getImageTiles) — из частей потребитель собирает
                // композит «здание + земля» (sc2img, см. SC2IMG.md)
                if (size >= 512)
                {
                    imageTiles[i] = decodeTile16(imageData, 512);
                }

                const buint32 tgxSize = size > 512 ? size - 512 : 0;
                const buint32 buildingWidth = imageHeader.widthInGame;
                const buint32 buildingHeight = static_cast<buint32>(imageHeader.baseHeight) + 7;
                const bool validBuildingSize = buildingWidth > 0 && buildingHeight > 0 &&
                    buildingWidth <= maxImageSide && buildingHeight <= maxImageSide;

                if (tgxSize > 0 && validBuildingSize)
                {
                    images.push_back(decodeTgx16(imageData + 512, tgxSize,
                        buildingWidth, buildingHeight));
                }
                else
                {
                    // Tile-only кадр (TGX-части нет) — пустой «main»:
                    // композит соберётся из тайлов группы
                    images.push_back(makeEmptyImage());
                }
                break;
            }

            case DataType::BitMap:      // raw 16 бит, w x (h - 7)
                if (height > 7)
                {
                    images.push_back(decodeRaw16(imageData, size, width, height - 7));
                }
                else
                {
                    images.push_back(makeEmptyImage());
                }
                break;

            case DataType::BitMap1:     // raw 16 бит, w x h
                images.push_back(decodeRaw16(imageData, size, width, height));
                break;

            default:
                __blib_log_warning("gm1: unknown data type %u, frame %u skipped",
                    static_cast<uint32_t>(header.dataType), i);
                images.push_back(makeEmptyImage());
                break;
            }
        }

        return images;
    }

    const std::vector<GM1File::ImageHeader>& GM1File::getImageHeaders() const
    {
        return imageHeaders;
    }

    const std::vector<blib::graphics::Image>& GM1File::getImageTiles() const
    {
        return imageTiles;
    }

    const GM1File::Header& GM1File::getHeader() const
    {
        return header;
    }
}
