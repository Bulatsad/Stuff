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

            // Удобные обёртки над log() по уровням (потокобезопасно).
            // logDebug/logDebugFormat в release-сборке ничего не делают.
            void logInfo(_In const std::string& text);
            void logWarning(_In const std::string& text);
            void logError(_In const std::string& text);
            void logDebug(_In const std::string& text);

            // printf-стиль: logInfoFormat("value %d", 42). Форматирование
            // идёт в стековый буфер (512), длинные строки обрезаются
            // (vsnprintf гарантирует завершающий нуль). В "..." допустимы
            // только printf-совместимые типы (std::string — UB).
            void logInfoFormat(_In const char* fmt, ...);
            void logWarningFormat(_In const char* fmt, ...);
            void logErrorFormat(_In const char* fmt, ...);
            void logDebugFormat(_In const char* fmt, ...);

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

// Удобные логирование-макросы (Qt-стиль: qInfo/qWarning/qError/qDebug).
// Принимают printf-формат:
//   __blib_log_error("failed to load %s: %d", name.c_str(), code);
// Строка без спецификаторов выводится как есть.
#define __blib_log_info(...)    blib::console::Console::instance().logInfoFormat(__VA_ARGS__)
#define __blib_log_warning(...) blib::console::Console::instance().logWarningFormat(__VA_ARGS__)
#define __blib_log_error(...)   blib::console::Console::instance().logErrorFormat(__VA_ARGS__)

#ifdef BLIB_DEBUG
#define __blib_log_debug(...)   blib::console::Console::instance().logDebugFormat(__VA_ARGS__)
#else
// В release debug-логирование полностью вырезается на этапе компиляции
#define __blib_log_debug(...)   ((void)0)
#endif
