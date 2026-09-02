#include <blib/core/algorithm/hash/crc32Hasher.h>

namespace blib
{
    namespace algorithm
    {
        namespace hash
        {
            // ================================================================
            // IHasher: информация об алгоритме
            // ================================================================

            const char* Crc32Hasher::algorithmName() const
            {
                return "CRC32";
            }

            buint64 Crc32Hasher::digestSize() const
            {
                return digestLength;
            }

            // ================================================================
            // Ядро алгоритма
            // ================================================================

            const buint32* Crc32Hasher::getTable()
            {
                // Таблица генерируется один раз при первом обращении и
                // дальше только читается (см. doc-блок в заголовке).
                // Значения детерминированы, поэтому гонка при первом
                // обращении не приводит к рассинхрону данных; класс
                // в целом не thread-safe
                static buint32 table[256];
                static bool ready = false;

                if (!ready)
                {
                    for (buint32 i = 0; i < 256; ++i)
                    {
                        buint32 c = i;
                        for (buint32 k = 0; k < 8; ++k)
                        {
                            c = (c & 1) ? (polynomial ^ (c >> 1)) : (c >> 1);
                        }
                        table[i] = c;
                    }
                    ready = true;
                }

                return table;
            }

            buint32 Crc32Hasher::hashInit()
            {
                return initValue;
            }

            buint32 Crc32Hasher::update(
                     buint32 state,
                _In const buint8* data,
                     size_t size)
            {
                const buint32* table = getTable();

                // Побайтное табличное обновление регистра
                for (size_t i = 0; i < size; ++i)
                {
                    state = table[(state ^ data[i]) & 0xFF] ^ (state >> 8);
                }

                return state;
            }

            buint32 Crc32Hasher::hashFinal(buint32 state)
            {
                return state ^ finalXor;
            }

            buint32 Crc32Hasher::hashBuffer(_In const buint8* data, size_t size)
            {
                return hashFinal(update(hashInit(), data, size));
            }

            // ================================================================
            // Основная операция
            // ================================================================

            bool Crc32Hasher::hash(
                _In  blib::core::IInputStream&  is,
                _Out blib::core::IOutputStream& digest)
            {
                // Начальное состояние регистра
                buint32 state = hashInit();

                // Буфер чтения на стеке: крупные чтения уменьшают
                // количество виртуальных вызовов read()
                buint8 readBuffer[readBufferSize];

                // Читаем входной поток до EOF, обновляя состояние
                for (;;)
                {
                    size_t got = is.read(readBuffer, sizeof(readBuffer));
                    if (got == 0)
                        break; // EOF

                    state = update(state, readBuffer, got);
                }

                // Итоговое значение CRC-32
                buint32 crc = hashFinal(state);

                // Дайджест: 4 байта little-endian (см. doc-блок класса)
                buint8 out[digestLength];
                for (buint32 i = 0; i < digestLength; ++i)
                {
                    out[i] = static_cast<buint8>(crc >> (8 * i));
                }

                // Поток может принимать данные частями, поэтому пишем
                // в цикле до полного успеха; 0 от write() означает
                // отказ принять данные -- ошибка
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
