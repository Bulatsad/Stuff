#include <beng/server/worldManager.h>

#include <beng/core/scene.h>
#include <beng/server/iServerGame.h>

#include <blib/core/console/console.h>
#include <blib/core/fileStream.h>

namespace beng
{
    namespace server
    {
        WorldManager::WorldManager()
            : scene(nullptr)
            , game(nullptr)
        {
        }

        void WorldManager::bind(_In Scene& scene, _In IServerGame& game)
        {
            this->scene = &scene;
            this->game = &game;
        }

        void WorldManager::buildWorld()
        {
            // Сброс данных с сохранением реестра типов (Scene::reset),
            // затем хук игры строит контент
            this->scene->reset();

            // Серверные (реплицируемые) сущности — с высокой базы ID:
            // клиент воспроизводит их в зеркале через createEntityWithId
            // (контракт id ≥ nextEntityId), а сцена клиента занята
            // локальным контентом с низкими ID — пространства не
            // пересекаются (см. serverEntityIdBase в scene.h)
            this->scene->setNextEntityId(serverEntityIdBase);

            this->game->onWorldBuild(*this->scene);
        }

        bool WorldManager::saveWorld(_In const char* path)
        {
            blib::core::FileStream file;
            blib::core::FileStream::OpenModeFlags mode;
            mode.storage |= static_cast<buint8>(blib::core::OpenMode::Write);
            mode.storage |= static_cast<buint8>(blib::core::OpenMode::Truncate);

            if (file.open(path, mode) != blib::core::FileStatus::OK)
            {
                __blib_log_error("WorldManager: cannot open '%s' for save", path);
                return false;
            }

            const blib::core::SaveStatus status = this->scene->save(file);
            if (status != blib::core::SaveStatus::None)
            {
                __blib_log_error("WorldManager: scene save failed (status %u)", static_cast<buint32>(status));
                return false;
            }
            return true;
        }

        bool WorldManager::loadWorld(_In const char* path)
        {
            blib::core::FileStream file;
            blib::core::FileStream::OpenModeFlags mode;
            mode.storage |= static_cast<buint8>(blib::core::OpenMode::Read);

            if (file.open(path, mode) != blib::core::FileStatus::OK)
            {
                __blib_log_error("WorldManager: cannot open '%s' for load", path);
                return false;
            }

            // load требует пустую сцену: сброс без сноса реестра типов
            this->scene->reset();

            const blib::core::LoadStatus status = this->scene->load(file);
            if (status != blib::core::LoadStatus::None)
            {
                __blib_log_error("WorldManager: scene load failed (status %u)", static_cast<buint32>(status));
                return false;
            }

            // Строгая проверка round-trip (результат — в логе)
            if (!this->scene->verify())
            {
                __blib_log_warning("WorldManager: scene verify after load failed");
            }

            return true;
        }

    } // namespace server
} // namespace beng
