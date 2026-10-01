#include <gravelands/client/core/clientCore.h>

#include <gravelands/common/config.h>

namespace gravelands
{
    bool ClientCore::initialize(bool imguiEnabled)
    {
        beng::client::ClientApplicationParams params;
        params.width = imguiEnabled ? windowWidth : pieWindowWidth;
        params.height = imguiEnabled ? windowHeight : pieWindowHeight;
        params.title = gameTitle;
        params.imguiEnabled = imguiEnabled;
        // Движковый client-side prediction игрока: оболочка шлёт
        // WASD-команды с dedup, реконсилирует предсказание со свежайшим
        // серверным сэмплом и перекрывает зеркало (см. CLIENT.md);
        // порог снапа — общий контракт игры (config.h)
        params.predictionEnabled = true;
        params.predictionSnapDistance = predictionSnapDistance;

        if (!this->application.initialize(this->game, params))
        {
            return false;
        }

        // Local-server mode: одиночный запуск клиента сам хостит
        // авторитетный сервер in-process (сетевой путь — настоящий
        // loopback TCP; startLocalServer сам коннектит клиента к
        // поднятому серверу). Порт занят (внешний/PIE-сервер уже
        // слушает) — startLocalServer вернёт false, клиент подключится
        // к внешнему серверу обычным connect()
        if (!this->application.startLocalServer(this->localServerGame, serverDefaultPort))
        {
            this->application.getReplicationClient().connect(serverDefaultPort);
        }

        return true;
    }

    void ClientCore::tick()
    {
        this->application.tick();
    }

    void ClientCore::shutdown()
    {
        this->application.shutdown();
    }

    bool ClientCore::isRunning() const
    {
        return this->application.isRunning();
    }

    buint64 ClientCore::getColorTextureId() const
    {
        return this->application.getColorTextureId();
    }

} // namespace gravelands
