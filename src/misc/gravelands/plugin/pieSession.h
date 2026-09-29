#pragma once

#include <gravelands/client/core/clientCore.h>
#include <gravelands/common/config.h>
#include <gravelands/server/core/serverCore.h>

namespace gravelands
{
    /**
     * PieSession — Play In Editor: in-process хостинг пары ядер игры
     * (авторитетный сервер + клиент с собственным окном), связь —
     * настоящий loopback TCP (сетевой код-путь идентичен продакшену
     * с двумя exe — см. ARCHITECTURE.md, «Эдитор»).
     *
     * Жизненный цикл:
     * - start(): сервер слушает порт, затем клиент подключается к нему;
     *   клиент в PIE-режиме — без собственного ImGui (контекст эдитора);
     * - tick(): server.tick() + client.tick() (вызывается из хука каркаса);
     * - stop(): клиент гасится первым (его окно/GL), затем сервер.
     */
    class PieSession
    {
    public:
        PieSession();
        ~PieSession();

        PieSession(const PieSession&) = delete;
        PieSession& operator=(const PieSession&) = delete;

        /**
         * Запустить сессию (сервер + клиент).
         * @param port Порт loopback-сервера (дефолт — serverDefaultPort)
         * @return false — порт занят/сеть недоступна
         */
        bool start(buint32 port = serverDefaultPort);

        /**
         * Один кадр сессии: сервер + клиент.
         */
        void tick();

        /**
         * Остановить сессию (клиент → сервер, идемпотентно).
         */
        void stop();

        /**
         * Сессия запущена?
         */
        bool isRunning() const { return running; }

    private:
        gravelands::ServerCore server;
        gravelands::ClientCore client;
        bool running;
    };

} // namespace gravelands
