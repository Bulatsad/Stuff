#include <blib/test/src/test.h>

#include <blib/core/console/console.h>

#include <algorithm>
#include <thread>
#include <vector>

using blib::console::Console;
using blib::console::ConsoleLine;
using blib::console::ConsoleMessageType;
using blib::console::ConsoleVariable;
using blib::console::ConsoleVariableFlags;

// ============================================================
// Вспомогательные функции
// ============================================================

namespace
{
    // Дренирует весь буфер вывода консоли в вектор
    std::vector<ConsoleLine> drainOutput()
    {
        std::vector<ConsoleLine> result;
        ConsoleLine line;
        while (Console::instance().getOutput().popLine(line))
            result.push_back(line);
        return result;
    }

    // Ищет в выводе строку заданного уровня
    bool hasLine(const std::vector<ConsoleLine>& lines, ConsoleMessageType type, const std::string& text)
    {
        for (const ConsoleLine& line : lines)
        {
            if (line.type == type && line.text == text)
                return true;
        }
        return false;
    }
}

// ============================================================
// 1. tokenizeLine
// ============================================================

BLIB_TEST_CASE("tokenizeLine: splits by spaces and tabs")
{
    std::vector<std::string> tokens = blib::console::tokenizeLine("foo bar   baz\tqux");

    BLIB_TEST_REQUIRE(tokens.size() == 4);
    BLIB_TEST_CHECK(tokens[0] == "foo");
    BLIB_TEST_CHECK(tokens[1] == "bar");
    BLIB_TEST_CHECK(tokens[2] == "baz");
    BLIB_TEST_CHECK(tokens[3] == "qux");
}

BLIB_TEST_CASE("tokenizeLine: quotes group tokens and are stripped")
{
    std::vector<std::string> tokens = blib::console::tokenizeLine("set name \"hello world\"");

    BLIB_TEST_REQUIRE(tokens.size() == 3);
    BLIB_TEST_CHECK(tokens[0] == "set");
    BLIB_TEST_CHECK(tokens[1] == "name");
    BLIB_TEST_CHECK(tokens[2] == "hello world");
}

BLIB_TEST_CASE("tokenizeLine: empty and whitespace-only input")
{
    BLIB_TEST_CHECK(blib::console::tokenizeLine("").empty());
    BLIB_TEST_CHECK(blib::console::tokenizeLine("   \t  ").empty());
}

BLIB_TEST_CASE("tokenizeLine: unclosed quote takes the rest of the line")
{
    std::vector<std::string> tokens = blib::console::tokenizeLine("echo \"unfinished");

    BLIB_TEST_REQUIRE(tokens.size() == 2);
    BLIB_TEST_CHECK(tokens[0] == "echo");
    BLIB_TEST_CHECK(tokens[1] == "unfinished");
}

// ============================================================
// 2. commonPrefix
// ============================================================

BLIB_TEST_CASE("commonPrefix: empty list gives empty prefix")
{
    std::vector<std::string> empty;
    BLIB_TEST_CHECK(blib::console::commonPrefix(empty).empty());
}

BLIB_TEST_CASE("commonPrefix: single string is the string itself")
{
    std::vector<std::string> one = { "help " };
    BLIB_TEST_CHECK(blib::console::commonPrefix(one) == "help ");
}

BLIB_TEST_CASE("commonPrefix: shared prefix")
{
    std::vector<std::string> strs = { "fov ", "fps " };
    BLIB_TEST_CHECK(blib::console::commonPrefix(strs) == "f");
}

BLIB_TEST_CASE("commonPrefix: no shared prefix")
{
    std::vector<std::string> strs = { "abc", "xyz" };
    BLIB_TEST_CHECK(blib::console::commonPrefix(strs).empty());
}

// ============================================================
// 3. ConsoleVariable
// ============================================================

BLIB_TEST_CASE("ConsoleVariable: typed getters")
{
    ConsoleVariable var("testCvarGetters", "true");
    BLIB_TEST_CHECK(var.getBool());
    BLIB_TEST_CHECK(var.getString() == "true");
    BLIB_TEST_CHECK(var.getDefaultValue() == "true");

    ConsoleVariable num("testCvarGettersNum", "42");
    BLIB_TEST_CHECK(num.getInt() == 42);
    BLIB_TEST_CHECK(num.getFloat() == 42.0f);

    ConsoleVariable flt("testCvarGettersFloat", "3.5");
    BLIB_TEST_CHECK(flt.getFloat() == 3.5f);
}

BLIB_TEST_CASE("ConsoleVariable: bool truth forms")
{
    ConsoleVariable t1("testCvarBool1", "1");
    ConsoleVariable t2("testCvarBool2", "on");
    ConsoleVariable t3("testCvarBool3", "yes");
    ConsoleVariable f1("testCvarBool4", "0");
    ConsoleVariable f2("testCvarBool5", "banana");

    BLIB_TEST_CHECK(t1.getBool() && t2.getBool() && t3.getBool());
    BLIB_TEST_CHECK(!f1.getBool() && !f2.getBool());
}

BLIB_TEST_CASE("ConsoleVariable: invalid values convert to zero without throwing")
{
    ConsoleVariable var("testCvarInvalid", "not-a-number");

    BLIB_TEST_CHECK(var.getInt() == 0);
    BLIB_TEST_CHECK(var.getFloat() == 0.0f);
}

BLIB_TEST_CASE("ConsoleVariable: set changes value and fires callback once")
{
    int calls = 0;
    std::string seenValue;
    ConsoleVariable var("testCvarSet", "0", ConsoleVariableFlags::None,
        [&calls, &seenValue](const ConsoleVariable& v)
        {
            ++calls;
            seenValue = v.getString();
        });

    BLIB_TEST_CHECK(var.set("5"));
    BLIB_TEST_CHECK(var.getString() == "5");
    BLIB_TEST_CHECK(var.getInt() == 5);
    BLIB_TEST_REQUIRE(calls == 1);
    BLIB_TEST_CHECK(seenValue == "5");

    // Повторная установка того же значения — коллбэк не дёргается
    BLIB_TEST_CHECK(var.set("5"));
    BLIB_TEST_CHECK(calls == 1);
}

BLIB_TEST_CASE("ConsoleVariable: typed setters format values")
{
    ConsoleVariable var("testCvarTypedSet", "");

    var.setBool(true);
    BLIB_TEST_CHECK(var.getString() == "true");
    BLIB_TEST_CHECK(var.getBool());

    var.setInt(-17);
    BLIB_TEST_CHECK(var.getInt() == -17);

    var.setFloat(2.5f);
    BLIB_TEST_CHECK(var.getFloat() == 2.5f);

    var.setString("hello");
    BLIB_TEST_CHECK(var.getString() == "hello");
}

BLIB_TEST_CASE("ConsoleVariable: read-only rejects set")
{
    ConsoleVariable var("testCvarReadOnly", "10", ConsoleVariableFlags::ReadOnly);

    BLIB_TEST_CHECK(!var.set("20"));
    BLIB_TEST_CHECK(var.getString() == "10");
    BLIB_TEST_CHECK(var.isReadOnly());
}

BLIB_TEST_CASE("ConsoleVariable: reset restores default and fires callback")
{
    int calls = 0;
    ConsoleVariable var("testCvarReset", "10", ConsoleVariableFlags::None,
        [&calls](const ConsoleVariable&) { ++calls; });

    BLIB_TEST_CHECK(var.set("20"));
    BLIB_TEST_CHECK(calls == 1);

    var.reset();
    BLIB_TEST_CHECK(var.getString() == "10");
    BLIB_TEST_CHECK(calls == 2); // set + reset

    var.reset();
    BLIB_TEST_CHECK(calls == 2); // значение уже default — коллбэк не дёргается
}

BLIB_TEST_CASE("ConsoleVariable: flags archive and cheat")
{
    ConsoleVariable a("testCvarArchive", "0", ConsoleVariableFlags::Archive);
    ConsoleVariable c("testCvarCheat", "0", ConsoleVariableFlags::Cheat);

    BLIB_TEST_CHECK(a.isArchive());
    BLIB_TEST_CHECK(!a.isCheat());
    BLIB_TEST_CHECK(!a.isReadOnly());

    BLIB_TEST_CHECK(c.isCheat());
    BLIB_TEST_CHECK(!c.isArchive());
}

// ============================================================
// 4. ConsoleCommand
// ============================================================

BLIB_TEST_CASE("ConsoleCommand: stores name/help and passes args")
{
    std::vector<std::string> gotArgs;
    blib::console::ConsoleCommand cmd("testCmd", "does nothing",
        [&gotArgs](const std::vector<std::string>& args) { gotArgs = args; });

    BLIB_TEST_CHECK(cmd.getName() == "testCmd");
    BLIB_TEST_CHECK(cmd.getHelp() == "does nothing");

    std::vector<std::string> args = { "a", "b c" };
    cmd.execute(args);
    BLIB_TEST_REQUIRE(gotArgs.size() == 2);
    BLIB_TEST_CHECK(gotArgs[0] == "a");
    BLIB_TEST_CHECK(gotArgs[1] == "b c");
}

// ============================================================
// 5. Console: реестры
// ============================================================

BLIB_TEST_CASE("Console: registerVariable returns same instance on duplicate")
{
    Console& c = Console::instance();
    drainOutput(); // изолируемся от вывода предыдущих тестов

    ConsoleVariable* first = c.registerVariable("testCvarDup", "1");
    ConsoleVariable* second = c.registerVariable("testCvarDup", "2");

    BLIB_TEST_REQUIRE(first != nullptr);
    BLIB_TEST_CHECK(first == second);
    BLIB_TEST_CHECK(first->getString() == "1"); // перезаписи не было

    std::vector<ConsoleLine> lines = drainOutput();
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Warning,
        "variable 'testCvarDup' is already registered"));
}

BLIB_TEST_CASE("Console: findVariable")
{
    Console& c = Console::instance();

    c.registerVariable("testCvarFind", "7");
    ConsoleVariable* found = c.findVariable("testCvarFind");
    BLIB_TEST_REQUIRE(found != nullptr);
    BLIB_TEST_CHECK(found->getInt() == 7);

    BLIB_TEST_CHECK(c.findVariable("testCvarNotRegistered") == nullptr);
}

BLIB_TEST_CASE("Console: registerCommand duplicate is ignored")
{
    Console& c = Console::instance();
    drainOutput();

    c.registerCommand("testCmdDup", "help1", [](const std::vector<std::string>&) {});
    c.registerCommand("testCmdDup", "help2", [](const std::vector<std::string>&) {});

    std::vector<ConsoleLine> lines = drainOutput();
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Warning,
        "command 'testCmdDup' is already registered"));
}

// ============================================================
// 6. Console: execute
// ============================================================

BLIB_TEST_CASE("Console: execute unknown command logs error")
{
    Console& c = Console::instance();
    drainOutput();

    c.execute("testUnknownCommandXY");

    std::vector<ConsoleLine> lines = drainOutput();
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Error,
        "unknown command or variable: testUnknownCommandXY"));
}

BLIB_TEST_CASE("Console: execute prints variable value without args")
{
    Console& c = Console::instance();
    drainOutput();

    c.registerVariable("testCvarPrint", "55");
    c.execute("testCvarPrint");

    std::vector<ConsoleLine> lines = drainOutput();
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Info, "testCvarPrint = 55"));
}

BLIB_TEST_CASE("Console: execute sets variable and fires callback")
{
    Console& c = Console::instance();
    int calls = 0;
    c.registerVariable("testCvarExec", "1", ConsoleVariableFlags::None,
        [&calls](const ConsoleVariable&) { ++calls; });
    drainOutput();

    c.execute("testCvarExec 42");

    ConsoleVariable* var = c.findVariable("testCvarExec");
    BLIB_TEST_REQUIRE(var != nullptr);
    BLIB_TEST_CHECK(var->getInt() == 42);
    BLIB_TEST_CHECK(calls == 1);

    std::vector<ConsoleLine> lines = drainOutput();
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Info, "testCvarExec = 42"));
}

BLIB_TEST_CASE("Console: execute joins multi-token values")
{
    Console& c = Console::instance();

    c.registerVariable("testCvarMulti", "");
    c.execute("testCvarMulti \"hello world\"");

    ConsoleVariable* var = c.findVariable("testCvarMulti");
    BLIB_TEST_REQUIRE(var != nullptr);
    BLIB_TEST_CHECK(var->getString() == "hello world");
}

BLIB_TEST_CASE("Console: execute rejects read-only variable")
{
    Console& c = Console::instance();
    c.registerVariable("testCvarExecRO", "3", ConsoleVariableFlags::ReadOnly);
    drainOutput();

    c.execute("testCvarExecRO 99");

    ConsoleVariable* var = c.findVariable("testCvarExecRO");
    BLIB_TEST_REQUIRE(var != nullptr);
    BLIB_TEST_CHECK(var->getInt() == 3); // значение не изменилось

    std::vector<ConsoleLine> lines = drainOutput();
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Warning,
        "variable 'testCvarExecRO' is read-only"));
}

BLIB_TEST_CASE("Console: execute dispatches command with parsed args")
{
    Console& c = Console::instance();
    std::vector<std::string> gotArgs;
    c.registerCommand("testCmdExec", "test",
        [&gotArgs](const std::vector<std::string>& args) { gotArgs = args; });

    c.execute("testCmdExec one \"two three\"");

    BLIB_TEST_REQUIRE(gotArgs.size() == 2);
    BLIB_TEST_CHECK(gotArgs[0] == "one");
    BLIB_TEST_CHECK(gotArgs[1] == "two three");
}

BLIB_TEST_CASE("Console: execute echoes command line")
{
    Console& c = Console::instance();
    drainOutput();

    c.execute("help");

    std::vector<ConsoleLine> lines = drainOutput();
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Command, "help"));
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Info, "commands:"));
}

// ============================================================
// 7. Console: история
// ============================================================

BLIB_TEST_CASE("Console: history navigation up and down")
{
    Console& c = Console::instance();

    c.execute("testHistoryLineAlpha1"); // unknown, но в историю попадает
    c.execute("testHistoryLineAlpha2");

    const std::string* up1 = c.historyUp();
    BLIB_TEST_REQUIRE(up1 != nullptr);
    BLIB_TEST_CHECK(*up1 == "testHistoryLineAlpha2");

    const std::string* up2 = c.historyUp();
    BLIB_TEST_REQUIRE(up2 != nullptr);
    BLIB_TEST_CHECK(*up2 == "testHistoryLineAlpha1");

    const std::string* down1 = c.historyDown();
    BLIB_TEST_REQUIRE(down1 != nullptr);
    BLIB_TEST_CHECK(*down1 == "testHistoryLineAlpha2");

    const std::string* down2 = c.historyDown(); // ушли мимо конца истории
    BLIB_TEST_CHECK(down2 == nullptr);
}

BLIB_TEST_CASE("Console: history does not duplicate consecutive lines")
{
    Console& c = Console::instance();

    c.execute("testHistoryDupBeta");
    c.execute("testHistoryDupBeta"); // дубликат подряд не добавляется

    const std::string* up1 = c.historyUp();
    BLIB_TEST_REQUIRE(up1 != nullptr);
    BLIB_TEST_CHECK(*up1 == "testHistoryDupBeta");

    const std::string* up2 = c.historyUp();
    BLIB_TEST_REQUIRE(up2 != nullptr);
    BLIB_TEST_CHECK(*up2 != "testHistoryDupBeta");
}

BLIB_TEST_CASE("Console: execute resets history navigation")
{
    Console& c = Console::instance();

    c.execute("testHistoryResetGamma1");
    c.execute("testHistoryResetGamma2");
    c.historyUp(); // ушли вверх по истории
    c.execute("testHistoryResetGamma3");

    const std::string* up = c.historyUp();
    BLIB_TEST_REQUIRE(up != nullptr);
    BLIB_TEST_CHECK(*up == "testHistoryResetGamma3"); // курсор сброшен на последнюю
}

// ============================================================
// 8. Console: дополнение
// ============================================================

BLIB_TEST_CASE("Console: complete returns sorted candidates")
{
    Console& c = Console::instance();

    c.registerCommand("zzCompleteCmd", "test", [](const std::vector<std::string>&) {});
    c.registerVariable("zzCompleteVar", "0");

    std::vector<std::string> candidates;
    c.complete("zzComp", candidates);

    BLIB_TEST_REQUIRE(candidates.size() == 2);
    BLIB_TEST_CHECK(candidates[0] == "zzCompleteCmd ");
    BLIB_TEST_CHECK(candidates[1] == "zzCompleteVar ");
    BLIB_TEST_CHECK(std::is_sorted(candidates.begin(), candidates.end()));

    candidates.clear();
    c.complete("noSuchPrefixXY", candidates);
    BLIB_TEST_CHECK(candidates.empty());
}

// ============================================================
// 9. ConsoleOutput
// ============================================================

BLIB_TEST_CASE("ConsoleOutput: add/popLine preserve order and types")
{
    Console& c = Console::instance();
    c.getOutput().clear();

    c.log(ConsoleMessageType::Info, "first");
    c.log(ConsoleMessageType::Warning, "second");
    c.log(ConsoleMessageType::Error, "third");

    ConsoleLine line;
    BLIB_TEST_REQUIRE(c.getOutput().popLine(line));
    BLIB_TEST_CHECK(line.text == "first");
    BLIB_TEST_CHECK(line.type == ConsoleMessageType::Info);

    BLIB_TEST_REQUIRE(c.getOutput().popLine(line));
    BLIB_TEST_CHECK(line.text == "second");
    BLIB_TEST_CHECK(line.type == ConsoleMessageType::Warning);

    BLIB_TEST_REQUIRE(c.getOutput().popLine(line));
    BLIB_TEST_CHECK(line.text == "third");
    BLIB_TEST_CHECK(line.type == ConsoleMessageType::Error);

    BLIB_TEST_CHECK(!c.getOutput().popLine(line));
    BLIB_TEST_CHECK(c.getOutput().isEmpty());
}

BLIB_TEST_CASE("ConsoleOutput: clear drains buffered lines")
{
    Console& c = Console::instance();

    c.log(ConsoleMessageType::Info, "to-be-cleared");
    c.getOutput().clear();

    ConsoleLine line;
    BLIB_TEST_CHECK(!c.getOutput().popLine(line));
}

// ============================================================
// 10. Удобное логирование (методы + printf-формат + макросы)
// ============================================================

BLIB_TEST_CASE("Console: logInfo/logWarning/logError write correct types")
{
    Console& c = Console::instance();
    c.getOutput().clear();

    c.logInfo("info line");
    c.logWarning("warning line");
    c.logError("error line");

    std::vector<ConsoleLine> lines = drainOutput();
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Info, "info line"));
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Warning, "warning line"));
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Error, "error line"));
}

BLIB_TEST_CASE("Console: format logging with printf placeholders")
{
    Console& c = Console::instance();
    c.getOutput().clear();

    c.logInfoFormat("value %d, name %s", 42, "test");
    c.logErrorFormat("%s failed with code %d", "load", -3);
    c.logWarningFormat("plain text without placeholders");

    std::vector<ConsoleLine> lines = drainOutput();
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Info, "value 42, name test"));
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Error, "load failed with code -3"));
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Warning, "plain text without placeholders"));
}

BLIB_TEST_CASE("Console: logging macros route to the same output")
{
    Console& c = Console::instance();
    c.getOutput().clear();

    __blib_log_info("macro info %d", 1);
    __blib_log_warning("macro warning %d", 2);
    __blib_log_error("macro error %d", 3);
#ifdef BLIB_DEBUG
    __blib_log_debug("macro debug %d", 4);
#endif

    std::vector<ConsoleLine> lines = drainOutput();
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Info, "macro info 1"));
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Warning, "macro warning 2"));
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Error, "macro error 3"));
#ifdef BLIB_DEBUG
    // debug-уровень пока пишет как Info
    BLIB_TEST_CHECK(hasLine(lines, ConsoleMessageType::Info, "macro debug 4"));
#endif
}

// ============================================================
// 11. Потокобезопасность log()
// ============================================================

BLIB_TEST_CASE("Console: log from multiple threads is lossless")
{
    Console& c = Console::instance();
    c.getOutput().clear();

    const int threadsCount = 3;
    const int linesPerThread = 200;

    std::vector<std::thread> producers;
    producers.reserve(threadsCount);
    for (int t = 0; t < threadsCount; ++t)
    {
        producers.emplace_back([&c, t, linesPerThread]()
            {
                for (int i = 0; i < linesPerThread; ++i)
                    c.log(ConsoleMessageType::Info, "thread " + std::to_string(t));
            });
    }

    // Ёмкость буфера (2048) больше общего числа сообщений (600) —
    // потерь нет, цикл гарантированно завершается
    int collected = 0;
    ConsoleLine line;
    while (collected < threadsCount * linesPerThread)
    {
        if (c.getOutput().popLine(line))
            ++collected;
    }

    for (std::thread& t : producers)
        t.join();

    BLIB_TEST_CHECK(collected == threadsCount * linesPerThread);
}
