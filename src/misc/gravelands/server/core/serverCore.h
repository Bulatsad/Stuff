#pragma once

#include <gravelands/common/config.h>
#include <gravelands/common/protocol.h>
#include <gravelands/server/core/movementSystem.h>
#include <gravelands/server/core/networkServer.h>

#include <beng/core/scene.h>
#include <beng/core/time.h>
#include <beng/systems/transformSystem.h>

namespace gravelands
{
    /**
     * ServerCore — авторитетное ядро сервера Gravelands.
     *
     * Назначение:
     * - Владеет авторитетной Scene (ECS beng-core) и её системами;
     * - Шагает симуляцию фиксированными тиками (serverFixedDelta):
     *   реальное время накапливается в аккумуляторе, остаток
     *   догоняется следующими тиками — время не теряется;
     * - Сетевой слой: NetworkServer (MVP: один клиент, loopback) —
     *   команды игрока применяются к его юниту, после каждого тика
     *   клиенту шлётся снапшот авторитетных позиций.
     *
     * Паттерн «lib + тонкий exe»: ServerCore даёт frame-API
     * (initialize/tick/shutdown) и НЕ владеет главным циклом —
     * цикл крутит тонкий exe (main.cpp). Этим же API пользуется
     * эдитор (Play mode — in-process хостинг, см. ARCHITECTURE.md).
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
         * Инициализировать ядро: регистрация компонентов, систем,
         * создание игрока, сетевой слушатель, сброс таймера.
         * Вызывать один раз перед циклом.
         *
         * @param port Порт loopback-слушателя (дефолт — serverDefaultPort)
         * @return true при успехе (сеть может быть недоступна — порт занят)
         */
        bool initialize(buint32 port = serverDefaultPort);

        /**
         * Один фрейм ядра: опрос сети → команды → шаг симуляции
         * фиксированными тиками (аккумулятор) → снапшоты клиенту.
         * Вызывается из цикла тонкого exe каждый кадр.
         */
        void tick();

        /**
         * Корректно остановить ядро (сеть + лог статистики).
         */
        void shutdown();

        /**
         * Работает ли ядро (флаг выключается в shutdown).
         * Условие продолжения цикла в тонком exe.
         */
        bool isRunning() const { return running; }

        /**
         * Авторитетная сцена (для диагностики/PIE).
         */
        beng::Scene& getScene() { return scene; }

    private:
        // Максимум юнитов в снапшоте (буфер сбора позиций)
        static constexpr buint32 maxSnapshotUnits = 64;

        // Авторитетная сцена: все Entity/компоненты/системы живут здесь
        beng::Scene scene;

        // Пересчёт мировых координат Transform-иерархии (Scene хранит сырой указатель)
        beng::TransformSystem transformSystem;

        // Перемещение юнитов по входным векторам (приоритет 0)
        MovementSystem movementSystem;

        // Сетевая сторона сервера (клиент/команды/снапшоты)
        NetworkServer networkServer;

        // Таймер реального времени (dt между вызовами tick)
        beng::Time time;

        // Аккумулятор реального времени для фиксированных тиков (секунды)
        float accumulator;

        // Счётчик выполненных тиков (для heartbeat-логов и снапшотов)
        buint64 tickCounter;

        // Номер последней залогированной секунды симуляции
        // (защита от повторного heartbeat в кадрах без тиков)
        buint64 lastHeartbeatSecond;

        // Сущность игрока (авторитетный юнит, управляется сетью)
        beng::EntityID playerEntity;

        // Флаг жизни ядра
        bool running;

        // Сбор и отправка снапшота за прошедший тик
        void broadcastSnapshot();
    };

} // namespace gravelands
