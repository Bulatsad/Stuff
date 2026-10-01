#pragma once

#include <blib/blibint.h>

namespace gravelands
{
    // ========== Общие константы игры Gravelands ==========
    // В этом модуле живут определения, нужные и клиенту, и серверу:
    // константы, позже — общие компоненты ECS, пакеты протокола, формулы.

    // Название игры (заголовок окна, лог-префиксы)
    constexpr const char* gameTitle = "Gravelands";

    // Стабильный id игры: имя игрового модуля эдитора (контракт
    // GameModuleFunctions — см. BENG.md), имя gravelands.dll и
    // значение опции beng-editor.exe --game=gravelands. Нижний
    // регистр; не меняется при переименовании заголовков/титулов
    constexpr const char* gameModuleName = "gravelands";

    // ========== Симуляция (сервер) ==========

    // Тикрейт авторитетной симуляции: тиков в секунду.
    // Сервер шагает симуляцию фиксированными шагами (детерминизм,
    // квантованные снапшоты); клиент рендерит с переменным dt.
    // 60 Гц — вдвое меньше квантование тиков и рендер-задержка
    // интерполяции (2 тика = 33 мс) против 30 Гц
    constexpr buint32 serverTickRate = 60;

    // Длительность одного фиксированного тика симуляции (секунд)
    constexpr float serverFixedDelta = 1.0f / static_cast<float>(serverTickRate);

    // ========== Сеть ==========

    // Порт локального сервера (loopback; PIE и local-server mode
    // используют тот же)
    constexpr buint32 serverDefaultPort = 42777;

    // Скорость перемещения игрового юнита (мир. ед./с)
    constexpr float playerMoveSpeed = 60.0f;

    // Порог реконсиляции движкового client-side prediction игрока
    // (мир. ед.; передаётся в ClientApplicationParams — см. clientCore.cpp):
    // при расхождении предсказанной позиции со свежайшим серверным
    // сэмплом больше этой дистанции предсказание снапается на сервер.
    // Штатное расхождение — v·латентность кадра (движение одинаковое на
    // обеих сторонах, сервер просто отстаёт) — порог заведомо выше
    constexpr float predictionSnapDistance = 80.0f;

    // Радиус визуализации игрового юнита (сфера-плейсхолдер клиента;
    // визуал живёт на клиенте, радиус — общий контракт)
    constexpr float playerVisualRadius = 25.0f;

    // Стартовая позиция игрока на плоскости XZ
    constexpr float playerStartX = 0.0f;
    constexpr float playerStartZ = -60.0f;

    // Границы мира для юнитов (квадрат на XZ, от -границы до +границы)
    constexpr float worldBounds = 150.0f;

    // ========== Клиентское окно ==========

    // Размеры окна клиента по умолчанию (в пикселях)
    constexpr buint32 windowWidth = 1800;
    constexpr buint32 windowHeight = 1000;

    // Размеры окна клиента в PIE-режиме: меньше обычного — клиентское
    // окно в PIE рендерится ПАРАЛЛЕЛЬНО вьюпорту эдитора на одном
    // потоке (двойной GL-контекст), кадр и так дорог
    constexpr buint32 pieWindowWidth = 1280;
    constexpr buint32 pieWindowHeight = 720;

} // namespace gravelands
