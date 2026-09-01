/**
 * autoDebugExample.cpp - демонстрация автоматического debug wrapping
 * 
 * Этот пример показывает как DefaultAllocator автоматически включает
 * debug проверки в debug сборках без изменения кода.
 * 
 * Компиляция не требуется - это справочный файл с примерами кода.
 */

#include <blib/system/memory/allocator.h>
#include <blib/system/memory/defaultAllocator.h>

#include <iostream>
#include <cstring>

// ============================================================================
// Пример 1: Автоматическое определение режима
// ============================================================================

void example1_automaticMode()
{
    std::cout << "=== Example 1: Automatic Debug Mode Detection ===" << std::endl;

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    std::cout << "Running in DEBUG mode - DebugAllocator is ENABLED" << std::endl;
    std::cout << "  - Guard bytes active" << std::endl;
    std::cout << "  - Poison memory active" << std::endl;
    std::cout << "  - Double-free detection active" << std::endl;
    std::cout << "  - Overhead: +40 bytes per allocation" << std::endl;
#else
    std::cout << "Running in RELEASE mode - DebugAllocator is DISABLED" << std::endl;
    std::cout << "  - Maximum performance" << std::endl;
    std::cout << "  - Minimal overhead" << std::endl;
#endif

    // Проверяем traits
    constexpr bool isStateless = blib::memory::AllocatorTraits<blib::memory::DefaultAllocator>::isStateless;
    std::cout << "DefaultAllocator is " << (isStateless ? "stateless" : "stateful") << std::endl;
}

// ============================================================================
// Пример 2: Обычное использование (прозрачное для режима)
// ============================================================================

void example2_normalUsage()
{
    std::cout << "\n=== Example 2: Normal Usage (Mode Transparent) ===" << std::endl;

    // Пользователь пишет один и тот же код для debug и release
    blib::memory::Allocator alloc;

    std::cout << "Allocating 256 bytes..." << std::endl;
    void* ptr = alloc.allocate(256);

    // Используем память
    std::memset(ptr, 0xAB, 256);
    std::cout << "Memory initialized" << std::endl;

    // Освобождаем
    alloc.deallocate(ptr, 256);
    std::cout << "Deallocated successfully" << std::endl;

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    std::cout << "(Debug checks were performed automatically)" << std::endl;
#else
    std::cout << "(No debug overhead)" << std::endl;
#endif
}

// ============================================================================
// Пример 3: Debug проверки работают автоматически (только в debug!)
// ============================================================================

void example3_automaticChecks()
{
    std::cout << "\n=== Example 3: Automatic Debug Checks ===" << std::endl;

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    std::cout << "WARNING: This example will ABORT in debug mode!" << std::endl;
    std::cout << "Comment out the error line to continue." << std::endl;

    blib::memory::Allocator alloc;
    char* buffer = static_cast<char*>(alloc.allocate(64));

    std::cout << "Allocated 64 bytes" << std::endl;

    // ОШИБКА: Buffer overflow (только детектируется в debug!)
    // std::memset(buffer, 'X', 70); // UNCOMMENT to trigger error in debug

    std::cout << "Normal operations..." << std::endl;
    std::memset(buffer, 'A', 64); // OK

    alloc.deallocate(buffer, 64);
    std::cout << "Deallocated (guards checked automatically)" << std::endl;

#else
    std::cout << "Running in release mode - no debug checks" << std::endl;
    std::cout << "Buffer overflow would NOT be detected!" << std::endl;

    blib::memory::Allocator alloc;
    char* buffer = static_cast<char*>(alloc.allocate(64));
    
    // В release нет детекции - код просто работает быстро
    std::memset(buffer, 'A', 64);
    alloc.deallocate(buffer, 64);
    
    std::cout << "Fast allocation without overhead" << std::endl;
#endif
}

// ============================================================================
// Пример 4: Управление через макросы
// ============================================================================

void example4_macroControl()
{
    std::cout << "\n=== Example 4: Macro Control ===" << std::endl;
    std::cout << "Compile-time control through defines:" << std::endl;
    std::cout << std::endl;
    std::cout << "1. Default behavior:" << std::endl;
    std::cout << "   Debug build   -> BLIB_DEBUG_ALLOCATOR_ENABLED (automatic)" << std::endl;
    std::cout << "   Release build -> disabled (automatic)" << std::endl;
    std::cout << std::endl;
    std::cout << "2. Force disable in debug:" << std::endl;
    std::cout << "   #define BLIB_DEBUG_ALLOCATOR_DISABLED" << std::endl;
    std::cout << "   (before including defaultAllocator.h)" << std::endl;
    std::cout << std::endl;
    std::cout << "3. Force enable in release:" << std::endl;
    std::cout << "   #define BLIB_DEBUG_ALLOCATOR_ENABLED" << std::endl;
    std::cout << "   (before including defaultAllocator.h)" << std::endl;
    std::cout << std::endl;

    std::cout << "Current configuration:" << std::endl;
#ifdef BLIB_DEBUG
    std::cout << "  BLIB_DEBUG: defined" << std::endl;
#else
    std::cout << "  BLIB_DEBUG: not defined" << std::endl;
#endif

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    std::cout << "  BLIB_DEBUG_ALLOCATOR_ENABLED: defined" << std::endl;
#else
    std::cout << "  BLIB_DEBUG_ALLOCATOR_ENABLED: not defined" << std::endl;
#endif

#ifdef BLIB_DEBUG_ALLOCATOR_DISABLED
    std::cout << "  BLIB_DEBUG_ALLOCATOR_DISABLED: defined" << std::endl;
#else
    std::cout << "  BLIB_DEBUG_ALLOCATOR_DISABLED: not defined" << std::endl;
#endif
}

// ============================================================================
// Пример 5: Workflow - один код для обоих режимов
// ============================================================================

class DataProcessor
{
public:
    DataProcessor() {}

    void processData()
    {
        // Один и тот же код работает в debug и release
        blib::memory::Allocator alloc;

        // Выделяем рабочий буфер
        size_t bufferSize = 4096;
        char* workBuffer = static_cast<char*>(alloc.allocate(bufferSize));

        // Обрабатываем данные
        std::memset(workBuffer, 0, bufferSize);
        
        // Делаем что-то полезное...
        for (size_t i = 0; i < bufferSize; ++i)
        {
            workBuffer[i] = static_cast<char>(i % 256);
        }

        // Освобождаем
        alloc.deallocate(workBuffer, bufferSize);

        // В debug: все проверки прошли автоматически
        // В release: максимальная производительность
    }
};

void example5_workflow()
{
    std::cout << "\n=== Example 5: Real-World Workflow ===" << std::endl;
    std::cout << "Processing data with automatic memory safety..." << std::endl;

    DataProcessor processor;
    processor.processData();

    std::cout << "Data processed successfully" << std::endl;

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    std::cout << "(All memory operations were validated automatically)" << std::endl;
#else
    std::cout << "(Maximum performance, no validation overhead)" << std::endl;
#endif
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "blib::memory Automatic Debug Allocator Example" << std::endl;
    std::cout << "===============================================\n" << std::endl;

    example1_automaticMode();
    example2_normalUsage();
    example3_automaticChecks();
    example4_macroControl();
    example5_workflow();

    std::cout << "\n=== All examples completed ===" << std::endl;
    std::cout << "\nKey takeaway: Write code once, get automatic memory safety in debug!" << std::endl;

    return 0;
}
