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

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

// ============================================================================
// Пример 1: Базовое использование расширенной статистики
// ============================================================================

void example1_basicExtendedStats()
{
    std::cout << "=== Example 1: Basic Extended Statistics ===" << std::endl;

    auto& global = blib::memory::GlobalAllocator::instance();

    // Включаем расширенную статистику
    std::cout << "Enabling extended statistics..." << std::endl;
    global.setExtendedStatsEnabled(true);

    // Создаём аллокатор
    blib::memory::Allocator alloc;

    // Выделяем память различных размеров для заполнения histogram
    std::cout << "Allocating memory of various sizes..." << std::endl;

    void* small1 = alloc.allocate(32);      // bucket 0: 0-64 bytes
    void* small2 = alloc.allocate(64);      // bucket 0
    void* medium1 = alloc.allocate(128);    // bucket 1: 65-256 bytes
    void* medium2 = alloc.allocate(200);    // bucket 1
    void* large1 = alloc.allocate(512);     // bucket 2: 257-1KB
    void* large2 = alloc.allocate(2048);    // bucket 3: 1KB-4KB
    void* huge = alloc.allocate(100000);    // bucket 5: 16KB-64KB

    std::cout << "Allocated 7 blocks of varying sizes" << std::endl;

    // Получаем histogram
    size_t histogram[9];
    if (global.getHistogram(histogram))
    {
        std::cout << "\nHistogram:" << std::endl;
        const char* labels[] = {
            "0-64 bytes", "65-256 bytes", "257-1KB", "1KB-4KB",
            "4KB-16KB", "16KB-64KB", "64KB-256KB", "256KB-1MB", "1MB+"
        };

        for (int i = 0; i < 9; ++i)
        {
            if (histogram[i] > 0)
            {
                std::cout << "  " << labels[i] << ": " << histogram[i] << std::endl;
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
    std::cout << "Extended statistics disabled" << std::endl;
}

// ============================================================================
// Пример 2: Per-thread статистика
// ============================================================================

void workerThread(int threadId, int allocCount)
{
    blib::memory::Allocator alloc;

    std::cout << "Thread " << threadId << " starting..." << std::endl;

    // Каждый поток выделяет различные размеры
    for (int i = 0; i < allocCount; ++i)
    {
        size_t size = (threadId + 1) * 100 + i * 10;
        void* ptr = alloc.allocate(size);
        
        // Имитация работы
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        alloc.deallocate(ptr, size);
    }

    std::cout << "Thread " << threadId << " finished" << std::endl;
}

void example2_perThreadStats()
{
    std::cout << "\n=== Example 2: Per-Thread Statistics ===" << std::endl;

    auto& global = blib::memory::GlobalAllocator::instance();

    // Включаем расширенную статистику
    global.setExtendedStatsEnabled(true);
    std::cout << "Extended statistics enabled" << std::endl;

    // Запускаем несколько потоков
    std::cout << "Starting 4 worker threads..." << std::endl;
    
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

    std::cout << "\nAll threads finished. Statistics:" << std::endl;

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
    std::cout << "\n=== Example 3: Full Statistics Dump ===" << std::endl;

    auto& global = blib::memory::GlobalAllocator::instance();

    // Включаем всё: leak tracking + extended stats
    std::cout << "Enabling all statistics..." << std::endl;
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

    std::cout << "Allocated " << ptrs.size() << " blocks" << std::endl;

    // Освобождаем половину
    for (size_t i = 0; i < ptrs.size() / 2; ++i)
    {
        alloc.deallocate(ptrs[i], sizes[i]);
    }

    std::cout << "Deallocated half of the blocks" << std::endl;

    // Выводим полную статистику
    std::cout << "\nFull statistics dump:" << std::endl;
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
    std::cout << "\n=== Example 4: Realtime Monitoring ===" << std::endl;

    auto& global = blib::memory::GlobalAllocator::instance();
    global.setExtendedStatsEnabled(true);

    blib::memory::Allocator alloc;

    std::cout << "Simulating allocation patterns..." << std::endl;

    for (int iteration = 0; iteration < 5; ++iteration)
    {
        std::cout << "\n--- Iteration " << (iteration + 1) << " ---" << std::endl;

        // Выделяем память
        std::vector<void*> tempAllocs;
        for (int i = 0; i < 10; ++i)
        {
            size_t size = (i + 1) * 100;
            tempAllocs.push_back(alloc.allocate(size));
        }

        // Показываем текущую статистику
        std::cout << "Current allocated: " << global.getCurrentAllocated() << " bytes" << std::endl;
        std::cout << "Peak allocated: " << global.getPeakAllocated() << " bytes" << std::endl;
        std::cout << "Active allocations: " << global.getAllocationCount() << std::endl;

        // Освобождаем
        size_t size = 100;
        for (void* ptr : tempAllocs)
        {
            alloc.deallocate(ptr, size);
            size += 100;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nFinal histogram:" << std::endl;
    size_t histogram[9];
    if (global.getHistogram(histogram))
    {
        const char* labels[] = {
            "0-64B", "65-256B", "257-1KB", "1KB-4KB",
            "4KB-16KB", "16KB-64KB", "64KB-256KB", "256KB-1MB", "1MB+"
        };

        for (int i = 0; i < 9; ++i)
        {
            std::cout << "  " << labels[i] << ": " << histogram[i] << std::endl;
        }
    }

    global.setExtendedStatsEnabled(false);
}

// ============================================================================
// Пример 5: Анализ паттернов аллокаций
// ============================================================================

void example5_allocationPatterns()
{
    std::cout << "\n=== Example 5: Allocation Pattern Analysis ===" << std::endl;

    auto& global = blib::memory::GlobalAllocator::instance();
    global.setExtendedStatsEnabled(true);

    blib::memory::Allocator alloc;

    // Паттерн 1: Множество маленьких аллокаций
    std::cout << "\nPattern 1: Many small allocations..." << std::endl;
    std::vector<void*> smallAllocs;
    for (int i = 0; i < 100; ++i)
    {
        smallAllocs.push_back(alloc.allocate(32));
    }

    size_t histogram1[9];
    global.getHistogram(histogram1);
    std::cout << "Small allocations (0-64B): " << histogram1[0] << std::endl;

    // Очистка
    for (void* ptr : smallAllocs)
    {
        alloc.deallocate(ptr, 32);
    }
    smallAllocs.clear();

    // Паттерн 2: Несколько больших аллокаций
    std::cout << "\nPattern 2: Few large allocations..." << std::endl;
    std::vector<void*> largeAllocs;
    for (int i = 0; i < 10; ++i)
    {
        largeAllocs.push_back(alloc.allocate(10000));
    }

    size_t histogram2[9];
    global.getHistogram(histogram2);
    std::cout << "Large allocations (4KB-16KB): " << histogram2[4] << std::endl;

    // Очистка
    for (void* ptr : largeAllocs)
    {
        alloc.deallocate(ptr, 10000);
    }

    // Финальный dump
    std::cout << "\nFull pattern analysis:" << std::endl;
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
        std::cout << "[MemoryProfiler] " << name << " started" << std::endl;
#endif
    }

    ~MemoryProfiler()
    {
#ifdef BLIB_DEBUG
        auto& global = blib::memory::GlobalAllocator::instance();
        size_t endAllocated = global.getCurrentAllocated();
        size_t diff = endAllocated - startAllocated;
        
        std::cout << "[MemoryProfiler] " << name << " finished" << std::endl;
        std::cout << "  Memory delta: " << static_cast<ptrdiff_t>(diff) << " bytes" << std::endl;
        
        global.setExtendedStatsEnabled(false);
#endif
    }

private:
    const char* name;
    size_t startAllocated = 0;
};

void example6_conditionalStats()
{
    std::cout << "\n=== Example 6: Conditional Statistics (Debug only) ===" << std::endl;

#ifdef BLIB_DEBUG
    std::cout << "Running in DEBUG mode - profiling enabled" << std::endl;
#else
    std::cout << "Running in RELEASE mode - profiling disabled" << std::endl;
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
    std::cout << "blib::memory Extended Statistics Examples" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    example1_basicExtendedStats();
    example2_perThreadStats();
    example3_fullDump();
    example4_realtimeMonitoring();
    example5_allocationPatterns();
    example6_conditionalStats();

    std::cout << "\n=== All examples completed ===" << std::endl;
    return 0;
}
