#pragma once

#include <beng/config.h>
#include <beng/core/componentReflection.h>

#include <blib/blibint.h>

namespace beng
{
    class Scene;

    // ========== Лимиты репликации (правило проекта: именованные константы) ==========

    // Максимум wire-типов на мир (= maxComponentTypes: тип не может
    // иметь wire-id больше, чем локальных типов в сцене)
    constexpr buint32 maxWireTypes = maxComponentTypes;

    // Максимум реплицируемых полей на один тип компонента (MVP-лимит
    // снапшот-структур без аллокаций; превышение — warning + обрезка)
    constexpr buint32 maxReplicatedFields = 6;

    /**
     * ReplicationSchema — схема сетевой репликации мира (beng-core):
     * какие типы компонентов и какие их поля возятся по сети.
     *
     * Строится из реестра сцены: тип участвует в репликации, если у
     * его дескриптора рефлексии есть хотя бы одно поле с флагом
     * `FunctionField::replicated` (см. componentReflection.h). Такие
     * типы получают wire-id (порядковый номер в списке схемы) —
     * компактный идентификатор на проводе; соответствие wire-id ↔
     * стабильное имя типа передаётся в Welcome при подключении
     * (см. replicationCodec.h) и сверяется хешем схемы.
     *
     * Схема — значение локального мира: у сервера и клиента свои
     * инстансы (локальные ComponentType могут не совпадать — порядок
     * регистрации разный); совместимость гарантирует СТАБИЛЬНОЕ ИМЯ
     * типа + хеш набора реплицируемых полей.
     *
     * Хеш схемы типа — FNV-1a 32: имя типа + имена/kind'ы
     * реплицируемых полей. Любое изменение набора полей (добавление/
     * удаление/переименование/смена типа) меняет хеш — клиент с
     * другой версией откажется от сессии (протокол не эволюционирует
     * молча).
     */
    class __beng_api ReplicationSchema
    {
    public:
        /**
         * Один wire-тип: стабильное имя, хеш схемы полей и локальный
         * ComponentType ЭТОЙ сцены (у пира может быть другой).
         */
        struct WireType
        {
            const char* name;      // стабильное имя (T::componentTypeName)
            buint32 schemaHash;    // FNV-1a над набором реплицируемых полей
            ComponentType localTypeId; // ComponentType в этой сцене
        };

        /**
         * Пустая схема (ничего не реплицируется). Заполняется build().
         */
        ReplicationSchema();

        /**
         * Построить схему из реестра типов сцены: сканирует
         * зарегистрированные типы, собирает их реплицируемые поля
         * и вычисляет хеши. Вызывать после регистрации всех типов
         * (до первого Welcome).
         */
        void build(_In const Scene& scene);

        /**
         * Количество wire-типов (0 — репликация пуста).
         */
        buint32 getWireTypeCount() const { return this->wireTypeCount; }

        /**
         * Wire-тип по wire-id (nullptr вне границ).
         */
        const WireType* getWireType(buint32 wireId) const;

        /**
         * Wire-id локального типа; 0xFF (invalidComponentType) —
         * тип не реплицируется (нет реплицируемых полей).
         */
        ComponentType getWireIdForLocalType(ComponentType localTypeId) const;

        /**
         * Локальный тип по wire-id; invalidComponentType вне границ.
         */
        ComponentType getLocalTypeForWireId(buint32 wireId) const;

        /**
         * Число реплицируемых полей локального типа (0 — тип не
         * реплицируется).
         */
        buint32 getReplicatedFieldCount(ComponentType localTypeId) const;

        /**
         * Поле репликации локального типа по индексу (индексы — в
         * порядке следования полей дескриптора; nullptr вне границ).
         */
        const IComponentField* getReplicatedField(ComponentType localTypeId, buint32 index) const;

    private:
        WireType wireTypes[maxWireTypes];
        buint32 wireTypeCount;

        // per локальный ComponentType: индексы реплицируемых полей
        // в списке полей дескриптора (replicatedFieldIndices[T][i] =
        // позиция i-го реплицируемого поля в ComponentTypeDescriptor)
        // и сам дескриптор (резолв индекса → IComponentField)
        buint8 replicatedFieldIndices[maxComponentTypes][maxReplicatedFields];
        buint8 replicatedFieldCounts[maxComponentTypes];
        const ComponentTypeDescriptor* descriptors[maxComponentTypes];

        // Регистрация типа в схеме (вызывается build'ом)
        void registerType(ComponentType localTypeId, _In const ComponentTypeDescriptor& descriptor);
    };

} // namespace beng
