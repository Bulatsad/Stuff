#pragma once

#include <blib/blibint.h>
#include <blib/config.h>

// Экспорт символов для shared library.
// По умолчанию макрос пустой (static-сборка). В shared-сборке Windows
// dllimport у потребителей НЕ используем (многие beng-классы имеют
// inline/шаблонные члены — их импортов в DLL нет): экспортирует только
// сам модуль (beng_export → dllexport), потребители ссылаются напрямую,
// линкер резолвит через импорт-таблицу (паттерн blib — см. blib/config.h).
#define __beng_api

#ifdef __blib_compile_platform_windows
#if ____blib_configuration_library_type_value == ____blib_configuration_library_type_shared

#ifdef beng_export
#undef __beng_api
#define __beng_api __declspec(dllexport)
#endif // beng_export

// Static data members: dllimport у потребителей, пусто в сборке модуля
// (паттерн __blib_data_api — см. blib/config.h)
#ifdef beng_export
#define __beng_data_api
#else
#define __beng_data_api __declspec(dllimport)
#endif // beng_export

#endif // shared
#endif // __blib_compile_platform_windows

// Вне shared-сборки Windows данные не декорируются
#ifndef __beng_data_api
#define __beng_data_api
#endif

namespace beng
{
    // ========== ECS Core Types ==========

    // Идентификатор Entity в пределах Scene.
    // ID выдаются последовательно и никогда не переиспользуются,
    // поэтому сохранённый ID безопасно использовать до конца жизни Scene.
    typedef buint64 EntityID;

    // Зарезервированный ID: означает "нет Entity" (нет родителя и т.п.)
    constexpr EntityID invalidEntity = 0;

    // Тип идентификатора компонента (8-битный, локальный индекс типа
    // в Scene: задаёт бит в ComponentMask и слот в таблице пулов сцены)
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
