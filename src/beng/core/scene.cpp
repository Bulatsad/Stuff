#include <beng/core/scene.h>

#include <blib/core/json/json.h>
#include <blib/core/memoryStream.h>
#include <blib/system/memory/globalAllocator.h>

namespace beng
{
    Scene::Scene()
        : nextEntityId(1) // 0 зарезервирован для invalidEntity
        , transformTypeId(invalidComponentType)
        , systemsDirty(false)
    {
        entities.reserve(defaultEntityReserveSize);
        entityMasks.reserve(defaultEntityReserveSize);

        // ИНВАРИАНТ: сущность не может существовать без TransformComponent.
        // Сцена регистрирует его САМА, первым типом (typeId 0) — явная
        // регистрация в коде игры/движка стала бы fatal-дубликатом имени.
        registerComponentType<TransformComponent>();
        transformTypeId = findTypeId<TransformComponent>();

        __blib_log_info("Scene created");
    }

    Scene::~Scene()
    {
        __blib_log_info("Scene destroying (%u entities, %u component types, %u systems)",
            static_cast<unsigned int>(entities.size()),
            static_cast<unsigned int>(typeNames.size()),
            static_cast<unsigned int>(systems.size()));

        clear();

        __blib_log_info("Scene destroyed");
    }

    EntityID Scene::createEntity()
    {
        // Защита от переполнения (через buint64Max не доживёт ни одна игра,
        // но дешевле проверить, чем получить UB от 0-переполнения)
        if (__blib_unlikely(nextEntityId == buint64Max))
        {
            __blib_fatal("Entity ID overflow in Scene");
        }

        EntityID id = nextEntityId++;

        // Добавить в плотные массивы (ID + пустая маска)
        entities.push_back(id);
        entityMasks.push_back(0);

        // Добавить в sparse lookup
        entityLookup[id] = static_cast<buint32>(entities.size() - 1);

        // ИНВАРИАНТ: каждая сущность рождается с TransformComponent
        // (дефолтный TRS). Без Transform сущность существовать не может;
        // тип зарегистрирован в конструкторе сцены (transformTypeId == 0).
        addComponent<TransformComponent>(id, this);

        __blib_log_debug("Created Entity %llu (index %u)",
            static_cast<unsigned long long>(id),
            static_cast<unsigned int>(entities.size() - 1));

        return id;
    }

    void Scene::setNextEntityId(EntityID nextId)
    {
        // Только вперёд: понизить границу нельзя (ниже неё ID уже
        // выданы — коллизии). Повторный вызов с меньшим/равным
        // значением — no-op (перезапуск сервера: база уже поднята)
        if (nextId > nextEntityId)
        {
            nextEntityId = nextId;
        }
    }

    EntityID Scene::createEntityWithId(EntityID id)
    {
        // ID 0 зарезервирован под invalidEntity — сущность с ним
        // создать нельзя (нарушение протокола)
        if (__blib_unlikely(id == invalidEntity))
        {
            __blib_fatal("Scene::createEntityWithId: reserved id 0");
        }

        // Защита от переполнения (как в createEntity)
        if (__blib_unlikely(id == buint64Max))
        {
            __blib_fatal("Entity ID overflow in Scene");
        }

        // Коллизия: id уже выдан (или был выдан и освобождён) —
        // переиспользование запрещено. Протокол нарушен — отказ
        // БЕЗ изменений (вызывающий логирует и пропускает запись)
        if (__blib_unlikely(id < nextEntityId))
        {
            return invalidEntity;
        }

        // Пропуск диапазона: серверные ID монотонны, но клиент мог
        // не видеть часть сущностей (подключился позже, дельта-
        // снапшоты) — nextEntityId перепрыгивает за выданный id
        nextEntityId = id + 1;

        // Добавить в плотные массивы (ID + пустая маска)
        entities.push_back(id);
        entityMasks.push_back(0);

        // Добавить в sparse lookup
        entityLookup[id] = static_cast<buint32>(entities.size() - 1);

        // ИНВАРИАНТ: сущность рождается с TransformComponent (как в createEntity)
        addComponent<TransformComponent>(id, this);

        __blib_log_debug("Created Entity %llu (explicit id, index %u)",
            static_cast<unsigned long long>(id),
            static_cast<unsigned int>(entities.size() - 1));

        return id;
    }

    void Scene::destroyEntity(EntityID id)
    {
        auto it = entityLookup.find(id);
        if (it == entityLookup.end())
        {
            __blib_log_warning("Entity %llu not found, cannot destroy",
                static_cast<unsigned long long>(id));
            return;
        }

        buint32 index = it->second;

        __blib_log_debug("Destroying Entity %llu (index %u)",
            static_cast<unsigned long long>(id), static_cast<unsigned int>(index));

        // Удалить все компоненты Entity из пулов.
        // Проходим по битам маски; для каждого установленного бита
        // вызываем type-erased destroyer конкретного пула.
        ComponentMask mask = entityMasks[index];
        for (ComponentType typeId = 0; mask != 0 && typeId < componentMaskBits; ++typeId)
        {
            ComponentMask bit = static_cast<ComponentMask>(1) << typeId;
            if ((mask & bit) != 0)
            {
                void* poolPtr = componentPools[typeId];
                void (*destroyer)(void*, EntityID) = componentPoolDestroyers[typeId];
                if (poolPtr != nullptr && destroyer != nullptr)
                {
                    destroyer(poolPtr, id);
                }
                else
                {
                    // Маска утверждает что компонент есть, а пула нет —
                    // рассогласование состояния (не должно происходить)
                    __blib_log_warning("Entity %llu: component type %d has no pool destroyer",
                        static_cast<unsigned long long>(id), static_cast<int>(typeId));
                }
                mask &= ~bit;
            }
        }

        // Swap and pop из entities/entityMasks (для сохранения плотности)
        buint32 lastIndex = static_cast<buint32>(entities.size() - 1);
        if (index != lastIndex)
        {
            // Переместить последнюю Entity на место удалённой
            entities[index] = entities[lastIndex];
            entityMasks[index] = entityMasks[lastIndex];

            // Обновить lookup для перемещённой Entity.
            // ВАЖНО: operator[] может рехешировать map и инвалидировать
            // итераторы, поэтому удаление ниже — по ключу, а не по it.
            entityLookup[entities[index]] = index;
        }

        // Удалить последние элементы плотных массивов
        entities.pop_back();
        entityMasks.pop_back();

        // Удалить из lookup (по ключу — безопасно после возможного рехеша)
        entityLookup.erase(id);
    }

    void Scene::setComponentBit(EntityID entityId, ComponentType typeId, bool value)
    {
        if (__blib_unlikely(typeId >= componentMaskBits))
        {
            __blib_fatal("Component type %d exceeds mask width (%d bits)",
                static_cast<int>(typeId), static_cast<int>(componentMaskBits));
        }

        auto it = entityLookup.find(entityId);
        if (__blib_unlikely(it == entityLookup.end()))
        {
            __blib_fatal("Cannot update component mask of non-existent Entity %llu",
                static_cast<unsigned long long>(entityId));
        }

        ComponentMask bit = static_cast<ComponentMask>(1) << typeId;
        if (value)
        {
            entityMasks[it->second] |= bit;
        }
        else
        {
            entityMasks[it->second] &= ~bit;
        }
    }

    bool Scene::getComponentBit(EntityID entityId, ComponentType typeId) const
    {
        if (typeId >= componentMaskBits)
        {
            return false;
        }

        auto it = entityLookup.find(entityId);
        if (it == entityLookup.end())
        {
            return false;
        }

        ComponentMask bit = static_cast<ComponentMask>(1) << typeId;
        return (entityMasks[it->second] & bit) != 0;
    }

    void Scene::addSystem(_In ISystem* system)
    {
        if (system == nullptr)
        {
            __blib_log_warning("Cannot add null system to Scene");
            return;
        }

        // Проверка что система ещё не добавлена
        for (ISystem* existing : systems)
        {
            if (existing == system)
            {
                __blib_log_warning("System %s already added to Scene", system->getName());
                return;
            }
        }

        systems.push_back(system);
        systemsDirty = true; // требуется пересортировка

        __blib_log_info("Added system: %s (priority %d)",
            system->getName(), static_cast<int>(system->getPriority()));
    }

    void Scene::removeSystem(_In ISystem* system)
    {
        if (system == nullptr)
        {
            __blib_log_warning("Cannot remove null system from Scene");
            return;
        }

        auto it = std::find(systems.begin(), systems.end(), system);
        if (it == systems.end())
        {
            __blib_log_warning("System %s not found in Scene", system->getName());
            return;
        }

        systems.erase(it);

        __blib_log_info("Removed system: %s", system->getName());
    }

    void Scene::update(float deltaTime)
    {
        // Пересортировать системы если нужно
        if (systemsDirty)
        {
            sortSystems();
            systemsDirty = false;
        }

        // Вызвать update для каждой системы
        for (ISystem* system : systems)
        {
            system->update(*this, deltaTime);
        }
    }

    void Scene::sortSystems()
    {
        std::sort(systems.begin(), systems.end(), [](ISystem* a, ISystem* b) {
            return a->getPriority() < b->getPriority();
        });

        __blib_log_debug("Systems sorted by priority:");
        for (ISystem* system : systems)
        {
            __blib_log_debug("  - %s (priority %d)",
                system->getName(), static_cast<int>(system->getPriority()));
        }
    }

    void Scene::clear()
    {
        // Удалить Entity (их компоненты удаляются ниже вместе с пулами,
        // но на случай ручного вызова clear в будущем — зачищаем всё)
        entities.clear();
        entityMasks.clear();
        entityLookup.clear();

        // Удалить все component pools (через type-erased deleter функции)
        for (ComponentType typeId = 0; typeId < maxComponentTypes; ++typeId)
        {
            void* poolPtr = componentPools[typeId];
            void (*deleter)(void*) = componentPoolDeleters[typeId];
            if (poolPtr != nullptr && deleter != nullptr)
            {
                deleter(poolPtr);
            }
            componentPools[typeId] = nullptr;
            componentPoolDeleters[typeId] = nullptr;
            componentPoolDestroyers[typeId] = nullptr;
            componentPoolFactories[typeId] = nullptr;
            componentPoolChunkSizes[typeId] = 0;
            componentPoolCreators[typeId] = nullptr;
            componentPoolGetters[typeId] = nullptr;
        }

        // Очистить таблицу типов: имена (литералы — не владеем) и словарь
        // (ключи-строки освобождают буферы через containerAllocator)
        typeNames.clear();
        typeIdByName.clear();

        // Системы не удаляем — Scene не владеет ими
        systems.clear();

        nextEntityId = 1;
        systemsDirty = false;
    }

    void Scene::reset()
    {
        // Сброс ДАННЫХ сцены без сноса реестра типов и списка систем:
        // сцена снова пуста, но типы зарегистрированы и системы
        // висят — хост может scene.load() (пулы создадутся фабриками)
        // или построить контент заново (setupWorld)

        // Сущности: компоненты уничтожаются вместе с пулами ниже —
        // их ResourceRef'ы к кешу отпускаются до unloadAll
        entities.clear();
        entityMasks.clear();
        entityLookup.clear();

        // Пулы уничтожаются (type-erased deleters); fn-таблицы
        // (deleters/factories/creators/getters) ОСТАЮТСЯ — пулы тут же
        // пересоздаются фабриками: addComponent/getComponentPool
        // требуют живого пула
        for (ComponentType typeId = 0; typeId < maxComponentTypes; ++typeId)
        {
            void* poolPtr = componentPools[typeId];
            void (*deleter)(void*) = componentPoolDeleters[typeId];
            if (poolPtr != nullptr && deleter != nullptr)
            {
                deleter(poolPtr);
            }
            componentPools[typeId] = nullptr;
        }

        const ComponentType typeCount = static_cast<ComponentType>(typeNames.size());
        for (ComponentType typeId = 0; typeId < typeCount; ++typeId)
        {
            if (componentPoolFactories[typeId] != nullptr)
            {
                componentPools[typeId] =
                    componentPoolFactories[typeId](componentPoolChunkSizes[typeId]);
            }
        }

        // Кеш ресурсов: слоты (тайлы/модели) освобождаются — после
        // reset сцена строит контент с чистого листа (dedup-индекс
        // остаётся согласованным)
        resources.unloadAll();

        // Системы не трогаем: список и привязки (setRenderTarget
        // систем) остаются валидными

        nextEntityId = 1;
    }

    blib::core::SaveStatus Scene::save(_In blib::core::IOutputStream& os) const
    {
        using blib::core::json::JsonError;
        using blib::core::json::JsonParser;
        using blib::core::json::JsonValue;

        // Магическая сигнатура файла
        if (__blib_unlikely(os.write(sceneSaveMagic, sceneSaveMagicSize) != sceneSaveMagicSize))
        {
            __blib_return_error(blib::core::SaveStatus::WriteFailed,
                "Scene::save: failed to write magic to stream");
        }

        // Документ собирается в память: компонентный JSON пишется в
        // MemoryStream, парсится обратно в DOM и вкладывается в общий
        // документ. Сериализуемость всех компонентов проверяется ДО
        // первой записи в os (кроме magic — см. контракт ISaveable:
        // тест проверяет статус, содержимое при ошибке не определено)
        JsonValue doc = JsonValue::makeObject();
        doc.set(sceneSaveFormatFieldName, JsonValue(sceneSaveFormatName));
        doc.set(sceneSaveVersionField, JsonValue(sceneSaveVersion));
        doc.set(sceneSaveNextEntityIdField, JsonValue(nextEntityId));
        JsonValue& entitiesArr = doc.set(sceneSaveEntitiesField, JsonValue::makeArray());

        for (buint32 denseIndex = 0; denseIndex < static_cast<buint32>(entities.size()); ++denseIndex)
        {
            const EntityID id = entities[denseIndex];
            ComponentMask mask = entityMasks[denseIndex];

            JsonValue entityObj = JsonValue::makeObject();
            entityObj.set(sceneSaveEntityIdField, JsonValue(id));
            JsonValue& compsArr = entityObj.set(sceneSaveEntityComponentsField, JsonValue::makeArray());

            for (ComponentType typeId = 0; mask != 0 && typeId < componentMaskBits; ++typeId)
            {
                const ComponentMask bit = static_cast<ComponentMask>(1) << typeId;
                if ((mask & bit) == 0)
                {
                    continue;
                }
                mask &= ~bit;

                IComponent* component = (componentPoolGetters[typeId] != nullptr)
                    ? componentPoolGetters[typeId](componentPools[typeId], id)
                    : nullptr;
                if (__blib_unlikely(component == nullptr))
                {
                    // Бит маски утверждает наличие компонента, а пула
                    // нет — рассогласование состояния (не сериализуемо)
                    __blib_log_warning("Scene::save: entity %llu type %d has no pool component, skipped",
                        static_cast<unsigned long long>(id), static_cast<int>(typeId));
                    continue;
                }

                // Компонент пишет свой JSON-объект в память
                blib::core::MemoryStream mem;
                const blib::core::SaveStatus status = component->save(mem);
                if (__blib_unlikely(status == blib::core::SaveStatus::Unsupported))
                {
                    __blib_return_error(blib::core::SaveStatus::ComponentNotSerializable,
                        "Scene::save: component type '%s' on entity %llu is not serializable",
                        typeNames[typeId], static_cast<unsigned long long>(id));
                }
                if (__blib_unlikely(status != blib::core::SaveStatus::None))
                {
                    __blib_return_error(status,
                        "Scene::save: component type '%s' on entity %llu failed to save",
                        typeNames[typeId], static_cast<unsigned long long>(id));
                }

                // Компонентный JSON -> DOM-узел для вложения в документ
                JsonValue data;
                mem.seek(0, blib::core::SeekOrigin::Begin);
                JsonParser parser;
                if (__blib_unlikely(parser.parse(mem, data) != JsonError::None))
                {
                    __blib_return_error(blib::core::SaveStatus::WriteFailed,
                        "Scene::save: component type '%s' on entity %llu produced invalid JSON",
                        typeNames[typeId], static_cast<unsigned long long>(id));
                }

                JsonValue entry = JsonValue::makeObject();
                entry.set(sceneSaveComponentTypeField, JsonValue(typeNames[typeId]));
                entry.set(sceneSaveComponentDataField, std::move(data));
                compsArr.pushBack(std::move(entry));
            }

            entitiesArr.pushBack(std::move(entityObj));
        }

        if (__blib_unlikely(doc.writeTo(os) != JsonError::None))
        {
            __blib_return_error(blib::core::SaveStatus::WriteFailed,
                "Scene::save: failed to write JSON document to stream");
        }
        return blib::core::SaveStatus::None;
    }

    blib::core::LoadStatus Scene::load(_In blib::core::IInputStream& is)
    {
        using blib::core::json::JsonError;
        using blib::core::json::JsonParser;
        using blib::core::json::JsonValue;

        // Фаза 1: magic + разбор документа (сцена не меняется)
        buint8 magic[sceneSaveMagicSize];
        if (__blib_unlikely(is.read(magic, sceneSaveMagicSize) != sceneSaveMagicSize))
        {
            __blib_return_error(blib::core::LoadStatus::ReadFailed,
                "Scene::load: stream too short (no magic)");
        }
        if (__blib_unlikely(std::memcmp(magic, sceneSaveMagic, sceneSaveMagicSize) != 0))
        {
            __blib_return_error(blib::core::LoadStatus::UnknownFormat,
                "Scene::load: magic mismatch (not a scene file)");
        }

        JsonParser parser;
        JsonValue doc;
        if (__blib_unlikely(parser.parse(is, doc) != JsonError::None))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "Scene::load: failed to parse JSON document (offset %llu)",
                static_cast<unsigned long long>(parser.getErrorOffset()));
        }
        if (__blib_unlikely(!doc.isObject()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "Scene::load: document root is not an object");
        }
        if (__blib_unlikely(!doc.has(sceneSaveFormatFieldName) ||
            !doc.get(sceneSaveFormatFieldName).isString() ||
            doc.get(sceneSaveFormatFieldName).asString() != sceneSaveFormatName))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "Scene::load: missing or wrong format field");
        }
        if (__blib_unlikely(!doc.has(sceneSaveVersionField) ||
            !doc.get(sceneSaveVersionField).isNumber() ||
            doc.get(sceneSaveVersionField).asBuint64() != sceneSaveVersion))
        {
            __blib_return_error(blib::core::LoadStatus::VersionMismatch,
                "Scene::load: unsupported format version");
        }
        if (__blib_unlikely(!doc.has(sceneSaveEntitiesField) ||
            !doc.get(sceneSaveEntitiesField).isArray()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "Scene::load: missing or malformed entities field");
        }
        if (__blib_unlikely(!doc.has(sceneSaveNextEntityIdField) ||
            !doc.get(sceneSaveNextEntityIdField).isNumber()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "Scene::load: missing or malformed nextEntityId field");
        }

        // load только в пустую сцену (созданные и уничтоженные сущности
        // тоже делают её «непустой»: nextEntityId != 1)
        if (__blib_unlikely(!entities.empty() || nextEntityId != 1))
        {
            __blib_return_error(blib::core::LoadStatus::SceneNotEmpty,
                "Scene::load: scene is not empty (%u entities)",
                static_cast<unsigned int>(entities.size()));
        }

        // Фаза 2: пре-валидация схемы и резолв имён типов (до мутаций):
        // любая ошибка здесь оставляет сцену нетронутой
        struct ComponentRef
        {
            ComponentType typeId;
            const JsonValue* data;
        };
        struct EntityRef
        {
            EntityID id;
            std::vector<ComponentRef, ContainerAllocator<ComponentRef>> comps;

            explicit EntityRef(_In const ContainerAllocator<ComponentRef>& alloc)
                : id(invalidEntity)
                , comps(alloc)
            {
            }
        };

        std::vector<EntityRef, ContainerAllocator<EntityRef>> entityRefs{
            ContainerAllocator<EntityRef>(&containerAllocator) };
        entityRefs.reserve(doc.get(sceneSaveEntitiesField).size());

        const JsonValue& entitiesArr = doc.get(sceneSaveEntitiesField);
        for (buint32 i = 0; i < entitiesArr.size(); ++i)
        {
            const JsonValue& entityNode = entitiesArr[i];
            if (__blib_unlikely(!entityNode.isObject() ||
                !entityNode.has(sceneSaveEntityIdField) ||
                !entityNode.get(sceneSaveEntityIdField).isNumber() ||
                !entityNode.has(sceneSaveEntityComponentsField) ||
                !entityNode.get(sceneSaveEntityComponentsField).isArray()))
            {
                __blib_return_error(blib::core::LoadStatus::InvalidData,
                    "Scene::load: malformed entity entry %u", static_cast<unsigned int>(i));
            }

            const EntityID id = static_cast<EntityID>(entityNode.get(sceneSaveEntityIdField).asBuint64());
            if (__blib_unlikely(id == invalidEntity))
            {
                __blib_return_error(blib::core::LoadStatus::InvalidData,
                    "Scene::load: entity %u has reserved id 0", static_cast<unsigned int>(i));
            }

            EntityRef ref{ ContainerAllocator<ComponentRef>(&containerAllocator) };
            ref.id = id;

            const JsonValue& compsArr = entityNode.get(sceneSaveEntityComponentsField);
            for (buint32 c = 0; c < compsArr.size(); ++c)
            {
                const JsonValue& compNode = compsArr[c];
                if (__blib_unlikely(!compNode.isObject() ||
                    !compNode.has(sceneSaveComponentTypeField) ||
                    !compNode.get(sceneSaveComponentTypeField).isString() ||
                    !compNode.has(sceneSaveComponentDataField)))
                {
                    __blib_return_error(blib::core::LoadStatus::InvalidData,
                        "Scene::load: malformed component entry %u of entity %u",
                        static_cast<unsigned int>(c), static_cast<unsigned int>(i));
                }

                // Резолв имени типа: ключ словаря строится с аллокатором
                // сцены (StdAllocatorAdapter без неявной конверсии)
                const JsonValue::String& typeName = compNode.get(sceneSaveComponentTypeField).asString();
                ComponentTypeNameString key(typeName.data(), typeName.size(),
                    blib::memory::StdAllocatorAdapter<char>(&containerAllocator));
                auto typeIt = typeIdByName.find(key);
                if (__blib_unlikely(typeIt == typeIdByName.end()))
                {
                    __blib_return_error(blib::core::LoadStatus::ComponentTypeNotRegistered,
                        "Scene::load: component type '%s' is not registered in the scene "
                        "(call registerComponentType<T>() before load)",
                        typeName.c_str());
                }

                ComponentRef cref;
                cref.typeId = typeIt->second;
                cref.data = &compNode.get(sceneSaveComponentDataField);
                ref.comps.push_back(cref);
            }

            // ИНВАРИАНТ: TransformComponent обязателен у каждой сущности
            // в файле. Файлы, сохранённые до введения инварианта (сущность
            // без Transform), загрузкой отвергаются. Ровно одна запись:
            // createEntity уже создал Transform, дубликат невозможен.
            buint32 transformEntryCount = 0;
            for (const ComponentRef& cref : ref.comps)
            {
                if (cref.typeId == transformTypeId)
                {
                    ++transformEntryCount;
                }
            }
            if (__blib_unlikely(transformEntryCount != 1))
            {
                __blib_return_error(blib::core::LoadStatus::InvalidData,
                    "Scene::load: entity %llu must have exactly one TransformComponent entry (found %u)",
                    static_cast<unsigned long long>(ref.id),
                    static_cast<unsigned int>(transformEntryCount));
            }

            entityRefs.push_back(std::move(ref));
        }

        // Фаза 3: мутация (создание сущностей и компонентов). При любой
        // ошибке — откат к исходному состоянию: сцена остаётся пустой,
        // реестр типов не трогается
        const buint32 entitiesBefore = static_cast<buint32>(entities.size());
        const EntityID nextEntityIdBefore = nextEntityId;
        ComponentMask poolsCreatedMask = 0;

        // Откат: уничтожить созданные сущности (с их компонентами),
        // удалить пулы, созданные этим load'ом, вернуть nextEntityId
        auto rollbackLoad = [this, entitiesBefore, nextEntityIdBefore, &poolsCreatedMask]() {
            while (static_cast<buint32>(entities.size()) > entitiesBefore)
            {
                destroyEntity(entities.back());
            }
            for (ComponentType typeId = 0; typeId < maxComponentTypes; ++typeId)
            {
                const ComponentMask bit = static_cast<ComponentMask>(1) << typeId;
                if ((poolsCreatedMask & bit) == 0)
                {
                    continue;
                }
                if (componentPools[typeId] != nullptr && componentPoolDeleters[typeId] != nullptr)
                {
                    componentPoolDeleters[typeId](componentPools[typeId]);
                }
                componentPools[typeId] = nullptr;
            }
            nextEntityId = nextEntityIdBefore;
        };

        for (const EntityRef& ref : entityRefs)
        {
            // ID в файле последовательны (никогда не переиспользуются),
            // поэтому порядок массива обязан совпадать с генерацией
            const EntityID createdId = createEntity();
            if (__blib_unlikely(createdId != ref.id))
            {
                __blib_log_error("Scene::load: entity id %llu in file does not match generated id %llu",
                    static_cast<unsigned long long>(ref.id),
                    static_cast<unsigned long long>(createdId));
                rollbackLoad();
                __blib_return_error(blib::core::LoadStatus::InvalidData,
                    "Scene::load: entity ids are not sequential");
            }

            for (const ComponentRef& cref : ref.comps)
            {
                // Пул типа создаётся по требованию (тип зарегистрирован,
                // пул — нет)
                void* poolPtr = componentPools[cref.typeId];
                if (poolPtr == nullptr)
                {
                    poolPtr = componentPoolFactories[cref.typeId](componentPoolChunkSizes[cref.typeId]);
                    if (__blib_unlikely(poolPtr == nullptr))
                    {
                        rollbackLoad();
                        __blib_return_error(blib::core::LoadStatus::ReadFailed,
                            "Scene::load: failed to create pool for type '%s'", typeNames[cref.typeId]);
                    }
                    componentPools[cref.typeId] = poolPtr;
                    poolsCreatedMask |= static_cast<ComponentMask>(1) << cref.typeId;
                }

                IComponent* component = nullptr;
                if (cref.typeId == transformTypeId)
                {
                    // ИНВАРИАНТ: Transform уже создан createEntity —
                    // второй компонент не создаём, данные файла загружаются
                    // в существующий (пул типа создан при регистрации в
                    // конструкторе сцены, поэтому poolPtr не null)
                    component = (componentPoolGetters[cref.typeId] != nullptr)
                        ? componentPoolGetters[cref.typeId](poolPtr, ref.id)
                        : nullptr;
                }
                else
                {
                    IComponent* (*creator)(void*, EntityID) = componentPoolCreators[cref.typeId];
                    if (__blib_unlikely(creator == nullptr))
                    {
                        // Тип зарегистрирован, но не loadable (нет default-ctor)
                        rollbackLoad();
                        __blib_return_error(blib::core::LoadStatus::InvalidData,
                            "Scene::load: component type '%s' is not loadable "
                            "(no default constructor)", typeNames[cref.typeId]);
                    }
                    component = creator(poolPtr, ref.id);
                }
                if (__blib_unlikely(component == nullptr))
                {
                    rollbackLoad();
                    __blib_return_error(blib::core::LoadStatus::ReadFailed,
                        "Scene::load: failed to create component type '%s' for entity %llu",
                        typeNames[cref.typeId], static_cast<unsigned long long>(ref.id));
                }

                // Компонентный JSON-узел -> MemoryStream -> load()
                blib::core::MemoryStream mem;
                if (__blib_unlikely(cref.data->writeTo(mem) != JsonError::None))
                {
                    rollbackLoad();
                    __blib_return_error(blib::core::LoadStatus::InvalidData,
                        "Scene::load: failed to serialize component data of type '%s'",
                        typeNames[cref.typeId]);
                }
                mem.seek(0, blib::core::SeekOrigin::Begin);
                const blib::core::LoadStatus status = component->load(mem);
                if (__blib_unlikely(status != blib::core::LoadStatus::None))
                {
                    rollbackLoad();
                    __blib_return_error(blib::core::LoadStatus::InvalidData,
                        "Scene::load: component type '%s' of entity %llu failed to load (status %u)",
                        typeNames[cref.typeId], static_cast<unsigned long long>(ref.id),
                        static_cast<unsigned int>(status));
                }

                setComponentBit(ref.id, cref.typeId, true);
            }
        }

        // nextEntityId из файла обязан покрывать все созданные ID —
        // иначе будущие createEntity() столкнутся с загруженными
        const EntityID loadedNextId = static_cast<EntityID>(doc.get(sceneSaveNextEntityIdField).asBuint64());
        if (__blib_unlikely(loadedNextId < nextEntityId))
        {
            rollbackLoad();
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "Scene::load: nextEntityId %llu collides with loaded entity ids",
                static_cast<unsigned long long>(loadedNextId));
        }
        nextEntityId = loadedNextId;

        // Фаза 4: восстановление контекстных связей (onLoaded) — по
        // возрастанию ComponentType внутри каждой сущности (см.
        // IComponent::onLoaded — порядок и зависимости)
        for (buint32 denseIndex = 0; denseIndex < static_cast<buint32>(entities.size()); ++denseIndex)
        {
            const EntityID id = entities[denseIndex];
            ComponentMask mask = entityMasks[denseIndex];
            for (ComponentType typeId = 0; mask != 0 && typeId < componentMaskBits; ++typeId)
            {
                const ComponentMask bit = static_cast<ComponentMask>(1) << typeId;
                if ((mask & bit) == 0)
                {
                    continue;
                }
                mask &= ~bit;

                IComponent* component = (componentPoolGetters[typeId] != nullptr)
                    ? componentPoolGetters[typeId](componentPools[typeId], id)
                    : nullptr;
                if (component != nullptr)
                {
                    component->onLoaded(*this);
                }
            }
        }

        return blib::core::LoadStatus::None;
    }

    bool Scene::strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const
    {
        // Защита от циклов (Scene -> пул -> компонент -> ownerScene -> ...)
        if (!session.enter(this, &other))
        {
            return true;
        }

        const Scene& o = static_cast<const Scene&>(other);

        if (nextEntityId != o.nextEntityId)
        {
            return false;
        }
        if (entities != o.entities || entityMasks != o.entityMasks)
        {
            return false;
        }

        // Компоненты: детерминированный порядок — сущности в dense-порядке,
        // типы по возрастанию ComponentType
        for (buint32 denseIndex = 0; denseIndex < static_cast<buint32>(entities.size()); ++denseIndex)
        {
            const EntityID id = entities[denseIndex];
            ComponentMask mask = entityMasks[denseIndex];
            for (ComponentType typeId = 0; mask != 0 && typeId < componentMaskBits; ++typeId)
            {
                const ComponentMask bit = static_cast<ComponentMask>(1) << typeId;
                if ((mask & bit) == 0)
                {
                    continue;
                }
                mask &= ~bit;

                IComponent* a = (componentPoolGetters[typeId] != nullptr)
                    ? componentPoolGetters[typeId](componentPools[typeId], id)
                    : nullptr;
                IComponent* b = (o.componentPoolGetters[typeId] != nullptr)
                    ? o.componentPoolGetters[typeId](o.componentPools[typeId], id)
                    : nullptr;

                if (a == nullptr || b == nullptr)
                {
                    if (a != b)
                    {
                        return false;
                    }
                    continue;
                }
                if (!a->strongCompare(*b, session))
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool Scene::verify() const
    {
        // Round-trip без RTTI: save -> MemoryStream -> свежая сцена с
        // копией реестра типов (без compile-time T) -> load ->
        // strongCompare (см. blib::core::ISaveLoadable::verify)
        blib::core::MemoryStream mem;
        if (this->save(mem) != blib::core::SaveStatus::None)
        {
            return false;
        }

        mem.seek(0, blib::core::SeekOrigin::Begin);
        Scene temp;
        temp.copyComponentTypeRegistryFrom(*this);
        if (temp.load(mem) != blib::core::LoadStatus::None)
        {
            return false;
        }

        return this->strongCompare(temp);
    }

    void Scene::copyComponentTypeRegistryFrom(_In const Scene& other)
    {
        // Сцена должна быть свежей: единственный разрешённый тип —
        // авто-зарегистрированный TransformComponent (typeId 0, инвариант).
        // Другие типы сместили бы локальные индексы относительно источника.
        if (__blib_unlikely(typeNames.size() != 1 ||
            std::strcmp(typeNames[0], TransformComponent::componentTypeName) != 0))
        {
            __blib_fatal("Scene::copyComponentTypeRegistryFrom: target scene has extra component types");
        }

        // Type-erased fn-таблицы: лямбды регистрации не захватывают
        // состояние — их можно безопасно копировать между сценами
        for (ComponentType typeId = 0; typeId < maxComponentTypes; ++typeId)
        {
            componentPoolDeleters[typeId] = other.componentPoolDeleters[typeId];
            componentPoolDestroyers[typeId] = other.componentPoolDestroyers[typeId];
            componentPoolFactories[typeId] = other.componentPoolFactories[typeId];
            componentPoolChunkSizes[typeId] = other.componentPoolChunkSizes[typeId];
            componentPoolCreators[typeId] = other.componentPoolCreators[typeId];
            componentPoolGetters[typeId] = other.componentPoolGetters[typeId];
            // Дескрипторы рефлексии — статические объекты компонентов:
            // копируются указателями, живы всё время процесса
            componentReflections[typeId] = other.componentReflections[typeId];
        }

        // Имена типов — литералы компонентов (живут всё время процесса),
        // словарь перестраивается с АЛЛОКАТОРОМ целевой сцены.
        // TransformComponent пропускаем: он уже зарегистрирован
        // конструктором целевой сцены (typeId 0 в обеих сценах).
        const ComponentType count = static_cast<ComponentType>(other.typeNames.size());
        for (ComponentType typeId = 0; typeId < count; ++typeId)
        {
            const char* name = other.typeNames[typeId];
            if (std::strcmp(name, TransformComponent::componentTypeName) == 0)
            {
                continue;
            }

            typeNames.push_back(name);

            ComponentTypeNameString key(name,
                blib::memory::StdAllocatorAdapter<char>(&containerAllocator));
            typeIdByName.emplace(std::move(key), typeId);
        }
    }

    // ========== Reflection / Inspector API (type-erased) ==========

    const ComponentTypeDescriptor* Scene::tryGetComponentReflection(ComponentType typeId) const
    {
        if (typeId >= maxComponentTypes)
        {
            return nullptr;
        }
        return componentReflections[typeId];
    }

    const char* Scene::getComponentTypeName(ComponentType typeId) const
    {
        return (typeId < typeNames.size()) ? typeNames[typeId] : nullptr;
    }

    EntityID Scene::getEntityId(buint32 denseIndex) const
    {
        return (denseIndex < entities.size()) ? entities[denseIndex] : invalidEntity;
    }

    bool Scene::hasComponent(EntityID entityId, ComponentType typeId) const
    {
        // Не-fatal: невалидные typeId/сущность → false (Inspector
        // работает с устаревшими ID после scene_load — см. GRAVELANDS.md)
        if (typeId >= maxComponentTypes)
        {
            return false;
        }
        return getComponentBit(entityId, typeId);
    }

    IComponent* Scene::tryGetComponent(EntityID entityId, ComponentType typeId)
    {
        if (typeId >= maxComponentTypes || componentPoolGetters[typeId] == nullptr)
        {
            return nullptr;
        }
        if (!getComponentBit(entityId, typeId))
        {
            return nullptr;
        }
        return componentPoolGetters[typeId](componentPools[typeId], entityId);
    }

    const IComponent* Scene::tryGetComponent(EntityID entityId, ComponentType typeId) const
    {
        // Неконстантный getter читает пул и не мутирует его — const_cast
        // оправдан: контракт const-версии гарантирует только чтение
        return const_cast<Scene*>(this)->tryGetComponent(entityId, typeId);
    }

    bool Scene::addComponent(EntityID entityId, ComponentType typeId)
    {
        // ИНВАРИАНТ: TransformComponent уже есть у каждой сущности —
        // явное добавление отклоняется (как шаблонный addComponent)
        if (__blib_unlikely(typeId == transformTypeId))
        {
            __blib_log_warning("Scene::addComponent: TransformComponent cannot be added "
                "(scene invariant: it already exists on every entity)");
            return false;
        }

        if (__blib_unlikely(typeId >= maxComponentTypes ||
            componentPoolCreators[typeId] == nullptr || componentPools[typeId] == nullptr))
        {
            __blib_log_warning("Scene::addComponent: type %u is not registered or not default-constructible",
                static_cast<unsigned int>(typeId));
            return false;
        }

        if (__blib_unlikely(entityLookup.find(entityId) == entityLookup.end()))
        {
            __blib_log_warning("Scene::addComponent: entity %llu does not exist",
                static_cast<unsigned long long>(entityId));
            return false;
        }

        // Компонент уже есть — повторное добавление запрещено
        if (getComponentBit(entityId, typeId))
        {
            __blib_log_warning("Scene::addComponent: component type %u already exists on entity %llu",
                static_cast<unsigned int>(typeId), static_cast<unsigned long long>(entityId));
            return false;
        }

        IComponent* component = componentPoolCreators[typeId](componentPools[typeId], entityId);
        if (__blib_unlikely(component == nullptr))
        {
            return false;
        }

        setComponentBit(entityId, typeId, true);
        return true;
    }

    bool Scene::removeComponent(EntityID entityId, ComponentType typeId)
    {
        // ИНВАРИАНТ: TransformComponent снять нельзя — сущность без
        // Transform существовать не может
        if (__blib_unlikely(typeId == transformTypeId))
        {
            __blib_log_warning("Scene::removeComponent: TransformComponent cannot be removed "
                "(scene invariant: every entity must have a Transform)");
            return false;
        }

        if (__blib_unlikely(typeId >= maxComponentTypes ||
            componentPools[typeId] == nullptr || componentPoolDestroyers[typeId] == nullptr))
        {
            return false;
        }

        if (__blib_unlikely(entityLookup.find(entityId) == entityLookup.end()))
        {
            return false;
        }

        if (!getComponentBit(entityId, typeId))
        {
            return false;
        }

        componentPoolDestroyers[typeId](componentPools[typeId], entityId);
        setComponentBit(entityId, typeId, false);
        return true;
    }

} // namespace beng
