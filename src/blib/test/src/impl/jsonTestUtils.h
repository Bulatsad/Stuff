#pragma once

// Общие хелперы группы тестов json (testjson.cpp / testjsonParser.cpp /
// testjsonWriter.cpp): разбор/запись/round-trip, тест-потоки с троттлингом
// и сбоями, сбоящий аллокатор для проверки JsonError::AllocationFailed.
//
// Header-only: anonymous namespace даёт каждой TU свою копию (internal
// linkage) — ODR-конфликтов между тремя файлами группы нет.

#include <cstring>

#include <blib/core/json/json.h>
#include <blib/core/memoryStream.h>

namespace
{
    /**
     * Разобрать JSON из C-строки (SliceStream поверх буфера, без копирования).
     */
    blib::core::json::JsonError parseText(_In const char* text, _Out blib::core::json::JsonValue& out)
    {
        blib::core::json::JsonParser parser;
        return parser.parse(text, std::strlen(text), out);
    }

    /**
     * Сериализовать значение в ByteArray.
     */
    blib::core::json::JsonError writeText(_In const blib::core::json::JsonValue& value, _Out blib::core::ByteArray& out, buint32 indentSpaces = 0)
    {
        blib::core::MemoryStream stream;
        blib::core::json::JsonWriter writer;
        blib::core::json::JsonWriteOptions options;
        options.indentSpaces = indentSpaces;
        blib::core::json::JsonError err = writer.write(stream, value, options);
        if (err != blib::core::json::JsonError::None)
            return err;
        out = stream.getData();
        return blib::core::json::JsonError::None;
    }

    /**
     * Точное совпадение сериализованного текста с ожидаемой строкой.
     */
    bool equalsText(_In const blib::core::json::JsonValue& value, _In const char* expected, buint32 indentSpaces = 0)
    {
        blib::core::ByteArray data;
        if (writeText(value, data, indentSpaces) != blib::core::json::JsonError::None)
            return false;
        size_t expectedLen = std::strlen(expected);
        if (data.size() != expectedLen)
            return false;
        return std::memcmp(data.data(), expected, expectedLen) == 0;
    }

    /**
     * Round-trip: значение -> компактный текст -> разбор -> глубокая эквивалентность.
     */
    bool roundTrip(_In const blib::core::json::JsonValue& value)
    {
        blib::core::ByteArray data;
        if (writeText(value, data) != blib::core::json::JsonError::None)
            return false;
        blib::core::json::JsonParser parser;
        blib::core::json::JsonValue parsed;
        if (parser.parse(reinterpret_cast<const char*>(data.data()), data.size(), parsed) != blib::core::json::JsonError::None)
            return false;
        return parsed == value;
    }

    /**
     * Входной поток поверх буфера, выдающий не более maxChunk байт за вызов.
     * Проверяет, что парсер не полагается на размер куска чтения
     * (pushback-логика и побайтовое чтение работают при maxChunk == 1).
     */
    class ThrottledInputStream : public blib::core::IInputStream
    {
    public:
        ThrottledInputStream(_In const char* data, size_t size, size_t maxChunk)
            : data(data)
            , remaining(size)
            , maxChunk(maxChunk)
        {
        }

        size_t read(_Out void* buffer, size_t size) __blib_override
        {
            size_t n = size < this->maxChunk ? size : this->maxChunk;
            n = n < this->remaining ? n : this->remaining;
            if (!n)
                return 0;
            std::memcpy(buffer, this->data, n);
            this->data += n;
            this->remaining -= n;
            return n;
        }

        bool canSeek() const __blib_override { return false; }
        bool seek(bint64 offset, blib::core::SeekOrigin origin) __blib_override { (void)offset; (void)origin; return false; }
        buint64 tell() const __blib_override { return 0; }
        buint64 size() const __blib_override { return 0; }

    private:
        const char* data;
        size_t remaining;
        size_t maxChunk;
    };

    /**
     * Выходной поток-накопитель, принимающий не более maxChunk байт за вызов.
     * Проверяет цикл дописывания в JsonWriter::writeRaw и независимость
     * результата от размера куска.
     */
    class ThrottledOutputStream : public blib::core::IOutputStream
    {
    public:
        explicit ThrottledOutputStream(size_t maxChunk)
            : maxChunk(maxChunk)
        {
        }

        size_t write(_In const void* data, size_t size) __blib_override
        {
            size_t n = size < this->maxChunk ? size : this->maxChunk;
            const buint8* p = static_cast<const buint8*>(data);
            this->buffer.insert(this->buffer.end(), p, p + n);
            return n;
        }

        bool canSeek() const __blib_override { return false; }
        bool seek(bint64 offset, blib::core::SeekOrigin origin) __blib_override { (void)offset; (void)origin; return false; }
        buint64 tell() const __blib_override { return 0; }
        buint64 size() const __blib_override { return 0; }

        const blib::core::ByteArray& getData() const { return this->buffer; }

    private:
        blib::core::ByteArray buffer;
        size_t maxChunk;
    };

    /**
     * Выходной поток, который принимает ровно limit байт, а затем
     * каждый write возвращает 0 — StreamError посреди документа.
     */
    class FailingAfterBytesOutputStream : public blib::core::IOutputStream
    {
    public:
        explicit FailingAfterBytesOutputStream(size_t limit)
            : limit(limit)
        {
        }

        size_t write(_In const void* data, size_t size) __blib_override
        {
            size_t n = size < this->limit ? size : this->limit;
            if (!n)
                return 0;
            const buint8* p = static_cast<const buint8*>(data);
            this->buffer.insert(this->buffer.end(), p, p + n);
            this->limit -= n;
            return n;
        }

        bool canSeek() const __blib_override { return false; }
        bool seek(bint64 offset, blib::core::SeekOrigin origin) __blib_override { (void)offset; (void)origin; return false; }
        buint64 tell() const __blib_override { return 0; }
        buint64 size() const __blib_override { return 0; }

        const blib::core::ByteArray& getData() const { return this->buffer; }

    private:
        blib::core::ByteArray buffer;
        size_t limit;
    };

    /**
     * Аллокатор, который всегда отказывает: allocate() -> nullptr.
     * Через type-erasure (blib::memory::Allocator) подставляется парсеру
     * тест-хуком setDocumentAllocatorForTests — единственный способ
     * воспроизвести JsonError::AllocationFailed без реального OOM.
     *
     * ВАЖНО: покрывает только отказ на ПЕРВОЙ аллокации (узел-контейнер
     * создаётся до тела парсинга). Отказ на N-й аллокации уводит в
     * std::bad_alloc из std-контейнеров (политика проекта их не ловит) —
     * этот сценарий непокрываем.
     */
    struct FailingAllocator
    {
        void* allocate(size_t size)
        {
            (void)size;
            return nullptr;
        }

        void deallocate(_In void* ptr, size_t size)
        {
            (void)ptr;
            (void)size;
        }
    };
}
