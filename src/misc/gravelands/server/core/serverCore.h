#pragma once

#include <gravelands/common/config.h>
#include <gravelands/server/core/gravelandsServerGame.h>

#include <beng/core/scene.h>
#include <beng/server/serverApplication.h>

namespace gravelands
{
    /**
     * ServerCore — авторитетное ядро сервера Gravelands: тонкая
     * обёртка над движковым beng::server::ServerApplication + игровой
     * стороной GravelandsServerGame.
     *
     * Собственной симуляции/сети у игры больше нет: фиксированный
     * тикрейт, сетевой цикл (poll → команды → тики → снапшоты) и
     * репликация — движок (см. SERVER.md); игра даёт типы, системы,
     * юниты и кодек команд.
     *
     * Паттерн «lib + тонкий exe»: ServerCore даёт frame-API
     * (initialize/tick/shutdown) и НЕ владеет главным циклом —
     * цикл крутит тонкий exe (main.cpp). Этим же API пользуется
     * эдитор (Play mode — in-process хостинг, см. ARCHITECTURE.md)
     * и local-server mode клиента.
     */
    class ServerCore
    {
    public:
        ServerCore() = default;
        ~ServerCore() = default;

        // Ядро некопируемо: ServerApplication держит сцену/сеть
        ServerCore(const ServerCore&) = delete;
        ServerCore& operator=(const ServerCore&) = delete;
        ServerCore(ServerCore&&) = delete;
        ServerCore& operator=(ServerCore&&) = delete;

        /**
         * Инициализировать ядро: регистрация типов/систем игры,
         * контент мира, сетевой слушатель. Повторный запуск (PIE)
         * безопасен: регистрация с guard'ом, сцена сбрасывается,
         * слушатель пересоздаётся (см. GravelandsServerGame).
         *
         * @param port Порт loopback-слушателя (дефолт — serverDefaultPort)
         * @return false — сеть недоступна (порт занят)
         */
        bool initialize(buint32 port = serverDefaultPort);

        /**
         * Один фрейм ядра: опрос сети → команды → шаг симуляции
         * фиксированными тиками (аккумулятор) → снапшоты клиентам.
         */
        void tick();

        /**
         * Корректно остановить ядро (сеть + сцена).
         */
        void shutdown();

        /**
         * Работает ли ядро (условие продолжения цикла в тонком exe).
         */
        bool isRunning() const;

        /**
         * Авторитетная сцена (для диагностики/PIE).
         */
        beng::Scene& getScene();

    private:
        // Игра раньше ядра в списке членов — разрушается ПОСЛЕ него
        // (ServerApplication держит указатель на IServerGame)
        GravelandsServerGame game;
        beng::server::ServerApplication server;
    };

} // namespace gravelands
