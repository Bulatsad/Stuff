#include <gravelands/client/core/clientCore.h>

#include <blib/core/console/console.h>

// Тонкий exe клиента: только владение главным циклом.
// Вся логика — в gravelands-client-core (ClientCore, frame-API).
// Паттерн «lib + тонкий exe»: этот же core эдитор сможет
// хостить in-process в Play mode.
int main()
{
    // Вывод консоли ядра в stdout (для консольного запуска)
    blib::console::Console::instance().getOutput().setStdoutEcho(true);
    gravelands::ClientCore core;

    if (!core.initialize())
    {
        __blib_fatal("Failed to initialize Gravelands client core");
    }

    // Главный цикл: кадры идут пока открыто окно (Escape закрывает)
    while (core.isRunning())
    {
        core.tick();
    }

    core.shutdown();

    return 0;
}
