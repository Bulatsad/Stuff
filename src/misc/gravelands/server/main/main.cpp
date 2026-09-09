#include <gravelands/server/core/serverCore.h>

#include <blib/core/console/console.h>

// Тонкий exe сервера: только владение главным циклом.
// Вся логика — в gravelands-server-core (ServerCore, frame-API).
// Паттерн «lib + тонкий exe»: этот же core эдитор сможет
// хостить in-process в Play mode.
int main()
{
    // Вывод консоли ядра в stdout (консольное приложение)
    blib::console::Console::instance().getOutput().setStdoutEcho(true);

    gravelands::ServerCore core;

    if (!core.initialize())
    {
        __blib_fatal("Failed to initialize Gravelands server core");
    }

    // Главный цикл: сервер работает пока его не остановят (Ctrl+C)
    while (core.isRunning())
    {
        core.tick();
    }

    core.shutdown();

    return 0;
}
