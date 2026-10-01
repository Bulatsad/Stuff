#pragma once

#include <sc2img/core/sc2imgCore.h>

#include <blib/blibint.h>

#include <string>

namespace sc2img
{
    /**
     * Общий bounding box набора кадров в координатах заголовка GM1
     * (min — левый/верхний край самого крайнего кадра, max — правый/
     * нижний край самого дальнего). Используется режимом align:
     * каждый кадр кладётся на общий холст по своему horizontalOffset/
     * verticalOffset — кадры анимации совмещаются и не «прыгают».
     */
    struct AlignBox
    {
        buint32 minX;
        buint32 minY;
        buint32 maxX;
        buint32 maxY;
    };

    /**
     * Вычислить AlignBox по записям документа. Пустой документ —
     * нулевой box.
     */
    AlignBox computeAlignBox(_In const Sc2imgDocument& document);

    /**
     * Записать ОДНО изображение на диск в заданном формате.
     *
     * Режим align (options.alignByOffsets): кадр вписывается в общий
     * bounding box набора (см. computeAlignBox) — вокруг кадра
     * прозрачные поля. JPEG прозрачность не хранит: альфа отбрасывается
     * (см. .cpp, предупреждение в лог).
     *
     * @param entry изображение с метаданными
     * @param filePath полный путь выходного файла (с расширением)
     * @param options формат и флаг выравнивания
     * @return код ошибки; None — успех
     */
    Sc2imgError exportSingle(
        _In const ImageEntry& entry,
        _In const std::string& filePath,
        _In const ExportOptions& options,
        _In const AlignBox& alignBox);

    /**
     * Экспортировать изображения документа в папку outDir.
     *
     * Имена: TGX — "<baseName><ext>"; GM1 — "<baseName>_NNNN<ext>"
     * (NNNN — порядковый номер кадра, с ведущими нулями).
     *
     * @param document загруженный документ
     * @param outDir каталог вывода (должен существовать)
     * @param options формат и флаг выравнивания
     * @param selectedMask nullptr — экспортировать все; иначе массив
     *        из getImageCount() флагов: экспортируются только
     *        отмеченные (выбор изображений в GUI)
     * @param exportedCount [out] сколько файлов фактически записано
     * @return код ошибки; None — успех
     */
    Sc2imgError exportAll(
        _In const Sc2imgDocument& document,
        _In const std::string& outDir,
        _In const ExportOptions& options,
        _In_opt const buint8* selectedMask,
        _Out buint32& exportedCount);
}
