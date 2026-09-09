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

    // ========== Клиентское окно ==========

    // Размеры окна клиента по умолчанию (в пикселях)
    constexpr buint32 windowWidth = 1800;
    constexpr buint32 windowHeight = 1000;

} // namespace gravelands
