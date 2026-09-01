#include <blib/core/algorithm/compression/huffmanCompressor.h>

#include <cstring>

namespace blib
{
    namespace algorithm
    {
        namespace compression
        {
            // ================================================================
            // Константы
            // ================================================================

            // Magic bytes "BHUF" -- идентификация формата Huffman-сжатия blib
            const buint8 HuffmanCompressor::magic[4] = { 'B', 'H', 'U', 'F' };

            // Sentinel-значение: "нет потомка" в дереве
            static const buint16 nullNode = 0xFFFF;

            // ================================================================
            // Хелперы потокового ввода/вывода
            // ================================================================

            namespace
            {
                /**
                 * readAll -- прочитать ровно size байт из потока, обрабатывая
                 * частичные чтения (контракт IInputStream::read допускает их:
                 * "читает ДО size байт, возвращает фактически прочитанное").
                 *
                 * @param in    Входной поток
                 * @param buffer Приёмник (не nullptr, если size > 0)
                 * @param size  Количество байт
                 * @return true если прочитано ровно size байт; false при EOF
                 *         раньше времени
                 */
                bool readAll(core::IInputStream& in, _Out void* buffer, size_t size)
                {
                    buint8* dst = static_cast<buint8*>(buffer);
                    size_t total = 0;
                    while (total < size)
                    {
                        size_t n = in.read(dst + total, size - total);
                        if (n == 0)
                            return false; // EOF раньше времени
                        total += n;
                    }
                    return true;
                }

                /**
                 * writeAll -- записать все size байт, обрабатывая частичные
                 * записи (контракт IOutputStream::write допускает их:
                 * "пишет ДО size байт, возвращает фактически записанное").
                 *
                 * @param out  Выходной поток
                 * @param data Источник данных (не nullptr, если size > 0)
                 * @param size Количество байт
                 * @return true если записаны все байты; false при отказе
                 *         приёмника
                 */
                bool writeAll(core::IOutputStream& out, _In const void* data, size_t size)
                {
                    const buint8* src = static_cast<const buint8*>(data);
                    size_t total = 0;
                    while (total < size)
                    {
                        size_t n = out.write(src + total, size - total);
                        if (n == 0)
                            return false; // приёмник отказал
                        total += n;
                    }
                    return true;
                }
            }

            // ================================================================
            // ICompressor: информация об алгоритме
            // ================================================================

            const char* HuffmanCompressor::algorithmName() const
            {
                return "Huffman";
            }

            buint64 HuffmanCompressor::compressBound(buint64 inputSize) const
            {
                // Worst-case для Хаффмана: все 256 символов имеют равную частоту,
                // коды длиной 8 бит -- сжатый размер == оригинал.
                // Заголовок: magic(4) + version(1) + blockCount(4) = 9 байт
                // На каждый блок: origSize(8) + compSize(8) + symbolCount(2) +
                //   таблица частот (макс 256 * (1 + 4) = 1280) + paddingBits(1) +
                //   данные (== inputSize в worst-case)
                // Для одного блока: 9 + 8 + 8 + 2 + 1280 + 1 + inputSize
                // Упрощаем: inputSize + 1308

                // При blockSize > 0 -- несколько блоков, каждый со своим заголовком
                buint64 blockSize = this->settings.blockSize;
                if (blockSize == 0)
                {
                    // Один блок
                    return inputSize + 1308;
                }

                // Количество блоков (округление вверх)
                buint64 blockCount = (inputSize + blockSize - 1) / blockSize;
                if (blockCount == 0)
                    blockCount = 1;

                // Заголовок файла (9) + каждый блок (overhead 1299 + данные блока)
                return 9 + blockCount * 1299 + inputSize;
            }

            bool HuffmanCompressor::validateSettings(_In const CompressionSettings& s) const
            {
                // Huffman не использует dictionarySize и wordSize -- игнорируем их.
                // threadCount тоже игнорируется (однопоточная реализация).
                // blockSize: любое значение допустимо (0 = один блок).
                // level: все уровни допустимы.
                // Единственное ограничение: аллокатор должен быть рабочим
                // (но это невозможно проверить без аллокации).

                // Все настройки допустимы для Huffman
                return true;
            }

            // ================================================================
            // Подсчёт частот
            // ================================================================

            void HuffmanCompressor::countFrequencies(
                _In  const buint8* data,
                     buint64 dataSize,
                _Out buint64 freq[alphabetSize])
            {
                // Обнуляем таблицу частот
                std::memset(freq, 0, alphabetSize * sizeof(buint64));

                // Подсчёт вхождений каждого байта
                for (buint64 i = 0; i < dataSize; ++i)
                {
                    freq[data[i]]++;
                }
            }

            // ================================================================
            // Построение дерева Хаффмана
            // ================================================================

            buint16 HuffmanCompressor::buildTree(
                _In  const buint64 freq[alphabetSize],
                _Out HuffmanNode* nodes,
                _Out buint32& nodeCount)
            {
                nodeCount = 0;

                // Создаём листовые узлы для каждого символа с ненулевой частотой.
                // heap хранит индексы узлов в массиве nodes.
                // heapSize -- текущий размер кучи.
                buint16 heap[alphabetSize];
                buint32 heapSize = 0;

                for (buint32 i = 0; i < alphabetSize; ++i)
                {
                    if (freq[i] > 0)
                    {
                        HuffmanNode& node = nodes[nodeCount];
                        node.frequency = freq[i];
                        node.left      = nullNode;
                        node.right     = nullNode;
                        node.symbol    = static_cast<buint8>(i);
                        node.isLeaf    = 1;

                        heap[heapSize] = static_cast<buint16>(nodeCount);
                        heapSize++;
                        nodeCount++;
                    }
                }

                // Особые случаи
                if (heapSize == 0)
                {
                    // Пустые данные -- нет дерева
                    return nullNode;
                }

                if (heapSize == 1)
                {
                    // Единственный символ: создаём фиктивный корень,
                    // у которого единственный лист -- левый потомок.
                    // Код символа будет "0" (один бит).
                    HuffmanNode& root = nodes[nodeCount];
                    root.frequency = nodes[heap[0]].frequency;
                    root.left      = heap[0];
                    root.right     = nullNode;
                    root.symbol    = 0;
                    root.isLeaf    = 0;

                    buint16 rootIndex = static_cast<buint16>(nodeCount);
                    nodeCount++;
                    return rootIndex;
                }

                // ---- Min-heap (пирамида) для выбора двух минимальных узлов ----

                // Построение начальной кучи (heapify, O(n))
                // Сравнение по частоте: меньшая частота -- выше приоритет.
                // siftDown перемещает элемент вниз по куче до восстановления свойства.
                // Лямбды недоступны для static-метода -- используем макро-подобный подход
                // с inline-кодом.

                // siftDown: восстановить свойство кучи для элемента heap[pos]
                // nodes[heap[pos]].frequency должна быть <= потомков
                #define HUFFMAN_SIFT_DOWN(pos, sz)                                         \
                    do {                                                                    \
                        buint32 _p = (pos);                                                \
                        while (true)                                                        \
                        {                                                                   \
                            buint32 _left  = 2 * _p + 1;                                   \
                            buint32 _right = 2 * _p + 2;                                   \
                            buint32 _min   = _p;                                            \
                            if (_left < (sz) &&                                             \
                                nodes[heap[_left]].frequency < nodes[heap[_min]].frequency) \
                                _min = _left;                                               \
                            if (_right < (sz) &&                                            \
                                nodes[heap[_right]].frequency < nodes[heap[_min]].frequency)\
                                _min = _right;                                              \
                            if (_min == _p) break;                                          \
                            /* swap */                                                      \
                            buint16 _tmp = heap[_p];                                        \
                            heap[_p]     = heap[_min];                                      \
                            heap[_min]   = _tmp;                                            \
                            _p = _min;                                                      \
                        }                                                                   \
                    } while (false)

                // Построение кучи снизу вверх
                for (bint32 i = static_cast<bint32>(heapSize / 2) - 1; i >= 0; --i)
                {
                    HUFFMAN_SIFT_DOWN(static_cast<buint32>(i), heapSize);
                }

                // ---- Построение дерева: объединяем два минимальных узла ----

                while (heapSize > 1)
                {
                    // Извлекаем минимальный элемент (корень кучи)
                    buint16 minA = heap[0];
                    heap[0] = heap[heapSize - 1];
                    heapSize--;
                    HUFFMAN_SIFT_DOWN(0, heapSize);

                    // Извлекаем следующий минимальный элемент
                    buint16 minB = heap[0];
                    heap[0] = heap[heapSize - 1];
                    heapSize--;
                    HUFFMAN_SIFT_DOWN(0, heapSize);

                    // Создаём внутренний узел -- родитель двух минимальных
                    HuffmanNode& parent = nodes[nodeCount];
                    parent.frequency = nodes[minA].frequency + nodes[minB].frequency;
                    parent.left      = minA;
                    parent.right     = minB;
                    parent.symbol    = 0;
                    parent.isLeaf    = 0;

                    // Добавляем родителя обратно в кучу
                    heap[heapSize] = static_cast<buint16>(nodeCount);
                    heapSize++;
                    nodeCount++;

                    // siftUp: новый элемент может быть меньше родителя
                    buint32 child = heapSize - 1;
                    while (child > 0)
                    {
                        buint32 parentIdx = (child - 1) / 2;
                        if (nodes[heap[child]].frequency < nodes[heap[parentIdx]].frequency)
                        {
                            buint16 tmp     = heap[child];
                            heap[child]     = heap[parentIdx];
                            heap[parentIdx] = tmp;
                            child = parentIdx;
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                #undef HUFFMAN_SIFT_DOWN

                // Корень дерева -- единственный оставшийся элемент кучи
                return heap[0];
            }

            // ================================================================
            // Генерация таблицы кодов
            // ================================================================

            void HuffmanCompressor::buildCodeTable(
                _In  const HuffmanNode* nodes,
                     buint16 rootIndex,
                _Out CodeEntry table[alphabetSize])
            {
                // Обнуляем таблицу
                std::memset(table, 0, alphabetSize * sizeof(CodeEntry));

                if (rootIndex == nullNode)
                    return;

                // Буфер для накопления битового кода при обходе дерева
                buint8 code[32];
                std::memset(code, 0, sizeof(code));

                buildCodeTableRecursive(nodes, rootIndex, code, 0, table);
            }

            void HuffmanCompressor::buildCodeTableRecursive(
                _In  const HuffmanNode* nodes,
                     buint16 nodeIndex,
                     buint8 code[32],
                     buint8 depth,
                _Out CodeEntry table[alphabetSize])
            {
                const HuffmanNode& node = nodes[nodeIndex];

                if (node.isLeaf)
                {
                    // Лист -- записываем код символа
                    CodeEntry& entry = table[node.symbol];
                    entry.length = depth;

                    // Особый случай: единственный символ (depth == 0).
                    // Назначаем код "0" длиной 1 бит.
                    if (depth == 0)
                    {
                        entry.length = 1;
                        entry.bits[0] = 0;
                    }
                    else
                    {
                        std::memcpy(entry.bits, code, (depth + 7) / 8);
                    }
                    return;
                }

                // Левый потомок: добавляем бит 0 (бит уже 0 после memset)
                if (node.left != nullNode)
                {
                    // Бит depth остаётся 0 (code[depth/8] & (1 << (depth%8)) == 0)
                    // Убедимся что бит сброшен (может быть остаток от правой ветки)
                    code[depth / 8] &= ~(1 << (depth % 8));
                    buildCodeTableRecursive(nodes, node.left, code, depth + 1, table);
                }

                // Правый потомок: добавляем бит 1
                if (node.right != nullNode)
                {
                    code[depth / 8] |= (1 << (depth % 8));
                    buildCodeTableRecursive(nodes, node.right, code, depth + 1, table);

                    // Сбрасываем бит обратно для корректного возврата по рекурсии
                    code[depth / 8] &= ~(1 << (depth % 8));
                }
            }

            // ================================================================
            // BitWriter -- побитовая буферизованная запись в поток
            // ================================================================

            bool HuffmanCompressor::BitWriter::writeBit(buint8 bit)
            {
                if (bit)
                    this->currentByte |= (1 << this->bitPos);

                this->bitPos++;

                if (this->bitPos == 8)
                {
                    // Байт заполнен -- помещаем в буфер
                    if (!this->flushByte(this->currentByte))
                        return false;

                    this->currentByte = 0;
                    this->bitPos = 0;
                }

                return true;
            }

            bool HuffmanCompressor::BitWriter::writeCode(const CodeEntry& entry)
            {
                for (buint8 i = 0; i < entry.length; ++i)
                {
                    buint8 bit = (entry.bits[i / 8] >> (i % 8)) & 1;
                    if (!this->writeBit(bit))
                        return false;
                }
                return true;
            }

            buint8 HuffmanCompressor::BitWriter::finish()
            {
                buint8 paddingBits = 0;

                if (this->bitPos > 0)
                {
                    // Есть неполный байт -- дополняем нулями и записываем
                    paddingBits = 8 - this->bitPos;
                    if (!this->flushByte(this->currentByte))
                        return 0xFF;

                    this->currentByte = 0;
                    this->bitPos = 0;
                }

                // Сбрасываем остаток буфера в поток (с учётом частичных записей)
                if (this->bufferPos > 0)
                {
                    size_t writtenTotal = 0;
                    while (writtenTotal < this->bufferPos)
                    {
                        size_t written = this->stream->write(
                            this->buffer + writtenTotal, this->bufferPos - writtenTotal);
                        if (written == 0)
                            return 0xFF;
                        writtenTotal += written;
                    }
                    this->bufferPos = 0;
                }

                return paddingBits;
            }

            bool HuffmanCompressor::BitWriter::flushByte(buint8 byte)
            {
                this->buffer[this->bufferPos] = byte;
                this->bufferPos++;

                if (this->bufferPos >= this->bufferCapacity)
                {
                    // Частичная запись допустима по контракту IOutputStream --
                    // дописываем буфер до конца
                    size_t writtenTotal = 0;
                    while (writtenTotal < this->bufferPos)
                    {
                        size_t written = this->stream->write(
                            this->buffer + writtenTotal, this->bufferPos - writtenTotal);
                        if (written == 0)
                            return false;
                        writtenTotal += written;
                    }
                    this->bufferPos = 0;
                }

                return true;
            }

            // ================================================================
            // Сжатие одного блока
            // ================================================================

            bool HuffmanCompressor::compressBlock(
                _In  const buint8* data,
                     buint64 dataSize,
                _Out blib::core::IOutputStream& out,
                     blib::memory::Allocator& allocator)
            {
                // ---- 1. Подсчёт частот ----
                buint64 freq[alphabetSize];
                countFrequencies(data, dataSize, freq);

                // ---- 2. Подсчёт уникальных символов ----
                buint16 symbolCount = 0;
                for (buint32 i = 0; i < alphabetSize; ++i)
                {
                    if (freq[i] > 0)
                        symbolCount++;
                }

                // ---- 3. Построение дерева Хаффмана ----
                // Максимум узлов: 256 листьев + 255 внутренних = 511
                const buint32 maxNodes = 511;
                size_t nodesSize = maxNodes * sizeof(HuffmanNode);
                HuffmanNode* nodes = static_cast<HuffmanNode*>(allocator.allocate(nodesSize));
                if (!nodes)
                    return false;

                buint32 nodeCount = 0;
                buint16 rootIndex = buildTree(freq, nodes, nodeCount);

                // ---- 4. Генерация таблицы кодов ----
                CodeEntry table[alphabetSize];
                if (rootIndex != nullNode)
                {
                    buildCodeTable(nodes, rootIndex, table);
                }
                else
                {
                    std::memset(table, 0, sizeof(table));
                }

                // Дерево больше не нужно
                allocator.deallocate(nodes, nodesSize);
                nodes = nullptr;

                // ---- 5. Запись заголовка блока ----
                // originalSize (8 байт)
                buint64 originalSize = dataSize;
                if (!writeAll(out, &originalSize, sizeof(buint64)))
                    return false;

                // Резервируем место для compressedSize (8 байт) -- запишем позже
                buint64 compressedSizePlaceholder = 0;
                buint64 compressedSizePos = 0; // позиция в потоке (если seekable)

                // Запоминаем позицию перед записью compressedSize
                // (IOutputStream наследует IStream, который имеет tell/seek)
                blib::core::IStream* streamBase = static_cast<blib::core::IStream*>(&out);
                bool canSeek = streamBase->canSeek();
                if (canSeek)
                    compressedSizePos = streamBase->tell();
                if (!writeAll(out, &compressedSizePlaceholder, sizeof(buint64)))
                    return false;

                // symbolCount (2 байта)
                if (!writeAll(out, &symbolCount, sizeof(buint16)))
                    return false;

                // Таблица частот (компактный формат: только ненулевые)
                for (buint32 i = 0; i < alphabetSize; ++i)
                {
                    if (freq[i] > 0)
                    {
                        buint8 symbol = static_cast<buint8>(i);
                        buint32 freqTrunc = static_cast<buint32>(
                            freq[i] > 0xFFFFFFFFull ? 0xFFFFFFFFu : freq[i]);
                        if (!writeAll(out, &symbol, sizeof(buint8)))
                            return false;
                        if (!writeAll(out, &freqTrunc, sizeof(buint32)))
                            return false;
                    }
                }

                // ---- 6. Кодирование данных ----
                // Выделяем буфер для BitWriter (4 КБ)
                const buint32 bitWriterBufSize = 4096;
                size_t bitBufAllocSize = bitWriterBufSize * sizeof(buint8);
                buint8* bitBuf = static_cast<buint8*>(allocator.allocate(bitBufAllocSize));
                if (!bitBuf)
                    return false;

                BitWriter writer;
                writer.stream         = &out;
                writer.buffer         = bitBuf;
                writer.bufferCapacity = bitWriterBufSize;
                writer.bufferPos      = 0;
                writer.currentByte    = 0;
                writer.bitPos         = 0;

                // Резервируем 1 байт для paddingBits (запишем после кодирования)
                buint64 paddingBitsPos = 0;
                buint8 paddingBitsPlaceholder = 0;
                if (canSeek)
                    paddingBitsPos = streamBase->tell();
                if (!writeAll(out, &paddingBitsPlaceholder, sizeof(buint8)))
                {
                    allocator.deallocate(bitBuf, bitBufAllocSize);
                    return false;
                }

                // Кодируем каждый байт данных соответствующим битовым кодом
                for (buint64 i = 0; i < dataSize; ++i)
                {
                    const CodeEntry& entry = table[data[i]];
                    if (entry.length == 0)
                    {
                        // Символ без кода -- не должно происходить (freq > 0 => код есть)
                        allocator.deallocate(bitBuf, bitBufAllocSize);
                        return false;
                    }

                    if (!writer.writeCode(entry))
                    {
                        allocator.deallocate(bitBuf, bitBufAllocSize);
                        return false;
                    }
                }

                // Завершаем запись битов (flush неполного байта и буфера)
                buint8 paddingBits = writer.finish();
                if (paddingBits == 0xFF)
                {
                    allocator.deallocate(bitBuf, bitBufAllocSize);
                    return false;
                }

                allocator.deallocate(bitBuf, bitBufAllocSize);
                bitBuf = nullptr;

                // ---- 7. Записываем compressedSize и paddingBits ----
                if (canSeek)
                {
                    buint64 endPos = streamBase->tell();

                    // compressedSize = всё что между концом compressedSize-поля и endPos
                    // (symbolCount + таблица + paddingBits + encoded data)
                    buint64 compressedSize = endPos - (compressedSizePos + sizeof(buint64));

                    // Перемотка на позицию compressedSize и запись
                    if (!streamBase->seek(
                            static_cast<bint64>(compressedSizePos),
                            blib::core::SeekOrigin::Begin))
                        return false;
                    if (!writeAll(out, &compressedSize, sizeof(buint64)))
                        return false;

                    // Перемотка на позицию paddingBits и запись
                    if (!streamBase->seek(
                            static_cast<bint64>(paddingBitsPos),
                            blib::core::SeekOrigin::Begin))
                        return false;
                    if (!writeAll(out, &paddingBits, sizeof(buint8)))
                        return false;

                    // Возвращаемся в конец потока
                    if (!streamBase->seek(
                            static_cast<bint64>(endPos),
                            blib::core::SeekOrigin::Begin))
                        return false;
                }

                return true;
            }

            // ================================================================
            // Stored-блок (режим noCompression)
            // ================================================================

            bool HuffmanCompressor::compressStoredBlock(
                _In  const buint8* data,
                     buint64 dataSize,
                _Out blib::core::IOutputStream& out)
            {
                // Формат stored-блока: originalSize(8) + compressedSize(8) +
                // symbolCount(2) == 0 + сырые данные.
                // compressedSize = symbolCount(2) + dataSize; paddingBits нет.
                if (!writeAll(out, &dataSize, sizeof(buint64)))
                    return false;

                buint64 compSize = sizeof(buint16) + dataSize;
                if (!writeAll(out, &compSize, sizeof(buint64)))
                    return false;

                buint16 symbolCount = 0;
                if (!writeAll(out, &symbolCount, sizeof(buint16)))
                    return false;

                if (dataSize > 0 && !writeAll(out, data, static_cast<size_t>(dataSize)))
                    return false;

                return true;
            }

            // ================================================================
            // Декомпрессия одного блока
            // ================================================================

            bool HuffmanCompressor::decompressBlock(
                _In  blib::core::IInputStream& in,
                _Out blib::core::IOutputStream& out,
                     blib::memory::Allocator& allocator)
            {
                // ---- 1. Чтение заголовка блока ----

                // originalSize (8 байт)
                buint64 originalSize = 0;
                if (!readAll(in, &originalSize, sizeof(buint64)))
                    return false;

                // compressedSize (8 байт)
                buint64 compressedSize = 0;
                if (!readAll(in, &compressedSize, sizeof(buint64)))
                    return false;

                // symbolCount (2 байта)
                buint16 symbolCount = 0;
                if (!readAll(in, &symbolCount, sizeof(buint16)))
                    return false;

                // Символов не может быть больше размера алфавита (256)
                if (symbolCount > alphabetSize)
                    return false;

                // ---- 2. Stored-блок (noCompression): symbolCount == 0 ----
                // Данные лежат сырьём, без дерева и padding.
                if (symbolCount == 0)
                {
                    // compressedSize обязан равняться symbolCount(2) + сырые данные
                    if (compressedSize != sizeof(buint16) + originalSize)
                        return false;

                    // Копируем сырые данные в выходной поток чанками
                    const buint32 outBufCapacity = 4096;
                    size_t outBufAllocSize = outBufCapacity * sizeof(buint8);
                    buint8* outBuf = static_cast<buint8*>(allocator.allocate(outBufAllocSize));
                    if (!outBuf)
                        return false;

                    buint64 remaining = originalSize;
                    bool ok = true;
                    while (remaining > 0)
                    {
                        size_t chunk = static_cast<size_t>(
                            (remaining < outBufCapacity) ? remaining : outBufCapacity);
                        if (!readAll(in, outBuf, chunk))
                        {
                            ok = false;
                            break;
                        }
                        if (!writeAll(out, outBuf, chunk))
                        {
                            ok = false;
                            break;
                        }
                        remaining -= chunk;
                    }

                    allocator.deallocate(outBuf, outBufAllocSize);
                    return ok;
                }

                // Huffman-блок с непустой таблицей обязан содержать данные:
                // пустой вход кодируется нулевым количеством блоков
                if (originalSize == 0)
                    return false;

                // ---- 3. Чтение таблицы частот ----
                buint64 freq[alphabetSize];
                std::memset(freq, 0, sizeof(freq));

                for (buint16 i = 0; i < symbolCount; ++i)
                {
                    buint8 symbol = 0;
                    buint32 freqVal = 0;
                    if (!readAll(in, &symbol, sizeof(buint8)))
                        return false;
                    if (!readAll(in, &freqVal, sizeof(buint32)))
                        return false;

                    // Нулевая частота в таблице недопустима -- битый поток
                    if (freqVal == 0)
                        return false;

                    freq[symbol] = static_cast<buint64>(freqVal);
                }

                // ---- 4. Восстановление дерева Хаффмана из частот ----
                const buint32 maxNodes = 511;
                size_t nodesSize = maxNodes * sizeof(HuffmanNode);
                HuffmanNode* nodes = static_cast<HuffmanNode*>(allocator.allocate(nodesSize));
                if (!nodes)
                    return false;

                buint32 nodeCount = 0;
                buint16 rootIndex = buildTree(freq, nodes, nodeCount);

                // Дерево не построилось при ненулевых данных -- битый поток
                if (rootIndex == nullNode)
                {
                    allocator.deallocate(nodes, nodesSize);
                    return false;
                }

                // ---- 5. Чтение paddingBits ----
                buint8 paddingBits = 0;
                if (!readAll(in, &paddingBits, sizeof(buint8)))
                {
                    allocator.deallocate(nodes, nodesSize);
                    return false;
                }

                // Допустимы только 0..7 неиспользуемых бит в последнем байте.
                // Проверка ДО декодирования: иначе totalBits уходит в underflow
                // и цикл декодирования читает за границами encodedData.
                if (paddingBits > 7)
                {
                    allocator.deallocate(nodes, nodesSize);
                    return false;
                }

                // ---- 6. Чтение сжатых данных в буфер ----
                // compressedSize включает symbolCount + таблицу + paddingBits + encoded data.
                // Уже прочитано: symbolCount(2) + таблица(symbolCount * 5) + paddingBits(1)
                buint64 headerPartSize = sizeof(buint16) +
                    static_cast<buint64>(symbolCount) * (sizeof(buint8) + sizeof(buint32)) +
                    sizeof(buint8);

                // Защита от underflow: compressedSize обязан покрывать заголовок
                if (compressedSize < headerPartSize)
                {
                    allocator.deallocate(nodes, nodesSize);
                    return false;
                }

                buint64 encodedDataSize = compressedSize - headerPartSize;
                if (encodedDataSize == 0)
                {
                    // При ненулевых данных закодированные байты обязаны быть
                    allocator.deallocate(nodes, nodesSize);
                    return false;
                }

                buint8* encodedData = static_cast<buint8*>(
                    allocator.allocate(static_cast<size_t>(encodedDataSize)));
                if (!encodedData)
                {
                    allocator.deallocate(nodes, nodesSize);
                    return false;
                }

                if (!readAll(in, encodedData, static_cast<size_t>(encodedDataSize)))
                {
                    allocator.deallocate(encodedData, static_cast<size_t>(encodedDataSize));
                    allocator.deallocate(nodes, nodesSize);
                    return false;
                }

                // ---- 6. Декодирование: обход дерева по битам ----
                // Буфер вывода для минимизации вызовов write()
                const buint32 outBufCapacity = 4096;
                size_t outBufAllocSize = outBufCapacity * sizeof(buint8);
                buint8* outBuf = static_cast<buint8*>(allocator.allocate(outBufAllocSize));
                if (!outBuf)
                {
                    allocator.deallocate(encodedData, static_cast<size_t>(encodedDataSize));
                    allocator.deallocate(nodes, nodesSize);
                    return false;
                }

                buint32 outBufPos = 0;
                buint64 decodedCount = 0;
                buint16 currentNode = rootIndex;

                // Общее количество значащих бит
                buint64 totalBits = encodedDataSize * 8 - paddingBits;
                buint64 bitIndex = 0;

                while (decodedCount < originalSize && bitIndex < totalBits)
                {
                    // Извлекаем очередной бит
                    buint64 byteIdx = bitIndex / 8;
                    buint8  bitOff  = static_cast<buint8>(bitIndex % 8);
                    buint8  bit     = (encodedData[byteIdx] >> bitOff) & 1;
                    bitIndex++;

                    // Обходим дерево: 0 -- влево, 1 -- вправо
                    if (bit == 0)
                        currentNode = nodes[currentNode].left;
                    else
                        currentNode = nodes[currentNode].right;

                    // Проверка на невалидный узел
                    if (currentNode == nullNode)
                    {
                        allocator.deallocate(outBuf, outBufAllocSize);
                        allocator.deallocate(encodedData, static_cast<size_t>(encodedDataSize));
                        allocator.deallocate(nodes, nodesSize);
                        return false;
                    }

                    // Достигли листа -- декодировали символ
                    if (nodes[currentNode].isLeaf)
                    {
                        outBuf[outBufPos] = nodes[currentNode].symbol;
                        outBufPos++;
                        decodedCount++;

                        // Сброс буфера при заполнении
                        if (outBufPos >= outBufCapacity)
                        {
                            if (!writeAll(out, outBuf, outBufPos))
                            {
                                allocator.deallocate(outBuf, outBufAllocSize);
                                allocator.deallocate(encodedData, static_cast<size_t>(encodedDataSize));
                                allocator.deallocate(nodes, nodesSize);
                                return false;
                            }
                            outBufPos = 0;
                        }

                        // Возвращаемся к корню для декодирования следующего символа
                        currentNode = rootIndex;
                    }
                }

                // Сброс остатка буфера
                if (outBufPos > 0)
                {
                    if (!writeAll(out, outBuf, outBufPos))
                    {
                        allocator.deallocate(outBuf, outBufAllocSize);
                        allocator.deallocate(encodedData, static_cast<size_t>(encodedDataSize));
                        allocator.deallocate(nodes, nodesSize);
                        return false;
                    }
                }

                // Освобождаем ресурсы
                allocator.deallocate(outBuf, outBufAllocSize);
                allocator.deallocate(encodedData, static_cast<size_t>(encodedDataSize));
                allocator.deallocate(nodes, nodesSize);

                // Проверка: декодировали ровно столько, сколько ожидали
                return (decodedCount == originalSize);
            }

            // ================================================================
            // compress -- основная точка входа для сжатия
            // ================================================================

            bool HuffmanCompressor::compress(
                _In  blib::core::IInputStream&  decompressedInputStream,
                _Out blib::core::IOutputStream& compressedOutputStream)
            {
                blib::memory::Allocator& allocator = this->settings.memoryPoolForCompression;

                // Вход обязан быть seekable (контракт Huffman: позиционирование
                // по блокам при известном размере потока)
                if (!decompressedInputStream.canSeek())
                    return false;

                // Выход обязан быть seekable: compressedSize и paddingBits
                // записываются плейсхолдерами и патчатся в конце через seek.
                // Без этого поток получился бы битым при внешне успешном сжатии.
                if (!compressedOutputStream.canSeek())
                    return false;

                // Определяем размер входных данных.
                // size() == 0 по контракту IStream означает "размер неизвестен"
                // (либо поток действительно пуст) -- тогда читаем до EOF.
                buint64 inputStart = decompressedInputStream.tell();
                buint64 streamSize  = decompressedInputStream.size();

                buint8* wholeInput     = nullptr; // буфер всего входа (при неизвестном размере)
                buint64 wholeInputCap  = 0;       // сколько байт аллоцировано
                buint64 wholeInputSize = 0;       // сколько байт прочитано
                bool hasWholeInput = false;

                buint64 remainingSize = 0;

                if (streamSize == 0)
                {
                    // Размер неизвестен: читаем до EOF, растягивая буфер геометрически
                    while (true)
                    {
                        if (wholeInputSize == wholeInputCap)
                        {
                            buint64 newCap = (wholeInputCap == 0) ? 4096 : wholeInputCap * 2;
                            if (newCap < wholeInputCap)
                            {
                                // Переполнение счётчика -- отказ
                                if (wholeInput)
                                    allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                                return false;
                            }

                            buint8* newBuf = static_cast<buint8*>(
                                allocator.allocate(static_cast<size_t>(newCap)));
                            if (!newBuf)
                            {
                                if (wholeInput)
                                    allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                                return false;
                            }

                            if (wholeInputSize > 0)
                                std::memcpy(newBuf, wholeInput, static_cast<size_t>(wholeInputSize));
                            if (wholeInput)
                                allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));

                            wholeInput = newBuf;
                            wholeInputCap = newCap;
                        }

                        size_t n = decompressedInputStream.read(
                            wholeInput + wholeInputSize,
                            static_cast<size_t>(wholeInputCap - wholeInputSize));
                        if (n == 0)
                            break; // EOF
                        wholeInputSize += n;
                    }

                    remainingSize = wholeInputSize;
                    hasWholeInput = true;
                }
                else
                {
                    // Размер известен: сжимаем от текущей позиции
                    if (inputStart > streamSize)
                        return false;
                    remainingSize = streamSize - inputStart;
                }

                // Определяем количество блоков
                buint64 blockSize = this->settings.blockSize;
                buint32 blockCount = 0;

                if (remainingSize > 0)
                {
                    if (this->settings.level == CompressionSettings::CompressionLevel::noCompression)
                    {
                        // noCompression: весь поток -- один stored-блок
                        blockCount = 1;
                    }
                    else if (blockSize > 0)
                    {
                        blockCount = static_cast<buint32>(
                            (remainingSize + blockSize - 1) / blockSize);
                    }
                    else
                    {
                        blockCount = 1;
                    }
                }

                // ---- Запись глобального заголовка ----
                // Magic (4 байта)
                if (!writeAll(compressedOutputStream, magic, 4))
                {
                    if (wholeInput)
                        allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                    return false;
                }

                // Version (1 байт)
                if (!writeAll(compressedOutputStream, &formatVersion, sizeof(buint8)))
                {
                    if (wholeInput)
                        allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                    return false;
                }

                // Block count (4 байта)
                if (!writeAll(compressedOutputStream, &blockCount, sizeof(buint32)))
                {
                    if (wholeInput)
                        allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                    return false;
                }

                // ---- noCompression: stored-блок без дерева Хаффмана ----
                if (this->settings.level == CompressionSettings::CompressionLevel::noCompression)
                {
                    if (remainingSize > 0)
                    {
                        if (!hasWholeInput)
                        {
                            // Материализуем весь вход (нужен единый stored-блок)
                            wholeInput = static_cast<buint8*>(
                                allocator.allocate(static_cast<size_t>(remainingSize)));
                            if (!wholeInput)
                                return false;
                            wholeInputCap  = remainingSize;
                            wholeInputSize = remainingSize;

                            if (!readAll(decompressedInputStream,
                                    wholeInput, static_cast<size_t>(remainingSize)))
                            {
                                allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                                return false;
                            }
                        }

                        bool ok = compressStoredBlock(wholeInput, wholeInputSize, compressedOutputStream);
                        allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                        return ok;
                    }

                    // Пустой вход -- заголовок уже записан, блоков нет
                    if (wholeInput)
                        allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                    return true;
                }

                // ---- Обработка блоков Хаффмана ----
                buint64 processedSize = 0;

                for (buint32 blockIdx = 0; blockIdx < blockCount; ++blockIdx)
                {
                    // Размер текущего блока
                    buint64 currentBlockSize;
                    if (blockSize == 0)
                    {
                        // Один блок -- весь поток
                        currentBlockSize = remainingSize;
                    }
                    else
                    {
                        buint64 remaining = remainingSize - processedSize;
                        currentBlockSize = (remaining < blockSize) ? remaining : blockSize;
                    }

                    const buint8* blockData = nullptr;
                    buint8* allocatedBlock = nullptr;

                    if (hasWholeInput)
                    {
                        // Данные уже в памяти (вход с неизвестным размером)
                        blockData = wholeInput + processedSize;
                    }
                    else
                    {
                        // Читаем блок данных в память
                        allocatedBlock = static_cast<buint8*>(
                            allocator.allocate(static_cast<size_t>(currentBlockSize)));
                        if (!allocatedBlock)
                        {
                            if (wholeInput)
                                allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                            return false;
                        }

                        // Перемотка входного потока на позицию текущего блока
                        if (!decompressedInputStream.seek(
                                static_cast<bint64>(inputStart + processedSize),
                                blib::core::SeekOrigin::Begin))
                        {
                            allocator.deallocate(allocatedBlock, static_cast<size_t>(currentBlockSize));
                            if (wholeInput)
                                allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                            return false;
                        }

                        if (!readAll(decompressedInputStream,
                                allocatedBlock, static_cast<size_t>(currentBlockSize)))
                        {
                            allocator.deallocate(allocatedBlock, static_cast<size_t>(currentBlockSize));
                            if (wholeInput)
                                allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                            return false;
                        }

                        blockData = allocatedBlock;
                    }

                    // Сжимаем блок
                    bool ok = compressBlock(
                        blockData, currentBlockSize,
                        compressedOutputStream, allocator);

                    if (allocatedBlock)
                        allocator.deallocate(allocatedBlock, static_cast<size_t>(currentBlockSize));

                    if (!ok)
                    {
                        if (wholeInput)
                            allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));
                        return false;
                    }

                    processedSize += currentBlockSize;
                }

                if (wholeInput)
                    allocator.deallocate(wholeInput, static_cast<size_t>(wholeInputCap));

                return true;
            }

            // ================================================================
            // decompress -- основная точка входа для декомпрессии
            // ================================================================

            bool HuffmanCompressor::decompress(
                _In  blib::core::IInputStream&  compressedInputStream,
                _Out blib::core::IOutputStream& decompressedOutputStream)
            {
                blib::memory::Allocator& allocator = this->settings.memoryPoolForCompression;

                // ---- Чтение глобального заголовка ----
                // Magic (4 байта)
                buint8 readMagic[4];
                if (!readAll(compressedInputStream, readMagic, 4))
                    return false;

                // Проверяем magic
                if (readMagic[0] != magic[0] || readMagic[1] != magic[1] ||
                    readMagic[2] != magic[2] || readMagic[3] != magic[3])
                    return false;

                // Version (1 байт)
                buint8 version = 0;
                if (!readAll(compressedInputStream, &version, sizeof(buint8)))
                    return false;

                // Проверяем версию формата
                if (version != formatVersion)
                    return false;

                // Block count (4 байта)
                buint32 blockCount = 0;
                if (!readAll(compressedInputStream, &blockCount, sizeof(buint32)))
                    return false;

                // ---- Декомпрессия блоков ----
                for (buint32 blockIdx = 0; blockIdx < blockCount; ++blockIdx)
                {
                    if (!decompressBlock(compressedInputStream, decompressedOutputStream, allocator))
                        return false;
                }

                return true;
            }

        } // namespace compression
    } // namespace algorithm
} // namespace blib
