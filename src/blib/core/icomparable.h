#pragma once

#include <unordered_set>

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/inline.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/allocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>

namespace blib
{
namespace core
{
    // Константы комбинирования указателей в хеш-функции сеанса сравнения
    constexpr size_t comparePairHashSeed = 0x9E3779B9u;
    constexpr size_t comparePairHashShift = 6;
    constexpr size_t comparePairHashFoldShift = 2;

    // Forward declaration
    class IStrongComparable;

    /**
     * CompareSession - контекст одного сеанса строгого сравнения.
     *
     * Назначение:
     * - Защита от бесконечной рекурсии при обходе циклического графа
     *   объектов (например, TransformComponent::ownerScene -> Scene ->
     *   пул -> тот же компонент): пары указателей, которые уже сравниваются
     *   в текущем сеансе, считаются равными при повторной встрече
     * - Null-aware deep compare указателей на IStrongComparable
     *
     * Использование:
     * - Создаётся точкой входа IStrongComparable::strongCompare(other)
     *   автоматически; вручную создавать не нужно
     * - Каждая реализация strongCompare(other, session) ОБЯЗАНА
     *   начинаться с вызова session.enter(this, &other) и при
     *   возврате false немедленно вернуть true (пара уже сравнивается)
     *
     * Ограничения:
     * - Не thread-safe (сеанс локален для одного сравнения)
     * - Память: unordered_set аллоцирует через DefaultAllocator
     *   (StdAllocatorAdapter поверх blib::memory::Allocator)
     */
    class __blib_core_api CompareSession
    {
    private:
        // Пара сравниваемых объектов. Указатели сравниваются как адреса
        // подобъектов базового класса (this внутри реализаций).
        struct PairKey
        {
            const void* a;
            const void* b;
        };

        // Хеш пары указателей (комбинация двух адресов)
        struct PairHash
        {
            size_t operator()(_In const PairKey& key) const
            {
                size_t hash = reinterpret_cast<size_t>(key.a) * comparePairHashSeed;
                hash ^= reinterpret_cast<size_t>(key.b) + comparePairHashSeed +
                    (hash << comparePairHashShift) + (hash >> comparePairHashFoldShift);
                return hash;
            }
        };

        struct PairEq
        {
            bool operator()(_In const PairKey& x, _In const PairKey& y) const
            {
                return x.a == y.a && x.b == y.b;
            }
        };

        // Аллокатор контейнера. Объявлен ПЕРЕД контейнером: visited
        // хранит указатель на него (см. StdAllocatorAdapter).
        blib::memory::Allocator containerAllocator;

        // Множество пар, находящихся в сравнении прямо сейчас
        std::unordered_set<PairKey, PairHash, PairEq,
            blib::memory::StdAllocatorAdapter<PairKey>> visited;

    public:
        CompareSession()
            : visited(blib::memory::StdAllocatorAdapter<PairKey>(&containerAllocator))
        {
        }

        ~CompareSession() = default;

        CompareSession(const CompareSession&) = delete;
        CompareSession& operator=(const CompareSession&) = delete;

        /**
         * Войти в сравнение пары объектов.
         *
         * @return true - пара новая, сравнение надо продолжать;
         *         false - пара уже сравнивается (цикл) - считать равной
         *
         * Вызывается первой строкой каждой реализации
         * strongCompare(other, session).
         */
        bool enter(_In const void* a, _In const void* b)
        {
            PairKey key{ a, b };
            if (visited.count(key) != 0)
            {
                return false;
            }
            visited.insert(key);
            return true;
        }

        /**
         * Null-aware глубокое сравнение указателей на IStrongComparable.
         *
         * @return true если оба nullptr, оба указывают на один и тот же
         *         объект, или объекты равны по strongCompare
         */
        bool comparePointers(_In_opt const IStrongComparable* a, _In_opt const IStrongComparable* b);
    };

    /**
     * IStrongComparable - интерфейс СТРОГОГО бит-в-бит сравнения.
     *
     * Семантика (см. AGENTS.md, «Строгое сравнение (strongCompare)»):
     * - Полное совпадение всех значений класса, бит в бит;
     *   float/double сравниваются оператором == (бит-в-бит для
     *   конечных значений; NaN не сериализуем и не сравним)
     * - Указатели: сравнение переходит по ним и строго сравнивает
     *   то, на что они указывают (значения самих указателей не
     *   сравниваются); циклы разрешаются через CompareSession
     * - Контекстные указатели (Scene* и т.п.) сравниваются так же
     *   глубоко - с защитой от циклов; объект без восстановленного
     *   контекста (например, standalone-загрузка компонента со
     *   сценой) строго НЕ равен оригиналу
     * - Вызывающий обязан гарантировать одинаковый динамический
     *   тип обоих операндов (RTTI не используется): реализация
     *   вправе делать static_cast вниз без проверки типа
     * - Любой класс, реализующий IStrongComparable прямо или
     *   косвенно, получает operator==/operator!= из этого базового
     *   класса: они просто возвращают strongCompare
     */
    class __blib_core_api IStrongComparable
    {
    public:
        virtual ~IStrongComparable()
        {
        }

        /**
         * Точка входа строгого сравнения: создаёт сеанс и вызывает
         * виртуальное сравнение с ним.
         */
        bool strongCompare(_In const IStrongComparable& other) const
        {
            CompareSession session;
            return strongCompare(other, session);
        }

        /**
         * INTERNAL: вариант строгого сравнения с явным сеансом.
         * Реализация обязана начать с session.enter(this, &other)
         * (иначе защита от циклов не работает). Вызывается точкой
         * входа и реализациями родительских классов при обходе
         * полей-указателей.
         */
        virtual bool strongCompare(_In const IStrongComparable& other, _In CompareSession& session) const
            __blib_pure_virtual_function;

        // operator==/operator!= по проектному правилу возвращают strongCompare
        bool operator==(_In const IStrongComparable& other) const { return strongCompare(other); }
        bool operator!=(_In const IStrongComparable& other) const { return !strongCompare(other); }
    };

    __blib_inline bool CompareSession::comparePointers(_In_opt const IStrongComparable* a, _In_opt const IStrongComparable* b)
    {
        // Оба nullptr или один и тот же объект
        if (a == b)
        {
            return true;
        }
        // Один nullptr, второй нет
        if (a == nullptr || b == nullptr)
        {
            return false;
        }
        // Глубокое сравнение объектов (защита от циклов внутри)
        return a->strongCompare(*b, *this);
    }

    /**
     * IWeakComparable - интерфейс «слабого» (толерантного) сравнения.
     *
     * Назначение: задел на будущее - сравнение с допусками
     * (float epsilon, порядко-нечувствительные контейнеры и т.п.).
     * Пока семантики нет: weakCompare() по умолчанию сравнивает по
     * идентичности (объект равен только самому себе). Переопределять
     * необязательно.
     *
     * НЕ наследует IStrongComparable намеренно: IComparable
     * наследует оба интерфейса независимо — иначе статический
     * downcast из IStrongComparable& в конкретный тип становится
     * нелегальным (виртуальное наследование запрещает static_cast
     * вниз, а RTTI в проекте не используется).
     */
    class __blib_core_api IWeakComparable
    {
    public:
        virtual ~IWeakComparable()
        {
        }

        /**
         * Толерантное сравнение. По умолчанию - по идентичности;
         * семантика будет определена, когда появится реальная
         * потребность.
         */
        virtual bool weakCompare(_In const IWeakComparable& other) const
        {
            return this == &other;
        }
    };

    /**
     * IComparable - единый интерфейс сравнения.
     *
     * compare() - точка входа сравнения объекта: по умолчанию
     * возвращает strongCompare (строгое сравнение). Объединяет
     * IStrongComparable и IWeakComparable.
     *
     * ВНИМАНИЕ: все базы НЕвиртуальные. Виртуальное наследование
     * здесь запретило бы static_cast-даункасты (а RTTI проект не
     * использует) — поэтому IWeakComparable не наследует
     * IStrongComparable.
     */
    class __blib_core_api IComparable : public IStrongComparable, public IWeakComparable
    {
    public:
        virtual ~IComparable()
        {
        }

        /**
         * Сравнить объекты. По умолчанию - строгое сравнение.
         */
        virtual bool compare(_In const IStrongComparable& other) const
        {
            return strongCompare(other);
        }
    };

} // namespace core
} // namespace blib
