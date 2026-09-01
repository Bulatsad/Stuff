#pragma once

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/core/algorithm/compression/icompressor.h>

namespace blib
{
    namespace algorithm
    {
        namespace compression
        {
            /**
             * HuffmanCompressor - реализация ICompressor на основе алгоритма Хаффмана.
             *
             * Алгоритм:
             * - Энтропийное кодирование: часто встречающимся символам назначаются
             *   короткие битовые коды, редким -- длинные
             * - Оптимальный префиксный код (ни один код не является префиксом другого)
             * - Два прохода при сжатии: подсчёт частот, затем кодирование
             *
             * Бинарный формат сжатого потока:
             *   [Magic: 4 байта "BHUF"]
             *   [Version: 1 байт]
             *   [Block count: buint32]
             *   Для каждого Huffman-блока (symbolCount > 0):
             *     [Original block size: buint64]     -- размер оригинальных данных блока
             *     [Compressed block size: buint64]   -- размер сжатых данных (для seek)
             *     [Symbol count: buint16]            -- количество уникальных символов (1-256)
             *     Для каждого символа:
             *       [Byte value: buint8]
             *       [Frequency: buint32]
             *     [Padding bits: buint8]             -- неиспользуемых бит в последнем байте (0-7)
             *     [Encoded data: переменная длина]
             *   Stored-блок (level == noCompression, symbolCount == 0):
             *     [Original block size: buint64]
             *     [Compressed block size: buint64]   -- == 2 + original size
             *     [Symbol count: buint16]            -- 0
             *     [Raw data: original size байт]     -- без сжатия, без padding
             *
             * Настройки из CompressionSettings:
             * - blockSize: если > 0, данные разбиваются на блоки, каждый со своим
             *   деревом Хаффмана (улучшает сжатие на неоднородных данных).
             *   Если == 0, весь поток обрабатывается как один блок.
             * - level: при noCompression данные копируются без сжатия
             *   (stored-блок), остальные уровни сжимаются одинаково
             * - memoryPoolForCompression: ВСЕ аллокации проходят через него
             * - dictionarySize, wordSize, threadCount: игнорируются (не применимы)
             *
             * Ограничения:
             * - Входной поток должен поддерживать seek (canSeek() == true)
             *   при сжатии (позиционирование по блокам); при size() == 0
             *   (размер неизвестен) вход читается до EOF
             * - Выходной поток должен поддерживать seek (canSeek() == true):
             *   compressedSize/paddingBits патчатся в конце через seek
             * - При несоблюдении требований к потокам compress возвращает false
             * - Максимальная длина кода: 255 бит (достаточно для 256 символов)
             * - Не thread-safe
             */
            class __blib_core_api HuffmanCompressor : public ICompressor
            {
            public:
                // ---- ICompressor ----

                const char* algorithmName() const __blib_override;
                buint64 compressBound(buint64 inputSize) const __blib_override;
                bool validateSettings(_In const CompressionSettings& settings) const __blib_override;

                bool compress(
                    _In  blib::core::IInputStream&  decompressedInputStream,
                    _Out blib::core::IOutputStream& compressedOutputStream) __blib_override;

                bool decompress(
                    _In  blib::core::IInputStream&  compressedInputStream,
                    _Out blib::core::IOutputStream& decompressedOutputStream) __blib_override;

            private:
                // Количество возможных байтовых значений (алфавит)
                static const buint32 alphabetSize = 256;

                // Magic bytes для идентификации формата ("BHUF")
                static const buint8 magic[4];

                // Текущая версия формата
                static const buint8 formatVersion = 1;

                /**
                 * HuffmanNode - узел дерева Хаффмана.
                 * Лист содержит символ (байт) и его частоту.
                 * Внутренний узел содержит суммарную частоту поддеревьев.
                 */
                struct HuffmanNode
                {
                    buint64 frequency;     // частота символа (или сумма для внутренних узлов)
                    buint16 left;          // индекс левого потомка в массиве узлов (0xFFFF = нет)
                    buint16 right;         // индекс правого потомка в массиве узлов (0xFFFF = нет)
                    buint8  symbol;        // значение байта (значимо только для листьев)
                    buint8  isLeaf;        // 1 -- лист, 0 -- внутренний узел
                };

                /**
                 * CodeEntry - битовый код символа для таблицы кодирования.
                 * Максимальная длина кода для 256 символов -- 255 бит,
                 * но на практике не превышает ~30 бит для реальных данных.
                 * Храним до 32 байт (256 бит) -- покрывает любой случай.
                 */
                struct CodeEntry
                {
                    buint8 bits[32];       // битовый код (LSB первый байт, младший бит первый)
                    buint8 length;         // длина кода в битах (0 = символ не встречается)
                };

                // ---- Внутренние вспомогательные методы ----

                /**
                 * Подсчёт частот каждого байта в блоке данных.
                 *
                 * @param data      Указатель на данные блока
                 * @param dataSize  Размер данных в байтах
                 * @param freq      Выходной массив частот (256 элементов, обнуляется внутри)
                 */
                static void countFrequencies(
                    _In  const buint8* data,
                         buint64 dataSize,
                    _Out buint64 freq[alphabetSize]);

                /**
                 * Построение дерева Хаффмана из массива частот.
                 * Использует min-heap для выбора двух узлов с наименьшей частотой.
                 * Все узлы размещаются в заранее выделенном массиве nodes.
                 *
                 * @param freq       Массив частот (256 элементов)
                 * @param nodes      Массив для хранения узлов (минимум 511 элементов: 256 листьев + 255 внутренних)
                 * @param nodeCount  [out] Количество использованных узлов
                 * @return Индекс корневого узла в массиве nodes, или 0xFFFF при ошибке
                 */
                static buint16 buildTree(
                    _In  const buint64 freq[alphabetSize],
                    _Out HuffmanNode* nodes,
                    _Out buint32& nodeCount);

                /**
                 * Генерация таблицы битовых кодов обходом дерева.
                 *
                 * @param nodes      Массив узлов дерева
                 * @param rootIndex  Индекс корневого узла
                 * @param table      Выходная таблица кодов (256 элементов, обнуляется внутри)
                 */
                static void buildCodeTable(
                    _In  const HuffmanNode* nodes,
                         buint16 rootIndex,
                    _Out CodeEntry table[alphabetSize]);

                /**
                 * Рекурсивный обход дерева для генерации кодов.
                 *
                 * @param nodes      Массив узлов дерева
                 * @param nodeIndex  Текущий узел
                 * @param code       Текущий накапливаемый код
                 * @param depth      Текущая глубина (длина кода в битах)
                 * @param table      Выходная таблица кодов
                 */
                static void buildCodeTableRecursive(
                    _In  const HuffmanNode* nodes,
                         buint16 nodeIndex,
                         buint8 code[32],
                         buint8 depth,
                    _Out CodeEntry table[alphabetSize]);

                /**
                 * BitWriter -- побитовая буферизованная запись в выходной поток.
                 *
                 * Накапливает биты в однобайтовом буфере и при заполнении
                 * сбрасывает в промежуточный буфер вывода. Буфер сбрасывается
                 * в поток при заполнении или при вызове finish().
                 */
                struct BitWriter
                {
                    blib::core::IOutputStream* stream; // выходной поток
                    buint8* buffer;                    // буфер вывода (аллоцирован вызывающим)
                    buint32 bufferCapacity;            // размер буфера в байтах
                    buint32 bufferPos;                 // текущая позиция записи в буфере
                    buint8  currentByte;               // текущий накапливаемый байт
                    buint8  bitPos;                    // сколько бит записано в currentByte (0..7)

                    /** Записать один бит (0 или 1). */
                    bool writeBit(buint8 bit);

                    /** Записать битовый код из CodeEntry. */
                    bool writeCode(const CodeEntry& entry);

                    /**
                     * Завершить запись: сбросить неполный байт и буфер в поток.
                     * @return Количество неиспользуемых бит в последнем байте (0..7)
                     *         или 0xFF при ошибке записи
                     */
                    buint8 finish();

                private:
                    /** Поместить байт в буфер, при заполнении -- сбросить в поток. */
                    bool flushByte(buint8 byte);
                };

                /**
                 * Сжатие одного блока данных.
                 *
                 * @param data       Исходные данные блока
                 * @param dataSize   Размер данных
                 * @param out        Выходной поток
                 * @param allocator  Аллокатор для временных буферов
                 * @return true при успехе
                 */
                static bool compressBlock(
                    _In  const buint8* data,
                         buint64 dataSize,
                    _Out blib::core::IOutputStream& out,
                         blib::memory::Allocator& allocator);

                /**
                 * Запись stored-блока (режим noCompression): данные копируются
                 * без сжатия, symbolCount == 0. Формат см. в описании класса.
                 *
                 * @param data      Исходные данные
                 * @param dataSize  Размер данных
                 * @param out       Выходной поток
                 * @return true при успехе
                 */
                static bool compressStoredBlock(
                    _In  const buint8* data,
                         buint64 dataSize,
                    _Out blib::core::IOutputStream& out);

                /**
                 * Декомпрессия одного блока данных.
                 *
                 * @param in         Входной поток (позиционирован на начало блока)
                 * @param out        Выходной поток
                 * @param allocator  Аллокатор для временных буферов
                 * @return true при успехе
                 */
                static bool decompressBlock(
                    _In  blib::core::IInputStream& in,
                    _Out blib::core::IOutputStream& out,
                         blib::memory::Allocator& allocator);
            };
        }
    }
}
