#pragma once

#include <string>
#include <vector>

#include <beng/config.h>
#include <beng/core/icomponent.h>

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
     * Сериализуемое состояние (ISaveLoadable):
     * - ПОЛНОЕ содержимое модели: save() пишет {model: <SkinModel JSON> | null,
     *   isActive}; load() восстанавливает модель целиком (скелет, веса,
     *   геометрия, материалы с битмапами диффуза, клипы анимации) через
     *   blib::graphics::SkinModel::fromJson — при отсутствии модели
     *   аллоцирует её (GlobalAllocator + placement new)
     * - GL-состояние модели не сериализуемо (перезапечётся в draw);
     *   Bone::node (aiNode*) — контекст Assimp, после восстановления
     *   nullptr (рантайм на нём не зависит)
     * - verify(): standalone-roundtrip содержимого (true и для
     *   компонента с моделью: сериализуемое состояние — CPU-данные,
     *   GL-кэш в него не входит и перезапечётся при draw)
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
         * умеет Assimp). Перед загрузкой выгружает предыдущую модель.
         *
         * @param path Путь к файлу модели
         * @return true при успехе; при ошибке модель остаётся пустой
         */
        bool loadFromFile(_In const std::string& path);

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
         * Получить модель (может быть nullptr, если не загружена).
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
