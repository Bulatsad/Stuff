#pragma once

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/core/istream.h>
#include <blib/core/ostream.h>

#include <blib/system/memory/allocator.h>

namespace blib
{
    namespace algorithm
    {
        namespace compression
        {
            /**
             * CompressionSettings - параметры сжатия/декомпрессии.
             *
             * Все числовые параметры со значением 0 означают
             * "на усмотрение реализации" (реализация выбирает оптимальное значение).
             * Реализации, не использующие конкретный параметр, игнорируют его.
             */
            struct CompressionSettings
            {
                /**
                 * Степень сжатия.
                 * Необязательна к исполнению: реализация может поддерживать
                 * только часть уровней или игнорировать их полностью.
                 */
                enum class CompressionLevel
                {
                    noCompression,
                    Speedy,
                    normal,
                    max,
                    ultra
                };

                CompressionLevel level  = CompressionLevel::normal;

                buint64 dictionarySize  = 0;  // размер словаря (для словарных алгоритмов)
                buint64 wordSize        = 0;  // размер слова (для алгоритмов, использующих его)
                buint64 blockSize       = 0;  // размер блока квантования данных
                buint32 threadCount     = 1;  // количество потоков для распараллеливания

                /**
                 * Аллокатор, через который должны проходить ВСЕ выделения памяти
                 * в реализациях ICompressor. Реализации не имеют права выделять
                 * память каким-либо другим способом.
                 */
                blib::memory::Allocator memoryPoolForCompression;
            };

            /**
             * ICompressor - интерфейс алгоритма сжатия/декомпрессии данных.
             *
             * Контракт:
             * - Все аллокации внутри compress/decompress -- только через
             *   settings.memoryPoolForCompression
             * - Входной поток читается от текущей позиции; выходной --
             *   записывается от текущей позиции
             * - Частичные чтения/записи (допустимые по контрактам
             *   IInputStream::read и IOutputStream::write) должны
             *   обрабатываться реализацией
             * - При ошибке возвращается false; состояние потоков не определено
             */
            class __blib_core_api ICompressor
            {
            protected:
                CompressionSettings settings;

            public:
                virtual ~ICompressor() {}

                // ---- Настройки (не виртуальные, тривиальная логика) ----

                /**
                 * Установить настройки (move-семантика).
                 *
                 * Настройки ПЕРЕМЕЩАЮТСЯ внутрь компрессора: аллокатор
                 * memoryPoolForCompression переносится без копирования
                 * (копирование stateful-аллокатора не поддерживается,
                 * см. TODO в AllocatorImplWrapper::share()).
                 * Переданный объект настроек после вызова не валиден
                 * для использования аллокатора.
                 */
                void setSettings(CompressionSettings&& settings);
                const CompressionSettings& getSettings() const;

                // ---- Информация об алгоритме ----

                /**
                 * Имя алгоритма для логирования и диагностики.
                 * Например: "Huffman", "LZ77", "Deflate".
                 */
                virtual const char* algorithmName() const = 0;

                /**
                 * Оценка верхней границы размера сжатых данных (worst-case).
                 *
                 * Полезно для предаллокации выходных буферов.
                 * Возвращает 0 если оценка невозможна для данного алгоритма.
                 *
                 * @param inputSize Размер несжатых данных в байтах
                 * @return Максимальный размер сжатого результата в байтах (или 0)
                 */
                virtual buint64 compressBound(buint64 inputSize) const = 0;

                /**
                 * Проверка допустимости настроек для конкретной реализации.
                 *
                 * Позволяет узнать о несовместимости параметров до начала сжатия.
                 * Реализация проверяет поддерживаемые dictionarySize, blockSize и т.д.
                 *
                 * @param settings Настройки для проверки
                 * @return true если настройки допустимы
                 */
                virtual bool validateSettings(_In const CompressionSettings& settings) const = 0;

                // ---- Основные операции ----

                /**
                 * Сжать данные из входного потока в выходной.
                 *
                 * Реализация может требовать поддержки seek у потоков
                 * (например, двухпроходные алгоритмы); при её отсутствии
                 * compress возвращает false.
                 *
                 * @param decompressedInputStream  Источник несжатых данных
                 * @param compressedOutputStream   Приёмник сжатых данных
                 * @return true при успешном сжатии
                 */
                virtual bool compress(
                    _In  blib::core::IInputStream&  decompressedInputStream,
                    _Out blib::core::IOutputStream& compressedOutputStream) = 0;

                /**
                 * Распаковать данные из входного потока в выходной.
                 *
                 * @param compressedInputStream      Источник сжатых данных
                 * @param decompressedOutputStream   Приёмник распакованных данных
                 * @return true при успешной распаковке
                 */
                virtual bool decompress(
                    _In  blib::core::IInputStream&  compressedInputStream,
                    _Out blib::core::IOutputStream& decompressedOutputStream) = 0;
            };
        }
    }
}
