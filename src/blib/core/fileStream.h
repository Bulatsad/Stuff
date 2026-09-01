#pragma once

#include <fstream>

#include <blib/blibint.h>
#include <blib/utilmacro.h>
#include <blib/core/bytearray.h>
#include <blib/core/flags.h>
#include <blib/core/istream.h>
#include <blib/core/ostream.h>

namespace blib
{
namespace core
{
    /**
     * FileStatus - результат открытия файла.
     */
    enum class FileStatus : buint8
    {
        CantOpen, // Файл не удалось открыть

        OK        // Файл открыт успешно
    };

    /**
     * OpenMode - флаги режима открытия файла (комбинируются через Flags).
     */
    enum class OpenMode : buint8
    {
        Read = 0x01,     // Открыть для чтения
        Write = 0x02,    // Открыть для записи
        Binary = 0x04,   // Бинарный режим (без флага - текстовый)
        Truncate = 0x08, // Очистить файл при открытии (только с Write)
        Append = 0x10    // Писать в конец (только с Write, сильнее Truncate)
    };

    /**
     * FileStream - поток чтения/записи файла (переработанный blib::core::File).
     * 
     * Назначение:
     * - Единый файловый поток в системе: чтение, запись, позиционирование
     * - Реализует и IInputStream, и IOutputStream (режим задаётся при open)
     * 
     * Внутреннее устройство:
     * - Обёртка над std::filebuf (не std::fstream): у filebuf одна позиция,
     *   нет расхождения get/put указателей; sgetn/sputn/pubskeoff
     * - file объявлен mutable: tell()/size() логически const, но API
     *   std::filebuf не const-friendly
     * - fileSize кэшируется при open и растёт при write/seek - size()
     *   не переставляет позицию файла
     * 
     * Использование:
     *   FileStream f;
     *   FileStream::OpenModeFlags mode;
     *   mode.storage |= static_cast<buint8>(OpenMode::Read);
     *   mode.storage |= static_cast<buint8>(OpenMode::Binary);
     *   if (f.open("data.bin", mode) != FileStatus::OK)
     *       return;
     *   ByteArray all = f.readAll();
     * 
     * Ограничения:
     * - Не thread-safe
     * - seek без ограничения сверху (файл может расти); возвращает false
     *   только при ошибке позиционирования или отрицательной позиции
     * - size() возвращает кэш: размер на момент open + рост от записи.
     *   Внешние изменения файла (другим процессом) не отслеживаются
     */
    class __blib_core_api FileStream : public IInputStream, public IOutputStream
    {
    public:
        typedef Flags<OpenMode> OpenModeFlags;

        /**
         * Конструктор по умолчанию - файл не открыт.
         */
        FileStream();

        /**
         * Деструктор - закрывает файл, если открыт.
         */
        ~FileStream();

        /**
         * Move-конструктор: переносит открытый файл (std::filebuf movable,
         * источник после переноса закрыт).
         */
        FileStream(FileStream&& other) noexcept;

        /**
         * Move-присваивание: закрывает свой файл и забирает чужой.
         */
        FileStream& operator=(FileStream&& other) noexcept;

        // Копирование запрещено: файловый буфер (std::filebuf) некопируем.
        // Для "копии" потока читайте данные и создавайте новый FileStream.
        FileStream(const FileStream&) = delete;
        FileStream& operator=(const FileStream&) = delete;

        /**
         * Открыть файл с заданным режимом.
         * 
         * @param path Путь к файлу
         * @param mode Комбинация флагов OpenMode (Read/Write/Binary/Truncate/Append)
         * @return FileStatus::OK при успехе, FileStatus::CantOpen при ошибке
         * 
         * Правила:
         * - Без Read и без Write: файл не откроется (режим не задан)
         * - Truncate/Append требуют Write и взаимоисключающие (Append сильнее)
         * - Binary по умолчанию ВЫКЛЮЧЕН (текстовый режим)
         */
        FileStatus open(const char* path, OpenModeFlags mode);

        /**
         * Закрыть файл (повторный вызов безопасен).
         */
        void close();

        /**
         * Открыт ли файл.
         */
        bool isOpen() const;

        /**
         * Прочитать весь файл целиком в ByteArray (совместимость со старым File).
         * Позиция потока после вызова - конец файла.
         * 
         * @return Содержимое файла (пустой ByteArray если файл не открыт
         *         для чтения или пуст)
         */
        ByteArray readAll();

        size_t read(_Out void* buffer, size_t size) __blib_override;
        size_t write(_In const void* data, size_t size) __blib_override;

        bool canSeek() const __blib_override;
        bool seek(bint64 offset, SeekOrigin origin) __blib_override;
        buint64 tell() const __blib_override;
        buint64 size() const __blib_override;

    private:
        mutable std::filebuf file; // Буфер файла (mutable: tell/size логически const)
        buint8 readable;           // Открыт для чтения
        buint8 writable;           // Открыт для записи
        buint64 fileSize;          // Кэш размера файла (для size())
    };

} // namespace core
} // namespace blib
