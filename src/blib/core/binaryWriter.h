#pragma once

#include <cstring>
#include <string>

#include <blib/blibint.h>
#include <blib/inline.h>
#include <blib/utilmacro.h>
#include <blib/core/endian.h>
#include <blib/core/ostream.h>

namespace blib
{
namespace core
{
    /**
     * BinaryWriter - типизированная запись примитивов в выходной поток.
     * 
     * Назначение:
     * - Бинарная сериализация в любой IOutputStream
     *   (MemoryStream, FileStream, адаптеры std - например, std::stringstream,
     *   и т.д.)
     * - Явный порядок байтов (суффиксы LE/BE)
     * 
     * Соглашение об ошибках:
     * - Все методы возвращают bool: false если поток не принял данные
     *   целиком (полный приёмник, ошибка)
     * 
     * Использование:
     *   MemoryStream mem;
     *   BinaryWriter writer(mem);
     *   writer.writeU32BE(0x12345678);
     *   writer.writeString("hello");
     * 
     * Ограничения:
     * - Не thread-safe
     * - Не владеет потоком по умолчанию (borrow); владение - через
     *   конструктор от OutputStream&&
     */
    class __blib_core_api BinaryWriter
    {
    public:
        /**
         * Конструктор от внешнего потока (НЕвладеющий).
         * Время жизни потока должен обеспечить вызывающий.
         */
        explicit BinaryWriter(IOutputStream& stream);

        /**
         * Конструктор от type-erased потока (владеющий перенос).
         */
        explicit BinaryWriter(OutputStream&& stream);

        /**
         * Записать беззнаковый 8-битный байт.
         */
        bool writeU8(buint8 value);

        /**
         * Записать 16-битное целое (Little Endian).
         */
        bool writeU16LE(buint16 value);

        /**
         * Записать 32-битное целое (Little Endian).
         */
        bool writeU32LE(buint32 value);

        /**
         * Записать 64-битное целое (Little Endian).
         */
        bool writeU64LE(buint64 value);

        /**
         * Записать 16-битное целое (Big Endian).
         */
        bool writeU16BE(buint16 value);

        /**
         * Записать 32-битное целое (Big Endian).
         */
        bool writeU32BE(buint32 value);

        /**
         * Записать 64-битное целое (Big Endian).
         */
        bool writeU64BE(buint64 value);

        /**
         * Записать ровно size байт (целиком или false).
         */
        bool writeBytes(_In const void* data, size_t size);

        /**
         * Записать строку с нулевым терминатором (UTF-8).
         */
        bool writeString(_In const std::string& str);

        /**
         * Доступ к обёрнутому потоку (например, для позиционирования).
         */
        OutputStream& getStream();

    private:
        OutputStream stream; // Поток-приёмник (borrowed или владеемый)
    };

    __blib_inline BinaryWriter::BinaryWriter(IOutputStream& stream)
        : stream(OutputStream::borrow(stream))
    {
    }

    __blib_inline BinaryWriter::BinaryWriter(OutputStream&& stream)
        : stream(std::move(stream))
    {
    }

    __blib_inline bool BinaryWriter::writeU8(buint8 value)
    {
        return this->stream.write(&value, 1) == 1;
    }

    __blib_inline bool BinaryWriter::writeU16LE(buint16 value)
    {
        buint8 buf[2];
        blib::core::writeU16LE(buf, value);
        return this->writeBytes(buf, sizeof(buf));
    }

    __blib_inline bool BinaryWriter::writeU32LE(buint32 value)
    {
        buint8 buf[4];
        blib::core::writeU32LE(buf, value);
        return this->writeBytes(buf, sizeof(buf));
    }

    __blib_inline bool BinaryWriter::writeU64LE(buint64 value)
    {
        buint8 buf[8];
        blib::core::writeU64LE(buf, value);
        return this->writeBytes(buf, sizeof(buf));
    }

    __blib_inline bool BinaryWriter::writeU16BE(buint16 value)
    {
        buint8 buf[2];
        blib::core::writeU16BE(buf, value);
        return this->writeBytes(buf, sizeof(buf));
    }

    __blib_inline bool BinaryWriter::writeU32BE(buint32 value)
    {
        buint8 buf[4];
        blib::core::writeU32BE(buf, value);
        return this->writeBytes(buf, sizeof(buf));
    }

    __blib_inline bool BinaryWriter::writeU64BE(buint64 value)
    {
        buint8 buf[8];
        blib::core::writeU64BE(buf, value);
        return this->writeBytes(buf, sizeof(buf));
    }

    __blib_inline bool BinaryWriter::writeBytes(_In const void* data, size_t size)
    {
        if (!data || !size)
            return true;

        return this->stream.write(data, size) == size;
    }

    __blib_inline bool BinaryWriter::writeString(_In const std::string& str)
    {
        if (!this->writeBytes(str.c_str(), str.size()))
            return false;

        return this->writeU8(0);
    }

    __blib_inline OutputStream& BinaryWriter::getStream()
    {
        return this->stream;
    }

} // namespace core
} // namespace blib
