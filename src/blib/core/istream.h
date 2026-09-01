#pragma once

#include <type_traits>
#include <utility>

#include <blib/blibint.h>
#include <blib/inline.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/globalAllocator.h>

namespace blib
{
namespace core
{
    /**
     * SeekOrigin - точка отсчёта для позиционирования потока.
     * 
     * Соответствует std::ios_base::seekdir и конвенции C (SEEK_SET/SEEK_CUR/SEEK_END).
     */
    enum class SeekOrigin : buint8
    {
        Begin = 0,   // От начала потока
        Current = 1, // От текущей позиции
        End = 2      // От конца потока
    };

    /**
     * IStream - базовый интерфейс потока данных (data stream, не thread!).
     * 
     * Назначение:
     * - Единый контракт позиционных операций для всех потоков:
     *   позиционирование (seek), текущая позиция (tell), размер (size)
     * - Родительский интерфейс для IInputStream и IOutputStream
     * 
     * Позиции и размеры в байтах (buint64 - файлы и буферы могут превышать 4 ГБ).
     * 
     * Ограничения:
     * - Виртуальный интерфейс: реализуйте методы в своих потоках,
     *   либо используйте готовые MemoryStream/FileStream/SliceStream
     * - Не thread-safe (синхронизация снаружи)
     */
    class __blib_core_api IStream
    {
    public:
        virtual ~IStream()
        {
        }

        /**
         * Поддерживает ли поток позиционирование.
         * 
         * @return true если seek/tell осмыслены для этого потока
         */
        virtual bool canSeek() const = 0;

        /**
         * Переместить позицию потока.
         * 
         * @param offset Смещение в байтах (может быть отрицательным)
         * @param origin Точка отсчёта (Begin/Current/End)
         * @return true при успехе; false если позиция вышла за допустимые
         *         пределы или поток не поддерживает позиционирование
         *         (позиция при этом не изменяется)
         */
        virtual bool seek(bint64 offset, SeekOrigin origin) = 0;

        /**
         * Текущая позиция потока в байтах от начала.
         * 
         * @return Текущая позиция или 0 если позиционирование не поддерживается
         */
        virtual buint64 tell() const = 0;

        /**
         * Полный размер потока в байтах.
         * 
         * @return Размер в байтах или 0 если размер неизвестен/неприменим
         *         (например, для потоковых сокетов или неисчерпанного sink'а)
         */
        virtual buint64 size() const = 0;
    };

    /**
     * IInputStream - интерфейс входного потока (источника данных).
     * 
     * Контракт read:
     * - Читает ДО size байт, возвращает фактически прочитанное количество
     * - 0 означает конец данных (EOF); частичное чтение допустимо
     * - Не выходит за пределы данных
     * 
     * Использование:
     *   IInputStream& s = ...;
     *   buint8 buf[1024];
     *   size_t n = s.read(buf, sizeof(buf));   // 0 == EOF
     */
    class __blib_core_api IInputStream : public virtual IStream
    {
    public:
        /**
         * Прочитать байты из потока.
         * 
         * @param buffer Буфер назначения (не nullptr, если size > 0)
         * @param size   Максимальное количество байт
         * @return Фактически прочитано (0 при EOF)
         */
        virtual size_t read(_Out void* buffer, size_t size) = 0;
    };

    /**
     * InputStream - type-erased входной поток (value-класс).
     * 
     * Назначение:
     * - Хранение и передача "любого" входного потока по значению без шаблонов
     * - Может владеть потоком (heap-копия/перемещение источника) или
     *   ссылаться на внешний (borrow - невладеющий)
     * 
     * Архитектура:
     * - Указатель на IInputStream + флаг владения + размер heap-объекта
     * - Владение: объект всегда в heap (GlobalAllocator); move - только
     *   перенос указателя. SBO не используется осознанно: потоки stateful
     *   (std::fstream и т.п. не обязаны корректно переноситься побайтово),
     *   а перенос указателя дёшев и корректен для любых типов
     * 
     * Использование:
     *   MemoryStream mem = ...;
     *   InputStream in(mem);                      // владеющая копия mem
     *   InputStream in2 = InputStream::borrow(mem); // невладеющая ссылка на mem
     * 
     * Ограничения:
     * - Move-only (поток имеет состояние; копирование через value-класс
     *   не поддерживается - копируйте конкретные потоки напрямую)
     * - Операции над null-потоком: read возвращает 0, остальное - false/0
     * - borrow: время жизни внешнего потока должен обеспечить вызывающий
     */
    class __blib_core_api InputStream
    {
    public:
        /**
         * Конструктор по умолчанию - создаёт пустой (null) поток.
         */
        InputStream();

        /**
         * Конструктор type erasure - берёт владение копией/перемещением источника.
         * 
         * @tparam Src Конкретный тип потока (наследник IInputStream)
         * @param src  Экземпляр: rvalue перемещается, lvalue копируется
         * 
         * Compile-time проверки:
         * - Src должен наследовать IInputStream
         * - Src должен быть copy- ИЛИ move-конструируемым (lvalue копируется,
         *   rvalue перемещается; move-only потоки - например FileStream -
         *   можно передавать только как rvalue)
         * 
         * ВАЖНО: enable_if исключает сам InputStream из перегрузки,
         * чтобы move-конструктор не перехватывался шаблоном.
         */
        template<typename Src,
                 typename = typename std::enable_if<!std::is_same<typename std::decay<Src>::type, InputStream>::value>::type>
        explicit InputStream(Src&& src);

        /**
         * Деструктор - освобождает владеемый поток (если есть).
         */
        ~InputStream();

        /**
         * Создать НЕвладеющий InputStream поверх внешнего потока.
         * 
         * @param src Внешний поток (должен жить дольше результата)
         */
        static InputStream borrow(IInputStream& src);

        // Move-only семантика
        InputStream(InputStream&& other) noexcept;
        InputStream& operator=(InputStream&& other) noexcept;
        InputStream(const InputStream&) = delete;
        InputStream& operator=(const InputStream&) = delete;

        /**
         * Пуст ли поток (нет ни владеемого, ни заимствованного источника).
         */
        bool isNull() const;

        /**
         * Читает байты из потока. Для null-потока возвращает 0.
         */
        size_t read(_Out void* buffer, size_t size);

        bool canSeek() const;
        bool seek(bint64 offset, SeekOrigin origin);
        buint64 tell() const;
        buint64 size() const;

    private:
        IInputStream* impl;  // Владеемый (heap) или заимствованный поток
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
    // Реализация InputStream
    // ------------------------------------------------------------------------

    template<typename Src, typename>
    __blib_inline InputStream::InputStream(Src&& src)
        : impl(nullptr)
        , heapMem(nullptr)
        , owns(0)
        , ownedSize(0)
    {
        typedef typename std::decay<Src>::type SrcT;

        // Проверки совместимости источника.
        // lvalue требует copy-конструктор, rvalue - move: допускается
        // хотя бы один из них (move-only потоки, например FileStream).
        static_assert(std::is_base_of<IInputStream, SrcT>::value,
            "InputStream: source must derive from blib::core::IInputStream");
        static_assert(std::is_copy_constructible<SrcT>::value || std::is_move_constructible<SrcT>::value,
            "InputStream: source must be copy or move constructible");

        // Поток размещаем в heap через GlobalAllocator:
        // SBO не используется, т.к. потоки stateful и не обязаны
        // корректно переноситься побайтово (std::fstream и т.п.)
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

    __blib_inline InputStream::InputStream()
        : impl(nullptr)
        , heapMem(nullptr)
        , owns(0)
        , ownedSize(0)
    {
    }

    __blib_inline InputStream::~InputStream()
    {
        this->destroyImpl();
    }

    __blib_inline InputStream InputStream::borrow(IInputStream& src)
    {
        InputStream res;
        res.impl = &src;
        res.heapMem = nullptr;
        res.owns = 0;
        res.ownedSize = 0;
        return res;
    }

    __blib_inline InputStream::InputStream(InputStream&& other) noexcept
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

    __blib_inline InputStream& InputStream::operator=(InputStream&& other) noexcept
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

    __blib_inline bool InputStream::isNull() const
    {
        return this->impl == nullptr;
    }

    __blib_inline size_t InputStream::read(_Out void* buffer, size_t size)
    {
        if (!this->impl)
            return 0;
        return this->impl->read(buffer, size);
    }

    __blib_inline bool InputStream::canSeek() const
    {
        if (!this->impl)
            return false;
        return this->impl->canSeek();
    }

    __blib_inline bool InputStream::seek(bint64 offset, SeekOrigin origin)
    {
        if (!this->impl)
            return false;
        return this->impl->seek(offset, origin);
    }

    __blib_inline buint64 InputStream::tell() const
    {
        if (!this->impl)
            return 0;
        return this->impl->tell();
    }

    __blib_inline buint64 InputStream::size() const
    {
        if (!this->impl)
            return 0;
        return this->impl->size();
    }

    __blib_inline void InputStream::destroyImpl()
    {
        if (!this->impl)
            return;

        if (this->owns)
        {
            // Виртуальный деструктор разрушает поток (это может быть указатель
            // на подобъект базового класса - виртуальный вызов скорректирует
            // this сам), затем освобождаем память по ИСХОДНОМУ указателю
            // аллокации (heapMem), а не по impl
            this->impl->~IInputStream();
            blib::memory::GlobalAllocator::instance().deallocate(this->heapMem, this->ownedSize);
        }

        this->impl = nullptr;
        this->heapMem = nullptr;
        this->owns = 0;
        this->ownedSize = 0;
    }

} // namespace core
} // namespace blib
