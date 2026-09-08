#pragma once

#include <blib/blibint.h>
#include <blib/config.h>

// Экспорт символов для shared library
// Пока beng собирается как static library — макрос пустой
#define __beng_api

namespace beng
{
    // ========== ECS Core Types ==========

    // Идентификатор Entity в пределах Scene.
    // ID выдаются последовательно и никогда не переиспользуются,
    // поэтому сохранённый ID безопасно использовать до конца жизни Scene.
    typedef buint64 EntityID;

    // Зарезервированный ID: означает "нет Entity" (нет родителя и т.п.)
    constexpr EntityID invalidEntity = 0;

    // Тип идентификатора компонента (8-битный, индекс в реестре)
    typedef buint8 ComponentType;

    // Зарезервированный ID типа: "невалидный тип"
    constexpr ComponentType invalidComponentType = 0xFF;

    // Битовая маска компонентов Entity.
    // Разрядность маски = componentMaskBits — бит typeId означает
    // наличие компонента типа typeId у Entity.
    typedef buint64 ComponentMask;

    // ========== ECS Limits ==========

    // Максимальное количество типов компонентов.
    // Жёстко привязано к разрядности ComponentMask (componentMaskBits):
    // менять только вместе. Превышение — fatal error при регистрации.
    constexpr buint8 maxComponentTypes = 64;

    // Разрядность маски компонентов (бит на тип компонента)
    constexpr buint8 componentMaskBits = 64;

    // ========== Component Pool Settings ==========

    // Размер chunk по умолчанию (количество компонентов в одном блоке памяти)
    constexpr buint32 defaultComponentPoolChunkSize = 128;

    // ========== Spatial Grid Settings (для будущего) ==========

    // Размер ячейки пространственной сетки по умолчанию (в единицах мира)
    constexpr float defaultSpatialGridCellSize = 10.0f;

    // ========== Entity Settings ==========

    // Начальная резервация Entity в Scene
    constexpr buint32 defaultEntityReserveSize = 256;

} // namespace beng
