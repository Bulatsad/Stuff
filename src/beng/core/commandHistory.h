#pragma once

#include <beng/config.h>
#include <beng/core/componentReflection.h>
#include <beng/components/transform.h>

#include <blib/core/math/quaternion.h>
#include <blib/core/math/vector.h>
#include <blib/core/memoryStream.h>
#include <blib/system/memory/stdAllocatorAdapter.h>

#include <vector>

namespace beng
{
    class Scene;

    namespace editor
    {
        /**
         * TransformSnapshot — снимок локального TRS сущности
         * (position/rotation/scale) для команд gizmo.
         */
        struct __beng_api TransformSnapshot
        {
            blib::math::Vector<float, 3> position;
            blib::math::Quaternion<float> rotation;
            blib::math::Vector<float, 3> scale;

            TransformSnapshot()
                : position(0.0f, 0.0f, 0.0f)
                , rotation()
                , scale(1.0f, 1.0f, 1.0f)
            {
            }
        };

        /**
         * IEditorCommand — команда эдитора для undo/redo.
         *
         * Контракт:
         * - Команда создаётся УЖЕ выполненной (действие применено
         *   вызывающим/конструктором); undo() откатывает, redo()
         *   повторяет;
         * - undo/redo обязаны быть безопасны на «протухших» данных:
         *   сцена могла перезагрузиться (scene_load), сущность/компонент
         *   могли исчезнуть — команда резолвит всё заново по EntityID/
         *   typeId и молча пропускает невозможное;
         * - Команды живут в истории (CommandHistory), аллоцируются
         *   через GlobalAllocator; владелец уничтожает их явно.
         */
        class __beng_api IEditorCommand
        {
        public:
            virtual ~IEditorCommand() = default;

            /**
             * Имя команды (для UI/логов истории).
             */
            virtual const char* getName() const __blib_pure_virtual_function;

            /**
             * Откатить действие команды.
             */
            virtual void undo() __blib_pure_virtual_function;

            /**
             * Повторить действие команды (после undo).
             */
            virtual void redo() __blib_pure_virtual_function;
        };

        /**
         * ComponentFieldCommand — правка поля компонента через
         * рефлексию (Inspector): запоминает старое/новое FieldValue,
         * undo/redo применяет их полю (IComponentField::setValue).
         * Работает с ЛЮБЫМ компонентом с рефлексией — эдитор не знает
         * конкретных типов.
         */
        class __beng_api ComponentFieldCommand : public IEditorCommand
        {
        private:
            static constexpr const char* commandName = "Edit Field";

            Scene* scene;
            EntityID entityId;
            ComponentType typeId;
            buint32 fieldIndex;
            FieldValue oldValue;
            FieldValue newValue;

            void applyField(_In const FieldValue& value);

        public:
            ComponentFieldCommand(
                _In Scene* scene, EntityID entityId, ComponentType typeId,
                buint32 fieldIndex, _In const FieldValue& oldValue, _In const FieldValue& newValue);

            const char* getName() const __blib_override { return ComponentFieldCommand::commandName; }
            void undo() __blib_override { this->applyField(this->oldValue); }
            void redo() __blib_override { this->applyField(this->newValue); }
        };

        /**
         * TransformChangeCommand — смена локального TRS сущности
         * (gizmo: translate/rotate/scale; вращение не покрывается
         * рефлексией — снимок кватернионом).
         */
        class __beng_api TransformChangeCommand : public IEditorCommand
        {
        private:
            static constexpr const char* commandName = "Transform";

            Scene* scene;
            EntityID entityId;
            TransformSnapshot oldTrs;
            TransformSnapshot newTrs;

            void applyTrs(_In const TransformSnapshot& snapshot);

        public:
            TransformChangeCommand(
                _In Scene* scene, EntityID entityId,
                _In const TransformSnapshot& oldTrs, _In const TransformSnapshot& newTrs);

            const char* getName() const __blib_override { return TransformChangeCommand::commandName; }
            void undo() __blib_override { this->applyTrs(this->oldTrs); }
            void redo() __blib_override { this->applyTrs(this->newTrs); }
        };

        /**
         * EntityCreateCommand — создание сущности (undo: удалить;
         * redo: создать заново — с НОВЫМ ID: ID сущностей не
         * переиспользуются, инвариант сцены).
         */
        class __beng_api EntityCreateCommand : public IEditorCommand
        {
        private:
            static constexpr const char* commandName = "Create Entity";

            Scene* scene;
            EntityID entityId;

        public:
            // Конструктор создаёт сущность (команда выполнена сразу)
            explicit EntityCreateCommand(_In Scene* scene);

            const char* getName() const __blib_override { return EntityCreateCommand::commandName; }
            void undo() __blib_override;
            void redo() __blib_override;

            /**
             * Текущий ID созданной сущности (меняется после redo).
             */
            EntityID getEntityId() const { return this->entityId; }
        };

        /**
         * EntityDestroyCommand — удаление сущности со снимком её
         * компонентов (сериализация). undo: пересоздать сущность
         * (новый ID — инвариант) + восстановить компоненты из
         * снимков (Transform грузится в авто-созданный); redo: удалить.
         *
         * Ограничения (осознанные): ссылки ДРУГИХ компонентов на
         * пересозданную сущность (parent, BlobShadow target) не
         * восстанавливаются; несериализуемые компоненты (save() ==
         * Unsupported) в снимки не попадают и не восстанавливаются.
         */
        class __beng_api EntityDestroyCommand : public IEditorCommand
        {
        private:
            static constexpr const char* commandName = "Destroy Entity";

            struct ComponentSnapshot
            {
                ComponentType typeId;
                blib::core::MemoryStream data;
            };

            Scene* scene;
            EntityID entityId;

            // Снимки компонентов (аллокатор обязан жить дольше вектора)
            blib::memory::Allocator snapshotAllocator;
            std::vector<ComponentSnapshot, blib::memory::StdAllocatorAdapter<ComponentSnapshot>> snapshots;

            // Снять компоненты сущности (сериализацией) и удалить её
            void captureAndDestroy();

        public:
            // Конструктор снимает компоненты и удаляет сущность сразу
            EntityDestroyCommand(_In Scene* scene, EntityID entityId);

            const char* getName() const __blib_override { return EntityDestroyCommand::commandName; }
            void undo() __blib_override;
            void redo() __blib_override;
        };

        /**
         * ComponentAddCommand — добавление компонента (type-erased;
         * undo: удалить, redo: добавить заново).
         */
        class __beng_api ComponentAddCommand : public IEditorCommand
        {
        private:
            static constexpr const char* commandName = "Add Component";

            Scene* scene;
            EntityID entityId;
            ComponentType typeId;

        public:
            // Компонент УЖЕ добавлен вызывающим; команда только
            // откатывает/повторяет (см. CommandHistory::recordComponentAdd)
            ComponentAddCommand(_In Scene* scene, EntityID entityId, ComponentType typeId);

            const char* getName() const __blib_override { return ComponentAddCommand::commandName; }
            void undo() __blib_override;
            void redo() __blib_override;
        };

        /**
         * ComponentRemoveCommand — удаление компонента со снимком
         * (сериализация). undo: добавить + загрузить снимок;
         * redo: удалить. Несериализуемый компонент — снимка нет,
         * undo добавит пустой (default-ctor).
         */
        class __beng_api ComponentRemoveCommand : public IEditorCommand
        {
        private:
            static constexpr const char* commandName = "Remove Component";

            Scene* scene;
            EntityID entityId;
            ComponentType typeId;
            blib::core::MemoryStream snapshot;
            bool hasSnapshot;

        public:
            // Конструктор снимает компонент и удаляет его сразу
            ComponentRemoveCommand(_In Scene* scene, EntityID entityId, ComponentType typeId);

            const char* getName() const __blib_override { return ComponentRemoveCommand::commandName; }
            void undo() __blib_override;
            void redo() __blib_override;
        };

        /**
         * CommandHistory — история команд эдитора (undo/redo).
         *
         * - Владеет командами: аллокация через GlobalAllocator,
         *   уничтожение — через сохранённый deleter (тип известен
         *   только в момент push);
         * - Лимит истории (maxHistorySize): самые старые команды
         *   уничтожаются при переполнении;
         * - Новая команда очищает redo-стек (ветка повторов обрывается);
         * - Фабрики record* создают команду УЖЕ выполненной и пушат её;
         * - История не владеет сценой: сцена обязана жить дольше
         *   истории (команды резолвят компоненты по ссылке на сцену).
         */
        class __beng_api CommandHistory
        {
        public:
            // Лимит undo-стека (сверх лимита старые команды удаляются)
            static constexpr buint32 maxHistorySize = 128;

            CommandHistory();
            ~CommandHistory();

            CommandHistory(const CommandHistory&) = delete;
            CommandHistory& operator=(const CommandHistory&) = delete;

            /**
             * Очистить историю (undo и redo) — например, при смене
             * сцены (scene_load): команды ссылаются на старые данные.
             */
            void clear();

            /**
             * Отменить последнюю команду.
             * @return false — undo-стек пуст
             */
            bool undo();

            /**
             * Повторить отменённую команду.
             * @return false — redo-стек пуст
             */
            bool redo();

            buint32 getUndoCount() const;
            buint32 getRedoCount() const;

            // ========== Фабрики команд (команда уже выполнена) ==========

            /**
             * Правка поля компонента (Inspector): старое/новое значение.
             */
            void recordFieldChange(
                _In Scene& scene, EntityID entityId, ComponentType typeId,
                buint32 fieldIndex, _In const FieldValue& oldValue, _In const FieldValue& newValue);

            /**
             * Смена TRS (gizmo): старое/новое состояние.
             */
            void recordTransformChange(
                _In Scene& scene, EntityID entityId,
                _In const TransformSnapshot& oldTrs, _In const TransformSnapshot& newTrs);

            /**
             * Создание сущности (команда сама создаёт её).
             * @return ID созданной сущности
             */
            EntityID recordEntityCreate(_In Scene& scene);

            /**
             * Удаление сущности (команда снимает компоненты и удаляет).
             * @return false — сущности нет (команда не создана)
             */
            bool recordEntityDestroy(_In Scene& scene, EntityID entityId);

            /**
             * Добавление компонента (type-erased).
             * @return false — добавить не удалось (команда не создана)
             */
            bool recordComponentAdd(_In Scene& scene, EntityID entityId, ComponentType typeId);

            /**
             * Удаление компонента (со снимком).
             * @return false — компонента нет (команда не создана)
             */
            bool recordComponentRemove(_In Scene& scene, EntityID entityId, ComponentType typeId);

        private:
            // Запись стека: команда + её deleter (тип известен в push)
            struct CommandEntry
            {
                IEditorCommand* command;
                void (*destroy)(_In IEditorCommand*);
            };

            // Пушит команду (template — знает тип для deleter'а) и
            // обрезает историю по лимиту; redo-стек очищается
            template<typename T>
            void pushCommand(_In T* command);

            // Уничтожение одной записи (деструктор + GlobalAllocator)
            static void destroyEntry(_In CommandEntry& entry);

            // Обрезка undo-стека по лимиту
            void trimUndoStack();

            blib::memory::Allocator containerAllocator;
            std::vector<CommandEntry, blib::memory::StdAllocatorAdapter<CommandEntry>> undoStack;
            std::vector<CommandEntry, blib::memory::StdAllocatorAdapter<CommandEntry>> redoStack;
        };

    } // namespace editor
} // namespace beng
