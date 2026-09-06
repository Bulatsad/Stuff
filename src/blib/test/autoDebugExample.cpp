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

#include <blib/core/console/console.h>
#include <cstring>

// ============================================================================
// Пример 1: Автоматическое определение режима
// ============================================================================

void example1_automaticMode()
{
    __blib_log_info("=== Example 1: Automatic Debug Mode Detection ===");

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    __blib_log_info("Running in DEBUG mode - DebugAllocator is ENABLED");
    __blib_log_info("  - Guard bytes active");
    __blib_log_info("  - Poison memory active");
    __blib_log_info("  - Double-free detection active");
    __blib_log_info("  - Overhead: +40 bytes per allocation");
#else
    __blib_log_info("Running in RELEASE mode - DebugAllocator is DISABLED");
    __blib_log_info("  - Maximum performance");
    __blib_log_info("  - Minimal overhead");
#endif

    // Проверяем traits
    constexpr bool isStateless = blib::memory::AllocatorTraits<blib::memory::DefaultAllocator>::isStateless;
    __blib_log_info("DefaultAllocator is %s", isStateless ? "stateless" : "stateful");
}

// ============================================================================
// Пример 2: Обычное использование (прозрачное для режима)
// ============================================================================

void example2_normalUsage()
{
    __blib_log_info("\n=== Example 2: Normal Usage (Mode Transparent) ===");

    // Пользователь пишет один и тот же код для debug и release
    blib::memory::Allocator alloc;

    __blib_log_info("Allocating 256 bytes...");
    void* ptr = alloc.allocate(256);

    // Используем память
    std::memset(ptr, 0xAB, 256);
    __blib_log_info("Memory initialized");

    // Освобождаем
    alloc.deallocate(ptr, 256);
    __blib_log_info("Deallocated successfully");

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    __blib_log_info("(Debug checks were performed automatically)");
#else
    __blib_log_info("(No debug overhead)");
#endif
}

// ============================================================================
// Пример 3: Debug проверки работают автоматически (только в debug!)
// ============================================================================

void example3_automaticChecks()
{
    __blib_log_info("\n=== Example 3: Automatic Debug Checks ===");

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    __blib_log_info("WARNING: This example will ABORT in debug mode!");
    __blib_log_info("Comment out the error line to continue.");

    blib::memory::Allocator alloc;
    char* buffer = static_cast<char*>(alloc.allocate(64));

    __blib_log_info("Allocated 64 bytes");

    // ОШИБКА: Buffer overflow (только детектируется в debug!)
    // std::memset(buffer, 'X', 70); // UNCOMMENT to trigger error in debug

    __blib_log_info("Normal operations...");
    std::memset(buffer, 'A', 64); // OK

    alloc.deallocate(buffer, 64);
    __blib_log_info("Deallocated (guards checked automatically)");

#else
    __blib_log_info("Running in release mode - no debug checks");
    __blib_log_info("Buffer overflow would NOT be detected!");

    blib::memory::Allocator alloc;
    char* buffer = static_cast<char*>(alloc.allocate(64));
    
    // В release нет детекции - код просто работает быстро
    std::memset(buffer, 'A', 64);
    alloc.deallocate(buffer, 64);
    
    __blib_log_info("Fast allocation without overhead");
#endif
}

// ============================================================================
// Пример 4: Управление через макросы
// ============================================================================

void example4_macroControl()
{
    __blib_log_info("\n=== Example 4: Macro Control ===");
    __blib_log_info("Compile-time control through defines:");
    __blib_log_info("");
    __blib_log_info("1. Default behavior:");
    __blib_log_info("   Debug build   -> BLIB_DEBUG_ALLOCATOR_ENABLED (automatic)");
    __blib_log_info("   Release build -> disabled (automatic)");
    __blib_log_info("");
    __blib_log_info("2. Force disable in debug:");
    __blib_log_info("   #define BLIB_DEBUG_ALLOCATOR_DISABLED");
    __blib_log_info("   (before including defaultAllocator.h)");
    __blib_log_info("");
    __blib_log_info("3. Force enable in release:");
    __blib_log_info("   #define BLIB_DEBUG_ALLOCATOR_ENABLED");
    __blib_log_info("   (before including defaultAllocator.h)");
    __blib_log_info("");

    __blib_log_info("Current configuration:");
#ifdef BLIB_DEBUG
    __blib_log_info("  BLIB_DEBUG: defined");
#else
    __blib_log_info("  BLIB_DEBUG: not defined");
#endif

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    __blib_log_info("  BLIB_DEBUG_ALLOCATOR_ENABLED: defined");
#else
    __blib_log_info("  BLIB_DEBUG_ALLOCATOR_ENABLED: not defined");
#endif

#ifdef BLIB_DEBUG_ALLOCATOR_DISABLED
    __blib_log_info("  BLIB_DEBUG_ALLOCATOR_DISABLED: defined");
#else
    __blib_log_info("  BLIB_DEBUG_ALLOCATOR_DISABLED: not defined");
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
    __blib_log_info("\n=== Example 5: Real-World Workflow ===");
    __blib_log_info("Processing data with automatic memory safety...");

    DataProcessor processor;
    processor.processData();

    __blib_log_info("Data processed successfully");

#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    __blib_log_info("(All memory operations were validated automatically)");
#else
    __blib_log_info("(Maximum performance, no validation overhead)");
#endif
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    // CLI-пример: включаем stdout-эхо консоли, чтобы вывод был виден в терминале
    blib::console::Console::instance().getOutput().setStdoutEcho(true);

    __blib_log_info("blib::memory Automatic Debug Allocator Example");
    __blib_log_info("===============================================\n");

    example1_automaticMode();
    example2_normalUsage();
    example3_automaticChecks();
    example4_macroControl();
    example5_workflow();

    __blib_log_info("\n=== All examples completed ===");
    __blib_log_info("\nKey takeaway: Write code once, get automatic memory safety in debug!");

    return 0;
}
