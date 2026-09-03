#pragma once

#include <functional>
#include <string>

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/core/flags.h>
#include <blib/utilmacro.h>

namespace blib
{
    namespace console
    {
        // Флаги console-переменной (Quake-стиль)
        enum class ConsoleVariableFlags : buint32
        {
            None     = 0,
            Archive  = 1 << 0, // сохранять в cfg-файл (поддержка — позже)
            ReadOnly = 1 << 1, // менять значение запрещено
            Cheat    = 1 << 2  // чит-переменная (нужна для multiplayer-блокировок)
        };

        // Console-переменная (cvar). Значение хранится строкой (как в Quake),
        // типизированный доступ — через геттеры с конвертацией:
        //   getBool()/getInt()/getFloat()/getString()
        // Коллбэк onChanged вызывается при реальном изменении значения
        // (в т.ч. при reset()), но не при set() того же самого значения.
        class __blib_core_api ConsoleVariable
        {
        private:
            std::string name;
            std::string value;
            std::string defaultValue;
            blib::core::Flags<ConsoleVariableFlags, buint32> flags;
            std::function<void(const ConsoleVariable&)> onChanged;

        public:
            ConsoleVariable(
                _In const std::string& name,
                _In const std::string& defaultValue,
                ConsoleVariableFlags flags = ConsoleVariableFlags::None,
                std::function<void(const ConsoleVariable&)> onChanged = nullptr);

            const std::string& getName() const;
            const std::string& getString() const;
            bool getBool() const;
            bint32 getInt() const;
            float getFloat() const;

            const std::string& getDefaultValue() const;
            bool isReadOnly() const;
            bool isArchive() const;
            bool isCheat() const;

            // true — значение принято; false — переменная read-only.
            // Строка принимается как есть (конвертация — в typed-сеттерах).
            bool set(_In const std::string& newValue);
            void setString(_In const std::string& newValue);
            void setBool(bool newValue);
            void setInt(bint32 newValue);
            void setFloat(float newValue);

            // Вернуть значение по умолчанию (работает даже для read-only)
            void reset();
        };
    }
}
