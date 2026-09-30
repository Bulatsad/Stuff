#include <gravelands/server/core/serverCore.h>

namespace gravelands
{
    bool ServerCore::initialize(buint32 port)
    {
        return this->server.initialize(this->game, port);
    }

    void ServerCore::tick()
    {
        this->server.tick();
    }

    void ServerCore::shutdown()
    {
        this->server.shutdown();
    }

    bool ServerCore::isRunning() const
    {
        return this->server.isRunning();
    }

    beng::Scene& ServerCore::getScene()
    {
        return this->server.getScene();
    }

} // namespace gravelands
