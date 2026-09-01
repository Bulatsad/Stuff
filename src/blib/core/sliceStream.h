#pragma once

#include <cstring>

#include <blib/blibint.h>
#include <blib/inline.h>
#include <blib/utilmacro.h>
#include <blib/core/istream.h>

namespace blib
{
namespace core
{
    /**
     * SliceStream - входной поток-окно поверх существующего блока памяти.
     * 
     * Назначение:
     * - Чтение из фрагмента памяти без копирования данных
     * - Разбор подблоков: заголовок файла, отдельный чанк и т.п.
     * 
     * Использование:
     *   buint8 bigBuffer[1024];
     *   SliceStream slice(&bigBuffer[64], 256); // окно [64, 64+256)
     *   InputStream in(slice);
     * 
     * Ограничения:
     * - НЕвладеющий: данные не копируются и не освобождаются,
     *   время жизни блока памяти должен обеспечить вызывающий
     * - Только чтение (нет write)
     */
    class __blib_core_api SliceStream : public IInputStream
    {
    public:
        /**
         * Конструктор по умолчанию - пустое окно (size == 0, read возвращает 0).
         */
        SliceStream();

        /**
         * Конструктор окна над памятью.
         * 
         * @param data Указатель на начало данных (может быть nullptr при size == 0)
         * @param size Размер окна в байтах
         */
        SliceStream(_In const void* data, size_t size);

        size_t read(_Out void* buffer, size_t size) __blib_override;

        bool canSeek() const __blib_override;
        bool seek(bint64 offset, SeekOrigin origin) __blib_override;
        buint64 tell() const __blib_override;
        buint64 size() const __blib_override;

    private:
        const buint8* data; // Начало окна (не владеем)
        size_t sz;          // Размер окна в байтах
        size_t pos;         // Текущая позиция внутри окна
    };

    __blib_inline SliceStream::SliceStream()
        : data(nullptr)
        , sz(0)
        , pos(0)
    {
    }

    __blib_inline SliceStream::SliceStream(_In const void* data, size_t size)
        : data(static_cast<const buint8*>(data))
        , sz(size)
        , pos(0)
    {
    }

    __blib_inline size_t SliceStream::read(_Out void* buffer, size_t size)
    {
        if (!buffer || !size)
            return 0;

        // Доступный остаток окна (инвариант: pos <= sz)
        size_t available = this->sz - this->pos;
        size_t toRead = size < available ? size : available;

        if (toRead)
        {
            std::memcpy(buffer, this->data + this->pos, toRead);
            this->pos += toRead;
        }
        return toRead;
    }

    __blib_inline bool SliceStream::canSeek() const
    {
        return true;
    }

    __blib_inline bool SliceStream::seek(bint64 offset, SeekOrigin origin)
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
                base = static_cast<bint64>(this->sz);
                break;
            default:
                return false;
        }

        // Строгое позиционирование: только в пределах окна
        bint64 newPos = base + offset;
        if (newPos < 0 || newPos > static_cast<bint64>(this->sz))
            return false;

        this->pos = static_cast<size_t>(newPos);
        return true;
    }

    __blib_inline buint64 SliceStream::tell() const
    {
        return static_cast<buint64>(this->pos);
    }

    __blib_inline buint64 SliceStream::size() const
    {
        return static_cast<buint64>(this->sz);
    }

} // namespace core
} // namespace blib
