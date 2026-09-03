#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/core/console/consoleCommand.h>
#include <blib/core/console/consoleMessageType.h>
#include <blib/core/console/consoleOutput.h>
#include <blib/core/console/consoleVariable.h>
#include <blib/utilmacro.h>

namespace blib
{
    namespace console
    {
        // Разбивает строку на токены: разделители — пробелы/табы,
        // кавычки ("...") группируют всё внутри в один токен
        // (сами кавычки в токен не попадают). Эскейпов нет — v1.
        std::vector<std::string> __blib_core_api tokenizeLine(_In const std::string& line);

        // Общий префикс набора строк (для Tab-дополнения)
        std::string __blib_core_api commonPrefix(_In const std::vector<std::string>& strings);

        // Центральная точка консоли движка (Quake-стиль): реестры
        // команд и переменных, вывод, история ввода и дополнение.
        //
        // Синглтон — как blib::memory::GlobalAllocator. Потокобезопасность:
        //   - log()/getOutput().add() — из любого потока (MPSC-буфер);
        //   - регистрация, execute(), история, complete() — предполагают
        //     вызов из одного (главного/UI) потока, встроенных блокировок нет.
        class __blib_core_api Console
        {
        private:
            static const size_t maxHistoryEntries = 100;

            std::map<std::string, ConsoleVariable> variables;
            std::map<std::string, ConsoleCommand> commands;
            ConsoleOutput output;

            std::vector<std::string> history;
            size_t historyIndex; // позиция навигации; history.size() == "мимо конца"

            Console(); // регистрирует встроенные команды (help, list)

        public:
            Console(const Console&) = delete;
            Console& operator=(const Console&) = delete;

            static Console& instance();

            // Регистрация переменной. Возвращает указатель на экземпляр
            // в реестре (узлы std::map стабильны — указатель валиден
            // до конца жизни Console). При дубликате имя не перезаписывается,
            // возвращается уже существующая переменная + warning в вывод.
            ConsoleVariable* registerVariable(
                _In const std::string& name,
                _In const std::string& defaultValue,
                ConsoleVariableFlags flags = ConsoleVariableFlags::None,
                std::function<void(const ConsoleVariable&)> onChanged = nullptr);

            // nullptr, если переменная не зарегистрирована
            ConsoleVariable* findVariable(_In const std::string& name);

            // Регистрация команды (дубликат игнорируется + warning в вывод)
            void registerCommand(
                _In const std::string& name,
                _In const std::string& help,
                ConsoleCommand::Callback callback);

            // Потокобезопасно (любой поток)
            void log(ConsoleMessageType type, _In const std::string& text);

            ConsoleOutput& getOutput();

            // Исполнение строки как команды консоли:
            //   - "<command> [args...]" — вызов команды;
            //   - "<cvar>" — печать текущего значения;
            //   - "<cvar> <value>" — установка значения (Quake-поведение);
            //   - неизвестное имя — error в вывод.
            // Выполняемая строка эхнется в вывод как Command и попадёт в историю.
            void execute(_In const std::string& line);

            // История ввода. Навигация ведётся внутренним курсором,
            // который сбрасывается при execute(): UI зовёт historyUp()
            // по стрелке вверх, historyDown() — по стрелке вниз;
            // nullptr — курсор ушёл "мимо" истории (вернуть ввод пользователя).
            const std::string* historyUp();
            const std::string* historyDown();

            // Кандидаты Tab-дополнения: имена команд и переменных
            // с префиксом последнего токена строки ввода + пробел.
            // Список отсортирован. Если кандидат один — строка готова к исполнению.
            void complete(_In const std::string& input, _Out std::vector<std::string>& outCandidates) const;
        };
    }
}
