#pragma once

#include <beng/config.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

namespace beng
{
    class Scene;

    namespace server
    {
        class IServerGame;

        /**
         * WorldManager — персистентность мира сервера (beng-server):
         * тонкая обёртка над Scene::save/load (формат sceneSaveFormat)
         * + пересборка контента через хук игры.
         *
         * Миром ВЛАДЕЕТ ServerApplication (сцена — его); WorldManager —
         * фасад: сцена привязывается bind() и используется по указателю.
         *
         * - buildWorld: сброс сцены (реестр типов сохраняется) + хук
         *   игры onWorldBuild — начальный контент;
         * - resetWorld: то же, что buildWorld (сброс = пересборка);
         * - saveWorld/loadWorld: файловый round-trip сцены (load требует
         *   пустую сцену — вызывающий делает resetWorld заранее или
         *   loadWorld сам сбрасывает сцену).
         */
        class __beng_api WorldManager
        {
        public:
            WorldManager();
            ~WorldManager() = default;

            WorldManager(const WorldManager&) = delete;
            WorldManager& operator=(const WorldManager&) = delete;

            /**
             * Привязать сцену и игру (до любых операций).
             */
            void bind(_In Scene& scene, _In IServerGame& game);

            /**
             * Сбросить сцену и построить контент мира (хук onWorldBuild).
             * Реестр типов/системы сохраняются (Scene::reset).
             */
            void buildWorld();

            /**
             * Сохранить мир в файл (формат sceneSaveFormat).
             * @return false — сцена не сериализуема/сбой записи
             */
            bool saveWorld(_In const char* path);

            /**
             * Загрузить мир из файла: сцена сбрасывается (реестр типов
             * сохраняется) и загружается; при неудаче остаётся пустой
             * (вызывающий может buildWorld() как фолбэк).
             * @return false — файл битый/несовместимый
             */
            bool loadWorld(_In const char* path);

        private:
            Scene* scene;
            IServerGame* game;
        };

    } // namespace server
} // namespace beng
