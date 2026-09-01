#include <blib/test/src/test.h>

// Compression: интерфейс и реализация Huffman
#include <blib/core/algorithm/compression/icompressor.h>
#include <blib/core/algorithm/compression/huffmanCompressor.h>

// Потоки: MemoryStream для in-memory round-trip
#include <blib/core/istream.h>
#include <blib/core/ostream.h>
#include <blib/core/memoryStream.h>

// Аллокатор
#include <blib/system/memory/allocator.h>

#include <cstring>

using namespace blib::core;
using namespace blib::algorithm::compression;

// ============================================================
// Вспомогательные функции
// ============================================================

namespace
{
    /**
     * Хелпер: сжать данные через HuffmanCompressor и вернуть результат.
     * Использует MemoryStream для входа и выхода.
     *
     * @param compressor  Настроенный компрессор
     * @param data        Исходные данные
     * @param dataSize    Размер данных
     * @param outStream   [out] Выходной поток со сжатыми данными
     * @return true при успешном сжатии
     */
    bool compressData(
        HuffmanCompressor& compressor,
        const buint8* data,
        size_t dataSize,
        MemoryStream& outStream)
    {
        MemoryStream inStream;
        if (dataSize > 0)
            inStream.write(data, dataSize);
        inStream.seek(0, SeekOrigin::Begin);

        return compressor.compress(inStream, outStream);
    }

    /**
     * Хелпер: round-trip -- сжать и распаковать данные,
     * проверить что результат совпадает с оригиналом.
     *
     * @param compressor  Настроенный компрессор
     * @param data        Исходные данные
     * @param dataSize    Размер данных
     * @return true если round-trip прошёл успешно и данные совпали
     */
    bool roundTrip(
        HuffmanCompressor& compressor,
        const buint8* data,
        size_t dataSize)
    {
        // Сжатие
        MemoryStream compressed;
        if (!compressData(compressor, data, dataSize, compressed))
            return false;

        // Декомпрессия
        compressed.seek(0, SeekOrigin::Begin);
        MemoryStream decompressed;
        if (!compressor.decompress(compressed, decompressed))
            return false;

        // Проверка размера
        if (decompressed.size() != static_cast<buint64>(dataSize))
            return false;

        // Проверка содержимого
        if (dataSize > 0)
        {
            const ByteArray& result = decompressed.getData();
            if (std::memcmp(result.data(), data, dataSize) != 0)
                return false;
        }

        return true;
    }

    /**
     * Настроить компрессор дефолтными настройками.
     *
     * setSettings использует move-семантику: аллокатор переносится
     * без копирования (копирование stateful-аллокатора в Debug
     * даёт null-impl, т.к. share() ещё не реализован).
     */
    void configureDefaultCompressor(HuffmanCompressor& compressor)
    {
        compressor.setSettings(CompressionSettings());
    }

    /**
     * Настроить компрессор с заданным blockSize (move-семантика).
     */
    void configureBlockCompressor(HuffmanCompressor& compressor, buint64 blockSize)
    {
        CompressionSettings settings;
        settings.blockSize = blockSize;
        compressor.setSettings(std::move(settings));
    }

    /**
     * Настроить компрессор с уровнем noCompression (stored-блоки).
     */
    void configureNoCompressionCompressor(HuffmanCompressor& compressor)
    {
        CompressionSettings settings;
        settings.level = CompressionSettings::CompressionLevel::noCompression;
        compressor.setSettings(std::move(settings));
    }

    /**
     * Non-seekable input stream -- обёртка над MemoryStream,
     * которая возвращает canSeek() == false.
     */
    class NonSeekableInputStream : public IInputStream
    {
    public:
        explicit NonSeekableInputStream(const buint8* data, size_t size)
            : mem()
        {
            if (size > 0)
                this->mem.write(data, size);
            this->mem.seek(0, SeekOrigin::Begin);
        }

        size_t read(_Out void* buffer, size_t size) __blib_override
        {
            return this->mem.read(buffer, size);
        }

        bool    canSeek() const __blib_override { return false; }
        bool    seek(bint64, SeekOrigin) __blib_override { return false; }
        buint64 tell() const __blib_override { return 0; }
        buint64 size() const __blib_override { return 0; }

    private:
        MemoryStream mem;
    };

    /**
     * Non-seekable output stream -- обёртка над MemoryStream,
     * которая возвращает canSeek() == false.
     * Внутренний буфер растёт, поэтому write() всегда "успешен".
     */
    class NonSeekableOutputStream : public IOutputStream
    {
    public:
        size_t write(_In const void* data, size_t size) __blib_override
        {
            return this->mem.write(data, size);
        }

        bool    canSeek() const __blib_override { return false; }
        bool    seek(bint64, SeekOrigin) __blib_override { return false; }
        buint64 tell() const __blib_override { return 0; }
        buint64 size() const __blib_override { return 0; }

    private:
        MemoryStream mem;
    };

    /**
     * Chunked output stream -- пишет не более maxChunk байт за вызов write().
     * Частичная запись ЛЕГАЛЬНА по контракту IOutputStream::write
     * ("пишет ДО size байт, возвращает фактически записанное").
     */
    class ChunkedOutputStream : public IOutputStream
    {
    public:
        explicit ChunkedOutputStream(size_t maxChunk)
            : maxChunk(maxChunk)
            , mem()
        {
        }

        size_t write(_In const void* data, size_t size) __blib_override
        {
            size_t chunk = (size < this->maxChunk) ? size : this->maxChunk;
            return this->mem.write(data, chunk);
        }

        bool    canSeek() const __blib_override { return true; }
        bool    seek(bint64 offset, SeekOrigin origin) __blib_override { return this->mem.seek(offset, origin); }
        buint64 tell() const __blib_override { return this->mem.tell(); }
        buint64 size() const __blib_override { return this->mem.size(); }

        // Доступ к накопленным байтам для декомпрессии
        const ByteArray& getData() const { return this->mem.getData(); }

    private:
        size_t maxChunk;   // максимальный размер одной записи
        MemoryStream mem;  // накопитель
    };

    /**
     * FailOnNthWrite output stream -- N-й (0-based) вызов write()
     * возвращает 0 (отказ приёмника), остальные вызовы работают.
     * Имитирует точечный сбой записи, ловит игнорирование
     * возвращаемого значения write().
     */
    class FailOnNthWriteOutputStream : public IOutputStream
    {
    public:
        explicit FailOnNthWriteOutputStream(size_t failIndex)
            : failIndex(failIndex)
            , writeCalls(0)
            , mem()
        {
        }

        size_t write(_In const void* data, size_t size) __blib_override
        {
            if (this->writeCalls == this->failIndex)
            {
                this->writeCalls++;
                return 0;
            }

            this->writeCalls++;
            return this->mem.write(data, size);
        }

        bool    canSeek() const __blib_override { return true; }
        bool    seek(bint64 offset, SeekOrigin origin) __blib_override { return this->mem.seek(offset, origin); }
        buint64 tell() const __blib_override { return this->mem.tell(); }
        buint64 size() const __blib_override { return this->mem.size(); }

    private:
        size_t failIndex;     // индекс вызова write(), который "падает"
        size_t writeCalls;    // счётчик вызовов
        MemoryStream mem;     // накопитель
    };

    /**
     * Chunked input stream -- read() отдаёт не более maxChunk байт за вызов.
     * Частичное чтение ЛЕГАЛЬНО по контракту IInputStream::read.
     * Поток seekable, size() корректен.
     */
    class ChunkedInputStream : public IInputStream
    {
    public:
        explicit ChunkedInputStream(const buint8* data, size_t size, size_t maxChunk)
            : maxChunk(maxChunk)
            , mem()
        {
            if (size > 0)
                this->mem.write(data, size);
            this->mem.seek(0, SeekOrigin::Begin);
        }

        size_t read(_Out void* buffer, size_t size) __blib_override
        {
            size_t chunk = (size < this->maxChunk) ? size : this->maxChunk;
            return this->mem.read(buffer, chunk);
        }

        bool    canSeek() const __blib_override { return true; }
        bool    seek(bint64 offset, SeekOrigin origin) __blib_override { return this->mem.seek(offset, origin); }
        buint64 tell() const __blib_override { return this->mem.tell(); }
        buint64 size() const __blib_override { return this->mem.size(); }

    private:
        size_t maxChunk;   // максимальный размер одного чтения
        MemoryStream mem;  // источник данных
    };

    /**
     * UnknownSizeSeekableStream -- canSeek() == true, но size() возвращает 0.
     * По контракту IStream::size() == 0 означает "размер неизвестен/неприменим".
     * Данные при этом есть, и читаются до EOF.
     */
    class UnknownSizeSeekableStream : public IInputStream
    {
    public:
        explicit UnknownSizeSeekableStream(const buint8* data, size_t size)
            : mem()
        {
            if (size > 0)
                this->mem.write(data, size);
            this->mem.seek(0, SeekOrigin::Begin);
        }

        size_t read(_Out void* buffer, size_t size) __blib_override
        {
            return this->mem.read(buffer, size);
        }

        bool    canSeek() const __blib_override { return true; }
        bool    seek(bint64 offset, SeekOrigin origin) __blib_override { return this->mem.seek(offset, origin); }
        buint64 tell() const __blib_override { return this->mem.tell(); }
        buint64 size() const __blib_override { return 0; } // размер неизвестен

    private:
        MemoryStream mem;
    };
}

// ============================================================
// 1. ICompressor interface: базовые тесты интерфейса
// ============================================================

BLIB_TEST_CASE("ICompressor: algorithmName returns Huffman")
{
    HuffmanCompressor compressor;
    const char* name = compressor.algorithmName();

    BLIB_TEST_REQUIRE(name != nullptr);
    BLIB_TEST_CHECK(std::strcmp(name, "Huffman") == 0);
}

BLIB_TEST_CASE("ICompressor: setSettings/getSettings round trip")
{
    HuffmanCompressor compressor;

    CompressionSettings settings;
    settings.level = CompressionSettings::CompressionLevel::max;
    settings.blockSize = 4096;
    settings.dictionarySize = 1024;
    settings.wordSize = 8;
    settings.threadCount = 4;

    compressor.setSettings(std::move(settings));
    const CompressionSettings& got = compressor.getSettings();

    BLIB_TEST_CHECK(got.level == CompressionSettings::CompressionLevel::max);
    BLIB_TEST_CHECK(got.blockSize == 4096);
    BLIB_TEST_CHECK(got.dictionarySize == 1024);
    BLIB_TEST_CHECK(got.wordSize == 8);
    BLIB_TEST_CHECK(got.threadCount == 4);
}

BLIB_TEST_CASE("ICompressor: validateSettings accepts defaults")
{
    HuffmanCompressor compressor;
    CompressionSettings settings;
    BLIB_TEST_CHECK(compressor.validateSettings(settings) == true);
}

// ============================================================
// 2. Huffman: round-trip тесты
// ============================================================

BLIB_TEST_CASE("Huffman: round trip simple ASCII text")
{
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);
    const char* text = "Hello, World! This is a test of Huffman compression.";
    size_t len = std::strlen(text);

    BLIB_TEST_CHECK(roundTrip(compressor,
        reinterpret_cast<const buint8*>(text), len));
}

BLIB_TEST_CASE("Huffman: round trip single byte")
{
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);
    buint8 data = 0x42;
    BLIB_TEST_CHECK(roundTrip(compressor, &data, 1));
}

BLIB_TEST_CASE("Huffman: round trip repeated single byte")
{
    // Лучший случай для Huffman -- единственный символ,
    // код длиной 1 бит, степень сжатия ~8x
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const size_t size = 1000;
    buint8 data[size];
    std::memset(data, 0xAA, size);

    BLIB_TEST_CHECK(roundTrip(compressor, data, size));
}

BLIB_TEST_CASE("Huffman: round trip two distinct bytes")
{
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const size_t size = 200;
    buint8 data[size];
    for (size_t i = 0; i < size; ++i)
        data[i] = (i % 2 == 0) ? 0x00 : 0xFF;

    BLIB_TEST_CHECK(roundTrip(compressor, data, size));
}

BLIB_TEST_CASE("Huffman: round trip all 256 byte values")
{
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    buint8 data[256];
    for (int i = 0; i < 256; ++i)
        data[i] = static_cast<buint8>(i);

    BLIB_TEST_CHECK(roundTrip(compressor, data, 256));
}

BLIB_TEST_CASE("Huffman: round trip skewed distribution")
{
    // 90% один символ, 10% остальные -- проверяет что Huffman
    // корректно работает с сильно неравномерным распределением
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const size_t size = 1000;
    buint8 data[size];
    for (size_t i = 0; i < size; ++i)
    {
        if (i < 900)
            data[i] = 0x41;  // 'A' -- 90%
        else
            data[i] = static_cast<buint8>(i % 10);  // 0-9 -- по ~1% каждый
    }

    BLIB_TEST_CHECK(roundTrip(compressor, data, size));
}

BLIB_TEST_CASE("Huffman: round trip large data (64 KB)")
{
    // Проверяет работу буферизации (BitWriter, 4 КБ буфер)
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const size_t size = 65536;
    blib::memory::Allocator alloc;
    buint8* data = static_cast<buint8*>(alloc.allocate(size));
    BLIB_TEST_REQUIRE(data != nullptr);

    // Заполняем повторяющимся паттерном
    for (size_t i = 0; i < size; ++i)
        data[i] = static_cast<buint8>(i % 73);  // 73 уникальных значения

    bool ok = roundTrip(compressor, data, size);
    alloc.deallocate(data, size);

    BLIB_TEST_CHECK(ok);
}

BLIB_TEST_CASE("Huffman: round trip incompressible data (uniform random)")
{
    // Pseudo-random данные -- worst case для Huffman.
    // Все 256 символов примерно равновероятны.
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const size_t size = 2048;
    buint8 data[size];

    // Простой LCG для детерминированных pseudo-random данных
    buint32 seed = 0xDEADBEEF;
    for (size_t i = 0; i < size; ++i)
    {
        seed = seed * 1103515245 + 12345;
        data[i] = static_cast<buint8>((seed >> 16) & 0xFF);
    }

    BLIB_TEST_CHECK(roundTrip(compressor, data, size));
}

// ============================================================
// 3. Huffman: edge cases
// ============================================================

BLIB_TEST_CASE("Huffman: round trip empty data")
{
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    // Пустой входной поток
    MemoryStream inStream;
    MemoryStream compressed;

    bool ok = compressor.compress(inStream, compressed);
    BLIB_TEST_REQUIRE(ok);

    // Декомпрессия пустых данных
    compressed.seek(0, SeekOrigin::Begin);
    MemoryStream decompressed;
    ok = compressor.decompress(compressed, decompressed);
    BLIB_TEST_REQUIRE(ok);

    // Результат должен быть пуст
    BLIB_TEST_CHECK(decompressed.size() == 0);
}

BLIB_TEST_CASE("Huffman: round trip exactly one of each 256 symbol")
{
    // Ровно по одному экземпляру каждого символа -- все частоты равны.
    // Дерево Хаффмана будет глубоким (до 8 уровней), коды ~8 бит.
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    buint8 data[256];
    for (int i = 0; i < 256; ++i)
        data[i] = static_cast<buint8>(i);

    BLIB_TEST_CHECK(roundTrip(compressor, data, 256));
}

BLIB_TEST_CASE("Huffman: compress rejects non-seekable input stream")
{
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    buint8 data[] = { 1, 2, 3, 4, 5 };
    NonSeekableInputStream nonSeekable(data, sizeof(data));

    MemoryStream compressed;
    bool ok = compressor.compress(nonSeekable, compressed);

    // Должен вернуть false -- Huffman требует seekable входной поток
    BLIB_TEST_CHECK(ok == false);
}

BLIB_TEST_CASE("Huffman: decompress rejects wrong magic")
{
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    // Сначала создадим валидные сжатые данные
    const char* text = "test data";
    MemoryStream compressed;
    compressData(compressor, reinterpret_cast<const buint8*>(text),
        std::strlen(text), compressed);

    // Подменяем magic bytes
    ByteArray corruptedData = compressed.getData();
    BLIB_TEST_REQUIRE(corruptedData.size() >= 4);
    corruptedData[0] = 'X';  // Ломаем magic

    MemoryStream corruptedStream(std::move(corruptedData));
    MemoryStream decompressed;
    bool ok = compressor.decompress(corruptedStream, decompressed);

    BLIB_TEST_CHECK(ok == false);
}

BLIB_TEST_CASE("Huffman: decompress rejects wrong version")
{
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    // Создаём валидные сжатые данные
    const char* text = "test data";
    MemoryStream compressed;
    compressData(compressor, reinterpret_cast<const buint8*>(text),
        std::strlen(text), compressed);

    // Подменяем version byte (смещение 4 -- после magic)
    ByteArray corruptedData = compressed.getData();
    BLIB_TEST_REQUIRE(corruptedData.size() >= 5);
    corruptedData[4] = 0xFF;  // Невалидная версия

    MemoryStream corruptedStream(std::move(corruptedData));
    MemoryStream decompressed;
    bool ok = compressor.decompress(corruptedStream, decompressed);

    BLIB_TEST_CHECK(ok == false);
}

// ============================================================
// 4. Huffman: блочный режим (blockSize > 0)
// ============================================================

BLIB_TEST_CASE("Huffman: round trip with blockSize (data > block)")
{
    // blockSize = 64, данные 256 байт -> 4 блока
    HuffmanCompressor compressor;
    configureBlockCompressor(compressor, 64);

    buint8 data[256];
    for (int i = 0; i < 256; ++i)
        data[i] = static_cast<buint8>(i);

    BLIB_TEST_CHECK(roundTrip(compressor, data, 256));
}

BLIB_TEST_CASE("Huffman: round trip with blockSize (data == block)")
{
    // blockSize == размер данных -> ровно один блок
    HuffmanCompressor compressor;
    configureBlockCompressor(compressor, 100);

    buint8 data[100];
    for (int i = 0; i < 100; ++i)
        data[i] = static_cast<buint8>(i % 50);

    BLIB_TEST_CHECK(roundTrip(compressor, data, 100));
}

BLIB_TEST_CASE("Huffman: round trip with blockSize (data < block)")
{
    // blockSize больше данных -> один блок (данные не дотягивают)
    HuffmanCompressor compressor;
    configureBlockCompressor(compressor, 10000);

    const char* text = "Short data.";
    BLIB_TEST_CHECK(roundTrip(compressor,
        reinterpret_cast<const buint8*>(text), std::strlen(text)));
}

BLIB_TEST_CASE("Huffman: round trip with blockSize 1")
{
    // Экстремальный случай: каждый байт -- отдельный блок.
    // Каждый блок содержит единственный символ.
    HuffmanCompressor compressor;
    configureBlockCompressor(compressor, 1);

    buint8 data[] = { 0x10, 0x20, 0x30, 0x40, 0x50 };
    BLIB_TEST_CHECK(roundTrip(compressor, data, sizeof(data)));
}

BLIB_TEST_CASE("Huffman: multi-block heterogeneous data")
{
    // Первая половина -- все 0x00, вторая -- все 0xFF.
    // С блоками каждая половина будет иметь своё оптимальное дерево.
    HuffmanCompressor compressor;
    configureBlockCompressor(compressor, 128);

    const size_t size = 512;
    buint8 data[size];
    std::memset(data, 0x00, size / 2);
    std::memset(data + size / 2, 0xFF, size / 2);

    BLIB_TEST_CHECK(roundTrip(compressor, data, size));
}

// ============================================================
// 5. Huffman: свойства сжатия
// ============================================================

BLIB_TEST_CASE("Huffman: compression ratio for highly redundant data")
{
    // 1000 одинаковых байтов -- Huffman должен сжать в ~125 байт данных
    // (1 бит на символ) + overhead заголовка.
    // Сжатый размер должен быть значительно меньше оригинала.
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const size_t size = 1000;
    buint8 data[size];
    std::memset(data, 0xBB, size);

    MemoryStream compressed;
    bool ok = compressData(compressor, data, size, compressed);
    BLIB_TEST_REQUIRE(ok);

    // Сжатый размер должен быть < 50% оригинала (в реальности ~15%)
    BLIB_TEST_CHECK(compressed.size() < size / 2);
}

BLIB_TEST_CASE("Huffman: compressed size within compressBound")
{
    // Для различных типов данных: сжатый размер не должен
    // превышать compressBound(inputSize)
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    // Тест 1: текстовые данные
    {
        const char* text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";
        size_t len = std::strlen(text);

        MemoryStream compressed;
        bool ok = compressData(compressor,
            reinterpret_cast<const buint8*>(text), len, compressed);
        BLIB_TEST_REQUIRE(ok);

        buint64 bound = compressor.compressBound(static_cast<buint64>(len));
        BLIB_TEST_CHECK(compressed.size() <= bound);
    }

    // Тест 2: uniform данные (worst case)
    {
        const size_t size = 512;
        buint8 data[size];
        buint32 seed = 0x12345678;
        for (size_t i = 0; i < size; ++i)
        {
            seed = seed * 1103515245 + 12345;
            data[i] = static_cast<buint8>((seed >> 16) & 0xFF);
        }

        MemoryStream compressed;
        bool ok = compressData(compressor, data, size, compressed);
        BLIB_TEST_REQUIRE(ok);

        buint64 bound = compressor.compressBound(static_cast<buint64>(size));
        BLIB_TEST_CHECK(compressed.size() <= bound);
    }

    // Тест 3: пустые данные
    {
        MemoryStream compressed;
        bool ok = compressData(compressor, nullptr, 0, compressed);
        BLIB_TEST_REQUIRE(ok);

        buint64 bound = compressor.compressBound(0);
        BLIB_TEST_CHECK(compressed.size() <= bound);
    }
}

BLIB_TEST_CASE("Huffman: compressBound returns nonzero for all inputs")
{
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    // compressBound всегда > 0 (как минимум есть заголовок)
    BLIB_TEST_CHECK(compressor.compressBound(0) > 0);
    BLIB_TEST_CHECK(compressor.compressBound(1) > 0);
    BLIB_TEST_CHECK(compressor.compressBound(1000000) > 0);

    // compressBound >= inputSize (сжатие не может уменьшить размер
    // бесконечно -- overhead заголовка всегда есть)
    BLIB_TEST_CHECK(compressor.compressBound(100) > 100);
}

// ============================================================
// 6. Huffman: compressBound и validateSettings
// ============================================================

BLIB_TEST_CASE("Huffman: compressBound with blockSize")
{
    // С blockSize > 0 overhead больше (заголовок на каждый блок)
    HuffmanCompressor noBlocks;
    configureDefaultCompressor(noBlocks);
    HuffmanCompressor withBlocks;
    configureBlockCompressor(withBlocks, 64);

    buint64 inputSize = 1024;

    buint64 boundNoBlocks = noBlocks.compressBound(inputSize);
    buint64 boundWithBlocks = withBlocks.compressBound(inputSize);

    // С блоками overhead должен быть больше
    BLIB_TEST_CHECK(boundWithBlocks > boundNoBlocks);
}

BLIB_TEST_CASE("Huffman: validateSettings accepts any settings")
{
    HuffmanCompressor compressor;

    // Дефолтные
    CompressionSettings s1;
    BLIB_TEST_CHECK(compressor.validateSettings(s1));

    // noCompression
    CompressionSettings s2;
    s2.level = CompressionSettings::CompressionLevel::noCompression;
    BLIB_TEST_CHECK(compressor.validateSettings(s2));

    // ultra + большие значения
    CompressionSettings s3;
    s3.level = CompressionSettings::CompressionLevel::ultra;
    s3.blockSize = 1;
    s3.dictionarySize = 999999;
    s3.wordSize = 42;
    s3.threadCount = 16;
    BLIB_TEST_CHECK(compressor.validateSettings(s3));
}

BLIB_TEST_CASE("Huffman: multiple sequential compress/decompress")
{
    // Один объект компрессора используется несколько раз подряд --
    // убеждаемся что состояние не протекает между вызовами.
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    // Первый round-trip: текст
    {
        const char* text = "First compression pass.";
        BLIB_TEST_CHECK(roundTrip(compressor,
            reinterpret_cast<const buint8*>(text), std::strlen(text)));
    }

    // Второй round-trip: бинарные данные
    {
        buint8 data[128];
        for (int i = 0; i < 128; ++i)
            data[i] = static_cast<buint8>(i);
        BLIB_TEST_CHECK(roundTrip(compressor, data, 128));
    }

    // Третий round-trip: один байт
    {
        buint8 data = 0x00;
        BLIB_TEST_CHECK(roundTrip(compressor, &data, 1));
    }

    // Четвёртый round-trip: пустые данные
    {
        BLIB_TEST_CHECK(roundTrip(compressor, nullptr, 0));
    }
}

// ============================================================
// 7. Huffman: стресс/корректность
// ============================================================

BLIB_TEST_CASE("Huffman: round trip various sizes (1 to 4096 bytes)")
{
    // Проверяем корректность на широком диапазоне размеров.
    // Для экономии времени проверяем не все подряд, а с шагом.
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    blib::memory::Allocator alloc;

    // Шаг: 1 байт для малых размеров, потом увеличиваем
    size_t sizes[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
        15, 16, 17, 31, 32, 33, 63, 64, 65,
        100, 127, 128, 129, 255, 256, 257,
        500, 512, 1000, 1023, 1024, 1025,
        2000, 2048, 3000, 4000, 4095, 4096
    };
    size_t numSizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t s = 0; s < numSizes; ++s)
    {
        size_t dataSize = sizes[s];
        buint8* data = static_cast<buint8*>(alloc.allocate(dataSize));
        BLIB_TEST_REQUIRE(data != nullptr);

        // Заполняем детерминированным паттерном, зависящим от размера
        buint32 seed = static_cast<buint32>(dataSize * 7919);
        for (size_t i = 0; i < dataSize; ++i)
        {
            seed = seed * 1103515245 + 12345;
            data[i] = static_cast<buint8>((seed >> 16) & 0xFF);
        }

        bool ok = roundTrip(compressor, data, dataSize);
        alloc.deallocate(data, dataSize);

        BLIB_TEST_CHECK(ok);
        if (!ok)
            break;  // Не засоряем вывод сотнями ошибок
    }
}

BLIB_TEST_CASE("Huffman: decompress truncated data returns false")
{
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    // Создаём валидные сжатые данные
    const char* text = "Some data to compress for truncation test, make it long enough.";
    MemoryStream compressed;
    bool ok = compressData(compressor,
        reinterpret_cast<const buint8*>(text), std::strlen(text), compressed);
    BLIB_TEST_REQUIRE(ok);

    const ByteArray& fullData = compressed.getData();
    BLIB_TEST_REQUIRE(fullData.size() > 20);

    // Обрезаем данные на половине -- decompress должен вернуть false
    size_t truncatedSize = fullData.size() / 2;
    ByteArray truncated(fullData.begin(), fullData.begin() + truncatedSize);

    MemoryStream truncatedStream(std::move(truncated));
    MemoryStream decompressed;
    ok = compressor.decompress(truncatedStream, decompressed);

    BLIB_TEST_CHECK(ok == false);
}

BLIB_TEST_CASE("Huffman: round trip with blockSize and large data")
{
    // Большие данные с блочным режимом -- проверяет что все блоки
    // корректно записываются и читаются последовательно
    HuffmanCompressor compressor;
    configureBlockCompressor(compressor, 1024);

    const size_t size = 8192;  // 8 КБ -> 8 блоков по 1024
    blib::memory::Allocator alloc;
    buint8* data = static_cast<buint8*>(alloc.allocate(size));
    BLIB_TEST_REQUIRE(data != nullptr);

    // Каждый блок имеет свой паттерн -- проверяет независимость деревьев
    for (size_t i = 0; i < size; ++i)
    {
        size_t blockIdx = i / 1024;
        data[i] = static_cast<buint8>((i + blockIdx * 37) % 256);
    }

    bool ok = roundTrip(compressor, data, size);
    alloc.deallocate(data, size);

    BLIB_TEST_CHECK(ok);
}

BLIB_TEST_CASE("Huffman: decompress completely garbage data returns false")
{
    // Случайные мусорные данные -- не могут быть валидным Huffman-потоком
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    buint8 garbage[64];
    buint32 seed = 0xCAFEBABE;
    for (int i = 0; i < 64; ++i)
    {
        seed = seed * 1103515245 + 12345;
        garbage[i] = static_cast<buint8>((seed >> 16) & 0xFF);
    }

    MemoryStream garbageStream;
    garbageStream.write(garbage, sizeof(garbage));
    garbageStream.seek(0, SeekOrigin::Begin);

    MemoryStream decompressed;
    bool ok = compressor.decompress(garbageStream, decompressed);

    BLIB_TEST_CHECK(ok == false);
}

// ============================================================
// 8. Регрессионные тесты на баги из ревью (ожидают фиксов)
// ============================================================

BLIB_TEST_CASE("Huffman: compress to non-seekable output returns false")
{
    // БАГ: compress() требует seekable только от ВХОДНОГО потока,
    // но формат зависит от seek ВЫХОДНОГО: compressedSize и paddingBits
    // записываются плейсхолдерами и патчатся в конце через seek().
    // На non-seekable выходе заголовок остаётся с нулями, поток битый,
    // а compress возвращает true.
    // Ожидаемое поведение после фикса: false (симметрично входу).
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const char* text = "seekless output stream test data";
    MemoryStream inStream;
    inStream.write(text, std::strlen(text));
    inStream.seek(0, SeekOrigin::Begin);

    NonSeekableOutputStream nonSeekable;
    bool ok = compressor.compress(inStream, nonSeekable);

    BLIB_TEST_CHECK(ok == false);
}

BLIB_TEST_CASE("Huffman: noCompression level stores raw data")
{
    // БАГ: документация huffmanCompressor.h обещает, что level=noCompression
    // копирует данные без сжатия, но реализация level не читает вообще.
    // Ожидаемое поведение после фикса: сжатый размер >= размер оригинала
    // (сырые данные + заголовок), round-trip корректен.
    HuffmanCompressor compressor;
    CompressionSettings settings;
    settings.level = CompressionSettings::CompressionLevel::noCompression;
    compressor.setSettings(std::move(settings));

    const size_t size = 1000;
    blib::memory::Allocator alloc;
    buint8* data = static_cast<buint8*>(alloc.allocate(size));
    BLIB_TEST_REQUIRE(data != nullptr);
    std::memset(data, 0xBB, size);

    MemoryStream compressed;
    bool ok = compressData(compressor, data, size, compressed);
    BLIB_TEST_REQUIRE(ok);

    // Без сжатия вывод обязан быть не меньше входа
    BLIB_TEST_CHECK(compressed.size() >= static_cast<buint64>(size));

    // Round-trip обязан работать
    compressed.seek(0, SeekOrigin::Begin);
    MemoryStream decompressed;
    ok = compressor.decompress(compressed, decompressed);
    BLIB_TEST_REQUIRE(ok);

    BLIB_TEST_CHECK(decompressed.size() == static_cast<buint64>(size));
    if (decompressed.size() == static_cast<buint64>(size))
    {
        BLIB_TEST_CHECK(std::memcmp(decompressed.getData().data(), data, size) == 0);
    }

    alloc.deallocate(data, size);
}

BLIB_TEST_CASE("Huffman: compress detects failed header write")
{
    // БАГ: возвращаемые значения write() в compressBlock игнорируются
    // (originalSize, compressedSize, symbolCount, таблица частот, padding).
    // При сбое записи заголовка compress возвращает true с битым потоком.
    // 4-й вызов write() (0-based) -- запись поля compressedSize.
    // Ожидаемое поведение после фикса: false.
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const char* text = "header write failure test";
    MemoryStream inStream;
    inStream.write(text, std::strlen(text));
    inStream.seek(0, SeekOrigin::Begin);

    FailOnNthWriteOutputStream failing(4);
    bool ok = compressor.compress(inStream, failing);

    BLIB_TEST_CHECK(ok == false);
}

BLIB_TEST_CASE("Huffman: compress handles partial writes")
{
    // БАГ: контракт IOutputStream::write допускает частичную запись
    // ("пишет ДО size байт"), но BitWriter::flushByte требует записать
    // весь буфер за один вызов и падает на частичной записи.
    // Ожидаемое поведение после фикса: round-trip через поток,
    // принимающий не более 7 байт за вызов write().
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const char* text = "partial write handling test data, long enough";
    size_t len = std::strlen(text);

    MemoryStream inStream;
    inStream.write(text, len);
    inStream.seek(0, SeekOrigin::Begin);

    ChunkedOutputStream chunked(7);
    bool ok = compressor.compress(inStream, chunked);
    BLIB_TEST_REQUIRE(ok);

    // Декомпрессия накопленных байт и сверка с оригиналом
    MemoryStream compressed(chunked.getData());
    MemoryStream decompressed;
    ok = compressor.decompress(compressed, decompressed);
    BLIB_TEST_REQUIRE(ok);

    BLIB_TEST_CHECK(decompressed.size() == static_cast<buint64>(len));
    if (decompressed.size() == static_cast<buint64>(len))
    {
        BLIB_TEST_CHECK(std::memcmp(decompressed.getData().data(), text, len) == 0);
    }
}

BLIB_TEST_CASE("Huffman: compress handles partial reads from input")
{
    // БАГ: контракт IInputStream::read допускает частичное чтение,
    // но compress() требует, чтобы блок прочитался за ОДИН вызов read().
    // Ожидаемое поведение после фикса: round-trip через поток,
    // отдающий не более 3 байт за вызов read().
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const char* text = "partial read handling test data, long enough";
    size_t len = std::strlen(text);

    ChunkedInputStream chunked(reinterpret_cast<const buint8*>(text), len, 3);
    MemoryStream compressed;
    bool ok = compressor.compress(chunked, compressed);
    BLIB_TEST_REQUIRE(ok);

    compressed.seek(0, SeekOrigin::Begin);
    MemoryStream decompressed;
    ok = compressor.decompress(compressed, decompressed);
    BLIB_TEST_REQUIRE(ok);

    BLIB_TEST_CHECK(decompressed.size() == static_cast<buint64>(len));
    if (decompressed.size() == static_cast<buint64>(len))
    {
        BLIB_TEST_CHECK(std::memcmp(decompressed.getData().data(), text, len) == 0);
    }
}

BLIB_TEST_CASE("Huffman: decompress handles partial reads from input")
{
    // БАГ: decompressBlock читает заголовок и данные одиночными read(),
    // хотя контракт IInputStream допускает частичное чтение.
    // Ожидаемое поведение после фикса: decompress успешен
    // на потоке, отдающем не более 3 байт за вызов read().
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const char* text = "chunked decompress input test";
    size_t len = std::strlen(text);

    MemoryStream compressed;
    bool ok = compressData(compressor, reinterpret_cast<const buint8*>(text), len, compressed);
    BLIB_TEST_REQUIRE(ok);

    const ByteArray& bytes = compressed.getData();
    ChunkedInputStream chunked(bytes.data(), bytes.size(), 3);

    MemoryStream decompressed;
    ok = compressor.decompress(chunked, decompressed);
    BLIB_TEST_REQUIRE(ok);

    BLIB_TEST_CHECK(decompressed.size() == static_cast<buint64>(len));
    if (decompressed.size() == static_cast<buint64>(len))
    {
        BLIB_TEST_CHECK(std::memcmp(decompressed.getData().data(), text, len) == 0);
    }
}

BLIB_TEST_CASE("Huffman: compress handles seekable stream with unknown size")
{
    // БАГ: size() == 0 по контракту IStream означает "размер неизвестен",
    // но compress() считает remainingSize = size() - tell() и при
    // неизвестном размере молча "сжимает" НОЛЬ байт (blockCount = 0),
    // возвращая true с пустым результатом.
    // Ожидаемое поведение после фикса: данные читаются до EOF,
    // round-trip корректен.
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const char* text = "unknown size stream data";
    size_t len = std::strlen(text);

    UnknownSizeSeekableStream unknownSize(reinterpret_cast<const buint8*>(text), len);
    MemoryStream compressed;
    bool ok = compressor.compress(unknownSize, compressed);
    BLIB_TEST_REQUIRE(ok);

    compressed.seek(0, SeekOrigin::Begin);
    MemoryStream decompressed;
    ok = compressor.decompress(compressed, decompressed);
    BLIB_TEST_REQUIRE(ok);

    BLIB_TEST_CHECK(decompressed.size() == static_cast<buint64>(len));
    if (decompressed.size() == static_cast<buint64>(len))
    {
        BLIB_TEST_CHECK(std::memcmp(decompressed.getData().data(), text, len) == 0);
    }
}

BLIB_TEST_CASE("Huffman: decompress rejects corrupted compressedSize")
{
    // Защита от underflow: compressedSize < размер заголовка блока.
    // Сейчас отказ происходит косвенно (allocate гигантского размера
    // возвращает nullptr), после фикса должна быть явная проверка
    // ДО аллокации. Регрессия: сейчас тест уже проходит.
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const char* text = "compressed size corruption";
    MemoryStream compressed;
    bool ok = compressData(compressor,
        reinterpret_cast<const buint8*>(text), std::strlen(text), compressed);
    BLIB_TEST_REQUIRE(ok);

    ByteArray corrupted = compressed.getData();
    BLIB_TEST_REQUIRE(corrupted.size() >= 25);
    // compressedSize лежит по смещению 17..24 (buint64)
    for (size_t i = 17; i < 25; ++i)
        corrupted[i] = 0;

    MemoryStream corruptedStream(std::move(corrupted));
    MemoryStream decompressed;
    ok = compressor.decompress(corruptedStream, decompressed);

    BLIB_TEST_CHECK(ok == false);
}

BLIB_TEST_CASE("Huffman: decompress detects corrupted payload")
{
    // Формат v2: каждый блок содержит CRC-32 (IEEE 802.3) исходных данных.
    // Битые биты в payload при полном дереве (все 256 символов, коды по
    // 8 бит) декодируются в валидные, но НЕВЕРНЫЕ символы -- контрольная
    // сумма обязана поймать расхождение, и decompress возвращает false.
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    // 256 уникальных байт -> полное дерево, коды ровно по 8 бит
    buint8 data[256];
    for (int i = 0; i < 256; ++i)
        data[i] = static_cast<buint8>(i);

    MemoryStream compressed;
    bool ok = compressData(compressor, data, sizeof(data), compressed);
    BLIB_TEST_REQUIRE(ok);

    ByteArray corrupted = compressed.getData();
    // Payload блока начинается после: 9 (global) + 8 + 8 + 2 + 256*5 +
    // 1 (padding) + 4 (crc32) = 1312
    const size_t dataOffset = 9 + 8 + 8 + 2 + 256 * 5 + 1 + 4;
    BLIB_TEST_REQUIRE(corrupted.size() > dataOffset + 128);
    corrupted[dataOffset + 128] ^= 0xFF; // портим байт в середине payload

    MemoryStream corruptedStream(std::move(corrupted));
    MemoryStream decompressed;
    ok = compressor.decompress(corruptedStream, decompressed);

    // CRC-32 обязан детектировать повреждение
    BLIB_TEST_CHECK(ok == false);

    // Страховка: decompress не имеет права вернуть true с данными,
    // отличными от оригинала
    if (ok)
    {
        BLIB_TEST_CHECK(decompressed.size() == static_cast<buint64>(sizeof(data)));
        if (decompressed.size() == static_cast<buint64>(sizeof(data)))
        {
            BLIB_TEST_CHECK(std::memcmp(decompressed.getData().data(), data, sizeof(data)) == 0);
        }
    }
}

BLIB_TEST_CASE("Huffman: decompress rejects corrupted crc32 field")
{
    // Порча самого crc-поля (не данных) обязана давать false:
    // декодированные данные верны, но сверка с битым crc не сойдётся
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const char* text = "crc field corruption check";
    MemoryStream compressed;
    bool ok = compressData(compressor,
        reinterpret_cast<const buint8*>(text), std::strlen(text), compressed);
    BLIB_TEST_REQUIRE(ok);

    ByteArray corrupted = compressed.getData();
    // symbolCount читаем по смещению 25 (buint16, little-endian)
    BLIB_TEST_REQUIRE(corrupted.size() >= 27);
    buint16 symbolCount = static_cast<buint16>(
        corrupted[25] | (static_cast<buint16>(corrupted[26]) << 8));
    BLIB_TEST_REQUIRE(symbolCount > 0);

    // crc32 лежит сразу после paddingBits:
    // 9 + 8 + 8 + 2 + symbolCount*5 + 1
    size_t crcOffset = 9 + 8 + 8 + 2 + static_cast<size_t>(symbolCount) * 5 + 1;
    BLIB_TEST_REQUIRE(crcOffset + 4 <= corrupted.size());
    corrupted[crcOffset] ^= 0xFF; // ломаем первый байт crc32

    MemoryStream corruptedStream(std::move(corrupted));
    MemoryStream decompressed;
    ok = compressor.decompress(corruptedStream, decompressed);

    BLIB_TEST_CHECK(ok == false);
}

BLIB_TEST_CASE("Huffman: noCompression stored block rejects corrupted payload")
{
    // Stored-блок (v2) тоже содержит CRC-32: порча сырого байта данных
    // обязана детектироваться
    HuffmanCompressor compressor;
    configureNoCompressionCompressor(compressor);

    const char* text = "stored block payload corruption";
    MemoryStream compressed;
    bool ok = compressData(compressor,
        reinterpret_cast<const buint8*>(text), std::strlen(text), compressed);
    BLIB_TEST_REQUIRE(ok);

    ByteArray corrupted = compressed.getData();
    // Layout stored-блока: [global 9][origSize 8][compSize 8][symbolCount 2][crc32 4][raw]
    const size_t rawDataOffset = 9 + 8 + 8 + 2 + 4;
    BLIB_TEST_REQUIRE(corrupted.size() > rawDataOffset);
    corrupted[rawDataOffset] ^= 0xFF; // портим первый байт сырых данных

    MemoryStream corruptedStream(std::move(corrupted));
    MemoryStream decompressed;
    ok = compressor.decompress(corruptedStream, decompressed);

    BLIB_TEST_CHECK(ok == false);
}

BLIB_TEST_CASE("Huffman: noCompression stored block rejects corrupted crc32")
{
    // Порча crc-поля stored-блока при корректных сырых данных -- тоже false
    HuffmanCompressor compressor;
    configureNoCompressionCompressor(compressor);

    const char* text = "stored block crc corruption";
    MemoryStream compressed;
    bool ok = compressData(compressor,
        reinterpret_cast<const buint8*>(text), std::strlen(text), compressed);
    BLIB_TEST_REQUIRE(ok);

    ByteArray corrupted = compressed.getData();
    // crc32 stored-блока лежит по смещению 9 + 8 + 8 + 2 = 27
    const size_t crcOffset = 9 + 8 + 8 + 2;
    BLIB_TEST_REQUIRE(crcOffset + 4 <= corrupted.size());
    corrupted[crcOffset + 3] ^= 0x01; // ломаем старший байт crc32

    MemoryStream corruptedStream(std::move(corrupted));
    MemoryStream decompressed;
    ok = compressor.decompress(corruptedStream, decompressed);

    BLIB_TEST_CHECK(ok == false);
}

BLIB_TEST_CASE("Huffman: decompress rejects old format version 1")
{
    // Формат v2 несовместим с v1: потоки версии 1 (без CRC-32)
    // обязаны отклоняться decompress
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    const char* text = "old format version";
    MemoryStream compressed;
    bool ok = compressData(compressor,
        reinterpret_cast<const buint8*>(text), std::strlen(text), compressed);
    BLIB_TEST_REQUIRE(ok);

    ByteArray corrupted = compressed.getData();
    BLIB_TEST_REQUIRE(corrupted.size() >= 5);
    corrupted[4] = 1; // подменяем version на 1 (смещение 4 -- после magic)

    MemoryStream corruptedStream(std::move(corrupted));
    MemoryStream decompressed;
    ok = compressor.decompress(corruptedStream, decompressed);

    BLIB_TEST_CHECK(ok == false);
}

BLIB_TEST_CASE("Huffman: decompress rejects corrupted paddingBits")
{
    // БАГ: decompressBlock не валидировал paddingBits (допустимо 0..7).
    // При значении 8..255 totalBits = encodedDataSize*8 - paddingBits
    // мог дать underflow, после чего цикл декодирования читал
    // encodedData[] далеко за пределами буфера -- UB/OOB-чтение.
    // Фикс: явная проверка paddingBits <= 7 ДО чтения данных;
    // тест гарантирует стабильный false без выхода за границы.
    HuffmanCompressor compressor;
    configureDefaultCompressor(compressor);

    // 10 одинаковых байтов: 1 символ, код 1 бит -> 10 бит данных (2 байта)
    buint8 data[10];
    std::memset(data, 0x77, sizeof(data));

    MemoryStream compressed;
    bool ok = compressData(compressor, data, sizeof(data), compressed);
    BLIB_TEST_REQUIRE(ok);

    ByteArray corrupted = compressed.getData();
    // Layout: [global 9][origSize 8][compSize 8][symbolCount 2][freq 5][padding 1][crc32 4][data]
    // symbolCount читаем по смещению 25 (buint16, little-endian)
    BLIB_TEST_REQUIRE(corrupted.size() >= 27);
    buint16 symbolCount = static_cast<buint16>(
        corrupted[25] | (static_cast<buint16>(corrupted[26]) << 8));
    BLIB_TEST_REQUIRE(symbolCount == 1);

    size_t paddingBitsOffset = 9 + 8 + 8 + 2 + static_cast<size_t>(symbolCount) * 5;
    BLIB_TEST_REQUIRE(paddingBitsOffset + 1 <= corrupted.size());
    corrupted[paddingBitsOffset] = 0xFF; // невалидное значение (должно быть 0..7)

    MemoryStream corruptedStream(std::move(corrupted));
    MemoryStream decompressed;
    ok = compressor.decompress(corruptedStream, decompressed);

    BLIB_TEST_CHECK(ok == false);
}
