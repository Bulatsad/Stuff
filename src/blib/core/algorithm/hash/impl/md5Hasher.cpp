#include <blib/core/algorithm/hash/md5Hasher.h>

#include <cstring>

namespace blib
{
    namespace algorithm
    {
        namespace hash
        {
            // ================================================================
            // Константы
            // ================================================================

            // Начальное состояние (RFC 1321, раздел 3.3):
            // A = 0x67452301, B = 0xefcdab89, C = 0x98badcfe, D = 0x10325476
            const buint32 Md5Hasher::initState[4] =
            {
                0x67452301u,
                0xefcdab89u,
                0x98badcfeu,
                0x10325476u
            };

            // Сдвиги влево по раундам:
            // раунды  0..15: 7, 12, 17, 22 (повтор 4 раза)
            // раунды 16..31: 5,  9, 14, 20
            // раунды 32..47: 4, 11, 16, 23
            // раунды 48..63: 6, 10, 15, 21
            const buint32 Md5Hasher::shiftCount[64] =
            {
                7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
            };

            // K[i] = floor(abs(sin(i + 1)) * 2^32), i = 0..63 (RFC 1321, раздел 3.4)
            const buint32 Md5Hasher::roundConstant[64] =
            {
                0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
                0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
                0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
                0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
                0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
                0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
                0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
                0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
                0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
                0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
                0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
                0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
                0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
                0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
                0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
                0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u
            };

            // ================================================================
            // Вспомогательные функции
            // ================================================================

            /**
             * Циклический сдвиг 32-битного слова влево.
             * shift обязан быть в диапазоне 1..31 (иначе UB).
             */
            static buint32 leftRotate(buint32 value, buint32 shift)
            {
                return (value << shift) | (value >> (32 - shift));
            }

            // ================================================================
            // IHasher: информация об алгоритме
            // ================================================================

            const char* Md5Hasher::algorithmName() const
            {
                return "MD5";
            }

            buint64 Md5Hasher::digestSize() const
            {
                return digestLength;
            }

            // ================================================================
            // Основная операция
            // ================================================================

            bool Md5Hasher::hash(
                _In  blib::core::IInputStream&  is,
                _Out blib::core::IOutputStream& digest)
            {
                // Инициализация состояния (RFC 1321, раздел 3.3)
                Md5State state;
                state.words[0] = initState[0];
                state.words[1] = initState[1];
                state.words[2] = initState[2];
                state.words[3] = initState[3];
                state.buffered = 0;
                state.totalLength = 0;

                // Буфер чтения на стеке: крупные чтения уменьшают
                // количество виртуальных вызовов read()
                buint8 readBuffer[readBufferSize];

                // Читаем входной поток до EOF и обрабатываем данные блоками.
                // Неполный блок накапливается в state.buffer до следующего чтения.
                for (;;)
                {
                    size_t got = is.read(readBuffer, sizeof(readBuffer));
                    if (got == 0)
                        break; // EOF

                    state.totalLength += got;

                    const buint8* p = readBuffer;
                    size_t left = got;

                    // Доводим накопленный остаток до полного блока
                    if (state.buffered > 0)
                    {
                        size_t need = blockSize - state.buffered;
                        size_t take = need < left ? need : left;
                        std::memcpy(state.buffer + state.buffered, p, take);
                        state.buffered += static_cast<buint32>(take);
                        p += take;
                        left -= take;

                        if (state.buffered == blockSize)
                        {
                            processBlock(state.words, state.buffer);
                            state.buffered = 0;
                        }
                    }

                    // Обрабатываем полные блоки напрямую из буфера чтения
                    while (left >= blockSize)
                    {
                        processBlock(state.words, p);
                        p += blockSize;
                        left -= blockSize;
                    }

                    // Сохраняем хвост в накопительный буфер
                    if (left > 0)
                    {
                        std::memcpy(state.buffer, p, left);
                        state.buffered = static_cast<buint32>(left);
                    }
                }

                return finalize(state, digest);
            }

            // ================================================================
            // Обработка блока
            // ================================================================

            void Md5Hasher::processBlock(
                _In _Out buint32 state[4],
                _In     const buint8* block)
            {
                // Разворачиваем 16 слов блока (little-endian).
                // Явная побайтовая сборка не зависит от endianness платформы.
                buint32 m[16];
                for (buint32 i = 0; i < 16; ++i)
                {
                    m[i] =  static_cast<buint32>(block[i * 4 + 0])
                         | (static_cast<buint32>(block[i * 4 + 1]) << 8)
                         | (static_cast<buint32>(block[i * 4 + 2]) << 16)
                         | (static_cast<buint32>(block[i * 4 + 3]) << 24);
                }

                buint32 a = state[0];
                buint32 b = state[1];
                buint32 c = state[2];
                buint32 d = state[3];

                // 64 раунда: 4 этапа по 16 раундов с разными нелинейными
                // функциями и разной индексацией слова сообщения g
                for (buint32 i = 0; i < 64; ++i)
                {
                    buint32 f;
                    buint32 g;

                    if (i < 16)
                    {
                        // Этап 1: F(x, y, z) = (x & y) | (~x & z)
                        f = (b & c) | (~b & d);
                        g = i;
                    }
                    else if (i < 32)
                    {
                        // Этап 2: G(x, y, z) = (x & z) | (y & ~z)
                        f = (d & b) | (~d & c);
                        g = (5 * i + 1) % 16;
                    }
                    else if (i < 48)
                    {
                        // Этап 3: H(x, y, z) = x ^ y ^ z
                        f = b ^ c ^ d;
                        g = (3 * i + 5) % 16;
                    }
                    else
                    {
                        // Этап 4: I(x, y, z) = y ^ (x | ~z)
                        f = c ^ (b | ~d);
                        g = (7 * i) % 16;
                    }

                    // Сдвиг регистров: d <- c, c <- b, b <- b + f(...), a <- d
                    buint32 temp = d;
                    d = c;
                    c = b;
                    b += leftRotate(a + f + roundConstant[i] + m[g], shiftCount[i]);
                    a = temp;
                }

                // Прибавляем результат блока к состоянию
                state[0] += a;
                state[1] += b;
                state[2] += c;
                state[3] += d;
            }

            // ================================================================
            // Финализация
            // ================================================================

            bool Md5Hasher::finalize(
                _In _Out Md5State& state,
                _Out    blib::core::IOutputStream& digest)
            {
                // Длина сообщения в битах (mod 2^64).
                // Переполнение buint64 при умножении на 8 -- ожидаемое
                // поведение для сообщений длиннее 2^61 байт (по RFC)
                buint64 bitLength = state.totalLength * 8;

                // Первый байт паддинга: 0x80
                state.buffer[state.buffered++] = 0x80;

                // Дополняем нулями до 56 байт блока (последние 8 байт -- длина).
                // Если 0x80 не помещается в текущий блок (buffered > 56),
                // обрабатываем блок и начинаем паддинг в новом
                if (state.buffered > 56)
                {
                    std::memset(state.buffer + state.buffered, 0, blockSize - state.buffered);
                    processBlock(state.words, state.buffer);
                    state.buffered = 0;
                }
                std::memset(state.buffer + state.buffered, 0, 56 - state.buffered);

                // Длина сообщения в битах, little-endian (8 байт)
                for (buint32 i = 0; i < 8; ++i)
                {
                    state.buffer[56 + i] = static_cast<buint8>(bitLength >> (8 * i));
                }

                // Обрабатываем последний блок
                processBlock(state.words, state.buffer);

                // Собираем дайджест: каждое слово состояния в little-endian
                buint8 out[digestLength];
                for (buint32 i = 0; i < 4; ++i)
                {
                    out[i * 4 + 0] = static_cast<buint8>(state.words[i] >> 0);
                    out[i * 4 + 1] = static_cast<buint8>(state.words[i] >> 8);
                    out[i * 4 + 2] = static_cast<buint8>(state.words[i] >> 16);
                    out[i * 4 + 3] = static_cast<buint8>(state.words[i] >> 24);
                }

                // Записываем дайджест в поток. Поток может принимать данные
                // частями, поэтому пишем в цикле до полного успеха;
                // 0 от write() означает отказ принять данные -- ошибка
                size_t written = 0;
                while (written < digestLength)
                {
                    size_t n = digest.write(out + written, digestLength - written);
                    if (n == 0)
                        return false;
                    written += n;
                }

                return true;
            }
        }
    }
}
