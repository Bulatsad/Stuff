#pragma once

#include <string>
#include <vector>

#include <beng/config.h>
#include <beng/core/icomponent.h>

#include <blib/core/resource/resourceManager.h>
#include <blib/graphics/skinmodel.h>

struct aiScene;

namespace beng
{
    /**
     * SkinnedMeshComponent — компонент скелетной (анимируемой) модели.
     *
     * Назначение:
     * - Владеет blib::graphics::SkinModel (меши + скелет + аниматор),
     *   загруженной из файла через Assimp;
     * - Предоставляет модель системам (AnimationSystem продвигает
     *   анимацию, RenderSystem отрисовывает).
     *
     * Владение моделью — два режима:
     * - Кеш ресурсов (основной): компонент держит ResourceRef на слот
     *   сцены (Scene::getResources) — loadFromFile(path, rm). Модель
     *   разделяется между сущностями, dedup по содержимому;
     * - Owned-фолбэк (standalone): SkinModel* аллоцируется через
     *   GlobalAllocator — нужен verifyRoundTrip (свежий компонент без
     *   RM загружает содержимое в собственную модель) и legacy-пути.
     *   В рантайме при живом ref'е не используется.
     * getModel() возвращает АКТИВНУЮ модель (ref ?? owned).
     *
     * Сериализуемое состояние (ISaveLoadable):
     * - ПОЛНОЕ содержимое активной модели: save() пишет {model:
     *   <SkinModel JSON> | null, isActive}; load() восстанавливает
     *   содержимое в активную модель (ref-слот или owned-фолбэк).
     * - GL-состояние модели не сериализуемо (перезапечётся в draw);
     *   Bone::node (aiNode*) — контекст Assimp, после восстановления
     *   nullptr (рантайм на нём не зависит)
     * - verify(): standalone-roundtrip содержимого (true и для
     *   компонента с моделью: сериализуемое состояние — CPU-данные,
     *   GL-кэш в него не входит и перезапечётся при draw)
     *
     * Использование:
     *   scene.addComponent<SkinnedMeshComponent>(entity);
     *   meshComp.loadFromFile(path, scene.getResources());
     */
    class __beng_api SkinnedMeshComponent : public beng::IComponent
    {
    private:
        // Owned-фолбэк (standalone/verify): аллоцируется через
        // GlobalAllocator при загрузке БЕЗ ResourceManager
        blib::graphics::SkinModel* model;

        // Активный режим: ref на слот кеша ресурсов сцены
        blib::resource::ResourceRef modelRef;

    public:
        // Стабильное имя типа — идентичность типа в таблице типов Scene
        // (регистрация, резолв в шаблонных методах, save/load)
        static constexpr const char* componentTypeName = "beng.SkinnedMesh";

        // Не прятать 1-аргументную точку входа строгого сравнения
        using blib::core::IStrongComparable::strongCompare;

        SkinnedMeshComponent();
        ~SkinnedMeshComponent() override;

        SkinnedMeshComponent(const SkinnedMeshComponent&) = delete;
        SkinnedMeshComponent& operator=(const SkinnedMeshComponent&) = delete;

        /**
         * Загрузить модель из файла (md5mesh/fbx/obj/... — всё, что
         * умеет Assimp) в СОБСТВЕННУЮ модель (standalone-режим).
         * Перед загрузкой выгружает предыдущую модель.
         *
         * @param path Путь к файлу модели
         * @return true при успехе; при ошибке модель остаётся пустой
         */
        bool loadFromFile(_In const std::string& path);

        /**
         * Загрузить модель через кеш ресурсов: ключ = path. Если слот
         * уже «опечатан» — компонент просто берёт ref на общий слот
         * (dedup/разделение); иначе Assimp-загрузка в слот + commit.
         *
         * @param path Путь к файлу модели (= ключ кеша)
         * @param rm   Кеш ресурсов сцены (scene.getResources())
         * @return true при успехе
         */
        bool loadFromFile(_In const std::string& path, _In blib::resource::ResourceManager& rm);

        /**
         * Сменить меши (skin) у загруженной модели, сохранив скелет и
         * анимации. Новый файл обязан содержать полностью совпадающий
         * скелет (имена, иерархия, bind-поза) — иначе отказ, и модель
         * остаётся без изменений.
         *
         * @param force true — при несовместимом скелете загрузить как
         *        есть: веса костей, отсутствующих в текущем скелете,
         *        отбрасываются, остальные ренормализуются
         * @return true при успехе
         */
        bool loadSkinFromFile(_In const std::string& path, _In bool force = false);

        /**
         * Добавить анимации из внешнего файла (например, Mixamo
         * animation FBX без скина) к текущему списку клипов. Каналы,
         * ссылающиеся на отсутствующие кости, не применяются —
         * количество таких каналов пишется в консоль.
         * @return true при успехе
         */
        bool loadAnimationsFromFile(_In const std::string& path);

        /**
         * Выгрузить модель и вернуть память аллокатору.
         * Идемпотентно: повторный вызов безопасен.
         */
        void unload();

        /**
         * Получить АКТИВНУЮ модель: слот кеша ресурсов (ref), либо
         * owned-фолбэк (standalone/verify). nullptr, если модель не
         * загружена.
         */
        blib::graphics::SkinModel* getModel();
        const blib::graphics::SkinModel* getModel() const;

        // ========== ISaveLoadable: сериализация и сравнение ==========
        //
        // Формат save: JSON-объект {model: <SkinModel JSON> | null,
        // isActive}. load() восстанавливает содержимое модели целиком
        // (аллоцирует SkinModel при отсутствии). verify():
        // standalone-roundtrip содержимого (GL-состояние не входит в
        // сериализуемое состояние).

        /**
         * Сохранить состояние компонента в поток (JSON-объект).
         */
        blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;

        /**
         * Загрузить состояние компонента из потока (JSON-объект).
         */
        blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;

        /**
         * Строгое сравнение: базовые поля + model по null-состоянию +
         * полное содержимое моделей (делегирование SkinModel).
         */
        bool strongCompare(_In const blib::core::IStrongComparable& other,
            _In blib::core::CompareSession& session) const __blib_override;

        /**
         * Round-trip валидация (verifyRoundTrip).
         */
        bool verify() const __blib_override;
    };

} // namespace beng
