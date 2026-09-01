#pragma once

#include <cstring>
#include <string>

#include <blib/blibint.h>
#include <blib/inline.h>
#include <blib/utilmacro.h>
#include <blib/core/endian.h>
#include <blib/core/istream.h>

namespace blib
{
namespace core
{
    /**
     * BinaryReader - типизированное чтение примитивов из входного потока.
     * 
     * Назначение:
     * - Бинарная десериализация поверх любого IInputStream
     *   (MemoryStream, FileStream, SliceStream, адаптеры std и т.д.)
     * - Явный порядок байтов (суффиксы LE/BE) - никакого "нативного" формата
     * 
     * Соглашение об ошибках:
     * - Все методы возвращают bool: false при нехватке данных (EOF) или
     *   ошибке потока; выходные параметры при false не определены
     * - После false поток может быть в середине частичного чтения -
     *   продолжать разбор без сброса позиции не рекомендуется
     * 
     * Использование:
     *   MemoryStream mem = ...;
     *   BinaryReader reader(mem);
     *   buint32 magic;
     *   if (!reader.readU32BE(magic) || magic != 0x12345678)
     *       return;
     * 
     * Ограничения:
     * - Не thread-safe
     * - Не владеет потоком по умолчанию (borrow); владение - через
     *   конструктор от InputStream&&
     */
    class __blib_core_api BinaryReader
    {
    public:
        /**
         * Конструктор от внешнего потока (НЕвладеющий).
         * Время жизни потока должен обеспечить вызывающий.
         */
        explicit BinaryReader(IInputStream& stream);

        /**
         * Конструктор от type-erased потока (владеющий перенос).
         */
        explicit BinaryReader(InputStream&& stream);

        /**
         * Прочитать беззнаковый 8-битный байт.
         */
        bool readU8(_Out buint8& out);

        /**
         * Прочитать 16-битное целое (Little Endian).
         */
        bool readU16LE(_Out buint16& out);

        /**
         * Прочитать 32-битное целое (Little Endian).
         */
        bool readU32LE(_Out buint32& out);

        /**
         * Прочитать 64-битное целое (Little Endian).
         */
        bool readU64LE(_Out buint64& out);

        /**
         * Прочитать 16-битное целое (Big Endian).
         */
        bool readU16BE(_Out buint16& out);

        /**
         * Прочитать 32-битное целое (Big Endian).
         */
        bool readU32BE(_Out buint32& out);

        /**
         * Прочитать 64-битное целое (Big Endian).
         */
        bool readU64BE(_Out buint64& out);

        /**
         * Прочитать ровно size байт.
         * 
         * ВАЖНО: buffer должен вмещать size байт (семантика fread).
         * 
         * @return false если доступно меньше size байт (данные в buffer
         *         при этом прочитаны частично, но НЕ более size байт)
         */
        bool readBytes(_Out void* buffer, size_t size);

        /**
         * Прочитать строку до нулевого байта или до maxLength включительно.
         * Нулевой терминатор не включается в результат.
         * 
         * @param out       Строка результата
         * @param maxLength Максимальная длина строки (защита от повреждённых
         *                  данных: длинная строка без терминатора - это ошибка)
         * @return false если терминатор не найден в пределах maxLength
         */
        bool readString(_Out std::string& out, size_t maxLength);

        /**
         * Доступ к обёрнутому потоку (например, для позиционирования).
         */
        InputStream& getStream();

    private:
        InputStream stream; // Поток-источник (borrowed или владеемый)
    };

    __blib_inline BinaryReader::BinaryReader(IInputStream& stream)
        : stream(InputStream::borrow(stream))
    {
    }

    __blib_inline BinaryReader::BinaryReader(InputStream&& stream)
        : stream(std::move(stream))
    {
    }

    __blib_inline bool BinaryReader::readU8(_Out buint8& out)
    {
        return this->stream.read(&out, 1) == 1;
    }

    __blib_inline bool BinaryReader::readU16LE(_Out buint16& out)
    {
        buint8 buf[2];
        if (!this->readBytes(buf, sizeof(buf)))
            return false;
        out = blib::core::readU16LE(buf);
        return true;
    }

    __blib_inline bool BinaryReader::readU32LE(_Out buint32& out)
    {
        buint8 buf[4];
        if (!this->readBytes(buf, sizeof(buf)))
            return false;
        out = blib::core::readU32LE(buf);
        return true;
    }

    __blib_inline bool BinaryReader::readU64LE(_Out buint64& out)
    {
        buint8 buf[8];
        if (!this->readBytes(buf, sizeof(buf)))
            return false;
        out = blib::core::readU64LE(buf);
        return true;
    }

    __blib_inline bool BinaryReader::readU16BE(_Out buint16& out)
    {
        buint8 buf[2];
        if (!this->readBytes(buf, sizeof(buf)))
            return false;
        out = blib::core::readU16BE(buf);
        return true;
    }

    __blib_inline bool BinaryReader::readU32BE(_Out buint32& out)
    {
        buint8 buf[4];
        if (!this->readBytes(buf, sizeof(buf)))
            return false;
        out = blib::core::readU32BE(buf);
        return true;
    }

    __blib_inline bool BinaryReader::readU64BE(_Out buint64& out)
    {
        buint8 buf[8];
        if (!this->readBytes(buf, sizeof(buf)))
            return false;
        out = blib::core::readU64BE(buf);
        return true;
    }

    __blib_inline bool BinaryReader::readBytes(_Out void* buffer, size_t size)
    {
        if (!buffer || !size)
            return true;

        // Требуем строго size байт (частичное чтение - ошибка формата)
        return this->stream.read(buffer, size) == size;
    }

    __blib_inline bool BinaryReader::readString(_Out std::string& out, size_t maxLength)
    {
        out.clear();

        buint8 ch;
        for (size_t i = 0; i < maxLength; ++i)
        {
            if (!this->readU8(ch))
                return false;

            if (ch == 0)
                return true;

            out.push_back(static_cast<char>(ch));
        }

        // Достигли лимита, а терминатора нет - данные повреждены
        return false;
    }

    __blib_inline InputStream& BinaryReader::getStream()
    {
        return this->stream;
    }

} // namespace core
} // namespace blib
