#include <blib/core/console/console.h>

#include <algorithm>
#include <utility>

std::vector<std::string> blib::console::tokenizeLine(const std::string& line)
{
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;

    for (char c : line)
    {
        if (c == '"')
        {
            // Кавычки переключают режим и в токен не попадают
            inQuotes = !inQuotes;
            continue;
        }

        if (!inQuotes && (c == ' ' || c == '\t'))
        {
            // Разделитель вне кавычек завершает токен
            if (!current.empty())
            {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(c);
    }

    if (!current.empty())
        tokens.push_back(current);

    return tokens;
}

std::string blib::console::commonPrefix(const std::vector<std::string>& strings)
{
    if (strings.empty())
        return std::string();

    std::string prefix = strings[0];
    for (size_t i = 1; i < strings.size() && !prefix.empty(); ++i)
    {
        const std::string& s = strings[i];
        size_t matchLen = 0;
        while (matchLen < prefix.size() && matchLen < s.size() && prefix[matchLen] == s[matchLen])
            ++matchLen;
        prefix.resize(matchLen);
    }

    return prefix;
}

blib::console::Console::Console()
    : historyIndex(0)
{
    // Встроенные команды. Каллбэки захватывают this: Console — синглтон,
    // живущий до конца процесса, поэтому захват безопасен.
    this->registerCommand(
        "help",
        "lists all console commands",
        [this](const std::vector<std::string>&)
        {
            this->log(blib::console::ConsoleMessageType::Info, "commands:");
            for (const auto& pair : this->commands)
                this->log(blib::console::ConsoleMessageType::Info, "  " + pair.first + " - " + pair.second.getHelp());
        });

    this->registerCommand(
        "list",
        "lists all console variables and their values",
        [this](const std::vector<std::string>&)
        {
            this->log(blib::console::ConsoleMessageType::Info, "variables:");
            for (const auto& pair : this->variables)
                this->log(blib::console::ConsoleMessageType::Info, "  " + pair.first + " = " + pair.second.getString());
        });
}

blib::console::Console& blib::console::Console::instance()
{
    // Локальный статик: потокобезопасная инициализация (C++11)
    static Console console;
    return console;
}

blib::console::ConsoleVariable* blib::console::Console::registerVariable(
    const std::string& name,
    const std::string& defaultValue,
    blib::console::ConsoleVariableFlags flags,
    std::function<void(const ConsoleVariable&)> onChanged)
{
    auto res = this->variables.emplace(
        name,
        blib::console::ConsoleVariable(name, defaultValue, flags, std::move(onChanged)));

    if (!res.second)
        this->log(blib::console::ConsoleMessageType::Warning, "variable '" + name + "' is already registered");

    // Указатель на элемент std::map стабилен: вставка других элементов
    // его не инвалидирует
    return &res.first->second;
}

blib::console::ConsoleVariable* blib::console::Console::findVariable(const std::string& name)
{
    auto it = this->variables.find(name);
    return (it == this->variables.end()) ? nullptr : &it->second;
}

void blib::console::Console::registerCommand(
    const std::string& name,
    const std::string& help,
    blib::console::ConsoleCommand::Callback callback)
{
    auto res = this->commands.emplace(
        name,
        blib::console::ConsoleCommand(name, help, std::move(callback)));

    if (!res.second)
        this->log(blib::console::ConsoleMessageType::Warning, "command '" + name + "' is already registered");
}

void blib::console::Console::log(blib::console::ConsoleMessageType type, const std::string& text)
{
    this->output.add(type, text);
}

blib::console::ConsoleOutput& blib::console::Console::getOutput()
{
    return this->output;
}

void blib::console::Console::execute(const std::string& line)
{
    std::vector<std::string> tokens = blib::console::tokenizeLine(line);
    if (tokens.empty())
        return;

    // Эхо исполняемой строки — как в Quake
    this->output.add(blib::console::ConsoleMessageType::Command, line);

    // История пополняется только исполняемыми строками; курсор навигации
    // сбрасывается, чтобы Up снова стартовал с последней записи
    if (this->history.empty() || this->history.back() != line)
    {
        if (this->history.size() >= maxHistoryEntries)
            this->history.erase(this->history.begin());
        this->history.push_back(line);
    }
    this->historyIndex = this->history.size();

    const std::string& first = tokens[0];

    // 1. Имя совпало с командой — вызываем её
    auto cmdIt = this->commands.find(first);
    if (cmdIt != this->commands.end())
    {
        std::vector<std::string> args(tokens.begin() + 1, tokens.end());
        cmdIt->second.execute(args);
        return;
    }

    // 2. Имя совпало с переменной — Quake-поведение:
    //    без значения печатаем текущее, со значением — устанавливаем
    auto varIt = this->variables.find(first);
    if (varIt != this->variables.end())
    {
        if (tokens.size() == 1)
        {
            this->log(blib::console::ConsoleMessageType::Info, varIt->second.getName() + " = " + varIt->second.getString());
            return;
        }

        // Значение может состоять из нескольких токенов (например,
        // строка с пробелами) — собираем обратно с пробелами
        std::string value = tokens[1];
        for (size_t i = 2; i < tokens.size(); ++i)
        {
            value += " ";
            value += tokens[i];
        }

        if (!varIt->second.set(value))
        {
            this->log(blib::console::ConsoleMessageType::Warning, "variable '" + first + "' is read-only");
            return;
        }

        // Подтверждение изменения — фидбек прямо в консоли
        this->log(blib::console::ConsoleMessageType::Info, first + " = " + varIt->second.getString());
        return;
    }

    // 3. Ни команда, ни переменная
    this->log(blib::console::ConsoleMessageType::Error, "unknown command or variable: " + first);
}

const std::string* blib::console::Console::historyUp()
{
    if (this->history.empty())
        return nullptr;

    if (this->historyIndex > 0)
        --this->historyIndex;

    return &this->history[this->historyIndex];
}

const std::string* blib::console::Console::historyDown()
{
    if (this->history.empty() || this->historyIndex + 1 >= this->history.size())
    {
        // Курсор ушёл "мимо" последней записи — сигнал UI вернуть
        // пользовательский (недописанный) ввод
        this->historyIndex = this->history.size();
        return nullptr;
    }

    ++this->historyIndex;
    return &this->history[this->historyIndex];
}

void blib::console::Console::complete(const std::string& input, std::vector<std::string>& outCandidates) const
{
    outCandidates.clear();

    // Дополняем последний токен строки ввода (v1: курсор всегда в конце)
    size_t lastSpace = input.find_last_of(" \t");
    std::string token = (lastSpace == std::string::npos)
        ? input
        : input.substr(lastSpace + 1);

    for (const auto& pair : this->commands)
    {
        if (pair.first.compare(0, token.size(), token) == 0)
            outCandidates.push_back(pair.first + " ");
    }

    for (const auto& pair : this->variables)
    {
        if (pair.first.compare(0, token.size(), token) == 0)
            outCandidates.push_back(pair.first + " ");
    }

    std::sort(outCandidates.begin(), outCandidates.end());
}
