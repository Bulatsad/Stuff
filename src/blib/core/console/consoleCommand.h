#pragma once

#include <functional>
#include <string>
#include <vector>

#include <blib/config.h>
#include <blib/utilmacro.h>

namespace blib
{
    namespace console
    {
        // Console-команда. Коллбэк получает аргументы (уже без имени команды),
        // разобранные токенизатором с поддержкой кавычек.
        class __blib_core_api ConsoleCommand
        {
        public:
            typedef std::function<void(const std::vector<std::string>& args)> Callback;

        private:
            std::string name;
            std::string help;
            Callback callback;

        public:
            ConsoleCommand(
                _In const std::string& name,
                _In const std::string& help,
                Callback callback);

            const std::string& getName() const;
            const std::string& getHelp() const;

            void execute(_In const std::vector<std::string>& args) const;
        };
    }
}
