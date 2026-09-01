#include <blib/test/src/test.h>

// Hash: интерфейс и реализация MD5
#include <blib/core/algorithm/hash/ihasher.h>
#include <blib/core/algorithm/hash/md5Hasher.h>

// Потоки: MemoryStream для in-memory хеширования
#include <blib/core/istream.h>
#include <blib/core/ostream.h>
#include <blib/core/memoryStream.h>

#include <cmath>
#include <cstring>
#include <vector>

using namespace blib::core;
using namespace blib::algorithm::hash;

// ============================================================
// Вспомогательные функции
// ============================================================

namespace
{
    /**
     * Хелпер: вычислить хеш данных через IHasher и вернуть результат.
     *
     * @param hasher    Настроенный хешер
     * @param data      Исходные данные
     * @param dataSize  Размер данных
     * @param outStream [out] Выходной поток с дайджестом
     * @return true при успешном хешировании
     */
    bool hashData(
        IHasher& hasher,
        const buint8* data,
        size_t dataSize,
        MemoryStream& outStream)
    {
        MemoryStream inStream;
        if (dataSize > 0)
            inStream.write(data, dataSize);
        inStream.seek(0, SeekOrigin::Begin);

        return hasher.hash(inStream, outStream);
    }

    /**
     * Хелпер: проверить что хеш данных совпадает с ожидаемой hex-строкой.
     * Hex-строка должна задавать ровно 16 байт (32 символа).
     *
     * @param hasher   Настроенный хешер
     * @param data     Исходные данные
     * @param dataSize Размер данных
     * @param hex      Ожидаемый дайджест в hex (lowercase)
     * @return true при совпадении
     */
    bool expectHashHex(
        IHasher& hasher,
        const buint8* data,
        size_t dataSize,
        const char* hex)
    {
        // Переводим hex-строку в байты
        buint8 expected[16];
        for (size_t i = 0; i < 16; ++i)
        {
            auto nibble = [](char c) -> buint8
            {
                if (c >= '0' && c <= '9') return static_cast<buint8>(c - '0');
                if (c >= 'a' && c <= 'f') return static_cast<buint8>(c - 'a' + 10);
                return 0xFF;
            };

            buint8 hi = nibble(hex[i * 2]);
            buint8 lo = nibble(hex[i * 2 + 1]);
            if (hi == 0xFF || lo == 0xFF)
                return false; // некорректная hex-строка
            expected[i] = static_cast<buint8>((hi << 4) | lo);
        }

        MemoryStream outStream;
        if (!hashData(hasher, data, dataSize, outStream))
            return false;

        // Проверяем размер и содержимое дайджеста
        const ByteArray& got = outStream.getData();
        if (got.size() != 16)
            return false;
        return std::memcmp(got.data(), expected, 16) == 0;
    }

    /**
     * Эталонная one-shot реализация MD5 для кросс-проверки.
     *
     * Отличается от боевой реализации структурно: сообщение целиком
     * упаковывается в один буфер с паддингом и обрабатывается полными
     * блоками, а константы K вычисляются из определения
     * floor(abs(sin(i + 1)) * 2^32), а не берутся из таблицы.
     * Это ловит ошибки потоковой буферизации боевой реализации.
     *
     * @param data Исходные данные (может быть nullptr при size == 0)
     * @param size Размер данных
     * @param out  [out] 16 байт дайджеста
     */
    void md5Reference(const buint8* data, size_t size, buint8 out[16])
    {
        // Константы раундов из определения (независимо от боевой реализации)
        buint32 k[64];
        for (buint32 i = 0; i < 64; ++i)
        {
            double s = std::sin(static_cast<double>(i + 1));
            k[i] = static_cast<buint32>(std::fabs(s) * 4294967296.0);
        }

        // Сдвиги по раундам
        const buint32 shifts[64] =
        {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
        };

        // Упаковка сообщения с паддингом в один буфер:
        // данные + 0x80 + нули + длина в битах (8 байт, little-endian)
        buint64 bitLen = static_cast<buint64>(size) * 8;
        size_t paddedSize = ((size + 1 + 8 + 63) / 64) * 64;
        std::vector<buint8> msg(paddedSize, 0);
        if (size > 0)
            std::memcpy(msg.data(), data, size);
        msg[size] = 0x80;
        for (buint32 i = 0; i < 8; ++i)
            msg[paddedSize - 8 + i] = static_cast<buint8>(bitLen >> (8 * i));

        // Обработка полных блоков
        buint32 a0 = 0x67452301u;
        buint32 b0 = 0xefcdab89u;
        buint32 c0 = 0x98badcfeu;
        buint32 d0 = 0x10325476u;

        for (size_t off = 0; off < paddedSize; off += 64)
        {
            buint32 m[16];
            for (buint32 i = 0; i < 16; ++i)
            {
                m[i] =  static_cast<buint32>(msg[off + i * 4 + 0])
                     | (static_cast<buint32>(msg[off + i * 4 + 1]) << 8)
                     | (static_cast<buint32>(msg[off + i * 4 + 2]) << 16)
                     | (static_cast<buint32>(msg[off + i * 4 + 3]) << 24);
            }

            buint32 a = a0, b = b0, c = c0, d = d0;
            for (buint32 i = 0; i < 64; ++i)
            {
                buint32 f;
                buint32 g;
                if (i < 16)           { f = (b & c) | (~b & d); g = i; }
                else if (i < 32)      { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
                else if (i < 48)      { f = b ^ c ^ d;          g = (3 * i + 5) & 15; }
                else                  { f = c ^ (b | ~d);       g = (7 * i) & 15; }

                buint32 temp = d;
                d = c;
                c = b;
                buint32 sum = a + f + k[i] + m[g];
                b += (sum << shifts[i]) | (sum >> (32 - shifts[i]));
                a = temp;
            }

            a0 += a;
            b0 += b;
            c0 += c;
            d0 += d;
        }

        // Вывод в little-endian
        const buint32 words[4] = { a0, b0, c0, d0 };
        for (buint32 i = 0; i < 4; ++i)
        {
            out[i * 4 + 0] = static_cast<buint8>(words[i] >> 0);
            out[i * 4 + 1] = static_cast<buint8>(words[i] >> 8);
            out[i * 4 + 2] = static_cast<buint8>(words[i] >> 16);
            out[i * 4 + 3] = static_cast<buint8>(words[i] >> 24);
        }
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
     * Output stream с ограничением размера одной записи --
     * эмулирует приёмник, принимающий данные только малыми частями.
     * Позволяет проверить цикл записи в finalize().
     */
    class PartialWriteOutputStream : public IOutputStream
    {
    public:
        explicit PartialWriteOutputStream(MemoryStream& target, size_t maxChunk)
            : target(target)
            , maxChunk(maxChunk)
        {
        }

        size_t write(_In const void* data, size_t size) __blib_override
        {
            // Пишем не больше maxChunk байт за вызов
            size_t take = size < this->maxChunk ? size : this->maxChunk;
            return this->target.write(data, take);
        }

        bool    canSeek() const __blib_override { return this->target.canSeek(); }
        bool    seek(bint64 offset, SeekOrigin origin) __blib_override { return this->target.seek(offset, origin); }
        buint64 tell() const __blib_override { return this->target.tell(); }
        buint64 size() const __blib_override { return this->target.size(); }

    private:
        MemoryStream& target;
        size_t maxChunk;
    };

    /**
     * Output stream, отказывающийся принимать данные (write всегда 0) --
     * для проверки обработки ошибки записи.
     */
    class FailingOutputStream : public IOutputStream
    {
    public:
        size_t write(_In const void*, size_t) __blib_override { return 0; }

        bool    canSeek() const __blib_override { return false; }
        bool    seek(bint64, SeekOrigin) __blib_override { return false; }
        buint64 tell() const __blib_override { return 0; }
        buint64 size() const __blib_override { return 0; }
    };
}

// ============================================================
// 1. IHasher interface: базовые тесты интерфейса
// ============================================================

BLIB_TEST_CASE("IHasher: algorithmName returns MD5")
{
    Md5Hasher hasher;
    const char* name = hasher.algorithmName();

    BLIB_TEST_REQUIRE(name != nullptr);
    BLIB_TEST_CHECK(std::strcmp(name, "MD5") == 0);
}

BLIB_TEST_CASE("IHasher: digestSize returns 16")
{
    Md5Hasher hasher;
    BLIB_TEST_CHECK(hasher.digestSize() == 16);
}

BLIB_TEST_CASE("IHasher: setSettings/getSettings works")
{
    Md5Hasher hasher;

    // Move-семантика настроек не должна ломать хеширование
    hasher.setSettings(HashSettings());
    const HashSettings& got = hasher.getSettings();
    (void)got;

    // После setSettings хешер должен работать как обычно
    BLIB_TEST_CHECK(expectHashHex(hasher, reinterpret_cast<const buint8*>("abc"), 3,
        "900150983cd24fb0d6963f7d28e17f72"));
}

// ============================================================
// 2. MD5: известные векторы (RFC 1321)
// ============================================================

BLIB_TEST_CASE("MD5: RFC 1321 test vectors")
{
    Md5Hasher hasher;

    struct Vector
    {
        const char* data;
        const char* hex;
    };

    // Официальные тестовые векторы RFC 1321 (раздел A.5)
    const Vector vectors[] =
    {
        { "",                                                          "d41d8cd98f00b204e9800998ecf8427e" },
        { "a",                                                         "0cc175b9c0f1b6a831c399e269772661" },
        { "abc",                                                       "900150983cd24fb0d6963f7d28e17f72" },
        { "message digest",                                            "f96b697d7cb7938d525a2f31aaf161d0" },
        { "abcdefghijklmnopqrstuvwxyz",                                "c3fcd3d76192e4007dfb496cca67e13b" },
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
                                                                       "d174ab98d277d9f5a5611c2c9f419d9f" },
        { "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                                                                       "57edf4a22be3c955ac49da2e2107b67a" }
    };

    for (const Vector& v : vectors)
    {
        BLIB_TEST_CHECK(expectHashHex(hasher,
            reinterpret_cast<const buint8*>(v.data), std::strlen(v.data), v.hex));
    }
}

BLIB_TEST_CASE("MD5: empty input")
{
    Md5Hasher hasher;
    MemoryStream out;

    BLIB_TEST_CHECK(hashData(hasher, nullptr, 0, out));
    const ByteArray& got = out.getData();
    BLIB_TEST_CHECK(got.size() == 16);

    const buint8 expected[16] =
    {
        0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
        0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e
    };
    BLIB_TEST_CHECK(std::memcmp(got.data(), expected, 16) == 0);
}

BLIB_TEST_CASE("MD5: million 'a' characters")
{
    // Известный вектор: MD5("a" * 1 000 000) == 7707d6ae4e027c70eea2a935c2296f21
    std::vector<buint8> data(1000000, 'a');
    Md5Hasher hasher;

    BLIB_TEST_CHECK(expectHashHex(hasher, data.data(), data.size(),
        "7707d6ae4e027c70eea2a935c2296f21"));
}

// ============================================================
// 3. MD5: потоковые особенности
// ============================================================

BLIB_TEST_CASE("MD5: hash from current stream position")
{
    Md5Hasher hasher;

    // "prefix-abc": хешируем только хвост "abc" с позиции 7
    const char* full = "prefix-abc";
    MemoryStream in;
    in.write(full, std::strlen(full));
    BLIB_TEST_REQUIRE(in.seek(7, SeekOrigin::Begin));

    MemoryStream out;
    BLIB_TEST_REQUIRE(hasher.hash(in, out));

    const ByteArray& got = out.getData();
    BLIB_TEST_REQUIRE(got.size() == 16);

    const buint8 expected[16] =
    {
        0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0,
        0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72
    };
    BLIB_TEST_CHECK(std::memcmp(got.data(), expected, 16) == 0);
}

BLIB_TEST_CASE("MD5: non-seekable input stream")
{
    Md5Hasher hasher;
    NonSeekableInputStream in(
        reinterpret_cast<const buint8*>("message digest"), 14);

    MemoryStream out;
    BLIB_TEST_REQUIRE(hasher.hash(in, out));

    const ByteArray& got = out.getData();
    BLIB_TEST_REQUIRE(got.size() == 16);

    const buint8 expected[16] =
    {
        0xf9, 0x6b, 0x69, 0x7d, 0x7c, 0xb7, 0x93, 0x8d,
        0x52, 0x5a, 0x2f, 0x31, 0xaa, 0xf1, 0x61, 0xd0
    };
    BLIB_TEST_CHECK(std::memcmp(got.data(), expected, 16) == 0);
}

BLIB_TEST_CASE("MD5: deterministic across instances and repeated calls")
{
    const buint8 data[] = "some deterministic test data";

    Md5Hasher hasher1;
    Md5Hasher hasher2;

    MemoryStream a1, a2, a3;
    BLIB_TEST_REQUIRE(hashData(hasher1, data, sizeof(data) - 1, a1));
    BLIB_TEST_REQUIRE(hashData(hasher2, data, sizeof(data) - 1, a2));

    // Повторный вызов того же экземпляра после ДРУГОГО сообщения:
    // хешер не должен хранить состояние между вызовами
    BLIB_TEST_REQUIRE(hashData(hasher1,
        reinterpret_cast<const buint8*>("different"), 9, a3));
    MemoryStream a4;
    BLIB_TEST_REQUIRE(hashData(hasher1, data, sizeof(data) - 1, a4));

    const ByteArray& b1 = a1.getData();
    const ByteArray& b2 = a2.getData();
    const ByteArray& b3 = a3.getData();
    const ByteArray& b4 = a4.getData();

    BLIB_TEST_REQUIRE(b1.size() == 16);
    BLIB_TEST_CHECK(std::memcmp(b1.data(), b2.data(), 16) == 0);
    BLIB_TEST_CHECK(std::memcmp(b1.data(), b4.data(), 16) == 0);
    BLIB_TEST_CHECK(std::memcmp(b1.data(), b3.data(), 16) != 0);
}

BLIB_TEST_CASE("MD5: partial-write output stream")
{
    Md5Hasher hasher;
    MemoryStream in;
    in.write(reinterpret_cast<const buint8*>("abc"), 3);
    in.seek(0, SeekOrigin::Begin);

    // Приёмник принимает максимум 3 байта за вызов write()
    MemoryStream target;
    PartialWriteOutputStream out(target, 3);

    BLIB_TEST_REQUIRE(hasher.hash(in, out));

    const ByteArray& got = target.getData();
    BLIB_TEST_REQUIRE(got.size() == 16);

    const buint8 expected[16] =
    {
        0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0,
        0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72
    };
    BLIB_TEST_CHECK(std::memcmp(got.data(), expected, 16) == 0);
}

BLIB_TEST_CASE("MD5: failing output stream returns false")
{
    Md5Hasher hasher;
    MemoryStream in;
    in.write(reinterpret_cast<const buint8*>("abc"), 3);
    in.seek(0, SeekOrigin::Begin);

    FailingOutputStream out;
    BLIB_TEST_CHECK(hasher.hash(in, out) == false);
}

// ============================================================
// 4. MD5: кросс-проверка с эталонной one-shot реализацией
// ============================================================

BLIB_TEST_CASE("MD5: boundary lengths vs one-shot reference")
{
    Md5Hasher hasher;

    // Длины вокруг границ блоков (0..130) и несколько крупных
    const size_t lengths[] =
    {
        0, 1, 2, 3, 54, 55, 56, 57, 62, 63, 64, 65, 118, 119, 120,
        121, 126, 127, 128, 129, 130, 1000, 4095, 4096, 4097, 65536
    };

    for (size_t len : lengths)
    {
        // Детерминированные данные: байт зависит от индекса и длины
        std::vector<buint8> data(len);
        for (size_t i = 0; i < len; ++i)
            data[i] = static_cast<buint8>((i * 37 + len * 11) & 0xFF);

        buint8 expected[16];
        md5Reference(data.data(), data.size(), expected);

        MemoryStream out;
        if (!hashData(hasher, data.data(), data.size(), out))
        {
            BLIB_TEST_CHECK(false);
            continue;
        }

        const ByteArray& got = out.getData();
        if (got.size() != 16)
        {
            BLIB_TEST_CHECK(false);
            continue;
        }

        BLIB_TEST_CHECK(std::memcmp(got.data(), expected, 16) == 0);
    }
}

BLIB_TEST_CASE("MD5: large data (1 MB) vs one-shot reference")
{
    // Псевдослучайные данные без повторов в пределах блока
    std::vector<buint8> data(1024 * 1024);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<buint8>((i * 2654435761u) >> 24);

    buint8 expected[16];
    md5Reference(data.data(), data.size(), expected);

    Md5Hasher hasher;
    MemoryStream out;
    BLIB_TEST_REQUIRE(hashData(hasher, data.data(), data.size(), out));

    const ByteArray& got = out.getData();
    BLIB_TEST_REQUIRE(got.size() == 16);
    BLIB_TEST_CHECK(std::memcmp(got.data(), expected, 16) == 0);
}

BLIB_TEST_CASE("MD5: output size is exactly digestSize() for any input")
{
    Md5Hasher hasher;

    std::vector<buint8> data(100000);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<buint8>(i & 0xFF);

    MemoryStream out;
    BLIB_TEST_REQUIRE(hashData(hasher, data.data(), data.size(), out));
    BLIB_TEST_CHECK(out.getData().size() == static_cast<size_t>(hasher.digestSize()));
}
