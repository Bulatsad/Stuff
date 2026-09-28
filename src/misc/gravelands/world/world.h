#pragma once

#include <beng/config.h>

#include <blib/utilmacro.h>

#include <functional>
#include <string>
#include <vector>

// Форвард-декларации: мир привязывается к сцене beng и рендер-таргету
// blib-graphics, но не тянет их определения в потребителей (pimpl)
namespace beng
{
    class Scene;
}

namespace blib
{
    namespace graphics
    {
        class IRenderTarget;
    }
}

namespace gravelands
{
    /**
     * World — мир Gravelands: контент + системы игры, общие для
     * клиента и эдитора (плагин-модель, см. ARCHITECTURE.md).
     *
     * Назначение:
     * - Привязывается к ECS-сцене ХОСТА (initialize(scene)): регистрирует
     *   движковые типы beng-client и вешает системы Transform → Animation
     *   → BlobShadow → Light → Render. Сценой НЕ владеет: эдитор правит
     *   сцену каркаса (сериализация/панели), клиент — свою сцену;
     * - Строит тестовый мир (тайлы, сфера, деревья, тени, свет) и
     *   грузит скелетную модель-«танцора» в привязанную сцену;
     * - Даёт консольные команды scene_save/scene_load и отладочное
     *   управление светом клавишами;
     * - НЕ владеет окном, камерой, вводом и ImGui: рендер-таргет
     *   выдаётся хостом (клиент — свой FBO, эдитор — FBO вьюпорта).
     *
     * scene_load не пересоздаёт сцену: вызывается Scene::reset()
     * (сцена сбрасывается на месте, реестр типов/системы сохраняются)
     * — привязки хоста к сцене не рвутся.
     *
     * Паттерн «lib + тонкий exe»: World не владеет главным циклом —
     * хост вызывает update(dt) внутри своего кадра.
     */
    class World
    {
    public:
        World();
        ~World();

        // Мир некопируем и неперемещаем (владеет системами)
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = delete;
        World& operator=(World&&) = delete;

        /**
         * Привязаться к сцене хоста: регистрация типов компонентов и
         * систем МИРА. Строго до запуска цикла (реестр типов не
         * thread-safe). Вызывать один раз.
         *
         * Разделение ответственности со сценой хоста:
         * - типы компонентов регистрируются с guard'ом
         *   (isRegisteredComponentType) — хост мог зарегистрировать их
         *   сам (каркас EditorApplication регистрирует движковые типы
         *   заранее);
         * - мир вешает ТОЛЬКО свои системы (BlobShadow → Light);
         *   базовый рендер-пайплайн (Transform → Animation → Render)
         *   вешает ХОСТ — у эдитора его вешает каркас EditorApplication,
         *   у клиента — сам клиент. Мир их не дублирует: повторный
         *   RenderSystem отрисовал бы мир дважды.
         *
         * @param scene Сцена хоста (клиент — своя, эдитор — сцена
         *              каркаса EditorApplication)
         * @return true при успехе (пока всегда, зарезервировано под
         *         будущие сбои)
         */
        bool initialize(_In beng::Scene& scene);

        /**
         * Привязать рендер-таргет к LightSystem мира (FBO хоста).
         * RenderSystem — система хоста, таргет ей выставляет хост.
         */
        void setRenderTarget(_In_opt blib::graphics::IRenderTarget* target);

        /**
         * Построение тестового мира сущностями (тайлы, сфера, деревья,
         * тени, свет) в привязанной сцене: вся отрисовка — только
         * через Scene (см. BENG.md, «beng-client»). Из initialize()
         * хоста (или повторно после scene_load).
         */
        void setupWorld();

        /**
         * Загрузка тестовой скелетной модели-«танцора»:
         * ECS-сущность со SkinnedMeshComponent/AnimatorComponent,
         * NPR-материалы, запуск анимации, blob-тень под моделью.
         * Из initialize() хоста (или повторно после scene_load).
         */
        void loadDancerModel();

        /**
         * Регистрация консольных команд мира: scene_save / scene_load.
         * Коллбэки захватывают this — мир обязан жить до конца
         * процесса (команды вызываются из главного цикла).
         */
        void registerConsoleCommands();

        /**
         * Колбэк сброса сцены: вызывается после Scene::reset() внутри
         * scene_load (сцена сброшена, контент ещё не загружен). Хост
         * использует его, чтобы очистить состояние, привязанное к
         * старым данным (например, undo/redo-историю эдитора).
         */
        void setSceneResetCallback(_In_opt std::function<void()> callback);

        /**
         * Один шаг симуляции: scene.update(deltaTime) — системы
         * (Transform → Animation → BlobShadow → Light → Render).
         */
        void update(float deltaTime);

        /**
         * Отладочное управление светом клавишами (стрелки — азимут/
         * элевация, [ ] — интенсивность направленного; PageUp/PageDown —
         * интенсивность эмбиента). Крутит компоненты сцены напрямую.
         */
        void updateLight(float deltaTime);

        /**
         * ECS-сцена мира (сцена хоста, к которой мир привязан).
         */
        beng::Scene& getScene();

        /**
         * Сущности мира для отладочного UI хоста (invalidEntity —
         * сущности нет; после scene_load ID устаревают — работать
         * через tryGetComponent).
         */
        beng::EntityID getSphereEntity() const;
        beng::EntityID getDancerEntity() const;

        /**
         * Корректно остановить мир и освободить ресурсы (идемпотентно).
         * Сцену хоста не трогает.
         */
        void shutdown();

    private:
        /**
         * Консольная команда scene_save: записать сцену в JSON-файл.
         * args[0] — путь (необязателен; дефолт — sceneDefaultFilePath).
         */
        void saveSceneCommand(_In const std::vector<std::string>& args);

        /**
         * Консольная команда scene_load: загрузить сцену из JSON-файла
         * (сцена сбрасывается Scene::reset(); при неудаче откат на
         * дефолтный мир). args[0] — путь (необязателен; дефолт —
         * sceneDefaultFilePath).
         */
        void loadSceneCommand(_In const std::vector<std::string>& args);

        /**
         * Сброс сцены хоста для scene_load: Scene::reset() (реестр
         * типов и системы сохраняются) + сброс ссылок на сущности
         * (ID в файле могут не совпасть).
         */
        void resetScene();

    private:
        // Pimpl: скрывает сцену beng и графические объекты blib от
        // заголовка. Память — через GlobalAllocator (проектное
        // правило: никаких new/delete и smart pointers).
        struct WorldImpl;
        WorldImpl* impl;
    };

} // namespace gravelands
