#pragma once

#include <beng/config.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

namespace beng
{
    class Scene;

    namespace client
    {
        class ClientApplication;

        /**
         * IClientGame — хук-интерфейс игры для клиентского ядра beng
         * (паттерн композиции IServerGame, см. BENG.md): всё, что
         * относится к КОНКРЕТНОЙ игре (типы, системы, контент, ввод,
         * оверлей, кодек команд), живёт в реализации этого интерфейса;
         * ClientApplication — игра-агностичен и владеет оболочкой
         * (окно, рендер, камера, ImGui, сеть, ECS-пайплайн).
         *
         * Порядок вызовов жизненного цикла:
         * - initialize: onClientInitialize (регистрация типов/систем,
         *   контент, подключение к серверу через application);
         * - каждый кадр tick(): onInput → onNetworkUpdate →
         *   onSceneWillUpdate → scene.update (системы, рендер) →
         *   onSceneDidUpdate → onUi (внутри ImGui-кадра оболочки);
         * - сеть: Welcome → onSessionReady; разрыв сессии →
         *   onSessionLost (повторный Welcome — снова onSessionReady);
         * - shutdown: onShutdown (мир и сеть ещё живы).
         *
         * Интерполяция снапшотов и зеркала юнитов — движковые
         * (ReplicationClient/ReplicationClientState): игра сливает
         * события спавна/уничтожения зеркал в onNetworkUpdate через
         * application.getReplicationClient().getMirror() и вешает на
         * зеркала свой визуал.
         */
        class __beng_api IClientGame
        {
        public:
            virtual ~IClientGame() = default;

            /**
             * Стабильное имя игры (префикс логов, заголовок окна).
             */
            virtual const char* getGameName() const __blib_pure_virtual_function;

            /**
             * Инициализация игровой стороны: регистрация типов
             * компонентов (scene.registerComponentType<T>) и систем
             * (scene.addSystem), контент мира, консольные команды,
             * сетевое подключение. Вызывается ПОСЛЕ того, как оболочка
             * подняла базовый пайплайн (Transform/Animation/Render).
             *
             * @param application Ядро клиента: сеть, камера, таргет
             *        (игра хранит ссылку — интерфейс живёт дольше её)
             */
            virtual void onClientInitialize(_In Scene& scene, _In ClientApplication& application) __blib_pure_virtual_function;

            /**
             * Ввод игры (свои горячие клавиши) — до симуляции.
             * Консоль/тильда/Escape уже обработаны оболочкой; deltaTime
             * заморожен (0) при открытой консоли.
             */
            virtual void onInput(float deltaTime) __blib_pure_virtual_function;

            /**
             * Сетевой кадр игры: слив событий зеркал, отправка команд,
             * клиентское предсказание, follow-камеры. Вызывается ПОСЛЕ
             * poll сети (снапшоты уже применены/интерполированы).
             */
            virtual void onNetworkUpdate() __blib_pure_virtual_function;

            /**
             * Перед scene.update: состояние рендера (камера, свет),
             * которое должны увидеть системы этого кадра.
             */
            virtual void onSceneWillUpdate(float deltaTime) __blib_pure_virtual_function;

            /**
             * После scene.update (рендер уже в FBO оболочки).
             */
            virtual void onSceneDidUpdate(float deltaTime) __blib_pure_virtual_function;

            /**
             * Оверлей/панели игры — внутри ImGui-кадра оболочки
             * (только в оконном режиме; в PIE не вызывается).
             */
            virtual void onUi() __blib_pure_virtual_function;

            /**
             * Сессия открыта (Welcome принят, зеркало готово).
             */
            virtual void onSessionReady(buint32 tickRate, EntityID playerEntity) __blib_pure_virtual_function;

            /**
             * Сессия разорвана (сервер ушёл/переподключение).
             */
            virtual void onSessionLost() __blib_pure_virtual_function;

            /**
             * Гашение игровой стороны (мир и сеть ещё живы — можно
             * освобождать игровые ресурсы корректно).
             */
            virtual void onShutdown() __blib_pure_virtual_function;
        };

    } // namespace client
} // namespace beng
