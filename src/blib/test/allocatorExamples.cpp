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

#include <iostream>
#include <vector>
#include <memory>

// ============================================================================
// Пример 0: Автоматический Debug Mode (NEW!)
// ============================================================================

void example0_autoDebugMode()
{
    std::cout << "=== Example 0: Automatic Debug Mode ===" << std::endl;

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    std::cout << "Running in DEBUG mode - all allocators are wrapped in DebugAllocator!" << std::endl;
    std::cout << "  - DefaultAllocator = DebugAllocator<DefaultAllocatorImpl>" << std::endl;
    std::cout << "  - MallocAllocator = DebugAllocator<MallocAllocatorImpl>" << std::endl;
    std::cout << "  - PoolAllocator = DebugAllocator<PoolAllocatorImpl>" << std::endl;
    std::cout << "  - Guard bytes active" << std::endl;
    std::cout << "  - Poison memory active" << std::endl;
    std::cout << "  - Double-free detection active" << std::endl;
#else
    std::cout << "Running in RELEASE mode - no debug overhead" << std::endl;
    std::cout << "  - DefaultAllocator = DefaultAllocatorImpl" << std::endl;
    std::cout << "  - MallocAllocator = MallocAllocatorImpl" << std::endl;
    std::cout << "  - PoolAllocator = PoolAllocatorImpl" << std::endl;
    std::cout << "  - Maximum performance" << std::endl;
#endif

    std::cout << "\nAll examples below benefit from automatic debug checks!" << std::endl;
}

// ============================================================================
// Пример 1: Базовое использование type-erased Allocator
// ============================================================================

void example1_basicUsage()
{
    std::cout << "=== Example 1: Basic Usage ===" << std::endl;

    // Создаём дефолтный аллокатор
    blib::memory::Allocator alloc;

    // Выделяем память
    void* ptr = alloc.allocate(1024);
    
    if (ptr)
    {
        std::cout << "Allocated 1024 bytes at " << ptr << std::endl;
        
        // Используем память (например, placement new)
        int* intPtr = new (ptr) int(42);
        std::cout << "Value: " << *intPtr << std::endl;
        
        // Явно вызываем деструктор
        intPtr->~int();
        
        // Освобождаем память
        alloc.deallocate(ptr, 1024);
        std::cout << "Deallocated" << std::endl;
    }
}

// ============================================================================
// Пример 2: Использование MallocAllocator
// ============================================================================

void example2_mallocAllocator()
{
    std::cout << "\n=== Example 2: MallocAllocator ===" << std::endl;

    // Прямое использование MallocAllocator (stateless)
    blib::memory::MallocAllocator mallocAlloc;
    void* ptr = mallocAlloc.allocate(512);
    
    std::cout << "MallocAllocator: allocated 512 bytes at " << ptr << std::endl;
    
    mallocAlloc.deallocate(ptr, 512);

    // Или через type-erased Allocator
    blib::memory::Allocator alloc(blib::memory::MallocAllocator{});
    void* ptr2 = alloc.allocate(256);
    std::cout << "Through Allocator: allocated 256 bytes at " << ptr2 << std::endl;
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
    std::cout << "\n=== Example 3: PoolAllocator ===" << std::endl;

    // Создаём пул для объектов Entity
    blib::memory::PoolAllocator pool(sizeof(Entity), 128);
    
    std::cout << "Created pool with block size: " << pool.getBlockSize() << std::endl;
    std::cout << "Total blocks: " << pool.getTotalBlocks() << std::endl;

    // Выделяем объекты
    Entity* entities[10];
    
    for (int i = 0; i < 10; ++i)
    {
        void* memory = pool.allocate(sizeof(Entity));
        entities[i] = new (memory) Entity(i);
        std::cout << "Entity " << i << " created at " << entities[i] << std::endl;
    }

    std::cout << "Free blocks: " << pool.getApproximateFreeBlocks() << std::endl;

    // Освобождаем некоторые объекты
    for (int i = 0; i < 5; ++i)
    {
        entities[i]->~Entity();
        pool.deallocate(entities[i], sizeof(Entity));
    }

    std::cout << "After deallocation, free blocks: " << pool.getApproximateFreeBlocks() << std::endl;

    // Переиспользование памяти
    void* memory = pool.allocate(sizeof(Entity));
    Entity* reusedEntity = new (memory) Entity(100);
    std::cout << "Reused entity at " << reusedEntity << std::endl;

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
    std::cout << "\n=== Example 4: STL Integration ===" << std::endl;

    // Создаём аллокатор
    blib::memory::Allocator alloc;

    // Используем с std::vector
    std::vector<int, blib::memory::StdAllocatorAdapter<int>> vec(&alloc);
    
    for (int i = 0; i < 10; ++i)
    {
        vec.push_back(i * 10);
    }

    std::cout << "Vector contents: ";
    for (int val : vec)
    {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Можно использовать с любым STL контейнером
    // std::list, std::map, std::set и т.д.
}

// ============================================================================
// Пример 5: Статистика через GlobalAllocator
// ============================================================================

void example5_statistics()
{
    std::cout << "\n=== Example 5: Memory Statistics ===" << std::endl;

    auto& global = blib::memory::GlobalAllocator::instance();

    std::cout << "Initial stats:" << std::endl;
    std::cout << "  Current: " << global.getCurrentAllocated() << " bytes" << std::endl;
    std::cout << "  Peak: " << global.getPeakAllocated() << " bytes" << std::endl;
    std::cout << "  Count: " << global.getAllocationCount() << std::endl;

    // Выделяем память через DefaultAllocator (который использует GlobalAllocator)
    blib::memory::Allocator alloc;
    
    void* ptr1 = alloc.allocate(1024);
    void* ptr2 = alloc.allocate(2048);
    void* ptr3 = alloc.allocate(4096);

    std::cout << "\nAfter allocations:" << std::endl;
    std::cout << "  Current: " << global.getCurrentAllocated() << " bytes" << std::endl;
    std::cout << "  Peak: " << global.getPeakAllocated() << " bytes" << std::endl;
    std::cout << "  Count: " << global.getAllocationCount() << std::endl;

    alloc.deallocate(ptr2, 2048);

    std::cout << "\nAfter one deallocation:" << std::endl;
    std::cout << "  Current: " << global.getCurrentAllocated() << " bytes" << std::endl;
    std::cout << "  Peak: " << global.getPeakAllocated() << " bytes" << std::endl;
    std::cout << "  Count: " << global.getAllocationCount() << std::endl;

    alloc.deallocate(ptr1, 1024);
    alloc.deallocate(ptr3, 4096);
}

// ============================================================================
// Пример 6: Копирование и перемещение Allocator
// ============================================================================

void example6_copyAndMove()
{
    std::cout << "\n=== Example 6: Copy and Move ===" << std::endl;

    // Создаём аллокатор с PoolAllocator
    blib::memory::PoolAllocator pool(64, 128);
    blib::memory::Allocator alloc1(std::move(pool));

    void* ptr1 = alloc1.allocate(64);
    std::cout << "Allocated through alloc1: " << ptr1 << std::endl;

    // Копирование - создаёт shared копию (для stateful - делит состояние)
    blib::memory::Allocator alloc2 = alloc1;
    void* ptr2 = alloc2.allocate(64);
    std::cout << "Allocated through alloc2 (shared): " << ptr2 << std::endl;

    // Глубокое копирование - независимая копия
    blib::memory::Allocator alloc3 = alloc1.clone();
    void* ptr3 = alloc3.allocate(64);
    std::cout << "Allocated through alloc3 (cloned): " << ptr3 << std::endl;

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
    std::cout << "\n=== Example 7: Entity Manager ===" << std::endl;

    EntityManager manager(256);

    // Создаём entities
    Entity* entities[20];
    for (int i = 0; i < 20; ++i)
    {
        entities[i] = manager.createEntity(i);
        std::cout << "Created entity " << i << " at " << entities[i] << std::endl;
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
    std::cout << "blib::memory Allocator Examples" << std::endl;
    std::cout << "================================\n" << std::endl;

    example0_autoDebugMode();  // NEW: показываем режим сборки
    example1_basicUsage();
    example2_mallocAllocator();
    example3_poolAllocator();
    example4_stlIntegration();
    example5_statistics();
    example6_copyAndMove();
    example7_entityManager();

    std::cout << "\n=== All examples completed ===" << std::endl;
    
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    std::cout << "\nNote: All allocations were automatically validated in debug mode!" << std::endl;
#else
    std::cout << "\nNote: Running in release mode - maximum performance!" << std::endl;
#endif
    
    return 0;
}
