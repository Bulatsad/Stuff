#pragma once

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/utilmacro.h>
#include <blib/algorithm/hash/ihasher.h>

namespace blib
{
    namespace algorithm
    {
        namespace hash
        {
            /**
             * Md5Hasher - реализация IHasher по алгоритму MD5 (RFC 1321).
             *
             * Алгоритм:
             * - Итеративный дайджест: входные данные обрабатываются блоками
             *   по 64 байта, состояние -- 4 слова по 32 бита (A, B, C, D)
             * - 64 раунда нелинейных преобразований на блок (4 этапа по 16)
             * - В конце добавляется паддинг: 0x80, нули до 56 байт блока
             *   и длина сообщения в битах (64 бита, little-endian)
             *
             * Результат:
             * - Ровно 16 сырых байт дайджеста (digestSize() == 16)
             * - Порядок байт: каждое слово состояния (A, B, C, D) записывается
             *   в little-endian, как предписывает RFC 1321
             *
             * Особенности реализации:
             * - Полностью потоковая: обрабатывает данные по мере чтения,
             *   не требует seek (подходят non-seekable потоки)
             * - Не выделяет память вообще (все буферы на стеке), поэтому
             *   settings.memoryPoolForHashing игнорируется
             * - Детерминирована и не имеет состояния между вызовами hash():
             *   один экземпляр можно использовать многократно
             *
             * Ограничения:
             * - MD5 криптографически устарел (коллизии): использовать
             *   только для контрольных сумм и проверки целостности,
             *   НЕ для безопасности/паролей/подписей
             * - Не thread-safe (как и весь интерфейс IHasher)
             */
            class __blib_api Md5Hasher : public IHasher
            {
            public:
                // ---- IHasher ----

                const char* algorithmName() const __blib_override;
                buint64 digestSize() const __blib_override;

                bool hash(
                    _In  blib::core::IInputStream&  is,
                    _Out blib::core::IOutputStream& digest) __blib_override;

            private:
                // Размер дайджеста MD5 в байтах
                static const buint64 digestLength = 16;

                // Размер блока обработки в байтах
                static const buint32 blockSize = 64;

                // Размер буфера чтения входного потока в байтах.
                // Большие чтения уменьшают количество виртуальных вызовов read()
                static const size_t readBufferSize = 4096;

                // Начальные значения состояния: A, B, C, D (см. RFC 1321, раздел 3.3)
                static const buint32 initState[4];

                // Сдвиги влево для каждого из 64 раундов
                static const buint32 shiftCount[64];

                // Константы раундов K[i] = floor(abs(sin(i + 1)) * 2^32)
                static const buint32 roundConstant[64];

                /**
                 * Md5State - внутреннее состояние потокового вычисления.
                 *
                 * Живёт только в пределах одного вызова hash():
                 * накапливает неполный блок входных данных и суммарную
                 * длину сообщения между чтениями из потока.
                 */
                struct Md5State
                {
                    buint32 words[4];           // текущее состояние дайджеста (A, B, C, D)
                    buint8  buffer[blockSize];  // накопленный неполный блок входных данных
                    buint32 buffered;           // сколько байт накоплено в buffer (0..63)
                    buint64 totalLength;        // суммарная длина данных в байтах (mod 2^64)
                };

                /**
                 * Обработка одного полного 64-байтного блока.
                 *
                 * @param state Текущее состояние дайджеста (A, B, C, D),
                 *              обновляется по месту
                 * @param block Указатель на 64 байта данных блока
                 */
                static void processBlock(
                    _In _Out buint32 state[4],
                    _In     const buint8* block);

                /**
                 * Финализация: паддинг (0x80 + нули + длина в битах),
                 * обработка последних блоков и запись дайджеста в поток.
                 *
                 * @param state  Состояние вычисления с накопленным остатком данных
                 * @param digest Выходной поток (получит ровно digestLength байт)
                 * @return true при успешной записи дайджеста
                 */
                static bool finalize(
                    _In _Out Md5State& state,
                    _Out    blib::core::IOutputStream& digest);
            };
        }
    }
}
