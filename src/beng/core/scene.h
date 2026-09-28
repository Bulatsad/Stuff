#pragma once

#include <beng/config.h>
#include <beng/core/icomponent.h>
#include <beng/core/componentReflection.h>
#include <beng/components/transform.h>
#include <beng/core/componentPool.h>
#include <beng/core/system.h>
#include <beng/core/sceneSaveFormat.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/stdAllocatorAdapter.h>
#include <blib/core/console/console.h>
#include <blib/core/resource/resourceManager.h>
#include <blib/core/isaveloadable.h>

#include <vector>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <string>
#include <cstring>
#include <cstdlib>

namespace beng
{
    /**
     * Scene - контейнер всех Entity, компонентов и систем.
     * 
     * Назначение:
     * - Владеет всеми Entity и их компонентами
     * - Управляет жизненным циклом Entity (create/destroy)
     * - Хранит component pools (один пул на тип компонента)
     * - Управляет системами (registration + update loop)
     * 
     * Архитектура:
     * 
     *   Scene
     *   ├─ Entity dense array (EntityID + ComponentMask)
     *   ├─ Entity sparse lookup (EntityID → dense index)
     *   ├─ Component pools (ComponentType → ComponentPool<T>)
     *   └─ Systems (ISystem* sorted by priority)
     * 
     * Использование:
     *   Scene scene;
     *   
     *   // TransformComponent сцена регистрирует САМА в конструкторе
     *   // (инвариант: каждая сущность рождается с Transform, typeId 0
     *   // зарезервирован за ним — см. createEntity). Остальные типы
     *   // регистрируются явно (имя типа берётся из T::componentTypeName;
     *   // коллизия имени в одной сцене — fatal; guard:
     *   // isRegisteredComponentType<T>).
     *   scene.registerComponentType<PhysicsComponent>();
     *   
     *   // Создание Entity — Transform уже на ней
     *   EntityID id = scene.createEntity();
     *   auto& transform = scene.getComponent<TransformComponent>(id);
     *   transform.setLocalPosition({10, 0, 0});
     *   
     *   // Добавление систем
     *   TransformSystem transformSystem;
     *   scene.addSystem(&transformSystem);
     *   
     *   // Главный цикл
     *   while (running) {
     *       scene.update(deltaTime);  // вызывает все системы
     *   }
     * 
     * Ограничения:
     * - Не thread-safe (все операции в main thread)
     * - Entity ID не переиспользуются после destroy (валидны до конца жизни Scene)
     * - Некопируем и неперемещаем: контейнеры держат указатель на собственный
     *   containerAllocator, переезд объекта оставил бы висячие ссылки
     * - Максимум maxComponentTypes (componentMaskBits) типов компонентов на сцену
     *   (из них слот typeId 0 всегда занят TransformComponent)
     * - Типы компонентов регистрируются ТОЛЬКО через эту сцену (глобального
     *   реестра нет): имя типа — его идентичность (T::componentTypeName)
     * - ИНВАРИАНТ: сущность не может существовать без TransformComponent.
     *   Сцена регистрирует его в конструкторе и добавляет каждой сущности
     *   в createEntity(); removeComponent<TransformComponent> — fatal;
     *   в файле сохранения Transform обязателен у каждой сущности
     *   (отсутствие — LoadStatus::InvalidData)
     *
     * Сохранение/загрузка (ISaveLoadable, см. sceneSaveFormat.h):
     * - save()/load() работают с IOutputStream/IInputStream: magic "JSON\0"
     *   + JSON-документ (формат/версия/nextEntityId/сущности)
     * - Типы в файле — по стабильным именам; load требует регистрации
     *   всех типов из файла (иначе LoadStatus::ComponentTypeNotRegistered)
     * - load() атомарен: при ошибке сцена возвращается в исходное
     *   состояние (зарегистрированные типы и существующие данные не
     *   теряются); load в непустую сцену — SceneNotEmpty
     * - Системы не сериализуются (код игры добавляет их после load);
     *   кеш ресурсов не сериализуется (модели перезагружаются в
     *   onLoaded компонентов через RM по сохранённым путям)
     */
    class __beng_api Scene : public blib::core::ISaveLoadable
    {
    public:
        // Не прятать 1-аргументную точку входа строгого сравнения
        // (IStrongComparable::strongCompare(other)) за перегрузкой ниже
        using blib::core::IStrongComparable::strongCompare;

        /**
         * Конструктор — создаёт пустую сцену.
         */
        Scene();

        /**
         * Деструктор — удаляет все Entity, компоненты и освобождает пулы.
         */
        ~Scene();

        // Запрет копирования и перемещения (см. комментарий к классу)
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) = delete;
        Scene& operator=(Scene&&) = delete;

        /**
         * Кеш ресурсов сцены (blib::resource::ResourceManager):
         * загруженные объекты по ключам, dedup по содержимому,
         * refcount-доступ. Компоненты держат ResourceRef'ы на слоты;
         * clear() (в деструкторе) уничтожает компоненты ДО деструктора
         * кеша — внешние ref'ы обязаны умереть раньше сцены (иначе
         * fatal в ~ResourceManager).
         */
        blib::resource::ResourceManager& getResources() { return this->resources; }
        const blib::resource::ResourceManager& getResources() const { return this->resources; }

        // ========== Entity Management ==========

        /**
         * Создать новую Entity в сцене.
         * 
         * @return Уникальный EntityID (никогда не переиспользуется)
         * 
         * Entity получает уникальный ID и добавляется в dense array.
         * ИНВАРИАНТ: каждая сущность рождается с TransformComponent
         * (сцена создаёт его сразу, с дефолтным TRS) — сущность без
         * Transform существовать не может. Работа с компонентами — через
         * Scene API (addComponent и т.д.), работа с самой Entity — по ID.
         */
        EntityID createEntity();

        /**
         * Удалить Entity и все её компоненты.
         * 
         * @param id ID Entity для удаления
         * 
         * Поведение:
         * - Удаляет ВСЕ компоненты Entity из пулов (через destroyers)
         * - Удаляет Entity из dense array и lookup
         * - ID не переиспользуется (гарантия что старые ID невалидны)
         * - Если Entity не существует → no-op (warning в лог)
         */
        void destroyEntity(EntityID id);

        /**
         * Получить количество Entity в сцене.
         * 
         * @return Количество активных Entity
         */
        buint32 getEntityCount() const { return static_cast<buint32>(entities.size()); }

        // ========== Component API (по EntityID) ==========

        /**
         * Проверить наличие компонента заданного типа у Entity.
         * 
         * @tparam T Тип компонента
         * @param entityId ID Entity
         * @return true если компонент существует
         */
        template<typename T>
        bool hasComponent(EntityID entityId) const;

        /**
         * Добавить компонент к Entity.
         * 
         * @tparam T Тип компонента
         * @tparam Args Типы аргументов конструктора компонента
         * @param entityId ID Entity
         * @param args Аргументы для конструктора компонента
         * @return Ссылка на созданный компонент (стабильна пока компонент жив)
         * 
         * Проверки:
         * - Если Entity не существует → fatal error
         * - Если компонент уже существует → fatal error
         * - Если тип не зарегистрирован в Scene → fatal error
         */
        template<typename T, typename... Args>
        T& addComponent(EntityID entityId, Args&&... args);

        /**
         * Получить компонент Entity (fatal если отсутствует).
         * 
         * @tparam T Тип компонента
         * @param entityId ID Entity
         * @return Ссылка на компонент
         * 
         * Проверки:
         * - Если Entity не существует → fatal error
         * - Если компонента нет → fatal error
         * 
         * Для nullable-доступа используйте tryGetComponent.
         */
        template<typename T>
        T& getComponent(EntityID entityId);

        template<typename T>
        const T& getComponent(EntityID entityId) const;

        /**
         * Получить компонент Entity или nullptr если его нет.
         * 
         * @tparam T Тип компонента
         * @param entityId ID Entity
         * @return Указатель на компонент или nullptr
         * 
         * Не логирует ошибки — штатный способ проверки наличия.
         */
        template<typename T>
        T* tryGetComponent(EntityID entityId);

        template<typename T>
        const T* tryGetComponent(EntityID entityId) const;

        /**
         * Получить существующий компонент или создать новый (resolve pattern).
         * 
         * @tparam T Тип компонента
         * @tparam Args Типы аргументов конструктора компонента
         * @param entityId ID Entity
         * @param args Аргументы для конструктора (используются только при создании)
         * @return Ссылка на существующий или новый компонент
         */
        template<typename T, typename... Args>
        T& resolveComponent(EntityID entityId, Args&&... args);

        /**
         * Удалить компонент у Entity.
         * 
         * @tparam T Тип компонента
         * @param entityId ID Entity
         * 
         * Поведение:
         * - Если компонент существует → удаляет его
         * - Если Entity или компонента нет → no-op (не ошибка)
         * - TransformComponent удалить НЕЛЬЗЯ (инвариант сцены) — fatal error
         */
        template<typename T>
        void removeComponent(EntityID entityId);

        // ========== Component Pool Management ==========

        /**
         * Зарегистрировать тип компонента в сцене (создать пул).
         * 
         * @tparam T Тип компонента (должен наследоваться от IComponent
         *         и объявлять static constexpr componentTypeName)
         * @param chunkSize Размер chunk для PoolAllocator (по умолчанию из config)
         * 
         * Проверки:
         * - Имя типа (T::componentTypeName) уже зарегистрировано в сцене →
         *   fatal error (коллизия имени; guard — isRegisteredComponentType<T>)
         * - Превышение лимита maxComponentTypes на сцену → fatal error
         * - Создаёт ComponentPool<T> через GlobalAllocator
         * 
         * Имя типа регистрируется в словаре сцены (typeIdByName) и получает
         * локальный ComponentType — индекс, задающий бит в ComponentMask.
         * 
         * ВАЖНО (инвариант сцены): TransformComponent регистрируется сценой
         * АВТОМАТИЧЕСКИ в конструкторе (всегда typeId 0) — повторная
         * регистрация TransformComponent в коде игры/движка запрещена
         * (fatal: имя уже зарегистрировано).
         * 
         * Использование:
         *   scene.registerComponentType<PhysicsComponent>();
         *   scene.registerComponentType<PhysicsComponent>(256); // custom chunk size
         * 
         *   // Идемпотентная регистрация из инициализации:
         *   if (!scene.isRegisteredComponentType<PhysicsComponent>()) {
         *       scene.registerComponentType<PhysicsComponent>();
         *   }
         */
        template<typename T>
        void registerComponentType(buint32 chunkSize = defaultComponentPoolChunkSize);

        /**
         * Проверить, зарегистрирован ли тип компонента в сцене.
         * 
         * @tparam T Тип компонента
         * @return true если имя типа есть в таблице типов сцены
         * 
         * Guard для идемпотентной регистрации (повторная регистрация
         * имени в одной сцене — fatal, см. registerComponentType).
         */
        template<typename T>
        bool isRegisteredComponentType() const;

        /**
         * Получить пул компонентов заданного типа.
         * 
         * @tparam T Тип компонента
         * @return Ссылка на ComponentPool<T> (всегда валидна после регистрации)
         * 
         * Проверки:
         * - Если тип не зарегистрирован → fatal error
         * 
         * Использование (в системах):
         *   ComponentPool<TransformComponent>& pool = scene.getComponentPool<TransformComponent>();
         *   for (auto it = pool.begin(); it != pool.end(); ++it) {
         *       TransformComponent& comp = *it;
         *       EntityID id = it.getEntityId();
         *       // ... обработка
         *   }
         */
        template<typename T>
        ComponentPool<T>& getComponentPool();

        template<typename T>
        const ComponentPool<T>& getComponentPool() const;

        /**
         * Получить пул компонентов без fatal: nullptr, если тип
         * не зарегистрирован в сцене. Для систем, работающих с
         * компонентом опционально (например, RenderSystem в сценах
         * без статических мешей) — см. getComponentPool для
         * строгого варианта.
         */
        template<typename T>
        ComponentPool<T>* tryGetComponentPool();

        // ========== Reflection / Inspector API (type-erased) ==========
        //
        // Доступ к рефлексии и перебору сущностей/типов БЕЗ compile-time
        // T — для Inspector/Hierarchy эдитора (эдитор не знает игровых
        // типов, см. ARCHITECTURE.md «Эдитор»).

        /**
         * Дескриптор рефлексии типа (ComponentTypeDescriptor) или
         * nullptr, если тип не зарегистрирован или не имеет рефлексии
         * (HasComponentReflection<T> == false).
         *
         * @param typeId Локальный индекс типа в сцене
         */
        const ComponentTypeDescriptor* tryGetComponentReflection(ComponentType typeId) const;

        /**
         * Количество зарегистрированных типов компонентов сцены.
         * Индексы типов — 0 .. getComponentTypeCount()-1 (Transform —
         * всегда 0, инвариант сцены).
         */
        buint32 getComponentTypeCount() const { return static_cast<buint32>(typeNames.size()); }

        /**
         * Стабильное имя типа по локальному индексу (литерал
         * T::componentTypeName; nullptr при выходе за границы).
         */
        const char* getComponentTypeName(ComponentType typeId) const;

        /**
         * Перебор сущностей сцены по плотному индексу
         * (0 .. getEntityCount()-1); invalidEntity при выходе за
         * границы. Порядок — dense (совпадает с порядком создания,
         * destroy двигает последнюю на место удалённой).
         */
        EntityID getEntityId(buint32 denseIndex) const;

        /**
         * Проверить наличие компонента заданного ТИПА (по локальному
         * индексу, не шаблонно) у сущности. Не-fatal: false при
         * невалидном типе/сущности — Inspector не должен ронять
         * эдитор на устаревших ID.
         */
        bool hasComponent(EntityID entityId, ComponentType typeId) const;

        /**
         * Получить компонент по локальному индексу типа (type-erased;
         * nullptr если сущность/тип/компонент отсутствуют).
         */
        IComponent* tryGetComponent(EntityID entityId, ComponentType typeId);

        const IComponent* tryGetComponent(EntityID entityId, ComponentType typeId) const;

        /**
         * Сбросить сцену в «пустое» состояние, СОХРАНИВ реестр типов
         * (имена, словарь, type-erased fn-таблицы), список систем и
         * привязки систем к рендер-таргету: уничтожаются все сущности
         * и их компоненты (пулы через deleters; ref'ы компонентов к
         * кешу ресурсов отпускаются), nextEntityId возвращается к 1,
         * кеш ресурсов очищается (unloadAll — слоты освобождаются).
         *
         * После reset() сцена снова пуста и готова:
         * - к Scene::load (типы зарегистрированы — пулы создаются
         *   фабриками по требованию);
         * - к повторному построению контента хостом.
         *
         * Нужен scene_load эдитора/клиента: сцена принадлежит хосту,
         * и вместо пересоздания объекта (Scene некопируема) сцена
         * сбрасывается на месте — привязки хоста к сцене не рвутся.
         */
        void reset();

        // ========== System Management ==========

        /**
         * Добавить систему в сцену.
         * 
         * @param system Указатель на систему (Scene не владеет, caller управляет временем жизни)
         * 
         * Системы автоматически сортируются по приоритету (getPriority()).
         * Системы с меньшим приоритетом выполняются первыми.
         */
        void addSystem(_In ISystem* system);

        /**
         * Удалить систему из сцены.
         * 
         * @param system Указатель на систему для удаления
         * 
         * Поведение:
         * - Если система не найдена → no-op (warning в лог)
         */
        void removeSystem(_In ISystem* system);

        /**
         * Получить количество систем в сцене.
         * 
         * @return Количество зарегистрированных систем
         */
        buint32 getSystemCount() const { return static_cast<buint32>(systems.size()); }

        // ========== Сохранение / Загрузка (ISaveLoadable) ==========

        /**
         * Сохранить сцену в поток (magic + JSON-документ, формат —
         * sceneSaveFormat.h).
         *
         * @param os Выходной поток
         * @return SaveStatus::None при успехе;
         *         SaveStatus::ComponentNotSerializable — у одного из
         *         компонентов save() == Unsupported (в поток ничего
         *         не пишется);
         *         SaveStatus::WriteFailed — сбой записи
         *
         * Сериализуются: nextEntityId, сущности с их ID и компоненты
         * (тип по имени + JSON компонента). Системы и кеш ресурсов
         * не пишутся.
         */
        blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;

        /**
         * Загрузить сцену из потока. Атомарно: при любой ошибке сцена
         * остаётся в исходном состоянии.
         *
         * @param is Входной поток (magic + JSON-документ)
         * @return LoadStatus::None при успехе;
         *         LoadStatus::UnknownFormat — не совпал magic;
         *         LoadStatus::InvalidData — документ повреждён/не
         *         соответствует схеме;
         *         LoadStatus::VersionMismatch — несовместимая версия;
         *         LoadStatus::SceneNotEmpty — сцена не пуста;
         *         LoadStatus::ComponentTypeNotRegistered — тип из файла
         *         не зарегистрирован в сцене
         *
         * Требования:
         * - Сцена должна быть пустой (нет сущностей, nextEntityId == 1);
         *   регистрировать типы можно заранее
         * - Все типы из файла обязаны быть зарегистрированы
         *   (registerComponentType<T>) до вызова load
         * - ИНВАРИАНТ: у каждой сущности в файле обязана быть запись
         *   TransformComponent — иначе LoadStatus::InvalidData (файлы,
         *   сохранённые до введения инварианта, загрузкой отвергаются);
         *   второй компонент при этом не создаётся: данные файла
         *   загружаются в Transform, авто-созданный createEntity
         * - После load вызывается onLoaded(Scene&) у каждого компонента
         *   (восстановление контекстных связей — см. IComponent)
         */
        blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;

        /**
         * Строгое (бит-в-бит) сравнение двух сцен: nextEntityId,
         * сущности (ID + маски), компоненты по строгому сравнению.
         * Зарегистрированные типы и системы не сравниваются.
         */
        bool strongCompare(_In const blib::core::IStrongComparable& other,
            _In blib::core::CompareSession& session) const __blib_override;

        /**
         * Round-trip валидация: save -> MemoryStream -> свежая Scene
         * (с копией реестра типов этой сцены) -> load -> strongCompare.
         */
        bool verify() const __blib_override;

        // ========== Update Loop ==========

        /**
         * Главный цикл обновления — вызывает все системы.
         * 
         * @param deltaTime Время с предыдущего кадра (в секундах)
         * 
         * Порядок выполнения:
         * 1. Системы сортируются по приоритету (если были изменения)
         * 2. Каждая система вызывает update(scene, deltaTime)
         * 3. Системы выполняются последовательно (не параллельно)
         */
        void update(float deltaTime);

    private:
        // ========== Component Mask Helpers ==========

        // Найти локальный ComponentType для типа T в этой сцене.
        // Линейный скан таблицы имён (≤ maxComponentTypes записей, без
        // аллокаций — горячий путь шаблонных методов сцены).
        // invalidComponentType если имя T не зарегистрировано в сцене.
        template<typename T>
        ComponentType findTypeId() const;

        // Строгий вариант findTypeId: fatal error если тип не зарегистрирован.
        template<typename T>
        ComponentType getTypeId() const;

        // Установить/сбросить бит в маске компонентов Entity.
        // fatal error при невалидном typeId или отсутствующей Entity.
        void setComponentBit(EntityID entityId, ComponentType typeId, bool value);

        // Прочитать бит маски компонентов Entity.
        // false для невалидного typeId (без fatal — это константный запрос)
        bool getComponentBit(EntityID entityId, ComponentType typeId) const;

        // Полностью очистить сцену: удалить Entity, пулы, системы.
        // Общий код деструктора (и будущего operator=).
        void clear();

        // Внутренний метод сортировки систем по приоритету
        void sortSystems();

        // Скопировать РЕЕСТР ТИПОВ (имена, словарь, type-erased
        // fn-таблицы) из другой сцены БЕЗ пулов и данных — пулы
        // создаются по требованию во время load (фабрики). Нужен
        // verify(): свежая сцена должна резолвить те же типы без
        // compile-time T (RTTI в проекте не используется)
        void copyComponentTypeRegistryFrom(_In const Scene& other);

        // ========== Entity Storage (Sparse Set) ==========

        // Адаптер STL-контейнеров к blib-аллокатору (StdAllocatorAdapter).
        // Все контейнеры Scene аллоцируют через GlobalAllocator,
        // а не через дефолтный ::operator new.
        template<typename U>
        using ContainerAllocator = blib::memory::StdAllocatorAdapter<U>;

        // Аллокатор служебных контейнеров. Объявлен ПЕРЕД контейнерами:
        // они хранят указатель на него. По умолчанию — DefaultAllocator
        // (прокси к GlobalAllocator).
        blib::memory::Allocator containerAllocator;

        // Кеш ресурсов сцены (см. getResources). Разрушается ПОСЛЕ
        // компонентов: clear() в деструкторе освобождает их ref'ы раньше
        blib::resource::ResourceManager resources;

        // Плотный массив ID Entity (индекс = dense index)
        std::vector<EntityID, ContainerAllocator<EntityID>> entities{
            ContainerAllocator<EntityID>(&containerAllocator) };

        // Плотный массив масок компонентов (индекс = dense index Entity)
        std::vector<ComponentMask, ContainerAllocator<ComponentMask>> entityMasks{
            ContainerAllocator<ComponentMask>(&containerAllocator) };

        // Разреженный map: EntityID → dense index в entities
        std::unordered_map<EntityID, buint32,
            std::hash<EntityID>, std::equal_to<EntityID>,
            ContainerAllocator<std::pair<const EntityID, buint32>>> entityLookup{
                ContainerAllocator<std::pair<const EntityID, buint32>>(&containerAllocator) };

        // Генератор ID для Entity (0 зарезервирован под invalidEntity)
        EntityID nextEntityId;

        // Локальный ComponentType обязательного TransformComponent.
        // Регистрируется в конструкторе ПЕРВЫМ (всегда 0) и никогда не
        // снимается — инвариант: каждая сущность рождается с Transform.
        ComponentType transformTypeId;

        // ========== Component Pools ==========

        // Тип строкового ключа словаря типов. Аллокатор blib — правило STL:
        // строки-ключи аллоцируют через containerAllocator (GlobalAllocator).
        using ComponentTypeNameString = std::basic_string<char, std::char_traits<char>,
            blib::memory::StdAllocatorAdapter<char>>;

        // Словарь имён типов: стабильное имя → локальный ComponentType.
        // Используется при регистрации (проверка коллизии имени) и в
        // Scene::load (резолв имени из файла). Горячий путь шаблонных
        // методов его не трогает — там линейный скан typeNames
        // (поиск по имени из словаря строил бы std::string-ключ и
        // аллоцировал бы память для имён длиннее SSO).
        std::unordered_map<ComponentTypeNameString, ComponentType,
            std::hash<ComponentTypeNameString>, std::equal_to<ComponentTypeNameString>,
            ContainerAllocator<std::pair<const ComponentTypeNameString, ComponentType>>> typeIdByName{
                ContainerAllocator<std::pair<const ComponentTypeNameString, ComponentType>>(&containerAllocator) };

        // Таблица имён типов: индекс (== локальный ComponentType) → имя.
        // Имена НЕ копируются — это литералы из классов компонентов
        // (T::componentTypeName), живущие всё время работы процесса.
        std::vector<const char*, ContainerAllocator<const char*>> typeNames{
            ContainerAllocator<const char*>(&containerAllocator) };

        // Пул на тип компонента (индекс = ComponentType, типов <= maxComponentTypes).
        // Сырые указатели type-erased: конкретный тип известен только
        // в template-методах, а плотные массивы дают O(1) доступ без хеширования.
        void* componentPools[maxComponentTypes] = { nullptr };

        // Функции уничтожения пулов (type-erased, вызываются в clear())
        void (*componentPoolDeleters[maxComponentTypes])(void*) = { nullptr };

        // Функции удаления компонента из пула по EntityID
        // (type-erased, используются в destroyEntity)
        void (*componentPoolDestroyers[maxComponentTypes])(void*, EntityID) = { nullptr };

        // Функции-фабрики пулов (type-erased): создают ComponentPool<T>
        // через GlobalAllocator. Используются Scene::load для создания
        // пула типа из файла по требованию и Scene::verify (копия
        // реестра — см. copyComponentTypeRegistryFrom)
        void* (*componentPoolFactories[maxComponentTypes])(buint32) = { nullptr };

        // ChunkSize пула на тип (параметр фабрики; хранится при регистрации)
        buint32 componentPoolChunkSizes[maxComponentTypes] = { 0 };

        // Создание компонента в пуле (default-ctor + ownerId) — Scene::load
        IComponent* (*componentPoolCreators[maxComponentTypes])(void*, EntityID) = { nullptr };

        // Получение компонента из пула по EntityID — save/strongCompare/onLoaded
        IComponent* (*componentPoolGetters[maxComponentTypes])(void*, EntityID) = { nullptr };

        // Дескрипторы рефлексии на тип (индекс = ComponentType).
        // Заполняются в registerComponentType (if constexpr
        // HasComponentReflection<T>); nullptr — тип без рефлексии.
        // Дескрипторы — статические объекты в .cpp компонентов
        // (живут всё время процесса, копируются между сценами в
        // copyComponentTypeRegistryFrom)
        const ComponentTypeDescriptor* componentReflections[maxComponentTypes] = { nullptr };

        // ========== Systems ==========

        // Список систем (упорядочены по приоритету)
        std::vector<ISystem*, ContainerAllocator<ISystem*>> systems{
            ContainerAllocator<ISystem*>(&containerAllocator) };

        // Флаг что системы нужно пересортировать
        bool systemsDirty;
    };

} // namespace beng

// Включаем template реализацию
#include <beng/core/impl/scene.inl>
