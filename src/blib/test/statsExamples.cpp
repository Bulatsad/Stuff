/**
 * statsExamples.cpp - примеры использования расширенной статистики blib::memory
 * 
 * Этот файл демонстрирует:
 * 1. Histogram распределения размеров аллокаций
 * 2. Per-thread статистику
 * 3. Полный dump статистики
 * 4. Интеграция статистики с leak tracking
 * 
 * Компиляция не требуется - это справочный файл с примерами кода.
 */

#include <blib/system/memory/allocator.h>
#include <blib/system/memory/globalAllocator.h>

#include <blib/core/console/console.h>
#include <vector>
#include <thread>
#include <chrono>

// ============================================================================
// Пример 1: Базовое использование расширенной статистики
// ============================================================================

void example1_basicExtendedStats()
{
    __blib_log_info("=== Example 1: Basic Extended Statistics ===");

    auto& global = blib::memory::GlobalAllocator::instance();

    // Включаем расширенную статистику
    __blib_log_info("Enabling extended statistics...");
    global.setExtendedStatsEnabled(true);

    // Создаём аллокатор
    blib::memory::Allocator alloc;

    // Выделяем память различных размеров для заполнения histogram
    __blib_log_info("Allocating memory of various sizes...");

    void* small1 = alloc.allocate(32);      // bucket 0: 0-64 bytes
    void* small2 = alloc.allocate(64);      // bucket 0
    void* medium1 = alloc.allocate(128);    // bucket 1: 65-256 bytes
    void* medium2 = alloc.allocate(200);    // bucket 1
    void* large1 = alloc.allocate(512);     // bucket 2: 257-1KB
    void* large2 = alloc.allocate(2048);    // bucket 3: 1KB-4KB
    void* huge = alloc.allocate(100000);    // bucket 5: 16KB-64KB

    __blib_log_info("Allocated 7 blocks of varying sizes");

    // Получаем histogram
    size_t histogram[9];
    if (global.getHistogram(histogram))
    {
        __blib_log_info("\nHistogram:");
        const char* labels[] = {
            "0-64 bytes", "65-256 bytes", "257-1KB", "1KB-4KB",
            "4KB-16KB", "16KB-64KB", "64KB-256KB", "256KB-1MB", "1MB+"
        };

        for (int i = 0; i < 9; ++i)
        {
            if (histogram[i] > 0)
            {
                __blib_log_info("  %s: %zu", labels[i], histogram[i]);
            }
        }
    }

    // Cleanup
    alloc.deallocate(small1, 32);
    alloc.deallocate(small2, 64);
    alloc.deallocate(medium1, 128);
    alloc.deallocate(medium2, 200);
    alloc.deallocate(large1, 512);
    alloc.deallocate(large2, 2048);
    alloc.deallocate(huge, 100000);

    // Выключаем статистику
    global.setExtendedStatsEnabled(false);
    __blib_log_info("Extended statistics disabled");
}

// ============================================================================
// Пример 2: Per-thread статистика
// ============================================================================

void workerThread(int threadId, int allocCount)
{
    blib::memory::Allocator alloc;

    __blib_log_info("Thread %d starting...", threadId);

    // Каждый поток выделяет различные размеры
    for (int i = 0; i < allocCount; ++i)
    {
        size_t size = (threadId + 1) * 100 + i * 10;
        void* ptr = alloc.allocate(size);
        
        // Имитация работы
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        alloc.deallocate(ptr, size);
    }

    __blib_log_info("Thread %d finished", threadId);
}

void example2_perThreadStats()
{
    __blib_log_info("\n=== Example 2: Per-Thread Statistics ===");

    auto& global = blib::memory::GlobalAllocator::instance();

    // Включаем расширенную статистику
    global.setExtendedStatsEnabled(true);
    __blib_log_info("Extended statistics enabled");

    // Запускаем несколько потоков
    __blib_log_info("Starting 4 worker threads...");
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back(workerThread, i, 10);
    }

    // Ждём завершения
    for (auto& t : threads)
    {
        t.join();
    }

    __blib_log_info("\nAll threads finished. Statistics:");

    // Выводим полную статистику (включая per-thread)
    global.dumpStats();

    // Выключаем статистику
    global.setExtendedStatsEnabled(false);
}

// ============================================================================
// Пример 3: Полный dump статистики
// ============================================================================

void example3_fullDump()
{
    __blib_log_info("\n=== Example 3: Full Statistics Dump ===");

    auto& global = blib::memory::GlobalAllocator::instance();

    // Включаем всё: leak tracking + extended stats
    __blib_log_info("Enabling all statistics...");
    global.setLeakTrackingEnabled(true);
    global.setExtendedStatsEnabled(true);

    blib::memory::Allocator alloc;

    // Выделяем разнообразную память
    std::vector<void*> ptrs;
    std::vector<size_t> sizes = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192};

    for (size_t size : sizes)
    {
        void* ptr = alloc.allocate(size);
        ptrs.push_back(ptr);
    }

    __blib_log_info("Allocated %zu blocks", ptrs.size());

    // Освобождаем половину
    for (size_t i = 0; i < ptrs.size() / 2; ++i)
    {
        alloc.deallocate(ptrs[i], sizes[i]);
    }

    __blib_log_info("Deallocated half of the blocks");

    // Выводим полную статистику
    __blib_log_info("\nFull statistics dump:");
    global.dumpStats();

    // Cleanup оставшихся
    for (size_t i = ptrs.size() / 2; i < ptrs.size(); ++i)
    {
        alloc.deallocate(ptrs[i], sizes[i]);
    }

    // Выключаем всё
    global.setLeakTrackingEnabled(false);
    global.setExtendedStatsEnabled(false);
}

// ============================================================================
// Пример 4: Мониторинг в реальном времени
// ============================================================================

void example4_realtimeMonitoring()
{
    __blib_log_info("\n=== Example 4: Realtime Monitoring ===");

    auto& global = blib::memory::GlobalAllocator::instance();
    global.setExtendedStatsEnabled(true);

    blib::memory::Allocator alloc;

    __blib_log_info("Simulating allocation patterns...");

    for (int iteration = 0; iteration < 5; ++iteration)
    {
        __blib_log_info("\n--- Iteration %d ---", iteration + 1);

        // Выделяем память
        std::vector<void*> tempAllocs;
        for (int i = 0; i < 10; ++i)
        {
            size_t size = (i + 1) * 100;
            tempAllocs.push_back(alloc.allocate(size));
        }

        // Показываем текущую статистику
        __blib_log_info("Current allocated: %zu bytes", global.getCurrentAllocated());
        __blib_log_info("Peak allocated: %zu bytes", global.getPeakAllocated());
        __blib_log_info("Active allocations: %zu", global.getAllocationCount());

        // Освобождаем
        size_t size = 100;
        for (void* ptr : tempAllocs)
        {
            alloc.deallocate(ptr, size);
            size += 100;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    __blib_log_info("\nFinal histogram:");
    size_t histogram[9];
    if (global.getHistogram(histogram))
    {
        const char* labels[] = {
            "0-64B", "65-256B", "257-1KB", "1KB-4KB",
            "4KB-16KB", "16KB-64KB", "64KB-256KB", "256KB-1MB", "1MB+"
        };

        for (int i = 0; i < 9; ++i)
        {
            __blib_log_info("  %s: %zu", labels[i], histogram[i]);
        }
    }

    global.setExtendedStatsEnabled(false);
}

// ============================================================================
// Пример 5: Анализ паттернов аллокаций
// ============================================================================

void example5_allocationPatterns()
{
    __blib_log_info("\n=== Example 5: Allocation Pattern Analysis ===");

    auto& global = blib::memory::GlobalAllocator::instance();
    global.setExtendedStatsEnabled(true);

    blib::memory::Allocator alloc;

    // Паттерн 1: Множество маленьких аллокаций
    __blib_log_info("\nPattern 1: Many small allocations...");
    std::vector<void*> smallAllocs;
    for (int i = 0; i < 100; ++i)
    {
        smallAllocs.push_back(alloc.allocate(32));
    }

    size_t histogram1[9];
    global.getHistogram(histogram1);
    __blib_log_info("Small allocations (0-64B): %zu", histogram1[0]);

    // Очистка
    for (void* ptr : smallAllocs)
    {
        alloc.deallocate(ptr, 32);
    }
    smallAllocs.clear();

    // Паттерн 2: Несколько больших аллокаций
    __blib_log_info("\nPattern 2: Few large allocations...");
    std::vector<void*> largeAllocs;
    for (int i = 0; i < 10; ++i)
    {
        largeAllocs.push_back(alloc.allocate(10000));
    }

    size_t histogram2[9];
    global.getHistogram(histogram2);
    __blib_log_info("Large allocations (4KB-16KB): %zu", histogram2[4]);

    // Очистка
    for (void* ptr : largeAllocs)
    {
        alloc.deallocate(ptr, 10000);
    }

    // Финальный dump
    __blib_log_info("\nFull pattern analysis:");
    global.dumpStats();

    global.setExtendedStatsEnabled(false);
}

// ============================================================================
// Пример 6: Conditional statistics (debug vs release)
// ============================================================================

class MemoryProfiler
{
public:
    MemoryProfiler(const char* name) : name(name)
    {
#ifdef BLIB_DEBUG
        auto& global = blib::memory::GlobalAllocator::instance();
        global.setExtendedStatsEnabled(true);
        startAllocated = global.getCurrentAllocated();
        __blib_log_info("[MemoryProfiler] %s started", name);
#endif
    }

    ~MemoryProfiler()
    {
#ifdef BLIB_DEBUG
        auto& global = blib::memory::GlobalAllocator::instance();
        size_t endAllocated = global.getCurrentAllocated();
        size_t diff = endAllocated - startAllocated;
        
        __blib_log_info("[MemoryProfiler] %s finished", name);
        __blib_log_info("  Memory delta: %lld bytes", static_cast<long long>(static_cast<ptrdiff_t>(diff)));
        
        global.setExtendedStatsEnabled(false);
#endif
    }

private:
    const char* name;
    size_t startAllocated = 0;
};

void example6_conditionalStats()
{
    __blib_log_info("\n=== Example 6: Conditional Statistics (Debug only) ===");

#ifdef BLIB_DEBUG
    __blib_log_info("Running in DEBUG mode - profiling enabled");
#else
    __blib_log_info("Running in RELEASE mode - profiling disabled");
#endif

    {
        MemoryProfiler profiler("Test Function");

        blib::memory::Allocator alloc;
        std::vector<void*> ptrs;

        for (int i = 0; i < 20; ++i)
        {
            ptrs.push_back(alloc.allocate((i + 1) * 50));
        }

        // Cleanup
        size_t size = 50;
        for (void* ptr : ptrs)
        {
            alloc.deallocate(ptr, size);
            size += 50;
        }
    } // profiler выведет результаты здесь
}

// ============================================================================
// Main - запуск примеров
// ============================================================================

int main()
{
    // CLI-пример: включаем stdout-эхо консоли, чтобы вывод был виден в терминале
    blib::console::Console::instance().getOutput().setStdoutEcho(true);

    __blib_log_info("blib::memory Extended Statistics Examples");
    __blib_log_info("==========================================\n");

    example1_basicExtendedStats();
    example2_perThreadStats();
    example3_fullDump();
    example4_realtimeMonitoring();
    example5_allocationPatterns();
    example6_conditionalStats();

    __blib_log_info("\n=== All examples completed ===");
    return 0;
}
