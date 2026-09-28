#pragma once

#include <beng/config.h>
#include <beng/core/icomponent.h>
#include <beng/core/componentReflection.h>

#include <blib/core/math/vector.h>
#include <blib/core/math/quaternion.h>
#include <blib/core/math/matrix.h>

#include <vector>

namespace beng
{
    // Forward declaration
    class Scene;

    /**
     * TransformComponent - позиция, поворот и масштаб с поддержкой иерархии.
     * 
     * Назначение:
     * - Хранит локальные координаты (относительно родителя)
     * - Вычисляет мировые координаты через иерархию (parent → child)
     * - Поддерживает parent/children связи (как в Unity)
     * - Кеширует мировую матрицу для производительности
     * 
     * ИНВАРИАНТ СЦЕНЫ: TransformComponent — обязательный компонент.
     * Каждая сущность рождается с ним (Scene::createEntity создаёт его
     * автоматически, тип регистрируется сценой в конструкторе), снять
     * его нельзя (removeComponent — fatal). Явные addComponent/
     * registerComponentType для Transform запрещены: доступ через
     * getComponent/resolveComponent.
     * 
     * Архитектура иерархии:
     * 
     *   Parent Transform
     *   ├─ Child 1 Transform (localPos относительно Parent)
     *   ├─ Child 2 Transform
     *   └─ Child 3 Transform
     *        └─ GrandChild Transform (localPos относительно Child 3)
     * 
     * Иерархия хранится в EntityID: компонент знает свой EntityID
     * (IComponent::getOwnerId) и использует его для поддержки
     * children-списков родителя.
     * 
     * Использование:
     *   EntityID parent = scene.createEntity(); // Transform уже создан
     *   auto& parentTransform = scene.getComponent<TransformComponent>(parent);
     *   parentTransform.setLocalPosition({10, 0, 0});
     *   
     *   EntityID child = scene.createEntity();
     *   auto& childTransform = scene.getComponent<TransformComponent>(child);
     *   childTransform.setLocalPosition({5, 0, 0});
     *   childTransform.setParent(parent);
     *   
     *   // Мировая позиция child = (15, 0, 0)
     *   auto worldPos = childTransform.getWorldPosition();
     * 
     * Ограничения:
     * - Не thread-safe
     * - Циклические зависимости не допускаются: setParent проверяет
     *   цепочку родителей и отклоняет установку при обнаружении цикла
     *   (включая установку самого себя родителем)
     * - Изменение parent требует доступа к Scene (для обновления children)
     */
    class __beng_api TransformComponent : public IComponent
    {
    public:
        // Стабильное имя типа — идентичность типа в таблице типов Scene
        // (регистрация, резолв в шаблонных методах, save/load)
        static constexpr const char* componentTypeName = "beng.Transform";

        // Не прятать 1-аргументную точку входа строгого сравнения
        // (IStrongComparable::strongCompare(other)) за перегрузкой ниже
        using blib::core::IStrongComparable::strongCompare;

        /**
         * Рефлексия компонента (контракт HasComponentReflection):
         * статический дескриптор полей для Inspector/эдитора —
         * см. componentReflection.h. Определение — в transform.cpp.
         *
         * Поля: position, scale (локальный TRS; вращение — кватернион,
         * правка рефлексией не предусмотрена — специализированный
         * контрол позже).
         */
        static const ComponentTypeDescriptor& componentReflection();

        /**
         * Конструктор TransformComponent.
         * 
         * @param scene Ссылка на Scene (нужна для доступа к родителю при вычислении мировых координат)
         * 
         * Вызывается сценой АВТОМАТИЧЕСКИ в Scene::createEntity (инвариант:
         * каждая сущность рождается с Transform) — в коде игры компонент
         * не создаётся вручную.
         * 
         * По умолчанию:
         * - localPosition = (0, 0, 0)
         * - localRotation = identity quaternion
         * - localScale = (1, 1, 1)
         * - parent = invalidEntity (нет родителя)
         */
        explicit TransformComponent(_In Scene* scene);

        /**
         * Конструктор по умолчанию: компонент вне сцены (ownerScene = nullptr).
         * Нужен verifyRoundTrip (ISaveLoadable::verify) — временная копия
         * конструируется без контекста сцены.
         */
        TransformComponent();

        // ========== ISaveLoadable: сериализация и сравнение ==========
        //
        // Формат save: JSON-объект (JsonValue::writeTo) с локальным TRS,
        // иерархией (parent/children), кешем мировой матрицы и isActive.
        // Контекст НЕ сериализуется: ownerScene и ownerId — забота
        // Scene::save/load (ownerId ставит пул, ownerScene возвращает
        // onLoaded); поэтому verify() без сцены — true, компонента
        // в сцене — false (строгая модель, см. ISaveLoadable).

        /**
         * Сохранить состояние компонента в поток (JSON-объект).
         *
         * @param os Выходной поток
         * @return SaveStatus::None при успехе
         */
        blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;

        /**
         * Загрузить состояние компонента из потока (JSON-объект).
         *
         * @param is Входной поток
         * @return LoadStatus::None при успехе
         *
         * ownerScene не восстанавливается (контекст) — его возвращает
         * Scene::load через onLoaded (см. onLoaded ниже).
         */
        blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;

        /**
         * Строгое (бит-в-бит) сравнение: базовые поля + TRS + иерархия +
         * кеш мировой матрицы. ownerScene сравнивается по null-состоянию.
         */
        bool strongCompare(_In const blib::core::IStrongComparable& other,
            _In blib::core::CompareSession& session) const __blib_override;

        /**
         * Round-trip валидация (verifyRoundTrip): save -> свежий
         * standalone-объект -> load -> strongCompare.
         */
        bool verify() const __blib_override;

        /**
         * Вторая фаза загрузки сцены: восстановить контекстную связь
         * со сценой (ownerScene). Вызывается Scene::load после того,
         * как все компоненты загружены.
         */
        void onLoaded(_In Scene& scene) __blib_override;

        // ========== Local Transform (относительно родителя) ==========

        /**
         * Установить локальную позицию (относительно родителя).
         * 
         * @param pos Новая локальная позиция
         * 
         * Помечает мировую матрицу (свою и всех детей) как dirty.
         */
        void setLocalPosition(const blib::math::Vector<float, 3>& pos);

        /**
         * Установить локальный поворот (относительно родителя).
         * 
         * @param rot Новый локальный поворот (quaternion)
         */
        void setLocalRotation(const blib::math::Quaternion<float>& rot);

        /**
         * Установить локальный масштаб (относительно родителя).
         * 
         * @param scale Новый локальный масштаб
         */
        void setLocalScale(const blib::math::Vector<float, 3>& scale);

        /**
         * Получить локальную позицию.
         * 
         * @return Const ссылка на localPosition
         */
        const blib::math::Vector<float, 3>& getLocalPosition() const { return localPosition; }

        /**
         * Получить локальный поворот.
         * 
         * @return Const ссылка на localRotation
         */
        const blib::math::Quaternion<float>& getLocalRotation() const { return localRotation; }

        /**
         * Получить локальный масштаб.
         * 
         * @return Const ссылка на localScale
         */
        const blib::math::Vector<float, 3>& getLocalScale() const { return localScale; }

        // ========== World Transform (в мировых координатах) ==========

        /**
         * Получить мировую позицию (вычисляется через иерархию).
         * 
         * @return Мировая позиция Entity
         * 
         * Если есть родитель:
         *   worldPos = parent.worldMatrix * localPosition
         * Если нет родителя:
         *   worldPos = localPosition
         */
        blib::math::Vector<float, 3> getWorldPosition() const;

        /**
         * Получить мировой поворот (вычисляется через иерархию).
         * 
         * @return Мировой поворот Entity (quaternion)
         * 
         * Если есть родитель:
         *   worldRot = parent.worldRotation * localRotation
         * Если нет родителя:
         *   worldRot = localRotation
         */
        blib::math::Quaternion<float> getWorldRotation() const;

        /**
         * Получить мировой масштаб (вычисляется через иерархию).
         * 
         * @return Мировой масштаб Entity
         * 
         * Если есть родитель:
         *   worldScale = parent.worldScale * localScale (покомпонентное умножение)
         * Если нет родителя:
         *   worldScale = localScale
         */
        blib::math::Vector<float, 3> getWorldScale() const;

        /**
         * Получить мировую матрицу трансформации.
         * 
         * @return 4x4 матрица трансформации (TRS: Translation-Rotation-Scale)
         * 
         * Кешируется и пересчитывается только при изменении локальных координат
         * или координат родителя. При пересчёте рекурсивно тянет мировую
         * матрицу родителя (порядок итерации по dense не важен).
         */
        blib::math::Matrix<float, 4, 4> getWorldMatrix() const;

        // ========== Hierarchy (parent/children) ==========

        /**
         * Установить родителя для этой Transform.
         * 
         * @param parentId EntityID родителя (invalidEntity чтобы убрать родителя)
         * 
         * Поведение:
         * - Удаляет себя из children списка старого родителя (если был)
         * - Добавляет себя в children список нового родителя
         * - Помечает мировую матрицу как dirty
         * - Отклоняет установку (warning + no-op) если:
         *   - новый родитель не существует или не имеет Transform
         *   - установка создаёт цикл (включая parent == собственный ID)
         */
        void setParent(EntityID parentId);

        /**
         * Получить EntityID родителя.
         * 
         * @return EntityID родителя или invalidEntity если нет родителя
         */
        EntityID getParent() const { return parent; }

        /**
         * Получить список детей.
         * 
         * @return Const ссылка на вектор EntityID детей
         */
        const std::vector<EntityID>& getChildren() const { return children; }

        /**
         * Пометить мировую матрицу как dirty (требуется пересчёт).
         * 
         * Автоматически вызывается при изменении локальных координат.
         * Также рекурсивно помечает всех детей как dirty.
         */
        void markDirty();

    private:
        // Локальные координаты (относительно родителя)
        blib::math::Vector<float, 3> localPosition;
        blib::math::Quaternion<float> localRotation;
        blib::math::Vector<float, 3> localScale;

        // Иерархия
        EntityID parent;                // ID родителя (invalidEntity если нет)
        std::vector<EntityID> children; // ID детей

        // Кеш мировых координат (пересчитывается при dirty)
        mutable blib::math::Matrix<float, 4, 4> worldMatrix;
        mutable bool worldMatrixDirty;

        // Ссылка на Scene (для доступа к Transform родителя)
        Scene* ownerScene;

        // Внутренний метод пересчёта мировой матрицы
        void updateWorldMatrix() const;
    };

} // namespace beng
