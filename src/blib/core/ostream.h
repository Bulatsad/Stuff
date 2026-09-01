#pragma once

#include <type_traits>
#include <utility>

#include <blib/blibint.h>
#include <blib/inline.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/globalAllocator.h>
#include <blib/core/istream.h>

namespace blib
{
namespace core
{
    /**
     * IOutputStream - интерфейс выходного потока (приёмника данных).
     * 
     * Контракт write:
     * - Пишет ДО size байт, возвращает фактически записанное количество
     * - Частичная запись допустима; 0 означает что данные не приняты
     *   (полный приёмник, ошибка и т.п.)
     */
    class __blib_core_api IOutputStream : public virtual IStream
    {
    public:
        /**
         * Записать байты в поток.
         * 
         * @param data Указатель на данные (не nullptr, если size > 0)
         * @param size Количество байт
         * @return Фактически записано
         */
        virtual size_t write(_In const void* data, size_t size) = 0;
    };

    /**
     * OutputStream - type-erased выходной поток (value-класс).
     * 
     * Назначение и архитектура - полный аналог InputStream (см. istream.h):
     * - Владение: heap-копия/перемещение источника через GlobalAllocator
     * - borrow: невладеющая ссылка на внешний поток
     * - Move-only; SBO не используется осознанно
     * 
     * Использование:
     *   std::stringstream ss;
     *   OutputStream out(StdOutputStreamAdapter(&ss)); // пишем в stringstream
     *   out.write(data, size);
     */
    class __blib_core_api OutputStream
    {
    public:
        /**
         * Конструктор по умолчанию - создаёт пустой (null) поток.
         */
        OutputStream();

        /**
         * Конструктор type erasure - берёт владение копией/перемещением источника.
         * 
         * @tparam Src Конкретный тип потока (наследник IOutputStream)
         * @param src  Экземпляр: rvalue перемещается, lvalue копируется
         * 
         * Compile-time проверки:
         * - Src должен наследовать IOutputStream
         * - Src должен быть copy- ИЛИ move-конструируемым (lvalue копируется,
         *   rvalue перемещается; move-only потоки - например FileStream -
         *   можно передавать только как rvalue)
         */
        template<typename Src,
                 typename = typename std::enable_if<!std::is_same<typename std::decay<Src>::type, OutputStream>::value>::type>
        explicit OutputStream(Src&& src);

        /**
         * Деструктор - освобождает владеемый поток (если есть).
         */
        ~OutputStream();

        /**
         * Создать НЕвладеющий OutputStream поверх внешнего потока.
         * 
         * @param src Внешний поток (должен жить дольше результата)
         */
        static OutputStream borrow(IOutputStream& src);

        // Move-only семантика
        OutputStream(OutputStream&& other) noexcept;
        OutputStream& operator=(OutputStream&& other) noexcept;
        OutputStream(const OutputStream&) = delete;
        OutputStream& operator=(const OutputStream&) = delete;

        /**
         * Пуст ли поток (нет ни владеемого, ни заимствованного источника).
         */
        bool isNull() const;

        /**
         * Записывает байты в поток. Для null-потока возвращает 0.
         */
        size_t write(_In const void* data, size_t size);

        bool canSeek() const;
        bool seek(bint64 offset, SeekOrigin origin);
        buint64 tell() const;
        buint64 size() const;

    private:
        IOutputStream* impl; // Владеемый (heap) или заимствованный поток
        void* heapMem;       // Начало heap-аллокации владеемого объекта.
                             // Может НЕ совпадать с impl: при множественном
                             // наследовании указатель на базовый класс
                             // смещается относительно начала объекта,
                             // а deallocate требует ровно тот указатель,
                             // который вернул allocate
        buint8 owns;         // 1 - владеем impl (heap, через GlobalAllocator)
        size_t ownedSize;    // sizeof владеемого объекта (для deallocate)

        void destroyImpl();
    };

    // ------------------------------------------------------------------------
    // Реализация OutputStream
    // ------------------------------------------------------------------------

    template<typename Src, typename>
    __blib_inline OutputStream::OutputStream(Src&& src)
        : impl(nullptr)
        , heapMem(nullptr)
        , owns(0)
        , ownedSize(0)
    {
        typedef typename std::decay<Src>::type SrcT;

        // Проверки совместимости источника.
        // lvalue требует copy-конструктор, rvalue - move: допускается
        // хотя бы один из них (move-only потоки, например FileStream).
        static_assert(std::is_base_of<IOutputStream, SrcT>::value,
            "OutputStream: source must derive from blib::core::IOutputStream");
        static_assert(std::is_copy_constructible<SrcT>::value || std::is_move_constructible<SrcT>::value,
            "OutputStream: source must be copy or move constructible");

        // Поток размещаем в heap через GlobalAllocator (обоснование см. InputStream)
        void* mem = blib::memory::GlobalAllocator::instance().allocate(sizeof(SrcT));
        if (!mem)
        {
            // Память не выделилась - остаёмся null-потоком
            this->impl = nullptr;
            this->heapMem = nullptr;
            this->owns = 0;
            this->ownedSize = 0;
            return;
        }

        this->heapMem = mem;
        this->impl = new (mem) SrcT(std::forward<Src>(src));
        this->owns = 1;
        this->ownedSize = sizeof(SrcT);
    }

    __blib_inline OutputStream::OutputStream()
        : impl(nullptr)
        , heapMem(nullptr)
        , owns(0)
        , ownedSize(0)
    {
    }

    __blib_inline OutputStream::~OutputStream()
    {
        this->destroyImpl();
    }

    __blib_inline OutputStream OutputStream::borrow(IOutputStream& src)
    {
        OutputStream res;
        res.impl = &src;
        res.heapMem = nullptr;
        res.owns = 0;
        res.ownedSize = 0;
        return res;
    }

    __blib_inline OutputStream::OutputStream(OutputStream&& other) noexcept
        : impl(other.impl)
        , heapMem(other.heapMem)
        , owns(other.owns)
        , ownedSize(other.ownedSize)
    {
        // Исходник больше не владеет потоком
        other.impl = nullptr;
        other.heapMem = nullptr;
        other.owns = 0;
        other.ownedSize = 0;
    }

    __blib_inline OutputStream& OutputStream::operator=(OutputStream&& other) noexcept
    {
        if (this != &other)
        {
            this->destroyImpl();

            this->impl = other.impl;
            this->heapMem = other.heapMem;
            this->owns = other.owns;
            this->ownedSize = other.ownedSize;

            other.impl = nullptr;
            other.heapMem = nullptr;
            other.owns = 0;
            other.ownedSize = 0;
        }
        return *this;
    }

    __blib_inline bool OutputStream::isNull() const
    {
        return this->impl == nullptr;
    }

    __blib_inline size_t OutputStream::write(_In const void* data, size_t size)
    {
        if (!this->impl)
            return 0;
        return this->impl->write(data, size);
    }

    __blib_inline bool OutputStream::canSeek() const
    {
        if (!this->impl)
            return false;
        return this->impl->canSeek();
    }

    __blib_inline bool OutputStream::seek(bint64 offset, SeekOrigin origin)
    {
        if (!this->impl)
            return false;
        return this->impl->seek(offset, origin);
    }

    __blib_inline buint64 OutputStream::tell() const
    {
        if (!this->impl)
            return 0;
        return this->impl->tell();
    }

    __blib_inline buint64 OutputStream::size() const
    {
        if (!this->impl)
            return 0;
        return this->impl->size();
    }

    __blib_inline void OutputStream::destroyImpl()
    {
        if (!this->impl)
            return;

        if (this->owns)
        {
            // Виртуальный деструктор разрушает поток (это может быть указатель
            // на подобъект базового класса - виртуальный вызов скорректирует
            // this сам), затем освобождаем память по ИСХОДНОМУ указателю
            // аллокации (heapMem), а не по impl
            this->impl->~IOutputStream();
            blib::memory::GlobalAllocator::instance().deallocate(this->heapMem, this->ownedSize);
        }

        this->impl = nullptr;
        this->heapMem = nullptr;
        this->owns = 0;
        this->ownedSize = 0;
    }

} // namespace core
} // namespace blib
