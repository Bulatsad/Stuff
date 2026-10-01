#include <sc2img/core/imageExport.h>

#include <blib/core/console/console.h>
#include <blib/system/memory/allocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>

#include <string>
#include <vector>

// stb_image_write (вендорный single-header, thirdparty/stb):
// реализация включается в этом единственном TU утилиты
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace sc2img
{
    namespace
    {
        // ---------------------------------------------------------------
        // Константы (правило проекта: без вшитых литералов)
        // ---------------------------------------------------------------

        // Качество JPEG-кодирования (1-100). Выше — лучше, но больше файл
        constexpr buint32 jpegQuality = 90;

        // Предельная сторона холста (Image::width/height — buint16);
        // при align сумма смещений теоретически может его превысить
        constexpr buint32 maxCanvasSide = 65535;

        // Минимальная ширина индекса кадра в имени файла (NNNN)
        constexpr buint32 indexFieldWidth = 4;

        // Разделитель каталогов Windows (пути утилиты — Windows-only)
        constexpr char pathSeparator = '\\';

        /**
         * Индекс кадра в имени файла с ведущими нулями: 7 → "0007".
         */
        std::string paddedIndex(_In buint32 index)
        {
            std::string digits = std::to_string(index);
            if (digits.size() < indexFieldWidth)
            {
                digits.insert(0, indexFieldWidth - digits.size(), '0');
            }
            return digits;
        }

        /**
         * Полный путь выходного файла: outDir + имя. outDir может быть
         * пустым (запись в текущий каталог) или с хвостовым слэшем.
         */
        std::string buildOutputPath(_In const std::string& outDir, _In const std::string& fileName)
        {
            if (outDir.empty())
            {
                return fileName;
            }

            std::string result = outDir;
            if (result.back() != pathSeparator && result.back() != '/')
            {
                result += pathSeparator;
            }
            result += fileName;
            return result;
        }

        /**
         * Имя выходного файла одной записи. TGX — одно изображение без
         * индекса; GM1 — с номером исходного кадра (sourceIndex: для
         * зданий — индекс первой части группы, см. sc2imgCore.h).
         */
        std::string buildFileName(
            _In const Sc2imgDocument& document,
            _In buint32 sourceIndex,
            _In OutputFormat format)
        {
            std::string name = document.getBaseName();
            if (document.getSourceFormat() == SourceFormat::Gm1)
            {
                name += '_';
                name += paddedIndex(sourceIndex);
            }
            name += outputExtension(format);
            return name;
        }

        /**
         * Плотный RGB-буфер для JPEG (stbi_write_jpg не принимает
         * stride): альфа отбрасывается. Собирается через blib-аллокатор
         * (правило проекта: std-контейнеры с StdAllocatorAdapter).
         */
        std::vector<buint8, blib::memory::StdAllocatorAdapter<buint8>> flattenRgb(
            _In const blib::graphics::Image& image)
        {
            // Аллокатор обязан жить дольше буфера (контракт
            // StdAllocatorAdapter — указатель без владения): локальная
            // переменная умирает после возврата буфера наружу
            blib::memory::Allocator rgbAllocator;
            std::vector<buint8, blib::memory::StdAllocatorAdapter<buint8>> rgb{
                blib::memory::StdAllocatorAdapter<buint8>(&rgbAllocator) };

            const buint32 pixelCount = static_cast<buint32>(image.width) * image.height;
            rgb.resize(pixelCount * 3);

            const blib::graphics::Color* source = static_cast<const blib::graphics::Color*>(image.getData());
            for (buint32 i = 0; i < pixelCount; ++i)
            {
                rgb[i * 3 + 0] = source[i].red;
                rgb[i * 3 + 1] = source[i].green;
                rgb[i * 3 + 2] = source[i].blue;
            }
            return rgb;
        }
    }

    AlignBox computeAlignBox(_In const Sc2imgDocument& document)
    {
        AlignBox box;
        box.minX = 0;
        box.minY = 0;
        box.maxX = 0;
        box.maxY = 0;

        const buint32 count = document.getImageCount();
        if (count == 0)
        {
            return box;
        }

        bool first = true;
        for (buint32 i = 0; i < count; ++i)
        {
            const ImageEntry& entry = document.getEntry(i);

            // Композиты зданий уже выровнены (смещения запечены) —
            // в общий align-box не входят
            if (entry.bakedOffsets)
            {
                continue;
            }

            const buint32 left = entry.horizontalOffset;
            const buint32 top = entry.verticalOffset;
            const buint32 right = static_cast<buint32>(entry.horizontalOffset) + entry.image.width;
            const buint32 bottom = static_cast<buint32>(entry.verticalOffset) + entry.image.height;

            if (first)
            {
                box.minX = left;
                box.minY = top;
                box.maxX = right;
                box.maxY = bottom;
                first = false;
            }
            else
            {
                if (left < box.minX) { box.minX = left; }
                if (top < box.minY) { box.minY = top; }
                if (right > box.maxX) { box.maxX = right; }
                if (bottom > box.maxY) { box.maxY = bottom; }
            }
        }
        return box;
    }

    Sc2imgError exportSingle(
        _In const ImageEntry& entry,
        _In const std::string& filePath,
        _In const ExportOptions& options,
        _In const AlignBox& alignBox)
    {
        if (__blib_unlikely(filePath.empty()))
        {
            __blib_return_error(Sc2imgError::InvalidArgs, "sc2img: empty output path");
        }

        // ---------------------------------------------------------------
        // Холст: либо сам кадр, либо общий bounding box набора (align).
        // Изображение вписывается через Image::update — прозрачные поля
        // остаются прозрачными
        // ---------------------------------------------------------------

        buint32 canvasWidth = entry.image.width;
        buint32 canvasHeight = entry.image.height;
        buint32 offsetX = 0;
        buint32 offsetY = 0;

        // Композит здания уже выровнен (смещения запечены) — align
        // к нему не применяется
        const bool useAlign = options.alignByOffsets && !entry.bakedOffsets;
        if (useAlign)
        {
            canvasWidth = alignBox.maxX - alignBox.minX;
            canvasHeight = alignBox.maxY - alignBox.minY;
            offsetX = entry.horizontalOffset - alignBox.minX;
            offsetY = entry.verticalOffset - alignBox.minY;
        }

        if (__blib_unlikely(canvasWidth == 0 || canvasHeight == 0 ||
            canvasWidth > maxCanvasSide || canvasHeight > maxCanvasSide))
        {
            __blib_return_error(Sc2imgError::ExportFailed,
                "sc2img: invalid canvas size %ux%u for %s", canvasWidth, canvasHeight, filePath.c_str());
        }

        blib::graphics::Image canvas;
        canvas.create(static_cast<buint16>(canvasWidth), static_cast<buint16>(canvasHeight),
            blib::graphics::Color::Transparent);
        canvas.update(static_cast<buint16>(offsetX), static_cast<buint16>(offsetY), entry.image);

        // ---------------------------------------------------------------
        // Запись: stb_image_write (C-граница вендорного кода)
        // ---------------------------------------------------------------

        const buint8* rgbaData = static_cast<const buint8*>(canvas.getData());
        const buint32 width = canvas.width;
        const buint32 height = canvas.height;
        const buint32 rgbaStride = width * blib::graphics::Color::bytesPerPixel();

        int writeResult = 0;
        switch (options.format)
        {
        case OutputFormat::Png:
            writeResult = stbi_write_png(filePath.c_str(),
                static_cast<int>(width), static_cast<int>(height), 4, rgbaData,
                static_cast<int>(rgbaStride));
            break;

        case OutputFormat::Bmp:
            // BMP 32 bpp (BI_BITFIELDS с альфа-маской) — прозрачность сохраняется
            writeResult = stbi_write_bmp(filePath.c_str(),
                static_cast<int>(width), static_cast<int>(height), 4, rgbaData);
            break;

        case OutputFormat::Jpeg:
        {
            // JPEG не хранит прозрачность: плотный RGB, альфа отбрасывается
            const std::vector<buint8, blib::memory::StdAllocatorAdapter<buint8>> rgb = flattenRgb(canvas);
            writeResult = stbi_write_jpg(filePath.c_str(),
                static_cast<int>(width), static_cast<int>(height), 3, rgb.data(),
                static_cast<int>(jpegQuality));
            break;
        }
        }

        if (__blib_unlikely(writeResult == 0))
        {
            __blib_return_error(Sc2imgError::ExportFailed,
                "sc2img: failed to write %s", filePath.c_str());
        }

        return Sc2imgError::None;
    }

    Sc2imgError exportAll(
        _In const Sc2imgDocument& document,
        _In const std::string& outDir,
        _In const ExportOptions& options,
        _In_opt const buint8* selectedMask,
        _Out buint32& exportedCount)
    {
        exportedCount = 0;

        if (__blib_unlikely(document.isEmpty()))
        {
            __blib_return_error(Sc2imgError::NoImages, "sc2img: document is empty, nothing to export");
        }

        // JPEG теряет прозрачность текстур — предупредить один раз
        // на операцию экспорта
        if (options.format == OutputFormat::Jpeg)
        {
            __blib_log_warning("sc2img: JPEG does not store transparency — alpha is discarded");
        }

        const AlignBox alignBox = computeAlignBox(document);
        const buint32 count = document.getImageCount();

        for (buint32 i = 0; i < count; ++i)
        {
            if (selectedMask != nullptr && selectedMask[i] == 0)
            {
                continue;
            }

            const ImageEntry& entry = document.getEntry(i);
            const std::string filePath = buildOutputPath(
                outDir, buildFileName(document, entry.sourceIndex, options.format));

            const Sc2imgError error = exportSingle(entry, filePath, options, alignBox);
            if (__blib_unlikely(error != Sc2imgError::None))
            {
                return error;
            }

            ++exportedCount;
        }

        __blib_log_info("sc2img: exported %u image(s) to %s", exportedCount,
            outDir.empty() ? "<current directory>" : outDir.c_str());

        return Sc2imgError::None;
    }
}
