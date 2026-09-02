#pragma once

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/utilmacro.h>
#include <blib/core/algorithm/hash/ihasher.h>

namespace blib
{
    namespace algorithm
    {
        namespace hash
        {
            /**
             * Crc32Hasher - реализация IHasher по алгоритму CRC-32 (IEEE 802.3).
             *
             * Алгоритм:
             * - Циклический избыточный код с отражённым полиномом 0xEDB88320
             *   (стандарт IEEE 802.3, тот же, что в Ethernet/gzip/PNG/zlib)
             * - Табличная реализация: таблица 256 32-битных значений,
             *   данные обрабатываются побайтно:
             *     state = table[(state ^ byte) & 0xFF] ^ (state >> 8)
             * - Начальное состояние 0xFFFFFFFF, финал -- xor 0xFFFFFFFF
             *
             * Результат:
             * - Ровно 4 сырых байта дайджеста (digestSize() == 4)
             * - Порядок байт: итоговое 32-битное значение в little-endian
             *   (значение 0xCBF43926 записывается как {0x26, 0x39, 0xF4, 0xCB})
             *
             * Особенности реализации:
             * - Полностью потоковая: обрабатывает данные по мере чтения,
             *   не требует seek (подходят non-seekable потоки)
             * - Не выделяет память вообще (таблица живёт в статической
             *   памяти), поэтому settings.memoryPoolForHashing игнорируется
             * - Детерминирован и не имеет состояния между вызовами hash():
             *   один экземпляр можно использовать многократно
             *
             * Дополнительный буферный API (без потоков), для потребителей,
             * у которых данные уже лежат в памяти (например, HuffmanCompressor):
             * - hashBuffer() -- one-shot CRC-32 буфера
             * - hashInit()/update()/hashFinal() -- инкрементальное вычисление
             *   данных, поступающих частями
             *
             * Ограничения:
             * - CRC-32 не криптографический алгоритм: годится только для
             *   контрольных сумм и обнаружения случайных повреждений,
             *   НЕ для безопасности/подписей
             * - Инициализация таблицы ленивая и не синхронизирована --
             *   класс не thread-safe (как и весь интерфейс IHasher)
             */
            class __blib_core_api Crc32Hasher : public IHasher
            {
            public:
                // ---- IHasher ----

                const char* algorithmName() const __blib_override;
                buint64 digestSize() const __blib_override;

                bool hash(
                    _In  blib::core::IInputStream&  is,
                    _Out blib::core::IOutputStream& digest) __blib_override;

                // ---- Буферный API (без потоков) ----

                /**
                 * Начальное состояние инкрементального вычисления.
                 *
                 * @return Начальное значение регистра (0xFFFFFFFF)
                 */
                static buint32 hashInit();

                /**
                 * Обновить состояние CRC очередной порцией данных.
                 *
                 * @param state Текущее состояние (из hashInit() или предыдущего update())
                 * @param data  Порция данных (не nullptr, если size > 0)
                 * @param size  Размер порции в байтах
                 * @return Новое состояние
                 */
                static buint32 update(
                         buint32 state,
                    _In const buint8* data,
                         size_t size);

                /**
                 * Финализация инкрементального вычисления: xor 0xFFFFFFFF.
                 *
                 * @param state Состояние после всех update()
                 * @return Итоговое значение CRC-32
                 */
                static buint32 hashFinal(buint32 state);

                /**
                 * One-shot CRC-32 буфера.
                 *
                 * @param data Данные (может быть nullptr при size == 0)
                 * @param size Размер данных в байтах
                 * @return Итоговое значение CRC-32
                 */
                static buint32 hashBuffer(
                    _In const buint8* data,
                         size_t size);

            private:
                // Полином CRC-32 IEEE 802.3 (отражённый)
                static const buint32 polynomial = 0xEDB88320u;

                // Размер дайджеста в байтах
                static const buint64 digestLength = 4;

                // Размер буфера чтения входного потока в байтах.
                // Большие чтения уменьшают количество виртуальных вызовов read()
                static const size_t readBufferSize = 4096;

                // Начальное значение регистра (init)
                static const buint32 initValue = 0xFFFFFFFFu;

                // Финальный xor регистра (final)
                static const buint32 finalXor = 0xFFFFFFFFu;

                /**
                 * Таблица CRC-32 (256 элементов). Генерируется лениво при
                 * первом обращении, дальше только читается. Значения
                 * детерминированы: повторная генерация из-за гонки даёт
                 * идентичные байты, поэтому рассинхрон данных невозможен.
                 * Тем не менее формально инициализация не синхронизирована --
                 * класс не thread-safe (см. doc-блок класса).
                 *
                 * @return Указатель на таблицу из 256 значений
                 */
                static const buint32* getTable();
            };
        }
    }
}
