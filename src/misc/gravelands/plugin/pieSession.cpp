#include <gravelands/plugin/pieSession.h>

#include <blib/core/console/console.h>

namespace gravelands
{
    PieSession::PieSession()
        : server()
        , client()
        , running(false)
        , paused(false)
    {
    }

    PieSession::~PieSession()
    {
        this->stop();
    }

    bool PieSession::start(buint32 port)
    {
        if (this->running)
        {
            return true; // уже запущена
        }

        // Сервер первым: клиент подключается к нему же (loopback TCP —
        // настоящий сетевой путь, как в продакшене)
        if (!this->server.initialize(port))
        {
            __blib_log_error("PIE: failed to start server on port %u", port);
            return false;
        }

        // Клиент в PIE — без собственного ImGui (контекст эдитора уже
        // есть; второй контекст сломал бы его кадр)
        if (!this->client.initialize(false))
        {
            __blib_log_error("PIE: failed to initialize client");
            this->server.shutdown();
            return false;
        }

        this->running = true;
        this->paused = false;
        __blib_log_info("PIE session started (port %u)", port);
        return true;
    }

    void PieSession::tick()
    {
        // Пауза (Pause в Unity): мир замирает целиком — симуляция и
        // сеть не двигаются, последний кадр остаётся в FBO клиента
        if (!this->running || this->paused)
        {
            return;
        }

        this->server.tick();
        this->client.tick();
    }

    void PieSession::stop()
    {
        if (!this->running)
        {
            return;
        }

        // Клиент первым (его окно/GL), затем сервер
        this->client.shutdown();
        this->server.shutdown();

        this->running = false;
        this->paused = false;
        __blib_log_info("PIE session stopped");
    }

    buint64 PieSession::getClientColorTextureId() const
    {
        // Вне запущенной сессии текстуры клиента уничтожены
        // (client.shutdown) — наружу не отдаём
        if (!this->running)
        {
            return 0;
        }

        return this->client.getColorTextureId();
    }

} // namespace gravelands
