#include <sc2img/gui/sc2imgApp.h>
#include <sc2img/core/imageExport.h>

#include <blib/core/console/console.h>
#include <blib/core/folder.h>

#include <Windows.h>

#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
    // ---------------------------------------------------------------
    // Константы CLI (правило проекта: без вшитых литералов)
    // ---------------------------------------------------------------

    // Флаги командной строки
    constexpr const char* headlessFlag = "--headless";
    constexpr const char* outDirFlag = "--out";
    constexpr const char* formatFlag = "--format";
    constexpr const char* alignFlag = "--align";
    constexpr const char* playerColorFlag = "--player-color";
    constexpr const char* recursiveFlag = "--recursive";

    // Значения флага --format
    constexpr const char* pngFormatName = "png";
    constexpr const char* bmpFormatName = "bmp";
    constexpr const char* jpgFormatName = "jpg";

    // Разделитель каталогов Windows (утилита — Windows-only)
    constexpr char pathSeparator = '\\';

    // Лимит индекса цвета игрока (палитра GM1)
    constexpr buint32 maxPlayerColorIndex = 31;

    /**
     * Конвертация одного файла: load + экспорт всех изображений.
     * Ошибки логируются внутри; возвращает код ошибки.
     */
    sc2img::Sc2imgError convertOneFile(
        _In const std::string& path,
        _In const std::string& outDir,
        _In const sc2img::ExportOptions& options,
        _In bool usePlayerColorOverride,
        _In buint8 playerColor)
    {
        sc2img::Sc2imgDocument document;
        sc2img::Sc2imgError error = document.load(path, usePlayerColorOverride, playerColor);
        if (__blib_unlikely(error != sc2img::Sc2imgError::None))
        {
            return error;
        }

        buint32 exportedCount = 0;
        return sc2img::exportAll(document, outDir, options, nullptr, exportedCount);
    }

    /**
     * Рекурсивный обход папки (blib::core::Folder): для каждого
     * .tgx/.gm1 — конвертация; подпапки — только при --recursive.
     */
    void processFolder(
        _In const std::string& dir,
        _In const std::string& outDir,
        _In const sc2img::ExportOptions& options,
        _In bool recursive,
        _In bool usePlayerColorOverride,
        _In buint8 playerColor,
        _Out buint32& errorCount)
    {
        // Folder перечисляет каталог маской "<path>*" — путь обязан
        // заканчиваться слэшем, иначе маска слипается с именем каталога
        std::string listingDir = dir;
        if (listingDir.back() != pathSeparator && listingDir.back() != '/')
        {
            listingDir += pathSeparator;
        }

        blib::core::Folder folder(listingDir);
        const std::vector<std::string> names = folder.getAllEntries();

        for (const std::string& name : names)
        {
            // getAllEntries возвращает в том числе "." и ".." (CORE.md)
            if (name == "." || name == "..")
            {
                continue;
            }

            const std::string fullPath = listingDir + name;

            blib::core::Folder child(fullPath);
            if (child.isFolder())
            {
                if (recursive)
                {
                    processFolder(fullPath, outDir, options, recursive,
                        usePlayerColorOverride, playerColor, errorCount);
                }
                continue;
            }

            if (sc2img::detectSourceFormat(fullPath) == sc2img::SourceFormat::Unknown)
            {
                continue;
            }

            const sc2img::Sc2imgError error = convertOneFile(
                fullPath, outDir, options, usePlayerColorOverride, playerColor);
            if (__blib_unlikely(error != sc2img::Sc2imgError::None))
            {
                ++errorCount;
            }
        }
    }

    /**
     * Headless-режим: конвертация без окна (GUI-ветка не инициализирует
     * рендер). Синтаксис:
     *   sc2img --headless <файл|папка> [--out каталог]
     *           [--format png|bmp|jpg] [--align] [--player-color N]
     *           [--recursive]
     */
    int runHeadless(_In int argc, _In char* argv[])
    {
        std::string inputPath;
        std::string outDir;
        sc2img::ExportOptions options;
        options.format = sc2img::OutputFormat::Png;
        options.alignByOffsets = false;

        bool recursive = false;
        bool usePlayerColorOverride = false;
        buint32 playerColor = 0;

        for (int i = 2; i < argc; ++i)
        {
            const char* argument = argv[i];
            if (std::strcmp(argument, outDirFlag) == 0 && i + 1 < argc)
            {
                outDir = argv[++i];
            }
            else if (std::strcmp(argument, formatFlag) == 0 && i + 1 < argc)
            {
                const char* formatName = argv[++i];
                if (std::strcmp(formatName, pngFormatName) == 0)
                {
                    options.format = sc2img::OutputFormat::Png;
                }
                else if (std::strcmp(formatName, bmpFormatName) == 0)
                {
                    options.format = sc2img::OutputFormat::Bmp;
                }
                else if (std::strcmp(formatName, jpgFormatName) == 0)
                {
                    options.format = sc2img::OutputFormat::Jpeg;
                }
                else
                {
                    __blib_log_error("sc2img: unknown format '%s' (expected png|bmp|jpg)", formatName);
                    return 1;
                }
            }
            else if (std::strcmp(argument, alignFlag) == 0)
            {
                options.alignByOffsets = true;
            }
            else if (std::strcmp(argument, playerColorFlag) == 0 && i + 1 < argc)
            {
                const long parsed = std::strtol(argv[++i], nullptr, 10);
                if (parsed < 0 || parsed > maxPlayerColorIndex)
                {
                    __blib_log_error("sc2img: player color out of range: %ld (0..%u)",
                        parsed, maxPlayerColorIndex);
                    return 1;
                }
                playerColor = static_cast<buint32>(parsed);
                usePlayerColorOverride = true;
            }
            else if (std::strcmp(argument, recursiveFlag) == 0)
            {
                recursive = true;
            }
            else if (argument[0] == '-')
            {
                __blib_log_error("sc2img: unknown option '%s'", argument);
                return 1;
            }
            else
            {
                inputPath = argument;
            }
        }

        if (inputPath.empty())
        {
            __blib_log_info("usage: sc2img --headless <file|folder> [--out dir] "
                "[--format png|bmp|jpg] [--align] [--player-color N] [--recursive]");
            return 1;
        }

        buint32 errorCount = 0;

        // Файл или папка (Win32-атрибуты — надёжнее, чем Folder:
        // его конструктор «поднимается» от не-папок вверх). Папка —
        // обход (рекурсия по --recursive)
        const DWORD attributes = GetFileAttributesA(inputPath.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            __blib_log_error("sc2img: input path not found: %s", inputPath.c_str());
            return 1;
        }

        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            processFolder(inputPath, outDir, options, recursive,
                usePlayerColorOverride, static_cast<buint8>(playerColor), errorCount);
        }
        else
        {
            const sc2img::Sc2imgError error = convertOneFile(
                inputPath, outDir, options, usePlayerColorOverride,
                static_cast<buint8>(playerColor));
            if (__blib_unlikely(error != sc2img::Sc2imgError::None))
            {
                ++errorCount;
            }
        }

        return errorCount == 0 ? 0 : 1;
    }

    /**
     * GUI-режим: окно утилиты (хост EditorApplication). Первый
     * аргумент командной строки (если есть) — файл, открываемый
     * на старте.
     */
    int runGui(_In int argc, _In char* argv[])
    {
        sc2img::Sc2imgApp app;
        if (!app.initialize())
        {
            __blib_fatal("Failed to initialize sc2img application");
        }

        if (argc > 1)
        {
            app.openFile(std::string(argv[1]));
        }

        // Главный цикл принадлежит тонкому exe (паттерн «lib + тонкий exe»)
        while (app.isRunning())
        {
            app.tick();
        }

        app.shutdown();
        return 0;
    }
}

int main(int argc, char* argv[])
{
    // CLI-приложение: дублировать лог в stdout (паттерн vochat)
    blib::console::Console::instance().getOutput().setStdoutEcho(true);

    if (argc > 1 && std::strcmp(argv[1], headlessFlag) == 0)
    {
        return runHeadless(argc, argv);
    }
    return runGui(argc, argv);
}
