#pragma once

#include <blib/blibint.h>
#include <blib/graphics/image.h>
#include <blib/system/memory/allocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>

#include <string>
#include <vector>

namespace sc2img
{
    /**
     * Коды ошибок конвертера (AGENTS.md, «Обработка ошибок»:
     * None = 0 — всегда успех; enum наследуется от buint32).
     */
    enum class Sc2imgError : buint32
    {
        None = 0,
        InvalidArgs,   // пустой путь/папка вывода, неизвестное расширение
        OpenFailed,    // файл не существует или не читается
        ParseFailed,   // файл прочитан, но формат не распознан
        NoImages,      // файл распарсен, но изображений нет
        ExportFailed   // не удалось записать изображение на диск
    };

    /**
     * Формат выходного файла. Дефолт — PNG: единственный из трёх
     * одновременно с альфа-каналом и сжатием. JPEG прозрачность
     * НЕ хранит (альфа отбрасывается, см. imageExport.h).
     */
    enum class OutputFormat : buint8
    {
        Png = 0,
        Bmp,
        Jpeg
    };

    /**
     * Формат входного файла (определяется по расширению).
     */
    enum class SourceFormat : buint8
    {
        Unknown = 0,
        Tgx,    // одиночное изображение
        Gm1     // контейнер: палитра + множество кадров
    };

    /**
     * Одна картинка файла + метаданные заголовка GM1 (имена полей —
     * по референс-декомпилу Stronghold Image Toolbox). Для TGX все
     * поля метаданных — нули (заголовка изображения нет).
     *
     * Для зданий (DataType::Building) одна запись = ОДНО здание:
     * части группы собраны в композит «здание + земля» (см. .cpp),
     * метаданные — от первой части, offsets обнулены.
     */
    struct ImageEntry
    {
        blib::graphics::Image image;  // RGBA, row-major, y=0 — верх
        buint16 horizontalOffset;     // смещение кадра (выравнивание, align)
        buint16 verticalOffset;
        buint8 part;                  // номер части (многочастные здания)
        buint8 subparts;
        buint16 baseHeight;           // базовая высота здания в игре
        buint8 direction;             // направление (0-3)
        buint8 performanceId;         // id производительности (не цвет игрока)

        // Номер исходного кадра файла: для обычных записей — индекс
        // кадра, для композита здания — индекс ПЕРВОЙ части группы.
        // По нему строится имя файла экспорта (трассируемость к файлу)
        buint32 sourceIndex;

        // Смещения уже «запечены» в изображение (композит здания):
        // align по offset'ам к таким записям не применяется
        bool bakedOffsets;
    };

    /**
     * Опции экспорта — единые для GUI и headless-CLI.
     */
    struct ExportOptions
    {
        OutputFormat format;
        // Вписать кадр в общий bounding box набора по horizontalOffset/
        // verticalOffset (см. imageExport.h): кадры анимаций совмещаются
        // и не «дёргаются» при воспроизведении. false — кадр как есть
        bool alignByOffsets;
    };

    /**
     * Sc2imgDocument — содержимое одного файла .tgx/.gm1 в CPU-памяти.
     *
     * Чистые данные без рендера: переиспользуется GUI (вкладка
     * изображений, экспорт) и headless-режимом CLI. Работает поверх
     * существующих парсеров blib-graphics (parsers::TGXFile/GM1File —
     * старый код, не переписываем; граница в .cpp).
     */
    class Sc2imgDocument
    {
    private:
        // Аллокатор служебных контейнеров. Объявлен ПЕРЕД векторами:
        // они хранят указатель на него (канон из scene.h). Дефолт —
        // DefaultAllocator (прокси к GlobalAllocator)
        blib::memory::Allocator containerAllocator;

        std::vector<ImageEntry, blib::memory::StdAllocatorAdapter<ImageEntry>> entries;

        SourceFormat sourceFormat;
        std::string filePath;  // путь, из которого загружен документ
        std::string baseName;  // имя файла без каталога и расширения

    public:
        Sc2imgDocument();
        ~Sc2imgDocument();

        Sc2imgDocument(const Sc2imgDocument&) = delete;
        Sc2imgDocument& operator=(const Sc2imgDocument&) = delete;

        /**
         * Загрузить .tgx/.gm1. Формат определяется по расширению
         * (case-insensitive).
         *
         * @param usePlayerColorOverride true — применить playerColorOverride
         *        ко всем палитровым кадрам GM1 (UI-выбор цвета игрока);
         *        false — использовать color из заголовка каждого кадра
         * @param playerColorOverride индекс цветового набора игрока
         * @return код ошибки; None — успех
         */
        Sc2imgError load(
            _In const std::string& path,
            _In bool usePlayerColorOverride,
            _In buint8 playerColorOverride);

        /**
         * Выгрузить содержимое (пустой документ).
         */
        void clear();

        bool isEmpty() const;
        buint32 getImageCount() const;
        SourceFormat getSourceFormat() const;
        const std::string& getFilePath() const;
        const std::string& getBaseName() const;
        const ImageEntry& getEntry(_In buint32 index) const;
    };

    /**
     * Определить формат входного файла по расширению пути
     * (case-insensitive): ".tgx" → Tgx, ".gm1" → Gm1, иначе Unknown.
     */
    SourceFormat detectSourceFormat(_In const std::string& path);

    /**
     * Расширение выходного файла для формата (".png"/".bmp"/".jpg").
     */
    const char* outputExtension(_In OutputFormat format);
}
