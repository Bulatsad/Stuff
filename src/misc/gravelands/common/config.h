#pragma once

#include <blib/blibint.h>

namespace gravelands
{
    // ========== Общие константы игры Gravelands ==========
    // В этом модуле живут определения, нужные и клиенту, и серверу:
    // константы, позже — общие компоненты ECS, пакеты протокола, формулы.

    // Название игры (заголовок окна, лог-префиксы)
    constexpr const char* gameTitle = "Gravelands";

    // ========== Симуляция (сервер) ==========

    // Тикрейт авторитетной симуляции: тиков в секунду.
    // Сервер шагает симуляцию фиксированными шагами (детерминизм,
    // квантованные снапшоты); клиент рендерит с переменным dt.
    constexpr buint32 serverTickRate = 30;

    // Длительность одного фиксированного тика симуляции (секунд)
    constexpr float serverFixedDelta = 1.0f / static_cast<float>(serverTickRate);

    // ========== Сеть ==========

    // Порт локального сервера (loopback; PIE использует тот же)
    constexpr buint32 serverDefaultPort = 42777;

    // Максимальный размер одного сетевого сообщения (заголовок +
    // полезная нагрузка, байт) — буферы фреймеров/декодеров
    constexpr buint32 maxPacketBytes = 2048;

    // Максимум юнитов в одном снапшоте (буферы сервера и клиента)
    constexpr buint32 maxNetworkUnits = 64;

    // Версия сетевого протокола (заголовок сообщений не содержит —
    // сверка при рукопожатии Welcome; рассинхрон версий — отказ)
    constexpr buint16 protocolVersion = 1;

    // Буфер интерполяции клиента: количество хранимых снапшотов
    // (между последними двумя интерполируем)
    constexpr buint32 snapshotInterpolationBufferSize = 2;

    // Скорость перемещения игрового юнита (мир. ед./с)
    constexpr float playerMoveSpeed = 60.0f;

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

} // namespace gravelands
