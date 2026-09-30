#pragma once

#include <beng/config.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

namespace beng
{
    class Scene;

    namespace server
    {
        /**
         * IServerGame — хук-интерфейс игры для серверного ядра beng
         * (паттерн хуков EditorApplication, см. BENG.md): всё, что
         * относится к КОНКРЕТНОЙ игре (типы, системы, контент, игроки,
         * кодек команд), живёт в реализации этого интерфейса;
         * ServerApplication — игра-агностичен.
         *
         * Порядок вызовов жизненного цикла:
         * - initialize: onServerInitialize (регистрация типов/систем) →
         *   onWorldBuild (контент мира);
         * - подключение клиента: onClientJoin → Welcome;
         * - каждый кадр: onClientCommand (команды) → тики симуляции
         *   (системы сцены, scene.update);
         * - отключение клиента: onClientLeave;
         * - сброс/загрузка мира: onWorldBuild заново (см. WorldManager).
         */
        class __beng_api IServerGame
        {
        public:
            virtual ~IServerGame() = default;

            /**
             * Стабильное имя игры (префикс логов).
             */
            virtual const char* getGameName() const __blib_pure_virtual_function;

            /**
             * Тикрейт авторитетной симуляции (тиков в секунду).
             * Фиксированный шаг = 1 / tickRate.
             */
            virtual buint32 getTickRate() const __blib_pure_virtual_function;

            /**
             * Порт loopback-слушателя по умолчанию (0 — не задан,
             * вызывающий обязан передать порт явно).
             */
            virtual buint32 getDefaultPort() const __blib_pure_virtual_function;

            /**
             * Инициализация игровой стороны: регистрация типов
             * компонентов (scene.registerComponentType<T>) и систем
             * (scene.addSystem). Вызывается ДО построения схемы
             * репликации и контента мира.
             */
            virtual void onServerInitialize(_In Scene& scene) __blib_pure_virtual_function;

            /**
             * Построить контент мира (первый старт и после сброса/
             * загрузки — см. WorldManager). Сцена в этот момент пуста.
             */
            virtual void onWorldBuild(_In Scene& scene) __blib_pure_virtual_function;

            /**
             * Клиент подключился (до Welcome): создать игрока и вернуть
             * его сущность (она попадёт в Welcome как playerEntityId);
             * invalidEntity — наблюдатель без юнита.
             */
            virtual EntityID onClientJoin(buint32 clientId) __blib_pure_virtual_function;

            /**
             * Клиент отключился: убрать его игрока/состояние.
             */
            virtual void onClientLeave(buint32 clientId) __blib_pure_virtual_function;

            /**
             * Команда игрока (payload — игра-специфичный кодек; движок
             * возит непрозрачные байты, сервер не доверяет клиенту).
             */
            virtual void onClientCommand(buint32 clientId,
                _In const buint8* payload, buint32 payloadSize) __blib_pure_virtual_function;
        };

    } // namespace server
} // namespace beng
