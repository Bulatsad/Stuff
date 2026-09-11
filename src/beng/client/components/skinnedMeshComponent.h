#pragma once

#include <string>

#include <beng/config.h>
#include <beng/core/component.h>

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
     * Владение памятью:
     * - SkinModel аллоцируется через blib::memory::GlobalAllocator
     *   (проектное правило: выделяющие new/delete запрещены);
     * - Уничтожается в деструкторе компонента (ComponentPool
     *   вызывает ~T() при destroy).
     *
     * Использование:
     *   scene.addComponent<SkinnedMeshComponent>(entity);
     *   meshComp.loadFromFile(path);
     */
    class __beng_api SkinnedMeshComponent : public beng::IComponent
    {
    private:
        blib::graphics::SkinModel* model;

    public:
        SkinnedMeshComponent();
        ~SkinnedMeshComponent() override;

        SkinnedMeshComponent(const SkinnedMeshComponent&) = delete;
        SkinnedMeshComponent& operator=(const SkinnedMeshComponent&) = delete;

        /**
         * Загрузить модель из файла (md5mesh/fbx/obj/... — всё, что
         * умеет Assimp). Перед загрузкой выгружает предыдущую модель.
         *
         * @param path Путь к файлу модели
         * @return true при успехе; при ошибке модель остаётся пустой
         */
        bool loadFromFile(_In const std::string& path);

        /**
         * Выгрузить модель и вернуть память аллокатору.
         * Идемпотентно: повторный вызов безопасен.
         */
        void unload();

        /**
         * Получить модель (может быть nullptr, если не загружена).
         */
        blib::graphics::SkinModel* getModel();
        const blib::graphics::SkinModel* getModel() const;
    };

} // namespace beng
