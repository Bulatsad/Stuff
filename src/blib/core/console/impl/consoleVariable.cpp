#include <blib/core/console/consoleVariable.h>

#include <utility>

blib::console::ConsoleVariable::ConsoleVariable(
    const std::string& name_,
    const std::string& defaultValue_,
    blib::console::ConsoleVariableFlags flags_,
    std::function<void(const ConsoleVariable&)> onChanged_)
    : name(name_)
    , value(defaultValue_)
    , defaultValue(defaultValue_)
    , flags{ static_cast<buint32>(flags_) }
    , onChanged(std::move(onChanged_))
{
}

const std::string& blib::console::ConsoleVariable::getName() const
{
    return this->name;
}

const std::string& blib::console::ConsoleVariable::getString() const
{
    return this->value;
}

bool blib::console::ConsoleVariable::getBool() const
{
    // "1"/"true"/"on"/"yes" — истина; всё остальное — ложь
    return this->value == "1"
        || this->value == "true"
        || this->value == "on"
        || this->value == "yes";
}

bint32 blib::console::ConsoleVariable::getInt() const
{
    // Невалидное число трактуем как 0 (геттеры не бросают)
    try
    {
        return std::stoi(this->value);
    }
    catch (...)
    {
        return 0;
    }
}

float blib::console::ConsoleVariable::getFloat() const
{
    try
    {
        return std::stof(this->value);
    }
    catch (...)
    {
        return 0.0f;
    }
}

const std::string& blib::console::ConsoleVariable::getDefaultValue() const
{
    return this->defaultValue;
}

bool blib::console::ConsoleVariable::isReadOnly() const
{
    return this->flags.isUp(blib::console::ConsoleVariableFlags::ReadOnly);
}

bool blib::console::ConsoleVariable::isArchive() const
{
    return this->flags.isUp(blib::console::ConsoleVariableFlags::Archive);
}

bool blib::console::ConsoleVariable::isCheat() const
{
    return this->flags.isUp(blib::console::ConsoleVariableFlags::Cheat);
}

bool blib::console::ConsoleVariable::set(const std::string& newValue)
{
    if (this->flags.isUp(blib::console::ConsoleVariableFlags::ReadOnly))
        return false;

    // Коллбэк дёргаем только при реальном изменении значения
    if (this->value == newValue)
        return true;

    this->value = newValue;
    if (this->onChanged)
        this->onChanged(*this);
    return true;
}

void blib::console::ConsoleVariable::setString(const std::string& newValue)
{
    this->set(newValue);
}

void blib::console::ConsoleVariable::setBool(bool newValue)
{
    this->set(newValue ? "true" : "false");
}

void blib::console::ConsoleVariable::setInt(bint32 newValue)
{
    this->set(std::to_string(newValue));
}

void blib::console::ConsoleVariable::setFloat(float newValue)
{
    this->set(std::to_string(newValue));
}

void blib::console::ConsoleVariable::reset()
{
    // reset работает даже для read-only (это "откат", а не пользовательский ввод)
    if (this->value == this->defaultValue)
        return;

    this->value = this->defaultValue;
    if (this->onChanged)
        this->onChanged(*this);
}
