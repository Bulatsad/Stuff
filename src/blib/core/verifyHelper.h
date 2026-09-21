#pragma once

#include <type_traits>

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/utilmacro.h>
#include <blib/core/isaveloadable.h>
#include <blib/core/memoryStream.h>
#include <blib/core/console/console.h>
#include <blib/system/memory/globalAllocator.h>

namespace blib
{
namespace core
{
    /**
     * verifyRoundTrip<T> - готовая реализация ISaveLoadable::verify().
     *
     * Назначение:
     * - Реализация verify() без RTTI: конкретный тип T известен
     *   статически (шаблон), поэтому «новый объект того же типа» -
     *   это T(), а сравнение - T::strongCompare
     * - Избавляет каждый сериализуемый класс от ручного кода
     *   round-trip валидации: компонент пишет однострочный override
     *
     * Требования к T:
     * - T должен быть default-конструируемым (verify создаёт T()):
     *   контекстные связи (Scene* и т.п.) восстановятся nullptr'ом
     * - T должен наследовать IStrongComparable (прямо или косвенно)
     * - static_assert'ы проверяются на месте вызова — T там полностью
     *   определён (шаблон инстанцируется в .cpp компонента)
     *
     * Использование (в классе-компоненте):
     *   bool verify() const __blib_override
     *   {
     *       return blib::core::verifyRoundTrip(*this);
     *   }
     *
     * Почему свободная функция, а не CRTP-база: CRTP-база требует
     * виртуального наследования ISaveLoadable (иначе в компоненте
     * оказывается два подобъекта интерфейса), а виртуальное
     * наследование запрещает static_cast-даункаст из IStrongComparable
     * в конкретный тип (RTTI проект не использует).
     *
     * Память:
     * - Временный объект T аллоцируется через GlobalAllocator
     *   (placement new) и корректно уничтожается после сравнения
     *
     * Ограничения:
     * - Возвращает false для объектов, чей контекст load не
     *   восстанавливает (см. ISaveLoadable::verify)
     */
    template<typename T>
    bool verifyRoundTrip(_In const T& self)
    {
        static_assert(std::is_base_of<IStrongComparable, T>::value,
            "verifyRoundTrip<T>: T must derive from blib::core::IStrongComparable");
        static_assert(std::is_default_constructible<T>::value,
            "verifyRoundTrip<T>: T must be default-constructible (verify() creates T())");

        MemoryStream stream;
        if (__blib_unlikely(self.save(stream) != SaveStatus::None))
        {
            __blib_log_warning("verify(): save to memory stream failed");
            return false;
        }

        // load() читает от текущей позиции - вернуть в начало
        stream.seek(0, SeekOrigin::Begin);

        void* mem = blib::memory::GlobalAllocator::instance().allocate(sizeof(T));
        if (__blib_unlikely(mem == nullptr))
        {
            __blib_log_warning("verify(): failed to allocate temporary object");
            return false;
        }

        T* loaded = new (mem) T();

        bool ok = (loaded->load(stream) == LoadStatus::None);
        if (ok)
        {
            ok = self.strongCompare(*loaded);
        }

        loaded->~T();
        blib::memory::GlobalAllocator::instance().deallocate(mem, sizeof(T));
        return ok;
    }

} // namespace core
} // namespace blib
