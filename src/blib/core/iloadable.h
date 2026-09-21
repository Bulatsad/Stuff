#pragma once

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/utilmacro.h>
#include <blib/core/istream.h>

namespace blib
{
namespace core
{
    /**
     * LoadStatus - коды результата загрузки объекта из потока.
     *
     * Паттерн ошибок проекта: None = 0 - всегда успех
     * (см. ERROR_HANDLING_ARCHITECTURE.md).
     */
    enum class LoadStatus : buint32
    {
        None = 0,                     // успех
        Unsupported,                  // тип не поддерживает десериализацию (default load)
        ReadFailed,                   // сбой чтения из потока
        UnknownFormat,                // не совпал magic формата файла
        VersionMismatch,              // неподдерживаемая версия формата
        InvalidData,                  // данные повреждены/не соответствуют схеме
        ComponentTypeNotRegistered,   // Scene: тип компонента из файла не зарегистрирован
        SceneNotEmpty                 // Scene: load в непустую сцену
    };

    /**
     * ILoadable - интерфейс загрузки объекта из входного потока.
     *
     * Контракт:
     * - load() читает состояние объекта из потока с ТЕКУЩЕЙ позиции
     *   (продолжение данных, записанных save() в текущем формате)
     * - Реализация по умолчанию возвращает LoadStatus::Unsupported:
     *   класс обязан переопределить load(), чтобы объявить себя
     *   десериализуемым
     * - Успешный load() обязан восстановить ВСЁ состояние бит-в-бит,
     *   иначе strongCompare/verify() честно покажут расхождение
     * - Восстановление контекстных связей (Scene* и т.п.) - через
     *   IComponent::onLoaded(Scene&) после загрузки всех данных
     *
     * Ограничения:
     * - load() НЕ const: заполняет объект
     * - Не thread-safe
     */
    class __blib_core_api ILoadable
    {
    public:
        virtual ~ILoadable()
        {
        }

        /**
         * Загрузить объект из потока (с текущей позиции).
         *
         * @param is Входной поток
         * @return LoadStatus::None при успехе
         */
        virtual LoadStatus load(_In IInputStream& is)
        {
            return LoadStatus::Unsupported;
        }
    };

} // namespace core
} // namespace blib
