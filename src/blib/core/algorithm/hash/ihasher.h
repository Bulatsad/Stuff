#pragma once

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/utilmacro.h>
#include <blib/core/istream.h>
#include <blib/core/ostream.h>
#include <blib/system/memory/allocator.h>

namespace blib
{
    namespace algorithm
    {
        namespace hash
        {
            /**
             * HashSettings - параметры вычисления хеша.
             *
             * Реализации, не использующие конкретный параметр, игнорируют его.
             */
            struct HashSettings
            {
                /**
                 * Аллокатор, через который должны проходить ВСЕ выделения памяти
                 * в реализациях IHasher. Реализации не имеют права выделять
                 * память каким-либо другим способом.
                 */
                blib::memory::Allocator memoryPoolForHashing;
            };

            /**
             * IHasher - интерфейс алгоритма хеширования данных.
             *
             * Контракт:
             * - Входной поток читается от ТЕКУЩЕЙ позиции до конца данных (EOF);
             *   позиционирование (seek) не требуется - подходят и
             *   non-seekable потоки (сокеты, пайпы)
             * - Выходной поток: записывается РОВНО digestSize() сырых байт
             *   дайджеста от текущей позиции. Порядок байт и их значение
             *   определяются конкретным алгоритмом
             * - Все аллокации внутри hash() -- только через
             *   settings.memoryPoolForHashing
             * - При ошибке (ошибка чтения/записи) возвращается false;
             *   содержимое выходного потока при этом не определено
             * - Хешер не имеет состояния между вызовами hash(): один экземпляр
             *   может последовательно хешировать разные потоки
             */
            class __blib_core_api IHasher
            {
            protected:
                HashSettings settings;

            public:
                virtual ~IHasher() {}

                // ---- Настройки (не виртуальные, тривиальная логика) ----

                /**
                 * Установить настройки (move-семантика).
                 *
                 * Настройки ПЕРЕМЕЩАЮТСЯ внутрь хешера: аллокатор
                 * memoryPoolForHashing переносится без копирования
                 * (копирование stateful-аллокатора не поддерживается).
                 * Переданный объект настроек после вызова не валиден
                 * для использования аллокатора.
                 */
                void setSettings(HashSettings&& settings);
                const HashSettings& getSettings() const;

                // ---- Информация об алгоритме ----

                /**
                 * Имя алгоритма для логирования и диагностики.
                 * Например: "MD5", "SHA-1", "SHA-256".
                 */
                virtual const char* algorithmName() const = 0;

                /**
                 * Размер дайджеста в байтах.
                 *
                 * Полезно для предаллокации выходных буферов и проверки
                 * результата hash(). Величина постоянна для алгоритма.
                 *
                 * @return Фиксированный размер дайджеста в байтах
                 */
                virtual buint64 digestSize() const = 0;

                // ---- Основная операция ----

                /**
                 * Вычислить хеш данных из входного потока и записать
                 * сырой дайджест в выходной поток.
                 *
                 * @param is     Источник данных (читается до EOF)
                 * @param digest Приёмник дайджеста (ровно digestSize() байт)
                 * @return true при успешном вычислении
                 */
                virtual bool hash(
                    _In  blib::core::IInputStream&  is,
                    _Out blib::core::IOutputStream& digest) = 0;
            };
        }
    }
}
