#include <blib/core/console/consoleOutput.h>

#include <cstdio>
#include <utility>

const char* blib::console::messageTypeToString(blib::console::ConsoleMessageType type)
{
    switch (type)
    {
        case blib::console::ConsoleMessageType::Info:
            return "info";
        case blib::console::ConsoleMessageType::Warning:
            return "warning";
        case blib::console::ConsoleMessageType::Error:
            return "error";
        case blib::console::ConsoleMessageType::Command:
            return "command";
    }

    return "unknown";
}

blib::console::ConsoleLine::ConsoleLine()
    : text()
    , type(blib::console::ConsoleMessageType::Info)
{
}

blib::console::ConsoleLine::ConsoleLine(std::string text_, blib::console::ConsoleMessageType type_)
    : text(std::move(text_))
    , type(type_)
{
}

blib::console::ConsoleOutput::ConsoleOutput(size_t capacity)
    : lines(capacity)
    , echoToStdout(false)
{
}

void blib::console::ConsoleOutput::setStdoutEcho(bool enabled)
{
    this->echoToStdout = enabled;
}

void blib::console::ConsoleOutput::add(blib::console::ConsoleMessageType type, const std::string& text)
{
    this->lines.push(blib::console::ConsoleLine(text, type));

    // Эхо в терминал: Warning/Error идут в stderr, Info/Command — в stdout.
    // Вся строка печатается одним fprintf-вызовом: стандартные потоки
    // сериализуют отдельные вызовы, поэтому строки от разных потоков
    // не перемешиваются между собой.
    if (this->echoToStdout)
    {
        FILE* stream = (type == blib::console::ConsoleMessageType::Warning
            || type == blib::console::ConsoleMessageType::Error)
            ? stderr
            : stdout;

        std::fprintf(stream, "%s: %s\n", blib::console::messageTypeToString(type), text.c_str());
    }
}

void blib::console::ConsoleOutput::add(blib::console::ConsoleMessageType type, std::string&& text)
{
    // Эхо печатаем до перемещения строки в очередь (после push строка
    // уже не принадлежит нам — читать её текст нельзя)
    if (this->echoToStdout)
    {
        FILE* stream = (type == blib::console::ConsoleMessageType::Warning
            || type == blib::console::ConsoleMessageType::Error)
            ? stderr
            : stdout;

        std::fprintf(stream, "%s: %s\n", blib::console::messageTypeToString(type), text.c_str());
    }

    this->lines.push(blib::console::ConsoleLine(std::move(text), type));
}

bool blib::console::ConsoleOutput::popLine(blib::console::ConsoleLine& out)
{
    return this->lines.pop(out);
}

bool blib::console::ConsoleOutput::isEmpty() const
{
    return this->lines.isEmpty();
}

void blib::console::ConsoleOutput::clear()
{
    // Дренируем всё, что продюсеры успели положить (только консюмер —
    // pop под readLock, повторные вызовы из нескольких потоков запрещены
    // контрактом очереди)
    blib::console::ConsoleLine dummy;
    while (this->lines.pop(dummy))
    {
    }
}
