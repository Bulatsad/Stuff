#include <blib/core/fileStream.h>

namespace blib
{
namespace core
{
    // ------------------------------------------------------------------------
    // FileStream
    // ------------------------------------------------------------------------

    FileStream::FileStream()
        : readable(0)
        , writable(0)
        , fileSize(0)
    {
    }

    FileStream::~FileStream()
    {
        this->close();
    }

    FileStream::FileStream(FileStream&& other) noexcept
        : file(std::move(other.file))
        , readable(other.readable)
        , writable(other.writable)
        , fileSize(other.fileSize)
    {
        // Исходник после переноса: файл закрыт, состояние сброшено
        other.readable = 0;
        other.writable = 0;
        other.fileSize = 0;
    }

    FileStream& FileStream::operator=(FileStream&& other) noexcept
    {
        if (this != &other)
        {
            this->close();

            this->file = std::move(other.file);
            this->readable = other.readable;
            this->writable = other.writable;
            this->fileSize = other.fileSize;

            other.readable = 0;
            other.writable = 0;
            other.fileSize = 0;
        }
        return *this;
    }

    FileStatus FileStream::open(const char* path, OpenModeFlags mode)
    {
        this->close();

        // Режим std::filebuf по флагам OpenMode
        std::ios_base::openmode openmode = std::ios_base::openmode(0);

        this->readable = mode.isUp(OpenMode::Read) ? 1 : 0;
        this->writable = mode.isUp(OpenMode::Write) ? 1 : 0;

        if (!this->readable && !this->writable)
            return FileStatus::CantOpen; // Режим не задан - открывать нечего

        if (this->readable)
            openmode |= std::ios::in;
        if (this->writable)
        {
            openmode |= std::ios::out;
            // Append сильнее Truncate (писать в конец поверх очистки)
            if (mode.isUp(OpenMode::Append))
                openmode |= std::ios::app;
            else if (mode.isUp(OpenMode::Truncate))
                openmode |= std::ios::trunc;
        }
        if (mode.isUp(OpenMode::Binary))
            openmode |= std::ios::binary;

        std::filebuf* opened = this->file.open(path, openmode);
        if (!opened)
            return FileStatus::CantOpen;

        // Кэшируем размер файла: позицию не трогаем (запоминаем и восстанавливаем)
        std::ios_base::openmode which =
            (this->readable ? std::ios::in : std::ios_base::openmode(0)) |
            (this->writable ? std::ios::out : std::ios_base::openmode(0));

        std::streamoff saved = this->file.pubseekoff(0, std::ios::cur, which);
        std::streamoff end = this->file.pubseekoff(0, std::ios::end, which);
        this->file.pubseekoff(saved, std::ios::beg, which);

        this->fileSize = end < 0 ? 0 : static_cast<buint64>(end);
        return FileStatus::OK;
    }

    void FileStream::close()
    {
        if (this->file.is_open())
            this->file.close();

        this->readable = 0;
        this->writable = 0;
        this->fileSize = 0;
    }

    bool FileStream::isOpen() const
    {
        return this->file.is_open();
    }

    ByteArray FileStream::readAll()
    {
        ByteArray res;
        if (!this->readable || !this->file.is_open())
            return res;

        // Читаем весь файл: в конец за размером, затем с начала до конца.
        // Позиция после вызова - конец файла (как у старого File).
        std::streamoff end = this->file.pubseekoff(0, std::ios::end, std::ios::in);
        this->file.pubseekoff(0, std::ios::beg, std::ios::in);

        if (end <= 0)
            return res;

        res.resize(static_cast<size_t>(end));

        std::streamsize got = this->file.sgetn(
            reinterpret_cast<char*>(&res[0]), static_cast<std::streamsize>(end));

        res.resize(got < 0 ? 0 : static_cast<size_t>(got));
        return res;
    }

    size_t FileStream::read(_Out void* buffer, size_t size)
    {
        if (!this->readable || !buffer || !size)
            return 0;

        std::streamsize got = this->file.sgetn(
            static_cast<char*>(buffer), static_cast<std::streamsize>(size));

        return got < 0 ? 0 : static_cast<size_t>(got);
    }

    size_t FileStream::write(_In const void* data, size_t size)
    {
        if (!this->writable || !data || !size)
            return 0;

        std::streamsize put = this->file.sputn(
            static_cast<const char*>(data), static_cast<std::streamsize>(size));

        if (put < 0)
            return 0;

        // Обновляем кэш размера: запись могла расширить файл
        std::streamoff cur = this->file.pubseekoff(0, std::ios::cur, std::ios::out);
        if (cur >= 0 && static_cast<buint64>(cur) > this->fileSize)
            this->fileSize = static_cast<buint64>(cur);

        return static_cast<size_t>(put);
    }

    bool FileStream::canSeek() const
    {
        return this->file.is_open();
    }

    bool FileStream::seek(bint64 offset, SeekOrigin origin)
    {
        if (!this->file.is_open())
            return false;

        std::ios_base::seekdir dir;
        switch (origin)
        {
            case SeekOrigin::Begin:
                dir = std::ios::beg;
                break;
            case SeekOrigin::Current:
                dir = std::ios::cur;
                break;
            case SeekOrigin::End:
                dir = std::ios::end;
                break;
            default:
                return false;
        }

        std::ios_base::openmode which =
            (this->readable ? std::ios::in : std::ios_base::openmode(0)) |
            (this->writable ? std::ios::out : std::ios_base::openmode(0));

        std::streamoff res = this->file.pubseekoff(
            static_cast<std::streamoff>(offset), dir, which);

        if (res < 0)
            return false;

        // Позиционирование за текущий конец расширяет наше представление размера
        if (static_cast<buint64>(res) > this->fileSize)
            this->fileSize = static_cast<buint64>(res);

        return true;
    }

    buint64 FileStream::tell() const
    {
        if (!this->file.is_open())
            return 0;

        std::ios_base::openmode which =
            (this->readable ? std::ios::in : std::ios_base::openmode(0)) |
            (this->writable ? std::ios::out : std::ios_base::openmode(0));

        std::streamoff res = this->file.pubseekoff(0, std::ios::cur, which);
        return res < 0 ? 0 : static_cast<buint64>(res);
    }

    buint64 FileStream::size() const
    {
        return this->fileSize;
    }

} // namespace core
} // namespace blib
