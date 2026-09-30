#pragma once

#include <beng/config.h>
#include <blib/blibint.h>

namespace beng
{
namespace editor
{
    class EditorApplication;

    // Версия контракта «эдитор ↔ игра» (GameModuleFunctions). Эдитор
    // сверяет её с версией модуля игры: рассинхрон — отказ от загрузки
    // (fatal) — контракт должен быть ABI-стабилен между релизами движка
    constexpr buint32 gameModuleContractVersion = 1;

    // Стабильное экспортируемое имя точки входа игровой DLL:
    // единственный символ, который эдитор берёт через GetProcAddress
    // (extern "C" — недекорированное имя). Движок не знает игровых
    // имён — все игры экспортируют один и тот же контракт
    constexpr const char* gameModuleEntryName = "bengGetGameModule";

    /**
     * GameModuleFunctions — ABI-контракт игрового модуля эдитора
     * (плагин-модель, см. ARCHITECTURE.md, «Эдитор»).
     *
     * Единственная точка стыковки «эдитор ↔ игра»: exe эдитора
     * получает структуру от игры (статическая линковка — вызовом
     * точки входа напрямую; DLL — экспорт bengGetGameModule) и
     * работает ТОЛЬКО через неё — конкретных игровых типов не знает.
     *
     * Структура — POD, стабильный ABI; функции и память структуры
     * принадлежат модулю игры (обычно статическая — освобождать
     * не нужно).
     */
    struct GameModuleFunctions
    {
        // Стабильный id игры (нижний регистр): имя DLL без расширения
        // и значение опции --game. Не меняется при переименовании
        // заголовков/титулов игры — это контрактный идентификатор
        const char* (*getGameName)();

        // Фабрика хоста эдитора игры (подкласс EditorApplication).
        // Память — GlobalAllocator (shared blib — один на процесс);
        // владелец гасит хост ПАРНОЙ функцией destroyEditorHost
        // (конкретный тип хоста известен только модулю игры)
        EditorApplication* (*createEditorHost)();

        // Парная фабрике функция уничтожения хоста: гасит и возвращает
        // память GlobalAllocator'у (вызывающий не знает конкретного
        // типа — сам деструктор звать не может, см. ARCHITECTURE.md)
        void (*destroyEditorHost)(_In EditorApplication* host);

        // Версия контракта, с которой собран модуль игры
        // (сверка с gameModuleContractVersion — см. выше)
        buint32 contractVersion;
    };
} // namespace editor
} // namespace beng
