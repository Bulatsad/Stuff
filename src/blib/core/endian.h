#pragma once

#include <blib/blibint.h>
#include <blib/inline.h>
#include <blib/utilmacro.h>

namespace blib
{
namespace core
{
    /**
     * endian.h - утилиты порядка байтов (Little/Big Endian) для сериализации.
     * 
     * Назначение:
     * - Переносимое чтение/запись 16/32/64-битных целых в явном порядке байтов
     * - Безопасны для невыровненных указателей (собирают значение побайтово)
     * - Не зависят от порядка байтов платформы: на x86 компилятор сворачивает
     *   побайтовые операции в эффективные mov/bswap
     * 
     * Соглашение:
     * - LE - Little Endian (младший байт первым)
     * - BE - Big Endian (старший байт первым)
     * - Формат, читаемый/записываемый BinaryReader/BinaryWriter, выбирается
     *   вызывающим явно (суффиксы LE/BE) - никакой магии с хост-endian
     */

    // ------------------------------------------------------------------------
    // Переворот порядка байтов
    // ------------------------------------------------------------------------

    /**
     * Перевернуть порядок байтов 16-битного значения.
     */
    __blib_inline buint16 swapU16(buint16 value)
    {
        return static_cast<buint16>((value << 8) | (value >> 8));
    }

    /**
     * Перевернуть порядок байтов 32-битного значения.
     */
    __blib_inline buint32 swapU32(buint32 value)
    {
        return ((value << 24) & 0xFF000000u) |
               ((value << 8)  & 0x00FF0000u) |
               ((value >> 8)  & 0x0000FF00u) |
               ((value >> 24) & 0x000000FFu);
    }

    /**
     * Перевернуть порядок байтов 64-битного значения.
     */
    __blib_inline buint64 swapU64(buint64 value)
    {
        return ((value << 56) & 0xFF00000000000000ull) |
               ((value << 40) & 0x00FF000000000000ull) |
               ((value << 24) & 0x0000FF0000000000ull) |
               ((value << 8)  & 0x000000FF00000000ull) |
               ((value >> 8)  & 0x00000000FF000000ull) |
               ((value >> 24) & 0x0000000000FF0000ull) |
               ((value >> 40) & 0x000000000000FF00ull) |
               ((value >> 56) & 0x00000000000000FFull);
    }

    // ------------------------------------------------------------------------
    // Чтение из буфера (Little Endian)
    // ------------------------------------------------------------------------

    /**
     * Прочитать 16-битное LE-значение из буфера (невыровненный доступ безопасен).
     */
    __blib_inline buint16 readU16LE(_In const buint8* p)
    {
        return static_cast<buint16>(p[0]) |
               (static_cast<buint16>(p[1]) << 8);
    }

    /**
     * Прочитать 32-битное LE-значение из буфера (невыровненный доступ безопасен).
     */
    __blib_inline buint32 readU32LE(_In const buint8* p)
    {
        return static_cast<buint32>(p[0]) |
               (static_cast<buint32>(p[1]) << 8) |
               (static_cast<buint32>(p[2]) << 16) |
               (static_cast<buint32>(p[3]) << 24);
    }

    /**
     * Прочитать 64-битное LE-значение из буфера (невыровненный доступ безопасен).
     */
    __blib_inline buint64 readU64LE(_In const buint8* p)
    {
        return static_cast<buint64>(p[0]) |
               (static_cast<buint64>(p[1]) << 8) |
               (static_cast<buint64>(p[2]) << 16) |
               (static_cast<buint64>(p[3]) << 24) |
               (static_cast<buint64>(p[4]) << 32) |
               (static_cast<buint64>(p[5]) << 40) |
               (static_cast<buint64>(p[6]) << 48) |
               (static_cast<buint64>(p[7]) << 56);
    }

    // ------------------------------------------------------------------------
    // Чтение из буфера (Big Endian)
    // ------------------------------------------------------------------------

    /**
     * Прочитать 16-битное BE-значение из буфера (невыровненный доступ безопасен).
     */
    __blib_inline buint16 readU16BE(_In const buint8* p)
    {
        return (static_cast<buint16>(p[0]) << 8) |
               static_cast<buint16>(p[1]);
    }

    /**
     * Прочитать 32-битное BE-значение из буфера (невыровненный доступ безопасен).
     */
    __blib_inline buint32 readU32BE(_In const buint8* p)
    {
        return (static_cast<buint32>(p[0]) << 24) |
               (static_cast<buint32>(p[1]) << 16) |
               (static_cast<buint32>(p[2]) << 8) |
               static_cast<buint32>(p[3]);
    }

    /**
     * Прочитать 64-битное BE-значение из буфера (невыровненный доступ безопасен).
     */
    __blib_inline buint64 readU64BE(_In const buint8* p)
    {
        return (static_cast<buint64>(p[0]) << 56) |
               (static_cast<buint64>(p[1]) << 48) |
               (static_cast<buint64>(p[2]) << 40) |
               (static_cast<buint64>(p[3]) << 32) |
               (static_cast<buint64>(p[4]) << 24) |
               (static_cast<buint64>(p[5]) << 16) |
               (static_cast<buint64>(p[6]) << 8) |
               static_cast<buint64>(p[7]);
    }

    // ------------------------------------------------------------------------
    // Запись в буфер
    // ------------------------------------------------------------------------

    /**
     * Записать 16-битное LE-значение в буфер.
     */
    __blib_inline void writeU16LE(_Out buint8* p, buint16 value)
    {
        p[0] = static_cast<buint8>(value);
        p[1] = static_cast<buint8>(value >> 8);
    }

    /**
     * Записать 32-битное LE-значение в буфер.
     */
    __blib_inline void writeU32LE(_Out buint8* p, buint32 value)
    {
        p[0] = static_cast<buint8>(value);
        p[1] = static_cast<buint8>(value >> 8);
        p[2] = static_cast<buint8>(value >> 16);
        p[3] = static_cast<buint8>(value >> 24);
    }

    /**
     * Записать 64-битное LE-значение в буфер.
     */
    __blib_inline void writeU64LE(_Out buint8* p, buint64 value)
    {
        p[0] = static_cast<buint8>(value);
        p[1] = static_cast<buint8>(value >> 8);
        p[2] = static_cast<buint8>(value >> 16);
        p[3] = static_cast<buint8>(value >> 24);
        p[4] = static_cast<buint8>(value >> 32);
        p[5] = static_cast<buint8>(value >> 40);
        p[6] = static_cast<buint8>(value >> 48);
        p[7] = static_cast<buint8>(value >> 56);
    }

    /**
     * Записать 16-битное BE-значение в буфер.
     */
    __blib_inline void writeU16BE(_Out buint8* p, buint16 value)
    {
        p[0] = static_cast<buint8>(value >> 8);
        p[1] = static_cast<buint8>(value);
    }

    /**
     * Записать 32-битное BE-значение в буфер.
     */
    __blib_inline void writeU32BE(_Out buint8* p, buint32 value)
    {
        p[0] = static_cast<buint8>(value >> 24);
        p[1] = static_cast<buint8>(value >> 16);
        p[2] = static_cast<buint8>(value >> 8);
        p[3] = static_cast<buint8>(value);
    }

    /**
     * Записать 64-битное BE-значение в буфер.
     */
    __blib_inline void writeU64BE(_Out buint8* p, buint64 value)
    {
        p[0] = static_cast<buint8>(value >> 56);
        p[1] = static_cast<buint8>(value >> 48);
        p[2] = static_cast<buint8>(value >> 40);
        p[3] = static_cast<buint8>(value >> 32);
        p[4] = static_cast<buint8>(value >> 24);
        p[5] = static_cast<buint8>(value >> 16);
        p[6] = static_cast<buint8>(value >> 8);
        p[7] = static_cast<buint8>(value);
    }

} // namespace core
} // namespace blib
