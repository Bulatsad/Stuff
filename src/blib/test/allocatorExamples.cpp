/**
 * allocatorExamples.cpp - примеры использования системы аллокаторов blib::memory
 * 
 * Этот файл демонстрирует различные способы использования аллокаторов:
 * 0. Автоматический debug mode для всех аллокаторов (NEW!)
 * 1. Базовое использование через type-erased Allocator
 * 2. Использование конкретных аллокаторов
 * 3. Интеграция с STL контейнерами
 * 4. Статистика памяти через GlobalAllocator
 * 
 * ВАЖНО: В debug сборках все аллокаторы автоматически оборачиваются в DebugAllocator!
 * 
 * Компиляция не требуется - это справочный файл с примерами кода.
 */

#include <blib/system/memory/allocator.h>
#include <blib/system/memory/globalAllocator.h>
#include <blib/system/memory/defaultAllocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>
#include <blib/system/memory/allocators/mallocAllocator.h>
#include <blib/system/memory/allocators/poolAllocator.h>

#include <blib/core/console/console.h>
#include <vector>
#include <memory>
#include <string>

// ============================================================================
// Пример 0: Автоматический Debug Mode (NEW!)
// ============================================================================

void example0_autoDebugMode()
{
    __blib_log_info("=== Example 0: Automatic Debug Mode ===");

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    __blib_log_info("Running in DEBUG mode - all allocators are wrapped in DebugAllocator!");
    __blib_log_info("  - DefaultAllocator = DebugAllocator<DefaultAllocatorImpl>");
    __blib_log_info("  - MallocAllocator = DebugAllocator<MallocAllocatorImpl>");
    __blib_log_info("  - PoolAllocator = DebugAllocator<PoolAllocatorImpl>");
    __blib_log_info("  - Guard bytes active");
    __blib_log_info("  - Poison memory active");
    __blib_log_info("  - Double-free detection active");
#else
    __blib_log_info("Running in RELEASE mode - no debug overhead");
    __blib_log_info("  - DefaultAllocator = DefaultAllocatorImpl");
    __blib_log_info("  - MallocAllocator = MallocAllocatorImpl");
    __blib_log_info("  - PoolAllocator = PoolAllocatorImpl");
    __blib_log_info("  - Maximum performance");
#endif

    __blib_log_info("\nAll examples below benefit from automatic debug checks!");
}

// ============================================================================
// Пример 1: Базовое использование type-erased Allocator
// ============================================================================

void example1_basicUsage()
{
    __blib_log_info("=== Example 1: Basic Usage ===");

    // Создаём дефолтный аллокатор
    blib::memory::Allocator alloc;

    // Выделяем память
    void* ptr = alloc.allocate(1024);
    
    if (ptr)
    {
        __blib_log_info("Allocated 1024 bytes at %p", ptr);
        
        // Используем память (например, placement new)
        int* intPtr = new (ptr) int(42);
        __blib_log_info("Value: %d", *intPtr);
        
        // Явно вызываем деструктор
        intPtr->~int();
        
        // Освобождаем память
        alloc.deallocate(ptr, 1024);
        __blib_log_info("Deallocated");
    }
}

// ============================================================================
// Пример 2: Использование MallocAllocator
// ============================================================================

void example2_mallocAllocator()
{
    __blib_log_info("\n=== Example 2: MallocAllocator ===");

    // Прямое использование MallocAllocator (stateless)
    blib::memory::MallocAllocator mallocAlloc;
    void* ptr = mallocAlloc.allocate(512);
    
    __blib_log_info("MallocAllocator: allocated 512 bytes at %p", ptr);
    
    mallocAlloc.deallocate(ptr, 512);

    // Или через type-erased Allocator
    blib::memory::Allocator alloc(blib::memory::MallocAllocator{});
    void* ptr2 = alloc.allocate(256);
    __blib_log_info("Through Allocator: allocated 256 bytes at %p", ptr2);
    alloc.deallocate(ptr2, 256);
}

// ============================================================================
// Пример 3: Использование PoolAllocator для частых аллокаций
// ============================================================================

struct Entity
{
    float x, y, z;
    int id;
    bool active;
    
    Entity(int id) : x(0), y(0), z(0), id(id), active(true) {}
};

void example3_poolAllocator()
{
    __blib_log_info("\n=== Example 3: PoolAllocator ===");

    // Создаём пул для объектов Entity
    blib::memory::PoolAllocator pool(sizeof(Entity), 128);
    
    __blib_log_info("Created pool with block size: %zu", pool.getBlockSize());
    __blib_log_info("Total blocks: %zu", pool.getTotalBlocks());

    // Выделяем объекты
    Entity* entities[10];
    
    for (int i = 0; i < 10; ++i)
    {
        void* memory = pool.allocate(sizeof(Entity));
        entities[i] = new (memory) Entity(i);
        __blib_log_info("Entity %d created at %p", i, entities[i]);
    }

    __blib_log_info("Free blocks: %zu", pool.getApproximateFreeBlocks());

    // Освобождаем некоторые объекты
    for (int i = 0; i < 5; ++i)
    {
        entities[i]->~Entity();
        pool.deallocate(entities[i], sizeof(Entity));
    }

    __blib_log_info("After deallocation, free blocks: %zu", pool.getApproximateFreeBlocks());

    // Переиспользование памяти
    void* memory = pool.allocate(sizeof(Entity));
    Entity* reusedEntity = new (memory) Entity(100);
    __blib_log_info("Reused entity at %p", reusedEntity);

    // Cleanup остальных объектов
    reusedEntity->~Entity();
    pool.deallocate(reusedEntity, sizeof(Entity));
    
    for (int i = 5; i < 10; ++i)
    {
        entities[i]->~Entity();
        pool.deallocate(entities[i], sizeof(Entity));
    }
}

// ============================================================================
// Пример 4: Интеграция с STL через StdAllocatorAdapter
// ============================================================================

void example4_stlIntegration()
{
    __blib_log_info("\n=== Example 4: STL Integration ===");

    // Создаём аллокатор
    blib::memory::Allocator alloc;

    // Используем с std::vector
    std::vector<int, blib::memory::StdAllocatorAdapter<int>> vec(&alloc);
    
    for (int i = 0; i < 10; ++i)
    {
        vec.push_back(i * 10);
    }

    // Собираем содержимое вектора в одну строку (консоль печатает построчно)
    std::string contents = "Vector contents:";
    for (int val : vec)
    {
        contents += " " + std::to_string(val);
    }
    __blib_log_info("%s", contents.c_str());

    // Можно использовать с любым STL контейнером
    // std::list, std::map, std::set и т.д.
}

// ============================================================================
// Пример 5: Статистика через GlobalAllocator
// ============================================================================

void example5_statistics()
{
    __blib_log_info("\n=== Example 5: Memory Statistics ===");

    auto& global = blib::memory::GlobalAllocator::instance();

    __blib_log_info("Initial stats:");
    __blib_log_info("  Current: %zu bytes", global.getCurrentAllocated());
    __blib_log_info("  Peak: %zu bytes", global.getPeakAllocated());
    __blib_log_info("  Count: %zu", global.getAllocationCount());

    // Выделяем память через DefaultAllocator (который использует GlobalAllocator)
    blib::memory::Allocator alloc;
    
    void* ptr1 = alloc.allocate(1024);
    void* ptr2 = alloc.allocate(2048);
    void* ptr3 = alloc.allocate(4096);

    __blib_log_info("\nAfter allocations:");
    __blib_log_info("  Current: %zu bytes", global.getCurrentAllocated());
    __blib_log_info("  Peak: %zu bytes", global.getPeakAllocated());
    __blib_log_info("  Count: %zu", global.getAllocationCount());

    alloc.deallocate(ptr2, 2048);

    __blib_log_info("\nAfter one deallocation:");
    __blib_log_info("  Current: %zu bytes", global.getCurrentAllocated());
    __blib_log_info("  Peak: %zu bytes", global.getPeakAllocated());
    __blib_log_info("  Count: %zu", global.getAllocationCount());

    alloc.deallocate(ptr1, 1024);
    alloc.deallocate(ptr3, 4096);
}

// ============================================================================
// Пример 6: Копирование и перемещение Allocator
// ============================================================================

void example6_copyAndMove()
{
    __blib_log_info("\n=== Example 6: Copy and Move ===");

    // Создаём аллокатор с PoolAllocator
    blib::memory::PoolAllocator pool(64, 128);
    blib::memory::Allocator alloc1(std::move(pool));

    void* ptr1 = alloc1.allocate(64);
    __blib_log_info("Allocated through alloc1: %p", ptr1);

    // Копирование - создаёт shared копию (для stateful - делит состояние)
    blib::memory::Allocator alloc2 = alloc1;
    void* ptr2 = alloc2.allocate(64);
    __blib_log_info("Allocated through alloc2 (shared): %p", ptr2);

    // Глубокое копирование - независимая копия
    blib::memory::Allocator alloc3 = alloc1.clone();
    void* ptr3 = alloc3.allocate(64);
    __blib_log_info("Allocated through alloc3 (cloned): %p", ptr3);

    // Cleanup
    alloc1.deallocate(ptr1, 64);
    alloc2.deallocate(ptr2, 64);
    alloc3.deallocate(ptr3, 64);
}

// ============================================================================
// Пример 7: Практичный use-case - Entity Pool
// ============================================================================

class EntityManager
{
public:
    EntityManager(size_t maxEntities)
        : entityPool(sizeof(Entity), maxEntities)
        , allocator(std::move(entityPool))
    {
    }

    Entity* createEntity(int id)
    {
        void* memory = allocator.allocate(sizeof(Entity));
        if (!memory)
        {
            return nullptr;
        }
        return new (memory) Entity(id);
    }

    void destroyEntity(Entity* entity)
    {
        if (entity)
        {
            entity->~Entity();
            allocator.deallocate(entity, sizeof(Entity));
        }
    }

private:
    blib::memory::PoolAllocator entityPool;
    blib::memory::Allocator allocator;
};

void example7_entityManager()
{
    __blib_log_info("\n=== Example 7: Entity Manager ===");

    EntityManager manager(256);

    // Создаём entities
    Entity* entities[20];
    for (int i = 0; i < 20; ++i)
    {
        entities[i] = manager.createEntity(i);
        __blib_log_info("Created entity %d at %p", i, entities[i]);
    }

    // Уничтожаем entities
    for (int i = 0; i < 20; ++i)
    {
        manager.destroyEntity(entities[i]);
    }
}

// ============================================================================
// Main - запуск всех примеров
// ============================================================================

int main()
{
    // CLI-пример: включаем stdout-эхо консоли, чтобы вывод был виден в терминале
    blib::console::Console::instance().getOutput().setStdoutEcho(true);

    __blib_log_info("blib::memory Allocator Examples");
    __blib_log_info("================================\n");

    example0_autoDebugMode();  // NEW: показываем режим сборки
    example1_basicUsage();
    example2_mallocAllocator();
    example3_poolAllocator();
    example4_stlIntegration();
    example5_statistics();
    example6_copyAndMove();
    example7_entityManager();

    __blib_log_info("\n=== All examples completed ===");
    
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    __blib_log_info("\nNote: All allocations were automatically validated in debug mode!");
#else
    __blib_log_info("\nNote: Running in release mode - maximum performance!");
#endif
    
    return 0;
}
