#pragma once

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/utilmacro.h>
#include <blib/core/ostream.h>

namespace blib
{
namespace core
{
    /**
     * SaveStatus - коды результата сохранения объекта в поток.
     *
     * Паттерн ошибок проекта: None = 0 - всегда успех
     * (см. ERROR_HANDLING_ARCHITECTURE.md).
     */
    enum class SaveStatus : buint32
    {
        None = 0,                 // успех
        Unsupported,              // тип не поддерживает сериализацию (default save)
        WriteFailed,              // сбой записи в поток
        ComponentNotSerializable  // Scene: один из компонентов не сериализуем
    };

    /**
     * ISaveable - интерфейс сохранения объекта в выходной поток.
     *
     * Контракт:
     * - save() пишет состояние объекта в поток с ТЕКУЩЕЙ позиции
     *   в текущем формате сохранения (сейчас - JSON: сериализованный
     *   JSON-объект; Scene вкладывает его в общий документ)
     * - Реализация по умолчанию возвращает SaveStatus::Unsupported:
     *   класс обязан переопределить save(), чтобы объявить себя
     *   сериализуемым
     * - Сериализуется ВСЁ persistent-состояние включая кэши -
     *   бит-в-бит round-trip (см. AGENTS.md, «Строгое сравнение»)
     *
     * Ограничения:
     * - save() - const: не меняет объект
     * - Не thread-safe
     */
    class __blib_core_api ISaveable
    {
    public:
        virtual ~ISaveable()
        {
        }

        /**
         * Сохранить объект в поток (с текущей позиции).
         *
         * @param os Выходной поток
         * @return SaveStatus::None при успехе
         */
        virtual SaveStatus save(_In IOutputStream& os) const
        {
            return SaveStatus::Unsupported;
        }
    };

} // namespace core
} // namespace blib
