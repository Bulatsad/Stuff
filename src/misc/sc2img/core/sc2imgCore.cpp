#include <sc2img/core/sc2imgCore.h>

#include <blib/core/console/console.h>
#include <blib/graphics/gm1.h>
#include <blib/graphics/tgx.h>

#include <Windows.h>

namespace sc2img
{
    namespace
    {
        // ---------------------------------------------------------------
        // Константы (правило проекта: без вшитых литералов)
        // ---------------------------------------------------------------

        constexpr const char* tgxExtension = ".tgx";
        constexpr const char* gm1Extension = ".gm1";

        constexpr const char* pngExtension = ".png";
        constexpr const char* bmpExtension = ".bmp";
        constexpr const char* jpgExtension = ".jpg";

        /**
         * Сравнить конец строки с суффиксом без учёта регистра
         * (ASCII). true — строка оканчивается суффиксом.
         */
        bool endsWithIgnoreCase(_In const std::string& text, _In const char* suffix)
        {
            const size_t suffixLength = std::char_traits<char>::length(suffix);
            if (text.size() < suffixLength)
            {
                return false;
            }

            const size_t offset = text.size() - suffixLength;
            for (size_t i = 0; i < suffixLength; ++i)
            {
                const char c = text[offset + i];
                const char lower = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
                if (lower != suffix[i])
                {
                    return false;
                }
            }
            return true;
        }

        /**
         * Имя файла без каталога и без расширения:
         * "C:\dir\castle.gm1" → "castle", "path/unit.tgx" → "unit".
         */
        std::string extractBaseName(_In const std::string& path)
        {
            const size_t lastSlash = path.find_last_of("\\/");
            const size_t nameBegin = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;

            const size_t lastDot = path.find_last_of('.');
            const size_t nameEnd = (lastDot == std::string::npos || lastDot < nameBegin)
                ? path.size()
                : lastDot;

            return path.substr(nameBegin, nameEnd - nameBegin);
        }
    }

    namespace
    {
        // ---------------------------------------------------------------
        // Композиция зданий (DataType::Building) — по референс-декомпилу
        // Stronghold Image Toolbox (см. GRAPHICS.md/SC2IMG.md): части
        // группы рисуются по своим offsets, затем ПОВЕРХ них кладутся
        // тайлы-ромбы (земля), холст обрезается по bounding box
        // ---------------------------------------------------------------

        // Направление Left в GM1ImageHeader.Image_Direction
        constexpr buint8 gm1DirectionLeft = 3;

        // Сдвиг тайла влево для направления Left (как в Toolbox)
        constexpr buint32 tileLeftShift = 14;

        // Ограничение стороны холста композита (buint16 у Image)
        constexpr buint32 maxCompositeSide = 4096;

        // Прозрачный пиксель изображения-источника
        bool isTransparent(_In const blib::graphics::Color& color)
        {
            return color.alpha == 0;
        }

        // X-начало тайла кадра: hOff + hStart со сдвигом влево для Left
        // (сатурация нулём — mainX может быть меньше сдвига)
        buint32 tileOriginX(_In const parsers::GM1File::ImageHeader& header, _In buint32 mainX)
        {
            if (header.direction == gm1DirectionLeft)
            {
                return (mainX > tileLeftShift) ? (mainX - tileLeftShift) : 0;
            }
            return mainX;
        }

        // Пустая картинка-заглушка (невалидный/слишком большой композит)
        blib::graphics::Image makeEmptyImage()
        {
            blib::graphics::Image image;
            image.create(1, 1, blib::graphics::Color::Transparent);
            return image;
        }

        /**
         * Собрать группу кадров [begin, end) в один композит «здание +
         * земля». Главные изображения кладутся первыми, тайлы — поверх
         * (порядок как в Toolbox); прозрачные пиксели не пишутся.
         */
        blib::graphics::Image composeBuildingGroup(
            _In const std::vector<blib::graphics::Image>& images,
            _In const std::vector<parsers::GM1File::ImageHeader>& headers,
            _In const std::vector<blib::graphics::Image>& tiles,
            _In size_t begin,
            _In size_t end)
        {
            // -----------------------------------------------------------
            // Bounding box группы: части + тайлы (тайлы учитываем, чтобы
            // лево-сдвинутые (direction == Left) не клипались — в Toolbox
            // обрезка только по частям, это его дефект)
            // -----------------------------------------------------------
            bool hasBounds = false;
            buint32 minX = 0;
            buint32 minY = 0;
            buint32 maxX = 0;
            buint32 maxY = 0;

            for (size_t i = begin; i < end; ++i)
            {
                const parsers::GM1File::ImageHeader& header = headers[i];
                const blib::graphics::Image& image = images[i];
                const blib::graphics::Image& tile = tiles[i];

                const buint32 mainX = static_cast<buint32>(header.horizontalOffset) +
                    static_cast<buint32>(header.horizontalStartOffset);

                const struct
                {
                    buint32 x;
                    buint32 y;
                    buint32 width;
                    buint32 height;
                } boxes[2] =
                {
                    { mainX, static_cast<buint32>(header.verticalOffset),
                      image.width, image.height },
                    { tileOriginX(header, mainX),
                      static_cast<buint32>(header.verticalOffset) + header.baseHeight,
                      tile.width, tile.height }
                };

                for (const auto& box : boxes)
                {
                    if (box.width == 0 || box.height == 0)
                    {
                        continue;
                    }

                    const buint32 boxMaxX = box.x + box.width;
                    const buint32 boxMaxY = box.y + box.height;

                    if (!hasBounds)
                    {
                        minX = box.x;
                        minY = box.y;
                        maxX = boxMaxX;
                        maxY = boxMaxY;
                        hasBounds = true;
                    }
                    else
                    {
                        if (box.x < minX) { minX = box.x; }
                        if (box.y < minY) { minY = box.y; }
                        if (boxMaxX > maxX) { maxX = boxMaxX; }
                        if (boxMaxY > maxY) { maxY = boxMaxY; }
                    }
                }
            }

            if (!hasBounds)
            {
                return makeEmptyImage();
            }

            const buint32 canvasWidth = maxX - minX;
            const buint32 canvasHeight = maxY - minY;
            if (canvasWidth == 0 || canvasHeight == 0 ||
                canvasWidth > maxCompositeSide || canvasHeight > maxCompositeSide)
            {
                __blib_log_warning("sc2img: building composite too large (%ux%u), skipped",
                    canvasWidth, canvasHeight);
                return makeEmptyImage();
            }

            blib::graphics::Image canvas;
            canvas.create(static_cast<buint16>(canvasWidth), static_cast<buint16>(canvasHeight),
                blib::graphics::Color::Transparent);

            // -----------------------------------------------------------
            // Части (первые), затем тайлы-земля (поверх) — порядок Toolbox
            // -----------------------------------------------------------
            for (size_t i = begin; i < end; ++i)
            {
                const blib::graphics::Image& image = images[i];
                if (image.width == 0 || image.height == 0)
                {
                    continue;
                }

                const parsers::GM1File::ImageHeader& header = headers[i];
                const buint32 originX = static_cast<buint32>(header.horizontalOffset) +
                    static_cast<buint32>(header.horizontalStartOffset) - minX;
                const buint32 originY = static_cast<buint32>(header.verticalOffset) - minY;

                for (buint16 y = 0; y < image.height; ++y)
                {
                    for (buint16 x = 0; x < image.width; ++x)
                    {
                        const blib::graphics::Color& color = image[x][y];
                        if (!isTransparent(color))
                        {
                            canvas[static_cast<buint16>(originX + x)]
                                  [static_cast<buint16>(originY + y)] = color;
                        }
                    }
                }
            }

            for (size_t i = begin; i < end; ++i)
            {
                const blib::graphics::Image& tile = tiles[i];
                if (tile.width == 0 || tile.height == 0)
                {
                    continue;
                }

                const parsers::GM1File::ImageHeader& header = headers[i];
                const buint32 mainX = static_cast<buint32>(header.horizontalOffset) +
                    static_cast<buint32>(header.horizontalStartOffset);
                const buint32 originX = tileOriginX(header, mainX) - minX;
                const buint32 originY = static_cast<buint32>(header.verticalOffset) +
                    header.baseHeight - minY;

                for (buint16 y = 0; y < tile.height; ++y)
                {
                    for (buint16 x = 0; x < tile.width; ++x)
                    {
                        const blib::graphics::Color& color = tile[x][y];
                        if (!isTransparent(color) &&
                            static_cast<buint32>(originX + x) < canvasWidth &&
                            static_cast<buint32>(originY + y) < canvasHeight)
                        {
                            canvas[static_cast<buint16>(originX + x)]
                                  [static_cast<buint16>(originY + y)] = color;
                        }
                    }
                }
            }

            return canvas;
        }
    }

    Sc2imgDocument::Sc2imgDocument()
        : containerAllocator()
        , entries{ blib::memory::StdAllocatorAdapter<ImageEntry>(&containerAllocator) }
        , sourceFormat(SourceFormat::Unknown)
        , filePath()
        , baseName()
    {
    }

    Sc2imgDocument::~Sc2imgDocument()
    {
    }

    Sc2imgError Sc2imgDocument::load(
        _In const std::string& path,
        _In bool usePlayerColorOverride,
        _In buint8 playerColorOverride)
    {
        // Предыдущее содержимое выгружаем ДО парсинга нового файла
        this->clear();

        if (__blib_unlikely(path.empty()))
        {
            __blib_return_error(Sc2imgError::InvalidArgs, "sc2img: empty input path");
        }

        const SourceFormat detectedFormat = detectSourceFormat(path);
        if (__blib_unlikely(detectedFormat == SourceFormat::Unknown))
        {
            __blib_return_error(Sc2imgError::InvalidArgs,
                "sc2img: unsupported input file: %s (expected .tgx or .gm1)", path.c_str());
        }

        // Файл существует? (парсеры не различают «не найден» и «битый»;
        // Win32 — граница системного вызова, конвертация на границе)
        if (__blib_unlikely(GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES))
        {
            __blib_return_error(Sc2imgError::OpenFailed, "sc2img: file not found: %s", path.c_str());
        }

        // ---------------------------------------------------------------
        // Парсинг существующими парсерами blib-graphics (старый код в
        // namespace parsers — не переписываем; это граница утилиты)
        // ---------------------------------------------------------------

        std::vector<blib::graphics::Image> images;
        std::vector<parsers::GM1File::ImageHeader> gm1Headers;
        std::vector<blib::graphics::Image> gm1Tiles;
        bool isGm1Building = false;

        if (detectedFormat == SourceFormat::Tgx)
        {
            parsers::TGXFile tgxParser;
            images = tgxParser.readFile(path);
        }
        else
        {
            parsers::GM1File gm1Parser;
            gm1Parser.playerColorOverridden = usePlayerColorOverride;
            gm1Parser.playerColorOverride = playerColorOverride;

            images = gm1Parser.loadFromFile(path);

            // Копии в std-векторы: парсер разрушится при выходе, а
            // документу нужны метаданные и тайлы кадров
            gm1Headers = gm1Parser.getImageHeaders();
            gm1Tiles = gm1Parser.getImageTiles();

            isGm1Building =
                gm1Parser.getHeader().dataType == parsers::GM1File::DataType::Building &&
                gm1Headers.size() == images.size() &&
                gm1Tiles.size() == images.size();
        }

        if (__blib_unlikely(images.empty()))
        {
            __blib_return_error(Sc2imgError::ParseFailed,
                "sc2img: no images parsed from %s (broken or unsupported file?)", path.c_str());
        }

        // ---------------------------------------------------------------
        // Заполнение записей: для зданий — композит на группу частей
        // (одно здание = одна запись, как в Stronghold Image Toolbox),
        // для остальных типов — кадр = запись
        // ---------------------------------------------------------------

        if (isGm1Building)
        {
            // Группа — кадры от part == 0 до следующего part == 0
            // (в корректном файле ровно subparts частей)
            size_t i = 0;
            while (i < images.size())
            {
                size_t groupEnd = i + 1;
                while (groupEnd < images.size() && gm1Headers[groupEnd].part != 0)
                {
                    ++groupEnd;
                }

                const parsers::GM1File::ImageHeader& header = gm1Headers[i];

                ImageEntry entry;
                entry.image = composeBuildingGroup(images, gm1Headers, gm1Tiles, i, groupEnd);
                entry.horizontalOffset = 0;  // смещения запечены в композит
                entry.verticalOffset = 0;
                entry.part = 0;
                entry.subparts = header.subparts;
                entry.baseHeight = header.baseHeight;
                entry.direction = header.direction;
                entry.performanceId = header.performanceId;
                entry.sourceIndex = static_cast<buint32>(i);
                entry.bakedOffsets = true;

                this->entries.push_back(std::move(entry));
                i = groupEnd;
            }
        }
        else
        {
            this->entries.reserve(images.size());
            for (size_t i = 0; i < images.size(); ++i)
            {
                ImageEntry entry;
                entry.image = std::move(images[i]);
                entry.horizontalOffset = 0;
                entry.verticalOffset = 0;
                entry.part = 0;
                entry.subparts = 0;
                entry.baseHeight = 0;
                entry.direction = 0;
                entry.performanceId = 0;
                entry.sourceIndex = static_cast<buint32>(i);
                entry.bakedOffsets = false;

                if (i < gm1Headers.size())
                {
                    entry.horizontalOffset = gm1Headers[i].horizontalOffset;
                    entry.verticalOffset = gm1Headers[i].verticalOffset;
                    entry.part = gm1Headers[i].part;
                    entry.subparts = gm1Headers[i].subparts;
                    entry.baseHeight = gm1Headers[i].baseHeight;
                    entry.direction = gm1Headers[i].direction;
                    entry.performanceId = gm1Headers[i].performanceId;
                }

                this->entries.push_back(std::move(entry));
            }
        }

        this->sourceFormat = detectedFormat;
        this->filePath = path;
        this->baseName = extractBaseName(path);

        __blib_log_info("sc2img: loaded %s: %zu image(s)%s",
            path.c_str(), this->entries.size(),
            (detectedFormat == SourceFormat::Gm1 && usePlayerColorOverride)
                ? " (player color overridden)" : "");

        return Sc2imgError::None;
    }

    void Sc2imgDocument::clear()
    {
        this->entries.clear();
        this->sourceFormat = SourceFormat::Unknown;
        this->filePath.clear();
        this->baseName.clear();
    }

    bool Sc2imgDocument::isEmpty() const
    {
        return this->entries.empty();
    }

    buint32 Sc2imgDocument::getImageCount() const
    {
        return static_cast<buint32>(this->entries.size());
    }

    SourceFormat Sc2imgDocument::getSourceFormat() const
    {
        return this->sourceFormat;
    }

    const std::string& Sc2imgDocument::getFilePath() const
    {
        return this->filePath;
    }

    const std::string& Sc2imgDocument::getBaseName() const
    {
        return this->baseName;
    }

    const ImageEntry& Sc2imgDocument::getEntry(_In buint32 index) const
    {
        return this->entries[index];
    }

    SourceFormat detectSourceFormat(_In const std::string& path)
    {
        if (endsWithIgnoreCase(path, tgxExtension))
        {
            return SourceFormat::Tgx;
        }
        if (endsWithIgnoreCase(path, gm1Extension))
        {
            return SourceFormat::Gm1;
        }
        return SourceFormat::Unknown;
    }

    const char* outputExtension(_In OutputFormat format)
    {
        switch (format)
        {
        case OutputFormat::Bmp:
            return bmpExtension;
        case OutputFormat::Jpeg:
            return jpgExtension;
        case OutputFormat::Png:
        default:
            return pngExtension;
        }
    }
}
