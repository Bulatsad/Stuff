#include <blib/core/console/consoleCommand.h>

#include <utility>

blib::console::ConsoleCommand::ConsoleCommand(
    const std::string& name_,
    const std::string& help_,
    Callback callback_)
    : name(name_)
    , help(help_)
    , callback(std::move(callback_))
{
}

const std::string& blib::console::ConsoleCommand::getName() const
{
    return this->name;
}

const std::string& blib::console::ConsoleCommand::getHelp() const
{
    return this->help;
}

void blib::console::ConsoleCommand::execute(const std::vector<std::string>& args) const
{
    if (this->callback)
        this->callback(args);
}
