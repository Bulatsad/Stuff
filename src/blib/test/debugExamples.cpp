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

#include <iostream>
#include <cstring>

// ============================================================================
// Пример 1: Базовое использование DebugAllocator
// ============================================================================

void example1_basicDebugAllocator()
{
    std::cout << "=== Example 1: Basic DebugAllocator ===" << std::endl;

    // Создаём DebugAllocator поверх MallocAllocator
    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );

    // Выделяем память
    void* ptr = debugAlloc.allocate(128);
    std::cout << "Allocated 128 bytes with debug wrapper at " << ptr << std::endl;

    // Используем память нормально
    std::memset(ptr, 0, 128);
    std::cout << "Memory initialized successfully" << std::endl;

    // Освобождаем память - проверит guard bytes
    debugAlloc.deallocate(ptr, 128);
    std::cout << "Deallocated successfully (guard bytes intact)" << std::endl;
}

// ============================================================================
// Пример 2: Детекция buffer overflow
// ============================================================================

void example2_bufferOverflow()
{
    std::cout << "\n=== Example 2: Buffer Overflow Detection ===" << std::endl;
    std::cout << "This example will ABORT due to buffer overflow detection!" << std::endl;

    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );

    // Выделяем 64 байта
    char* buffer = static_cast<char*>(debugAlloc.allocate(64));
    std::cout << "Allocated 64 bytes" << std::endl;

    // ОШИБКА: Пишем за пределы буфера (перезаписываем back guard)
    std::cout << "Writing beyond buffer boundary..." << std::endl;
    for (int i = 0; i < 70; ++i) // Перезапись!
    {
        buffer[i] = 'A';
    }

    // При deallocate обнаружит перезапись guard bytes
    std::cout << "Attempting to deallocate..." << std::endl;
    debugAlloc.deallocate(buffer, 64); // ABORT здесь!
    
    std::cout << "This line will never be reached!" << std::endl;
}

// ============================================================================
// Пример 3: Детекция buffer underflow
// ============================================================================

void example3_bufferUnderflow()
{
    std::cout << "\n=== Example 3: Buffer Underflow Detection ===" << std::endl;
    std::cout << "This example will ABORT due to buffer underflow detection!" << std::endl;

    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );

    char* buffer = static_cast<char*>(debugAlloc.allocate(64));
    std::cout << "Allocated 64 bytes" << std::endl;

    // ОШИБКА: Пишем перед началом буфера (перезаписываем front guard)
    std::cout << "Writing before buffer start..." << std::endl;
    buffer[-1] = 'X'; // Перезапись front guard!

    // При deallocate обнаружит перезапись
    std::cout << "Attempting to deallocate..." << std::endl;
    debugAlloc.deallocate(buffer, 64); // ABORT здесь!
    
    std::cout << "This line will never be reached!" << std::endl;
}

// ============================================================================
// Пример 4: Детекция use-after-free
// ============================================================================

void example4_useAfterFree()
{
    std::cout << "\n=== Example 4: Use-After-Free Detection ===" << std::endl;
    std::cout << "This example will ABORT due to use-after-free detection!" << std::endl;

    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );

    int* data = static_cast<int*>(debugAlloc.allocate(sizeof(int) * 10));
    std::cout << "Allocated array of 10 ints" << std::endl;

    // Нормальное использование
    for (int i = 0; i < 10; ++i)
    {
        data[i] = i * 10;
    }

    // Освобождаем память (DebugAllocator заполнит poison pattern)
    debugAlloc.deallocate(data, sizeof(int) * 10);
    std::cout << "Memory deallocated and poisoned" << std::endl;

    // ОШИБКА: Используем память после free
    std::cout << "Attempting to read freed memory..." << std::endl;
    int value = data[0]; // Чтение poison pattern (0xDEADC0DE)
    std::cout << "Read value: 0x" << std::hex << value << std::dec << std::endl;
    
    // Если попытаться deallocate снова - обнаружит double-free
    std::cout << "Attempting double-free..." << std::endl;
    debugAlloc.deallocate(data, sizeof(int) * 10); // ABORT здесь!
    
    std::cout << "This line will never be reached!" << std::endl;
}

// ============================================================================
// Пример 5: Детекция double-free
// ============================================================================

void example5_doubleFree()
{
    std::cout << "\n=== Example 5: Double-Free Detection ===" << std::endl;
    std::cout << "This example will ABORT due to double-free detection!" << std::endl;

    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );

    void* ptr = debugAlloc.allocate(256);
    std::cout << "Allocated 256 bytes" << std::endl;

    // Освобождаем первый раз
    debugAlloc.deallocate(ptr, 256);
    std::cout << "First deallocation successful" << std::endl;

    // ОШИБКА: Освобождаем второй раз
    std::cout << "Attempting second deallocation..." << std::endl;
    debugAlloc.deallocate(ptr, 256); // ABORT здесь!
    
    std::cout << "This line will never be reached!" << std::endl;
}

// ============================================================================
// Пример 6: Leak tracking с GlobalAllocator
// ============================================================================

void example6_leakTracking()
{
    std::cout << "\n=== Example 6: Leak Tracking ===" << std::endl;

    auto& global = blib::memory::GlobalAllocator::instance();

    // Включаем leak tracking
    std::cout << "Enabling leak tracking..." << std::endl;
    global.setLeakTrackingEnabled(true);

    // Выделяем память через DefaultAllocator (использует GlobalAllocator)
    blib::memory::Allocator alloc;

    void* ptr1 = alloc.allocate(1024);
    void* ptr2 = alloc.allocate(2048);
    void* ptr3 = alloc.allocate(4096);
    
    std::cout << "Allocated 3 blocks: 1024, 2048, 4096 bytes" << std::endl;

    // Освобождаем только один блок
    alloc.deallocate(ptr2, 2048);
    std::cout << "Deallocated middle block (2048 bytes)" << std::endl;

    // Намеренно "забываем" освободить ptr1 и ptr3 для демонстрации утечек

    // Проверяем утечки
    std::cout << "\nChecking for leaks..." << std::endl;
    size_t leakCount = global.dumpLeaks();
    
    if (leakCount > 0)
    {
        std::cout << "Leaks detected! See report above." << std::endl;
    }

    // Cleanup (освобождаем утечки чтобы не мешать другим примерам)
    alloc.deallocate(ptr1, 1024);
    alloc.deallocate(ptr3, 4096);

    // Выключаем leak tracking
    global.setLeakTrackingEnabled(false);
    std::cout << "Leak tracking disabled" << std::endl;
}

// ============================================================================
// Пример 7: Автоматический отчёт об утечках при завершении
// ============================================================================

void example7_automaticLeakReport()
{
    std::cout << "\n=== Example 7: Automatic Leak Report on Exit ===" << std::endl;

    auto& global = blib::memory::GlobalAllocator::instance();

    // Включаем leak tracking
    global.setLeakTrackingEnabled(true);
    std::cout << "Leak tracking enabled" << std::endl;

    // Выделяем память и "забываем" освободить
    blib::memory::Allocator alloc;
    void* leak1 = alloc.allocate(512);
    void* leak2 = alloc.allocate(1024);
    
    std::cout << "Allocated 2 blocks that will leak: 512, 1024 bytes" << std::endl;
    std::cout << "When program exits, GlobalAllocator destructor will report these leaks" << std::endl;
    
    // НЕ освобождаем память намеренно
    // При выходе из программы GlobalAllocator::~GlobalAllocator() выведет отчёт
}

// ============================================================================
// Пример 8: Интеграция DebugAllocator с type-erased Allocator
// ============================================================================

void example8_debugAllocatorWithTypeErasure()
{
    std::cout << "\n=== Example 8: DebugAllocator with Type Erasure ===" << std::endl;

    // Создаём DebugAllocator и оборачиваем в type-erased Allocator
    blib::memory::DebugAllocator<blib::memory::MallocAllocator> debugAlloc(
        blib::memory::MallocAllocator{}
    );
    
    blib::memory::Allocator alloc(std::move(debugAlloc));

    std::cout << "Created type-erased Allocator with DebugAllocator inside" << std::endl;

    // Теперь можем использовать как обычный Allocator
    void* ptr = alloc.allocate(256);
    std::cout << "Allocated 256 bytes" << std::endl;

    // Используем память
    std::memset(ptr, 0xAB, 256);

    // Освобождаем - DebugAllocator внутри проверит guard bytes
    alloc.deallocate(ptr, 256);
    std::cout << "Deallocated successfully (guards checked by DebugAllocator)" << std::endl;
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
    std::cout << "\n=== Example 9: Conditional Debug Allocator ===" << std::endl;

#ifdef BLIB_DEBUG
    std::cout << "Debug build: using DebugAllocator (with overhead)" << std::endl;
    ProductionAllocator alloc(blib::memory::MallocAllocator{});
#else
    std::cout << "Release build: using MallocAllocator (no overhead)" << std::endl;
    ProductionAllocator alloc;
#endif

    void* ptr = alloc.allocate(1024);
    std::cout << "Allocated 1024 bytes" << std::endl;
    
    alloc.deallocate(ptr, 1024);
    std::cout << "Deallocated successfully" << std::endl;
}

// ============================================================================
// Main - запуск примеров
// ============================================================================

int main()
{
    std::cout << "blib::memory Debug Tools Examples" << std::endl;
    std::cout << "==================================\n" << std::endl;

    std::cout << "WARNING: Some examples will intentionally trigger errors and abort!" << std::endl;
    std::cout << "Comment out examples 2-5 to run the rest.\n" << std::endl;

    // Безопасные примеры
    example1_basicDebugAllocator();
    example6_leakTracking();
    example8_debugAllocatorWithTypeErasure();
    example9_conditionalDebug();

    std::cout << "\n=== Safe examples completed ===" << std::endl;
    std::cout << "\nTo test error detection, uncomment and run these (one at a time!):" << std::endl;
    std::cout << "  - example2_bufferOverflow()" << std::endl;
    std::cout << "  - example3_bufferUnderflow()" << std::endl;
    std::cout << "  - example4_useAfterFree()" << std::endl;
    std::cout << "  - example5_doubleFree()" << std::endl;

    // ОПАСНЫЕ ПРИМЕРЫ - раскомментировать по одному для тестирования!
    // example2_bufferOverflow();     // ABORT!
    // example3_bufferUnderflow();    // ABORT!
    // example4_useAfterFree();       // ABORT!
    // example5_doubleFree();         // ABORT!

    // Этот пример безопасен, но оставит утечки для автоматического отчёта
    // example7_automaticLeakReport();

    return 0;
}
