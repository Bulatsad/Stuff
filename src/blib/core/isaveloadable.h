#pragma once

#include <blib/config.h>
#include <blib/utilmacro.h>
#include <blib/core/icomparable.h>
#include <blib/core/isaveable.h>
#include <blib/core/iloadable.h>

namespace blib
{
namespace core
{
    /**
     * ISaveLoadable - единый интерфейс сериализации и сравнения.
     *
     * Объединяет:
     * - ISaveable  - save() в выходной поток (default: Unsupported)
     * - ILoadable  - load() из входного потока (default: Unsupported)
     * - IComparable - strongCompare/weakCompare/compare
     *
     * verify() - чисто виртуальная валидация round-trip: сохранить
     * себя в MemoryStream, загрузить свежий объект ТОГО ЖЕ типа из
     * этого потока и вернуть strongCompare между ними. Готовую
     * реализацию даёт свободный шаблон blib::core::verifyRoundTrip<T>
     * (без RTTI, см. verifyHelper.h).
     *
     * ВАЖНО: все базы НЕвиртуальные — виртуальное наследование
     * запретило бы static_cast-даункаст из IStrongComparable& в
     * конкретный тип (RTTI в проекте не используется).
     */
    class __blib_core_api ISaveLoadable : public ISaveable, public ILoadable, public IComparable
    {
    public:
        virtual ~ISaveLoadable()
        {
        }

        /**
         * Валидация round-trip: save -> MemoryStream -> новый объект
         * того же типа -> load -> strongCompare.
         *
         * @return true если загруженная копия строго равна оригиналу
         *
         * Семантика строгая: объекты с контекстными указателями,
         * которые load не восстанавливает (например, компонент со
         * Scene* вне сцены), честно возвращают false - валидация
         * таких объектов выполняется на уровне контейнера
         * (Scene::verify()).
         */
        virtual bool verify() const __blib_pure_virtual_function;
    };

} // namespace core
} // namespace blib
