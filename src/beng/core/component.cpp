#include <beng/core/component.h>

namespace beng
{
    // Инициализация статических членов ComponentTypeRegistry
    ComponentType ComponentTypeRegistry::nextTypeId = 0;
    bool ComponentTypeRegistry::typeRegistered[maxComponentTypes] = { false };

    bool ComponentTypeRegistry::isRegistered(ComponentType typeId)
    {
        if (typeId >= maxComponentTypes)
        {
            return false;
        }
        return typeRegistered[typeId];
    }

    buint8 ComponentTypeRegistry::getRegisteredCount()
    {
        return nextTypeId;
    }

} // namespace beng
