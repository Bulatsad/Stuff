#pragma once

#include <istream>
#include <ostream>

#include <blib/blibint.h>
#include <blib/inline.h>
#include <blib/utilmacro.h>
#include <blib/core/istream.h>
#include <blib/core/ostream.h>

namespace blib
{
namespace core
{
    /**
     * StdInputStreamAdapter - входной поток поверх std::istream.
     * 
     * Назначение:
     * - Переходник от std-потоков (std::ifstream, std::stringstream,
     *   std::istringstream и т.п.) к интерфейсу blib::core::IInputStream
     * 
     * Использование:
     *   std::stringstream ss("data");
     *   InputStream in(StdInputStreamAdapter(&ss)); // ввод из stringstream
     * 
     * Ограничения:
     * - НЕвладеющий: хранит указатель на std::istream, время жизни
     *   которого должен обеспечить вызывающий
     * - Позиционирование не поддерживается (canSeek == false, seek == false,
     *   tell/size == 0): std::istream не гарантирует seek для всех
     *   реализаций; при необходимости используйте MemoryStream/FileStream
     * - read полагается на std::istream::read/gcount (проверяйте состояние
     *   исходного потока отдельно при ошибках)
     */
    class __blib_core_api StdInputStreamAdapter : public IInputStream
    {
    public:
        /**
         * Конструктор от внешнего std::istream (не владеет им).
         * 
         * @param stream Поток (может быть nullptr - тогда read возвращает 0)
         */
        explicit StdInputStreamAdapter(std::istream* stream);

        size_t read(_Out void* buffer, size_t size) __blib_override;

        bool canSeek() const __blib_override;
        bool seek(bint64 offset, SeekOrigin origin) __blib_override;
        buint64 tell() const __blib_override;
        buint64 size() const __blib_override;

    private:
        std::istream* stream; // Внешний поток (не владеем)
    };

    /**
     * StdOutputStreamAdapter - выходной поток поверх std::ostream.
     * 
     * Назначение:
     * - Переходник от std-потоков (std::ofstream, std::stringstream,
     *   std::ostringstream и т.п.) к интерфейсу blib::core::IOutputStream
     * 
     * Пример (сжатие не в файл, а в stringstream - мотивация всего API):
     *   std::stringstream ss;
     *   OutputStream out(StdOutputStreamAdapter(&ss));
     *   compressor.compress(in, out);
     *   std::string compressed = ss.str();
     * 
     * Ограничения:
     * - НЕвладеющий: хранит указатель на std::ostream
     * - Позиционирование не поддерживается (canSeek == false)
     */
    class __blib_core_api StdOutputStreamAdapter : public IOutputStream
    {
    public:
        /**
         * Конструктор от внешнего std::ostream (не владеет им).
         * 
         * @param stream Поток (может быть nullptr - тогда write возвращает 0)
         */
        explicit StdOutputStreamAdapter(std::ostream* stream);

        size_t write(_In const void* data, size_t size) __blib_override;

        bool canSeek() const __blib_override;
        bool seek(bint64 offset, SeekOrigin origin) __blib_override;
        buint64 tell() const __blib_override;
        buint64 size() const __blib_override;

    private:
        std::ostream* stream; // Внешний поток (не владеем)
    };

    // ------------------------------------------------------------------------
    // StdInputStreamAdapter
    // ------------------------------------------------------------------------

    __blib_inline StdInputStreamAdapter::StdInputStreamAdapter(std::istream* stream)
        : stream(stream)
    {
    }

    __blib_inline size_t StdInputStreamAdapter::read(_Out void* buffer, size_t size)
    {
        if (!this->stream || !buffer || !size)
            return 0;

        this->stream->read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
        return static_cast<size_t>(this->stream->gcount());
    }

    __blib_inline bool StdInputStreamAdapter::canSeek() const
    {
        return false;
    }

    __blib_inline bool StdInputStreamAdapter::seek(bint64 offset, SeekOrigin origin)
    {
        (void)offset;
        (void)origin;
        return false;
    }

    __blib_inline buint64 StdInputStreamAdapter::tell() const
    {
        return 0;
    }

    __blib_inline buint64 StdInputStreamAdapter::size() const
    {
        return 0;
    }

    // ------------------------------------------------------------------------
    // StdOutputStreamAdapter
    // ------------------------------------------------------------------------

    __blib_inline StdOutputStreamAdapter::StdOutputStreamAdapter(std::ostream* stream)
        : stream(stream)
    {
    }

    __blib_inline size_t StdOutputStreamAdapter::write(_In const void* data, size_t size)
    {
        if (!this->stream || !data || !size)
            return 0;

        this->stream->write(static_cast<const char*>(data), static_cast<std::streamsize>(size));

        // std::ostream::write либо пишет всё, либо выставляет failbit
        if (this->stream->fail())
            return 0;
        return size;
    }

    __blib_inline bool StdOutputStreamAdapter::canSeek() const
    {
        return false;
    }

    __blib_inline bool StdOutputStreamAdapter::seek(bint64 offset, SeekOrigin origin)
    {
        (void)offset;
        (void)origin;
        return false;
    }

    __blib_inline buint64 StdOutputStreamAdapter::tell() const
    {
        return 0;
    }

    __blib_inline buint64 StdOutputStreamAdapter::size() const
    {
        return 0;
    }

} // namespace core
} // namespace blib
