/**
 * debugExamples.cpp - примеры использования debug инструментов blib::memory
 * 
 * Этот файл демонстрирует:
 * 1. DebugAllocator для детекции buffer overflow/underflow
 * 2. Детекция use-after-free через poison memory
 * 3. Детекция double-free
 * 4. Leak tracking через GlobalAllocator
 * 
 * ВАЖНО: Эти примеры намеренно содержат ошибки для демонстрации детекции!
 * Некоторые примеры вызовут abort() при обнаружении ошибки.
 * 
 * Компиляция не требуется - это справочный файл с примерами кода.
 */

#include <blib/system/memory/allocator.h>
#include <blib/system/memory/globalAllocator.h>
#include <blib/system/memory/defaultAllocator.h>
#include <blib/system/memory/allocators/debugAllocator.h>
#include <blib/system/memory/allocators/mallocAllocator.h>

#include <blib/core/console/console.h>
#include <cstring>

// ============================================================================
// Пример 1: Базовое использование DebugAllocator
// ============================================================================

void example1_basicDebugAllocator()
{
    __blib_log_info("=== Example 1: Basic DebugAllocator ===");

    // Создаём DebugAllocator поверх MallocAllocator
    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );

    // Выделяем память
    void* ptr = debugAlloc.allocate(128);
    __blib_log_info("Allocated 128 bytes with debug wrapper at %p", ptr);

    // Используем память нормально
    std::memset(ptr, 0, 128);
    __blib_log_info("Memory initialized successfully");

    // Освобождаем память - проверит guard bytes
    debugAlloc.deallocate(ptr, 128);
    __blib_log_info("Deallocated successfully (guard bytes intact)");
}

// ============================================================================
// Пример 2: Детекция buffer overflow
// ============================================================================

void example2_bufferOverflow()
{
    __blib_log_info("\n=== Example 2: Buffer Overflow Detection ===");
    __blib_log_info("This example will ABORT due to buffer overflow detection!");

    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );

    // Выделяем 64 байта
    char* buffer = static_cast<char*>(debugAlloc.allocate(64));
    __blib_log_info("Allocated 64 bytes");

    // ОШИБКА: Пишем за пределы буфера (перезаписываем back guard)
    __blib_log_info("Writing beyond buffer boundary...");
    for (int i = 0; i < 70; ++i) // Перезапись!
    {
        buffer[i] = 'A';
    }

    // При deallocate обнаружит перезапись guard bytes
    __blib_log_info("Attempting to deallocate...");
    debugAlloc.deallocate(buffer, 64); // ABORT здесь!
    
    __blib_log_info("This line will never be reached!");
}

// ============================================================================
// Пример 3: Детекция buffer underflow
// ============================================================================

void example3_bufferUnderflow()
{
    __blib_log_info("\n=== Example 3: Buffer Underflow Detection ===");
    __blib_log_info("This example will ABORT due to buffer underflow detection!");

    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );

    char* buffer = static_cast<char*>(debugAlloc.allocate(64));
    __blib_log_info("Allocated 64 bytes");

    // ОШИБКА: Пишем перед началом буфера (перезаписываем front guard)
    __blib_log_info("Writing before buffer start...");
    buffer[-1] = 'X'; // Перезапись front guard!

    // При deallocate обнаружит перезапись
    __blib_log_info("Attempting to deallocate...");
    debugAlloc.deallocate(buffer, 64); // ABORT здесь!
    
    __blib_log_info("This line will never be reached!");
}

// ============================================================================
// Пример 4: Детекция use-after-free
// ============================================================================

void example4_useAfterFree()
{
    __blib_log_info("\n=== Example 4: Use-After-Free Detection ===");
    __blib_log_info("This example will ABORT due to use-after-free detection!");

    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );

    int* data = static_cast<int*>(debugAlloc.allocate(sizeof(int) * 10));
    __blib_log_info("Allocated array of 10 ints");

    // Нормальное использование
    for (int i = 0; i < 10; ++i)
    {
        data[i] = i * 10;
    }

    // Освобождаем память (DebugAllocator заполнит poison pattern)
    debugAlloc.deallocate(data, sizeof(int) * 10);
    __blib_log_info("Memory deallocated and poisoned");

    // ОШИБКА: Используем память после free
    __blib_log_info("Attempting to read freed memory...");
    int value = data[0]; // Чтение poison pattern (0xDEADC0DE)
    __blib_log_info("Read value: 0x%x", value);
    
    // Если попытаться deallocate снова - обнаружит double-free
    __blib_log_info("Attempting double-free...");
    debugAlloc.deallocate(data, sizeof(int) * 10); // ABORT здесь!
    
    __blib_log_info("This line will never be reached!");
}

// ============================================================================
// Пример 5: Детекция double-free
// ============================================================================

void example5_doubleFree()
{
    __blib_log_info("\n=== Example 5: Double-Free Detection ===");
    __blib_log_info("This example will ABORT due to double-free detection!");

    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );

    void* ptr = debugAlloc.allocate(256);
    __blib_log_info("Allocated 256 bytes");

    // Освобождаем первый раз
    debugAlloc.deallocate(ptr, 256);
    __blib_log_info("First deallocation successful");

    // ОШИБКА: Освобождаем второй раз
    __blib_log_info("Attempting second deallocation...");
    debugAlloc.deallocate(ptr, 256); // ABORT здесь!
    
    __blib_log_info("This line will never be reached!");
}

// ============================================================================
// Пример 6: Leak tracking с GlobalAllocator
// ============================================================================

void example6_leakTracking()
{
    __blib_log_info("\n=== Example 6: Leak Tracking ===");

    auto& global = blib::memory::GlobalAllocator::instance();

    // Включаем leak tracking
    __blib_log_info("Enabling leak tracking...");
    global.setLeakTrackingEnabled(true);

    // Выделяем память через DefaultAllocator (использует GlobalAllocator)
    blib::memory::Allocator alloc;

    void* ptr1 = alloc.allocate(1024);
    void* ptr2 = alloc.allocate(2048);
    void* ptr3 = alloc.allocate(4096);
    
    __blib_log_info("Allocated 3 blocks: 1024, 2048, 4096 bytes");

    // Освобождаем только один блок
    alloc.deallocate(ptr2, 2048);
    __blib_log_info("Deallocated middle block (2048 bytes)");

    // Намеренно "забываем" освободить ptr1 и ptr3 для демонстрации утечек

    // Проверяем утечки
    __blib_log_info("\nChecking for leaks...");
    size_t leakCount = global.dumpLeaks();
    
    if (leakCount > 0)
    {
        __blib_log_info("Leaks detected! See report above.");
    }

    // Cleanup (освобождаем утечки чтобы не мешать другим примерам)
    alloc.deallocate(ptr1, 1024);
    alloc.deallocate(ptr3, 4096);

    // Выключаем leak tracking
    global.setLeakTrackingEnabled(false);
    __blib_log_info("Leak tracking disabled");
}

// ============================================================================
// Пример 7: Автоматический отчёт об утечках при завершении
// ============================================================================

void example7_automaticLeakReport()
{
    __blib_log_info("\n=== Example 7: Automatic Leak Report on Exit ===");

    auto& global = blib::memory::GlobalAllocator::instance();

    // Включаем leak tracking
    global.setLeakTrackingEnabled(true);
    __blib_log_info("Leak tracking enabled");

    // Выделяем память и "забываем" освободить
    blib::memory::Allocator alloc;
    void* leak1 = alloc.allocate(512);
    void* leak2 = alloc.allocate(1024);
    
    __blib_log_info("Allocated 2 blocks that will leak: 512, 1024 bytes");
    __blib_log_info("When program exits, GlobalAllocator destructor will report these leaks");
    
    // НЕ освобождаем память намеренно
    // При выходе из программы GlobalAllocator::~GlobalAllocator() выведет отчёт
}

// ============================================================================
// Пример 8: Интеграция DebugAllocator с type-erased Allocator
// ============================================================================

void example8_debugAllocatorWithTypeErasure()
{
    __blib_log_info("\n=== Example 8: DebugAllocator with Type Erasure ===");

    // Создаём DebugAllocator и оборачиваем в type-erased Allocator
    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );
    
    blib::memory::Allocator alloc(std::move(debugAlloc));

    __blib_log_info("Created type-erased Allocator with DebugAllocator inside");

    // Теперь можем использовать как обычный Allocator
    void* ptr = alloc.allocate(256);
    __blib_log_info("Allocated 256 bytes");

    // Используем память
    std::memset(ptr, 0xAB, 256);

    // Освобождаем - DebugAllocator внутри проверит guard bytes
    alloc.deallocate(ptr, 256);
    __blib_log_info("Deallocated successfully (guards checked by DebugAllocator)");
}

// ============================================================================
// Пример 9: Conditional debug allocator (debug vs release)
// ============================================================================

#ifdef BLIB_DEBUG
    using ProductionAllocator = blib::memory::DebugAllocator<blib::memory::MallocAllocator>;
#else
    using ProductionAllocator = blib::memory::MallocAllocator;
#endif

void example9_conditionalDebug()
{
    __blib_log_info("\n=== Example 9: Conditional Debug Allocator ===");

#ifdef BLIB_DEBUG
    __blib_log_info("Debug build: using DebugAllocator (with overhead)");
    ProductionAllocator alloc(blib::memory::MallocAllocator{});
#else
    __blib_log_info("Release build: using MallocAllocator (no overhead)");
    ProductionAllocator alloc;
#endif

    void* ptr = alloc.allocate(1024);
    __blib_log_info("Allocated 1024 bytes");
    
    alloc.deallocate(ptr, 1024);
    __blib_log_info("Deallocated successfully");
}

// ============================================================================
// Main - запуск примеров
// ============================================================================

int main()
{
    // CLI-пример: включаем stdout-эхо консоли, чтобы вывод был виден в терминале
    blib::console::Console::instance().getOutput().setStdoutEcho(true);

    __blib_log_info("blib::memory Debug Tools Examples");
    __blib_log_info("==================================\n");

    __blib_log_info("WARNING: Some examples will intentionally trigger errors and abort!");
    __blib_log_info("Comment out examples 2-5 to run the rest.\n");

    // Безопасные примеры
    example1_basicDebugAllocator();
    example6_leakTracking();
    example8_debugAllocatorWithTypeErasure();
    example9_conditionalDebug();

    __blib_log_info("\n=== Safe examples completed ===");
    __blib_log_info("\nTo test error detection, uncomment and run these (one at a time!):");
    __blib_log_info("  - example2_bufferOverflow()");
    __blib_log_info("  - example3_bufferUnderflow()");
    __blib_log_info("  - example4_useAfterFree()");
    __blib_log_info("  - example5_doubleFree()");

    // ОПАСНЫЕ ПРИМЕРЫ - раскомментировать по одному для тестирования!
    // example2_bufferOverflow();     // ABORT!
    // example3_bufferUnderflow();    // ABORT!
    // example4_useAfterFree();       // ABORT!
    // example5_doubleFree();         // ABORT!

    // Этот пример безопасен, но оставит утечки для автоматического отчёта
    // example7_automaticLeakReport();

    return 0;
}
