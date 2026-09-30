#pragma once

#include <gravelands/client/core/clientCore.h>
#include <gravelands/common/config.h>
#include <gravelands/server/core/serverCore.h>

namespace gravelands
{
    /**
     * PieSession — Play In Editor: in-process хостинг пары ядер игры
     * (авторитетный сервер + headless-клиент), связь — настоящий
     * loopback TCP (сетевой код-путь идентичен продакшену с двумя
     * exe — см. ARCHITECTURE.md, «Эдитор»).
     *
     * Клиент в PIE — без ОС-окна и своего GL-контекста: рендерит
     * в свой FBO в контексте эдитора, кадр показывает Game-панель
     * эдитора (см. GRAVELANDS.md, «PIE»).
     *
     * Жизненный цикл:
     * - start(): сервер слушает порт, затем клиент подключается к нему;
     * - tick(): server.tick() + client.tick() (вызывается из хука каркаса);
     *   на паузе оба пропускаются (мир замирает);
     * - stop(): клиент гасится первым (его GL-ресурсы), затем сервер.
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
         * Один кадр сессии: сервер + клиент. На паузе — no-op
         * (мир замирает, Game-панель держит последний кадр).
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

        /**
         * Пауза (как Pause в Unity): сервер и клиент не тикают —
         * симуляция и сеть замирают, последний кадр остаётся в FBO.
         * На остановленной сессии флаг не имеет эффекта; start()
         * сбрасывает паузу.
         */
        void setPaused(bool paused) { this->paused = paused; }
        bool isPaused() const { return this->paused; }

        /**
         * Идентификатор GL-текстуры текущего кадра PIE-клиента
         * (цвет FBO) — вход Game-панели эдитора. 0 — сессия не
         * запущена (или не инициализирована).
         */
        buint64 getClientColorTextureId() const;

    private:
        gravelands::ServerCore server;
        gravelands::ClientCore client;
        bool running;
        bool paused;
    };

} // namespace gravelands
