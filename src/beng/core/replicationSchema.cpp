#include <beng/core/replicationSchema.h>

#include <beng/core/scene.h>

#include <blib/core/console/console.h>

namespace beng
{
    namespace
    {
        // Параметры FNV-1a 32 (хеш схемы полей типа)
        constexpr buint32 fnv1aOffsetBasis = 2166136261u;
        constexpr buint32 fnv1aPrime = 16777619u;

        /**
         * FNV-1a 32: детерминированный хеш потока байтов (не
         * криптография — защита от молчаливого рассинхрона схем).
         */
        buint32 fnv1a(_In const buint8* data, buint32 size)
        {
            buint32 hash = fnv1aOffsetBasis;
            for (buint32 i = 0; i < size; ++i)
            {
                hash ^= static_cast<buint32>(data[i]);
                hash *= fnv1aPrime;
            }
            return hash;
        }

        void hashString(_In_Out buint32& hash, _In const char* text)
        {
            while (*text != '\0')
            {
                hash ^= static_cast<buint32>(static_cast<buint8>(*text));
                hash *= fnv1aPrime;
                ++text;
            }
        }
    }

    ReplicationSchema::ReplicationSchema()
        : wireTypeCount(0)
    {
        for (buint32 t = 0; t < maxComponentTypes; ++t)
        {
            this->replicatedFieldCounts[t] = 0;
            this->descriptors[t] = nullptr;
        }
    }

    void ReplicationSchema::registerType(ComponentType localTypeId,
        _In const ComponentTypeDescriptor& descriptor)
    {
        if (this->wireTypeCount >= maxWireTypes)
        {
            __blib_log_warning("ReplicationSchema: wire type limit reached, type '%s' ignored",
                descriptor.getTypeName());
            return;
        }

        // Собрать реплицируемые поля типа (порядок — как в дескрипторе)
        buint8 fieldCount = 0;
        for (buint32 f = 0; f < descriptor.getFieldCount() && fieldCount < maxReplicatedFields; ++f)
        {
            const IComponentField* field = descriptor.getField(f);
            if (field != nullptr && field->isReplicated())
            {
                this->replicatedFieldIndices[localTypeId][fieldCount] = static_cast<buint8>(f);
                ++fieldCount;
            }
        }

        if (fieldCount == 0)
        {
            // Тип без реплицируемых полей в схеме не участвует:
            // его наличие/данные по сети не возятся
            this->replicatedFieldCounts[localTypeId] = 0;
            return;
        }

        if (descriptor.getFieldCount() > maxReplicatedFields)
        {
            __blib_log_warning("ReplicationSchema: type '%s' has more than %u replicated fields, extra ignored",
                descriptor.getTypeName(), maxReplicatedFields);
        }

        // Дескриптор хранится для резолва индексов полей (статический
        // объект компонента — живёт всё время процесса)
        this->descriptors[localTypeId] = &descriptor;
        this->replicatedFieldCounts[localTypeId] = fieldCount;

        // Хеш схемы: имя типа + имена/kind'ы реплицируемых полей.
        // Kind-байт обязателен: смена типа поля при том же имени
        // обязана менять хеш (иначе клиент прочитал бы мусор)
        buint32 hash = fnv1aOffsetBasis;
        hashString(hash, descriptor.getTypeName());
        for (buint8 i = 0; i < fieldCount; ++i)
        {
            const IComponentField* field = descriptor.getField(this->replicatedFieldIndices[localTypeId][i]);
            hashString(hash, field->getName());
            hash ^= static_cast<buint32>(field->getKind());
            hash *= fnv1aPrime;
        }

        WireType& wire = this->wireTypes[this->wireTypeCount];
        wire.name = descriptor.getTypeName();
        wire.schemaHash = hash;
        wire.localTypeId = localTypeId;
        ++this->wireTypeCount;
    }

    void ReplicationSchema::build(_In const Scene& scene)
    {
        this->wireTypeCount = 0;
        for (buint32 t = 0; t < maxComponentTypes; ++t)
        {
            this->replicatedFieldCounts[t] = 0;
            this->descriptors[t] = nullptr;
        }

        // Типы в порядке регистрации: wire-id = порядковый номер
        // реплицируемого типа (компактные id на проводе)
        const buint32 typeCount = scene.getComponentTypeCount();
        for (buint32 t = 0; t < typeCount && this->wireTypeCount < maxWireTypes; ++t)
        {
            const ComponentType localTypeId = static_cast<ComponentType>(t);
            const ComponentTypeDescriptor* descriptor = scene.tryGetComponentReflection(localTypeId);
            if (descriptor != nullptr)
            {
                this->registerType(localTypeId, *descriptor);
            }
        }
    }

    const ReplicationSchema::WireType* ReplicationSchema::getWireType(buint32 wireId) const
    {
        return (wireId < this->wireTypeCount) ? &this->wireTypes[wireId] : nullptr;
    }

    ComponentType ReplicationSchema::getWireIdForLocalType(ComponentType localTypeId) const
    {
        for (buint32 w = 0; w < this->wireTypeCount; ++w)
        {
            if (this->wireTypes[w].localTypeId == localTypeId)
            {
                return static_cast<ComponentType>(w);
            }
        }
        return invalidComponentType;
    }

    ComponentType ReplicationSchema::getLocalTypeForWireId(buint32 wireId) const
    {
        return (wireId < this->wireTypeCount)
            ? this->wireTypes[wireId].localTypeId
            : invalidComponentType;
    }

    buint32 ReplicationSchema::getReplicatedFieldCount(ComponentType localTypeId) const
    {
        return (localTypeId < maxComponentTypes)
            ? static_cast<buint32>(this->replicatedFieldCounts[localTypeId])
            : 0;
    }

    const IComponentField* ReplicationSchema::getReplicatedField(ComponentType localTypeId, buint32 index) const
    {
        // Индексы реплицируемых полей резолвятся через дескриптор типа
        if (localTypeId >= maxComponentTypes ||
            index >= static_cast<buint32>(this->replicatedFieldCounts[localTypeId]))
        {
            return nullptr;
        }

        const ComponentTypeDescriptor* descriptor = this->descriptors[localTypeId];
        if (descriptor == nullptr)
        {
            return nullptr;
        }

        const buint8 descriptorIndex = this->replicatedFieldIndices[localTypeId][index];
        return descriptor->getField(static_cast<buint32>(descriptorIndex));
    }

} // namespace beng
