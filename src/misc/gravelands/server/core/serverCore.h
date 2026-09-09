#pragma once

#include <gravelands/common/config.h>

#include <beng/core/scene.h>
#include <beng/core/time.h>
#include <beng/systems/transformSystem.h>

namespace gravelands
{
    /**
     * ServerCore — авторитетное ядро сервера Gravelands.
     * 
     * Назначение:
     * - Владеет авторитетной Scene (ECS beng-core) и её системами
     * - Шагает симуляцию фиксированными тиками (serverFixedDelta):
     *   реальное время накапливается в аккумуляторе, остаток
     *   догоняется следующими тиками — время не теряется
     * - Сетевой слой (TCP) подключится позже поверх этого ядра
     * 
     * Паттерн «lib + тонкий exe»: ServerCore даёт frame-API
     * (initialize/tick/shutdown) и НЕ владеет главным циклом —
     * цикл крутит тонкий exe (main.cpp). Этим же API сможет
     * пользоваться эдитор (Play mode).
     */
    class ServerCore
    {
    public:
        ServerCore();
        ~ServerCore() = default;

        // Ядро некопируемо: Scene держит указатель на собственный аллокатор
        ServerCore(const ServerCore&) = delete;
        ServerCore& operator=(const ServerCore&) = delete;
        ServerCore(ServerCore&&) = delete;
        ServerCore& operator=(ServerCore&&) = delete;

        /**
         * Инициализировать ядро: регистрация компонентов, систем, сброс таймера.
         * Вызывать один раз перед циклом.
         * 
         * @return true при успехе (пока всегда, зарезервировано под будущие сбои)
         */
        bool initialize();

        /**
         * Один фрейм ядра: измеряет реальное dt и шагает симуляцию
         * фиксированными тиками, пока аккумулятор позволяет.
         * Вызывается из цикла тонкого exe каждый кадр.
         */
        void tick();

        /**
         * Корректно остановить ядро.
         */
        void shutdown();

        /**
         * Работает ли ядро (флаг выключается в shutdown).
         * Условие продолжения цикла в тонком exe.
         */
        bool isRunning() const { return running; }

    private:
        // Авторитетная сцена: все Entity/компоненты/системы живут здесь
        beng::Scene scene;

        // Пересчёт мировых координат Transform-иерархии (Scene хранит сырой указатель)
        beng::TransformSystem transformSystem;

        // Таймер реального времени (dt между вызовами tick)
        beng::Time time;

        // Аккумулятор реального времени для фиксированных тиков (секунды)
        float accumulator;

        // Счётчик выполненных тиков (для heartbeat-логов)
        buint64 tickCounter;

        // Номер последней залогированной секунды симуляции
        // (защита от повторного heartbeat в кадрах без тиков)
        buint64 lastHeartbeatSecond;

        // Флаг жизни ядра
        bool running;
    };

} // namespace gravelands
