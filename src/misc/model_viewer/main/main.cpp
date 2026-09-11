#include <model_viewer/core/viewerCore.h>

#include <blib/core/console/console.h>

int main(int argc, char* argv[])
{
    // Дублировать вывод консоли в stdout (удобно при запуске из
    // командной строки) — паттерн из beng/test_ecs и gravelands
    blib::console::Console::instance().getOutput().setStdoutEcho(true);

    modelviewer::ViewerCore viewer;
    if (!viewer.initialize())
    {
        __blib_fatal("Failed to initialize Model Viewer core");
    }

    // Необязательный аргумент: путь к модели, загружаемой на старте
    // (удобно для быстрого просмотра и для smoke-тестов)
    if (argc > 1)
    {
        if (!viewer.loadModelFromFile(std::string(argv[1])))
        {
            __blib_log_error("failed to load model from command line: %s", argv[1]);
        }
    }

    // Главный цикл принадлежит тонкому exe (паттерн «lib + тонкий exe»)
    while (viewer.isRunning())
    {
        viewer.tick();
    }

    viewer.shutdown();
    return 0;
}
