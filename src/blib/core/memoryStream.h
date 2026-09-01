#pragma once

#include <cstring>
#include <utility>

#include <blib/blibint.h>
#include <blib/inline.h>
#include <blib/utilmacro.h>
#include <blib/core/bytearray.h>
#include <blib/core/istream.h>
#include <blib/core/ostream.h>

namespace blib
{
namespace core
{
    /**
     * MemoryStream - двунаправленный поток поверх байтового буфера в памяти.
     * 
     * Назначение:
     * - Чтение и запись в растущий ByteArray (std::vector<buint8>)
     * - Замена временных файлов: сериализация в память, сжатие в буфер и т.п.
     * 
     * Поведение:
     * - read: читает до конца данных; 0 при EOF
     * - write: пишет с текущей позиции, при необходимости буфер растёт
     * - seek: строгое позиционирование в пределах [0, size]; при выходе
     *   за пределы возвращает false и не меняет позицию
     * - Позиция общая для чтения и записи
     * 
     * Использование:
     *   MemoryStream s;
     *   s.write("hello", 5);
     *   s.seek(0, SeekOrigin::Begin);
     *   buint8 buf[5];
     *   s.read(buf, 5);            // "hello"
     *   ByteArray bytes = s.release(); // забрать буфер
     * 
     * Ограничения:
     * - Не thread-safe
     * - Данные хранятся в std::vector (ByteArray) - аллокации идут через
     *   std::allocator, а не через blib::memory::GlobalAllocator
     */
    class __blib_core_api MemoryStream : public IInputStream, public IOutputStream
    {
    public:
        /**
         * Конструктор по умолчанию - пустой поток (size == 0).
         */
        MemoryStream();

        /**
         * Конструктор от существующего буфера (копия данных).
         * Позиция - 0.
         */
        explicit MemoryStream(const ByteArray& buffer);

        /**
         * Конструктор от существующего буфера (перенос владения).
         * Позиция - 0.
         */
        explicit MemoryStream(ByteArray&& buffer);

        // Поток содержит std::vector - перемещение корректно (перемещение буфера)
        MemoryStream(MemoryStream&& other) noexcept;
        MemoryStream& operator=(MemoryStream&& other) noexcept;
        MemoryStream(const MemoryStream& other);
        MemoryStream& operator=(const MemoryStream& other);

        size_t read(_Out void* buffer, size_t size) __blib_override;
        size_t write(_In const void* data, size_t size) __blib_override;

        bool canSeek() const __blib_override;
        bool seek(bint64 offset, SeekOrigin origin) __blib_override;
        buint64 tell() const __blib_override;
        buint64 size() const __blib_override;

        /**
         * Константный доступ к буферу.
         */
        const ByteArray& getData() const;

        /**
         * Забрать буфер (перемещение наружу). Поток после этого пуст, позиция 0.
         */
        ByteArray release();

        /**
         * Очистить буфер и сбросить позицию в 0.
         */
        void clear();

    private:
        ByteArray data; // Буфер данных
        buint64 pos;    // Текущая позиция (общая для чтения и записи)
    };

    __blib_inline MemoryStream::MemoryStream()
        : pos(0)
    {
    }

    __blib_inline MemoryStream::MemoryStream(const ByteArray& buffer)
        : data(buffer)
        , pos(0)
    {
    }

    __blib_inline MemoryStream::MemoryStream(ByteArray&& buffer)
        : data(std::move(buffer))
        , pos(0)
    {
    }

    __blib_inline MemoryStream::MemoryStream(MemoryStream&& other) noexcept
        : data(std::move(other.data))
        , pos(other.pos)
    {
        // Позицию исходника сбрасываем, т.к. буфер переехал
        other.pos = 0;
    }

    __blib_inline MemoryStream& MemoryStream::operator=(MemoryStream&& other) noexcept
    {
        if (this != &other)
        {
            this->data = std::move(other.data);
            this->pos = other.pos;
            other.pos = 0;
        }
        return *this;
    }

    __blib_inline MemoryStream::MemoryStream(const MemoryStream& other)
        : data(other.data)
        , pos(other.pos)
    {
    }

    __blib_inline MemoryStream& MemoryStream::operator=(const MemoryStream& other)
    {
        if (this != &other)
        {
            this->data = other.data;
            this->pos = other.pos;
        }
        return *this;
    }

    __blib_inline size_t MemoryStream::read(_Out void* buffer, size_t size)
    {
        if (!buffer || !size)
            return 0;

        // Доступный остаток от текущей позиции (инвариант: pos <= size)
        size_t posSz = static_cast<size_t>(this->pos);
        size_t available = this->data.size() - posSz;
        size_t toRead = size < available ? size : available;

        if (toRead)
        {
            std::memcpy(buffer, &this->data[posSz], toRead);
            this->pos += toRead;
        }
        return toRead;
    }

    __blib_inline size_t MemoryStream::write(_In const void* data_, size_t size)
    {
        if (!data_ || !size)
            return 0;

        size_t endPos = static_cast<size_t>(this->pos) + size;

        // Буфер растёт при необходимости (write за текущим размером)
        if (endPos > this->data.size())
            this->data.resize(endPos);

        std::memcpy(&this->data[static_cast<size_t>(this->pos)], data_, size);
        this->pos += size;
        return size;
    }

    __blib_inline bool MemoryStream::canSeek() const
    {
        return true;
    }

    __blib_inline bool MemoryStream::seek(bint64 offset, SeekOrigin origin)
    {
        // Базовая позиция по точке отсчёта
        bint64 base;
        switch (origin)
        {
            case SeekOrigin::Begin:
                base = 0;
                break;
            case SeekOrigin::Current:
                base = static_cast<bint64>(this->pos);
                break;
            case SeekOrigin::End:
                base = static_cast<bint64>(this->data.size());
                break;
            default:
                return false;
        }

        // Строгое позиционирование: только в пределах [0, size]
        bint64 newPos = base + offset;
        if (newPos < 0 || newPos > static_cast<bint64>(this->data.size()))
            return false;

        this->pos = static_cast<buint64>(newPos);
        return true;
    }

    __blib_inline buint64 MemoryStream::tell() const
    {
        return this->pos;
    }

    __blib_inline buint64 MemoryStream::size() const
    {
        return static_cast<buint64>(this->data.size());
    }

    __blib_inline const ByteArray& MemoryStream::getData() const
    {
        return this->data;
    }

    __blib_inline ByteArray MemoryStream::release()
    {
        this->pos = 0;
        return std::move(this->data);
    }

    __blib_inline void MemoryStream::clear()
    {
        this->data.clear();
        this->pos = 0;
    }

} // namespace core
} // namespace blib
