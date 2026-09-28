#include <beng/core/commandHistory.h>

#include <beng/core/scene.h>

#include <blib/core/console/console.h>
#include <blib/system/memory/globalAllocator.h>

namespace beng
{
    namespace editor
    {
        // ========== ComponentFieldCommand ==========

        ComponentFieldCommand::ComponentFieldCommand(
            _In Scene* scene, EntityID entityId, ComponentType typeId,
            buint32 fieldIndex, _In const FieldValue& oldValue, _In const FieldValue& newValue)
            : scene(scene)
            , entityId(entityId)
            , typeId(typeId)
            , fieldIndex(fieldIndex)
            , oldValue(oldValue)
            , newValue(newValue)
        {
        }

        void ComponentFieldCommand::applyField(_In const FieldValue& value)
        {
            // Резолв заново: сущность/компонент могли исчезнуть
            // (удаление, scene_load) — команда молча пропускается
            IComponent* component = this->scene->tryGetComponent(this->entityId, this->typeId);
            if (__blib_unlikely(component == nullptr))
            {
                return;
            }

            const ComponentTypeDescriptor* descriptor = this->scene->tryGetComponentReflection(this->typeId);
            if (__blib_unlikely(descriptor == nullptr))
            {
                return;
            }

            const IComponentField* field = descriptor->getField(this->fieldIndex);
            if (__blib_unlikely(field == nullptr || value.kind != field->getKind()))
            {
                return;
            }

            field->setValue(*component, value);
        }

        // ========== TransformChangeCommand ==========

        TransformChangeCommand::TransformChangeCommand(
            _In Scene* scene, EntityID entityId,
            _In const TransformSnapshot& oldTrs, _In const TransformSnapshot& newTrs)
            : scene(scene)
            , entityId(entityId)
            , oldTrs(oldTrs)
            , newTrs(newTrs)
        {
        }

        void TransformChangeCommand::applyTrs(_In const TransformSnapshot& snapshot)
        {
            TransformComponent* transform =
                this->scene->tryGetComponent<TransformComponent>(this->entityId);
            if (__blib_unlikely(transform == nullptr))
            {
                return;
            }

            transform->setLocalPosition(snapshot.position);
            transform->setLocalRotation(snapshot.rotation);
            transform->setLocalScale(snapshot.scale);
        }

        // ========== EntityCreateCommand ==========

        EntityCreateCommand::EntityCreateCommand(_In Scene* scene)
            : scene(scene)
            , entityId(scene->createEntity())
        {
        }

        void EntityCreateCommand::undo()
        {
            // Сущность ещё жива? (у живой всегда есть Transform — typeId 0)
            if (this->scene->tryGetComponent(this->entityId, this->scene->getTransformTypeId()) != nullptr)
            {
                this->scene->destroyEntity(this->entityId);
            }
        }

        void EntityCreateCommand::redo()
        {
            // Пересоздание — с НОВЫМ ID (ID не переиспользуются)
            this->entityId = this->scene->createEntity();
        }

        // ========== EntityDestroyCommand ==========

        EntityDestroyCommand::EntityDestroyCommand(_In Scene* scene, EntityID entityId)
            : scene(scene)
            , entityId(entityId)
            , snapshotAllocator()
            , snapshots(blib::memory::StdAllocatorAdapter<ComponentSnapshot>(&this->snapshotAllocator))
        {
            this->captureAndDestroy();
        }

        void EntityDestroyCommand::captureAndDestroy()
        {
            // Снимки компонентов (сериализацией): Transform — тоже
            // (восстановление TRS при undo). Несериализуемые — пропуск
            const buint32 typeCount = this->scene->getComponentTypeCount();
            for (buint32 t = 0; t < typeCount; ++t)
            {
                const ComponentType typeId = static_cast<ComponentType>(t);
                if (!this->scene->hasComponent(this->entityId, typeId))
                {
                    continue;
                }

                IComponent* component = this->scene->tryGetComponent(this->entityId, typeId);
                if (__blib_unlikely(component == nullptr))
                {
                    continue;
                }

                blib::core::MemoryStream stream;
                const blib::core::SaveStatus status = component->save(stream);
                if (__blib_unlikely(status != blib::core::SaveStatus::None))
                {
                    __blib_log_warning("EntityDestroyCommand: component type %u is not serializable — "
                        "it will NOT be restored on undo",
                        static_cast<unsigned int>(typeId));
                    continue;
                }

                ComponentSnapshot snapshot;
                snapshot.typeId = typeId;
                snapshot.data = std::move(stream);
                this->snapshots.push_back(std::move(snapshot));
            }

            this->scene->destroyEntity(this->entityId);
        }

        void EntityDestroyCommand::undo()
        {
            // Пересоздание с НОВЫМ ID (инвариант: ID не переиспользуются);
            // Transform авто-создаётся createEntity — его снимок грузится
            // в него же, остальные компоненты добавляются заново
            this->entityId = this->scene->createEntity();

            for (const ComponentSnapshot& snapshot : this->snapshots)
            {
                IComponent* component = nullptr;
                if (snapshot.typeId == this->scene->getTransformTypeId())
                {
                    // Авто-созданный Transform (инвариант сцены)
                    component = this->scene->tryGetComponent(this->entityId, snapshot.typeId);
                }
                else if (this->scene->addComponent(this->entityId, snapshot.typeId))
                {
                    component = this->scene->tryGetComponent(this->entityId, snapshot.typeId);
                }
                else
                {
                    __blib_log_warning("EntityDestroyCommand: cannot restore component type %u on undo",
                        static_cast<unsigned int>(snapshot.typeId));
                    continue;
                }

                if (__blib_unlikely(component == nullptr))
                {
                    continue;
                }

                blib::core::MemoryStream readStream(snapshot.data);
                readStream.seek(0, blib::core::SeekOrigin::Begin);
                const blib::core::LoadStatus status = component->load(readStream);
                if (__blib_unlikely(status != blib::core::LoadStatus::None))
                {
                    __blib_log_warning("EntityDestroyCommand: component type %u failed to load snapshot (status %u)",
                        static_cast<unsigned int>(snapshot.typeId), static_cast<unsigned int>(status));
                }
            }

            // Вторая фаза: восстановление контекстных связей (модели —
            // через кеш сцены и т.п.). Снимки упорядочены по возрастанию
            // typeId (порядок регистрации) — как требует Scene::load
            for (const ComponentSnapshot& snapshot : this->snapshots)
            {
                IComponent* component = this->scene->tryGetComponent(this->entityId, snapshot.typeId);
                if (component != nullptr)
                {
                    component->onLoaded(*this->scene);
                }
            }
        }

        void EntityDestroyCommand::redo()
        {
            if (this->scene->tryGetComponent(this->entityId, this->scene->getTransformTypeId()) != nullptr)
            {
                this->scene->destroyEntity(this->entityId);
            }
        }

        // ========== ComponentAddCommand ==========

        ComponentAddCommand::ComponentAddCommand(_In Scene* scene, EntityID entityId, ComponentType typeId)
            : scene(scene)
            , entityId(entityId)
            , typeId(typeId)
        {
        }

        void ComponentAddCommand::undo()
        {
            this->scene->removeComponent(this->entityId, this->typeId);
        }

        void ComponentAddCommand::redo()
        {
            this->scene->addComponent(this->entityId, this->typeId);
        }

        // ========== ComponentRemoveCommand ==========

        ComponentRemoveCommand::ComponentRemoveCommand(_In Scene* scene, EntityID entityId, ComponentType typeId)
            : scene(scene)
            , entityId(entityId)
            , typeId(typeId)
            , snapshot()
            , hasSnapshot(false)
        {
            // Снимок до удаления (несериализуемый — undo вернёт пустой)
            IComponent* component = this->scene->tryGetComponent(this->entityId, this->typeId);
            if (component != nullptr && component->save(this->snapshot) == blib::core::SaveStatus::None)
            {
                this->hasSnapshot = true;
            }

            this->scene->removeComponent(this->entityId, this->typeId);
        }

        void ComponentRemoveCommand::undo()
        {
            if (!this->scene->addComponent(this->entityId, this->typeId))
            {
                return;
            }

            if (!this->hasSnapshot)
            {
                return;
            }

            IComponent* component = this->scene->tryGetComponent(this->entityId, this->typeId);
            if (__blib_unlikely(component == nullptr))
            {
                return;
            }

            blib::core::MemoryStream readStream(this->snapshot);
            readStream.seek(0, blib::core::SeekOrigin::Begin);
            const blib::core::LoadStatus status = component->load(readStream);
            if (__blib_unlikely(status != blib::core::LoadStatus::None))
            {
                __blib_log_warning("ComponentRemoveCommand: failed to load component snapshot (status %u)",
                    static_cast<unsigned int>(status));
                return;
            }

            // Восстановление контекста (модели — через кеш сцены)
            component->onLoaded(*this->scene);
        }

        void ComponentRemoveCommand::redo()
        {
            this->scene->removeComponent(this->entityId, this->typeId);
        }

        // ========== CommandHistory ==========

        CommandHistory::CommandHistory()
            : containerAllocator()
            , undoStack(blib::memory::StdAllocatorAdapter<CommandEntry>(&this->containerAllocator))
            , redoStack(blib::memory::StdAllocatorAdapter<CommandEntry>(&this->containerAllocator))
        {
        }

        CommandHistory::~CommandHistory()
        {
            this->clear();
        }

        void CommandHistory::destroyEntry(_In CommandEntry& entry)
        {
            if (entry.command != nullptr && entry.destroy != nullptr)
            {
                entry.destroy(entry.command);
            }
            entry.command = nullptr;
            entry.destroy = nullptr;
        }

        void CommandHistory::clear()
        {
            for (CommandEntry& entry : this->undoStack)
            {
                destroyEntry(entry);
            }
            for (CommandEntry& entry : this->redoStack)
            {
                destroyEntry(entry);
            }
            this->undoStack.clear();
            this->redoStack.clear();
        }

        void CommandHistory::trimUndoStack()
        {
            // Сверх лимита — уничтожаем САМЫЕ СТАРЫЕ (в начале)
            while (this->undoStack.size() > CommandHistory::maxHistorySize)
            {
                destroyEntry(this->undoStack.front());
                this->undoStack.erase(this->undoStack.begin());
            }
        }

        template<typename T>
        void CommandHistory::pushCommand(_In T* command)
        {
            // Новая команда обрывает ветку повторов
            for (CommandEntry& entry : this->redoStack)
            {
                destroyEntry(entry);
            }
            this->redoStack.clear();

            CommandEntry entry;
            entry.command = command;
            entry.destroy = [](_In IEditorCommand* command) {
                T* typed = static_cast<T*>(command);
                typed->~T();
                blib::memory::GlobalAllocator::instance().deallocate(typed, sizeof(T));
            };
            this->undoStack.push_back(entry);

            this->trimUndoStack();
        }

        bool CommandHistory::undo()
        {
            if (this->undoStack.empty())
            {
                return false;
            }

            CommandEntry entry = this->undoStack.back();
            this->undoStack.pop_back();
            entry.command->undo();
            this->redoStack.push_back(entry);
            return true;
        }

        bool CommandHistory::redo()
        {
            if (this->redoStack.empty())
            {
                return false;
            }

            CommandEntry entry = this->redoStack.back();
            this->redoStack.pop_back();
            entry.command->redo();
            this->undoStack.push_back(entry);
            this->trimUndoStack();
            return true;
        }

        buint32 CommandHistory::getUndoCount() const
        {
            return static_cast<buint32>(this->undoStack.size());
        }

        buint32 CommandHistory::getRedoCount() const
        {
            return static_cast<buint32>(this->redoStack.size());
        }

        void CommandHistory::recordFieldChange(
            _In Scene& scene, EntityID entityId, ComponentType typeId,
            buint32 fieldIndex, _In const FieldValue& oldValue, _In const FieldValue& newValue)
        {
            auto& globalAllocator = blib::memory::GlobalAllocator::instance();
            void* memory = globalAllocator.allocate(sizeof(ComponentFieldCommand));
            if (__blib_unlikely(memory == nullptr))
            {
                return;
            }
            ComponentFieldCommand* command = new (memory) ComponentFieldCommand(
                &scene, entityId, typeId, fieldIndex, oldValue, newValue);
            this->pushCommand(command);
        }

        void CommandHistory::recordTransformChange(
            _In Scene& scene, EntityID entityId,
            _In const TransformSnapshot& oldTrs, _In const TransformSnapshot& newTrs)
        {
            auto& globalAllocator = blib::memory::GlobalAllocator::instance();
            void* memory = globalAllocator.allocate(sizeof(TransformChangeCommand));
            if (__blib_unlikely(memory == nullptr))
            {
                return;
            }
            TransformChangeCommand* command = new (memory) TransformChangeCommand(
                &scene, entityId, oldTrs, newTrs);
            this->pushCommand(command);
        }

        EntityID CommandHistory::recordEntityCreate(_In Scene& scene)
        {
            auto& globalAllocator = blib::memory::GlobalAllocator::instance();
            void* memory = globalAllocator.allocate(sizeof(EntityCreateCommand));
            if (__blib_unlikely(memory == nullptr))
            {
                return invalidEntity;
            }
            EntityCreateCommand* command = new (memory) EntityCreateCommand(&scene);
            const EntityID entityId = command->getEntityId();
            this->pushCommand(command);
            return entityId;
        }

        bool CommandHistory::recordEntityDestroy(_In Scene& scene, EntityID entityId)
        {
            if (__blib_unlikely(scene.tryGetComponent(entityId, scene.getTransformTypeId()) == nullptr))
            {
                return false;
            }

            auto& globalAllocator = blib::memory::GlobalAllocator::instance();
            void* memory = globalAllocator.allocate(sizeof(EntityDestroyCommand));
            if (__blib_unlikely(memory == nullptr))
            {
                return false;
            }
            EntityDestroyCommand* command = new (memory) EntityDestroyCommand(&scene, entityId);
            this->pushCommand(command);
            return true;
        }

        bool CommandHistory::recordComponentAdd(_In Scene& scene, EntityID entityId, ComponentType typeId)
        {
            // Команда создаётся только если компонент реально добавлен
            if (!scene.addComponent(entityId, typeId))
            {
                return false;
            }

            auto& globalAllocator = blib::memory::GlobalAllocator::instance();
            void* memory = globalAllocator.allocate(sizeof(ComponentAddCommand));
            if (__blib_unlikely(memory == nullptr))
            {
                scene.removeComponent(entityId, typeId);
                return false;
            }
            ComponentAddCommand* command = new (memory) ComponentAddCommand(&scene, entityId, typeId);
            this->pushCommand(command);
            return true;
        }

        bool CommandHistory::recordComponentRemove(_In Scene& scene, EntityID entityId, ComponentType typeId)
        {
            if (__blib_unlikely(!scene.hasComponent(entityId, typeId)))
            {
                return false;
            }

            auto& globalAllocator = blib::memory::GlobalAllocator::instance();
            void* memory = globalAllocator.allocate(sizeof(ComponentRemoveCommand));
            if (__blib_unlikely(memory == nullptr))
            {
                return false;
            }
            ComponentRemoveCommand* command = new (memory) ComponentRemoveCommand(&scene, entityId, typeId);
            this->pushCommand(command);
            return true;
        }

    } // namespace editor
} // namespace beng
