#include <blib/system/memory/globalAllocator.h>
#include <blib/system/thread/rwlock.h>

#include <new>
#include <algorithm>
#include <unordered_map>
#include <cstdio>
#include <cstring>
#include <thread>

namespace blib
{
namespace memory
{
    /**
     * LeakTracker - внутренняя структура для отслеживания аллокаций.
     * Использует unordered_map для хранения адрес -> размер.
     * 
     * Pimpl idiom: скрывает std::unordered_map из публичного header.
     */
    struct LeakTracker
    {
        std::unordered_map<void*, size_t> allocations;
    };

    /**
     * StatsCollector - сборщик расширенной статистики.
     * 
     * Histogram bucket'ы:
     * [0] 0-64 bytes
     * [1] 65-256 bytes
     * [2] 257-1KB
     * [3] 1KB-4KB
     * [4] 4KB-16KB
     * [5] 16KB-64KB
     * [6] 64KB-256KB
     * [7] 256KB-1MB
     * [8] 1MB+
     */
    struct StatsCollector
    {
        static constexpr size_t HISTOGRAM_BUCKETS = 9;
        size_t histogram[HISTOGRAM_BUCKETS];

        // Per-thread статистика (thread_id -> stats)
        struct ThreadStats
        {
            size_t allocations;
            size_t deallocations;
            size_t bytesAllocated;
            size_t bytesDeallocated;

            ThreadStats() 
                : allocations(0)
                , deallocations(0)
                , bytesAllocated(0)
                , bytesDeallocated(0)
            {}
        };

        std::unordered_map<std::thread::id, ThreadStats> perThreadStats;

        StatsCollector()
        {
            std::memset(histogram, 0, sizeof(histogram));
        }

        // Получить индекс bucket'а для заданного размера
        static size_t getBucketIndex(size_t size)
        {
            if (size <= 64)          return 0;
            if (size <= 256)         return 1;
            if (size <= 1024)        return 2;
            if (size <= 4096)        return 3;
            if (size <= 16384)       return 4;
            if (size <= 65536)       return 5;
            if (size <= 262144)      return 6;
            if (size <= 1048576)     return 7;
            return 8;
        }
    };

    GlobalAllocator::GlobalAllocator()
        : currentAllocated(0)
        , peakAllocated(0)
        , allocationCount(0)
        , leakTrackingEnabled(false)
        , leakTracker(nullptr)
        , extendedStatsEnabled(false)
        , statsCollector(nullptr)
        , lock(nullptr)
    {
        // Создаём RWLocker для синхронизации
        // 
        // bootstrap exception: ::new/delete ЗАПРЕЩЁН в проекте, но здесь
        // нельзя использовать GlobalAllocator::allocate(), т.к. lock ещё
        // nullptr и allocate() упадёт на lock->writeLock().
        // Используем системный ::operator new напрямую (не new-выражение).
        void* lockMemory = ::operator new(sizeof(thread::RWLocker));
        lock = new (lockMemory) thread::RWLocker();
    }

    GlobalAllocator::~GlobalAllocator()
    {
        // Если leak tracking включен - выводим отчёт перед завершением
        if (leakTrackingEnabled && leakTracker)
        {
            size_t leakCount = dumpLeaks();
            if (leakCount > 0)
            {
                std::fprintf(stderr, "\n*** WARNING: %zu memory leak(s) detected at program exit! ***\n", leakCount);
            }
        }

        // Освобождаем StatsCollector
        // bootstrap exception: системный ::operator delete (парный к ::operator new выше)
        if (statsCollector)
        {
            StatsCollector* stats = static_cast<StatsCollector*>(statsCollector);
            stats->~StatsCollector();
            ::operator delete(stats);
            statsCollector = nullptr;
        }

        // Освобождаем LeakTracker
        // bootstrap exception: системный ::operator delete (парный к ::operator new выше)
        if (leakTracker)
        {
            LeakTracker* tracker = static_cast<LeakTracker*>(leakTracker);
            tracker->~LeakTracker();
            ::operator delete(tracker);
            leakTracker = nullptr;
        }

        // Освобождаем RWLocker
        // bootstrap exception: системный ::operator delete (парный к ::operator new выше)
        if (lock)
        {
            lock->~RWLocker();
            ::operator delete(lock);
            lock = nullptr;
        }
    }

    GlobalAllocator& GlobalAllocator::instance()
    {
        // Magic static - thread-safe инициализация с C++11
        // Гарантируется, что конструктор вызовется ровно один раз
        static GlobalAllocator instance;
        return instance;
    }

    void* GlobalAllocator::allocate(size_t size)
    {
        // bootstrap exception: это и есть сам аллокатор, системный ::operator new -
        // единственное разрешённое место получения памяти в обход GlobalAllocator
        void* ptr = ::operator new(size, std::nothrow);

        if (ptr)
        {
            // Обновляем статистику под write lock
            lock->writeLock();
            
            currentAllocated += size;
            allocationCount += 1;
            
            // Обновляем пиковое значение если текущее больше
            if (currentAllocated > peakAllocated)
            {
                peakAllocated = currentAllocated;
            }

            // Если leak tracking включен - сохраняем информацию об аллокации
            if (leakTrackingEnabled && leakTracker)
            {
                LeakTracker* tracker = static_cast<LeakTracker*>(leakTracker);
                tracker->allocations[ptr] = size;
            }

            // Если расширенная статистика включена - обновляем histogram и per-thread
            if (extendedStatsEnabled && statsCollector)
            {
                StatsCollector* stats = static_cast<StatsCollector*>(statsCollector);
                
                // Обновляем histogram
                size_t bucketIndex = StatsCollector::getBucketIndex(size);
                stats->histogram[bucketIndex]++;

                // Обновляем per-thread статистику
                std::thread::id threadId = std::this_thread::get_id();
                auto& threadStats = stats->perThreadStats[threadId];
                threadStats.allocations++;
                threadStats.bytesAllocated += size;
            }
            
            lock->writeUnlock();
        }

        return ptr;
    }

    void GlobalAllocator::deallocate(_In void* ptr, size_t size)
    {
        if (!ptr)
        {
            // nullptr можно безопасно игнорировать (аналогично delete nullptr)
            return;
        }

        // Обновляем статистику под write lock
        lock->writeLock();
        
        currentAllocated -= size;
        allocationCount -= 1;

        // Если leak tracking включен - удаляем запись об аллокации
        if (leakTrackingEnabled && leakTracker)
        {
            LeakTracker* tracker = static_cast<LeakTracker*>(leakTracker);
            tracker->allocations.erase(ptr);
        }

        // Если расширенная статистика включена - обновляем per-thread
        if (extendedStatsEnabled && statsCollector)
        {
            StatsCollector* stats = static_cast<StatsCollector*>(statsCollector);
            
            // Обновляем per-thread статистику
            std::thread::id threadId = std::this_thread::get_id();
            auto& threadStats = stats->perThreadStats[threadId];
            threadStats.deallocations++;
            threadStats.bytesDeallocated += size;
        }
        
        lock->writeUnlock();

        // bootstrap exception: парный системный ::operator delete к ::operator new в allocate()
        ::operator delete(ptr);
    }

    size_t GlobalAllocator::getCurrentAllocated() const
    {
        // Читаем статистику под read lock
        lock->readLock();
        size_t result = currentAllocated;
        lock->readUnock();
        return result;
    }

    size_t GlobalAllocator::getPeakAllocated() const
    {
        // Читаем статистику под read lock
        lock->readLock();
        size_t result = peakAllocated;
        lock->readUnock();
        return result;
    }

    size_t GlobalAllocator::getAllocationCount() const
    {
        // Читаем статистику под read lock
        lock->readLock();
        size_t result = allocationCount;
        lock->readUnock();
        return result;
    }

    void GlobalAllocator::setLeakTrackingEnabled(bool enabled)
    {
        lock->writeLock();

        if (enabled && !leakTrackingEnabled)
        {
            // Включаем leak tracking
            if (!leakTracker)
            {
                // bootstrap exception: GlobalAllocator::allocate() здесь невозможен:
                // 1) мы уже держим writeLock (SRWLOCK не реентерабелен - дедлок)
                // 2) самоссылка: запись о LeakTracker попала бы в сам LeakTracker
                // Используем системный ::operator new напрямую (не new-выражение)
                void* trackerMemory = ::operator new(sizeof(LeakTracker));
                leakTracker = new (trackerMemory) LeakTracker();
            }
            leakTrackingEnabled = true;
        }
        else if (!enabled && leakTrackingEnabled)
        {
            // Выключаем leak tracking
            leakTrackingEnabled = false;
            
            // Очищаем данные
            if (leakTracker)
            {
                LeakTracker* tracker = static_cast<LeakTracker*>(leakTracker);
                tracker->allocations.clear();
            }
        }

        lock->writeUnlock();
    }

    bool GlobalAllocator::isLeakTrackingEnabled() const
    {
        lock->readLock();
        bool result = leakTrackingEnabled;
        lock->readUnock();
        return result;
    }

    size_t GlobalAllocator::dumpLeaks() const
    {
        if (!leakTrackingEnabled || !leakTracker)
        {
            std::fprintf(stderr, "Leak tracking is not enabled!\n");
            return 0;
        }

        lock->readLock();

        LeakTracker* tracker = static_cast<LeakTracker*>(leakTracker);
        size_t leakCount = tracker->allocations.size();

        if (leakCount == 0)
        {
            std::fprintf(stderr, "No memory leaks detected.\n");
            lock->readUnock();
            return 0;
        }

        // Вычисляем общий размер утечек
        size_t totalLeakedBytes = 0;
        for (const auto& pair : tracker->allocations)
        {
            totalLeakedBytes += pair.second;
        }

        // Выводим отчёт
        std::fprintf(stderr, "\n========================================\n");
        std::fprintf(stderr, "*** MEMORY LEAK REPORT ***\n");
        std::fprintf(stderr, "========================================\n");
        std::fprintf(stderr, "Total leaks: %zu allocation(s)\n", leakCount);
        std::fprintf(stderr, "Total leaked: %zu bytes\n", totalLeakedBytes);
        std::fprintf(stderr, "----------------------------------------\n");

        // Выводим детали каждой утечки
        size_t index = 1;
        for (const auto& pair : tracker->allocations)
        {
            std::fprintf(stderr, "[%zu] Address: %p, Size: %zu bytes\n", 
                        index++, pair.first, pair.second);
        }

        std::fprintf(stderr, "========================================\n\n");
        std::fflush(stderr);

        lock->readUnock();
        return leakCount;
    }

    void GlobalAllocator::setExtendedStatsEnabled(bool enabled)
    {
        lock->writeLock();

        if (enabled && !extendedStatsEnabled)
        {
            // Включаем расширенную статистику
            if (!statsCollector)
            {
                // bootstrap exception: GlobalAllocator::allocate() здесь невозможен:
                // 1) мы уже держим writeLock (SRWLOCK не реентерабелен - дедлок)
                // 2) самоссылка: аллокация StatsCollector попала бы в его же histogram
                // Используем системный ::operator new напрямую (не new-выражение)
                void* statsMemory = ::operator new(sizeof(StatsCollector));
                statsCollector = new (statsMemory) StatsCollector();
            }
            extendedStatsEnabled = true;
        }
        else if (!enabled && extendedStatsEnabled)
        {
            // Выключаем расширенную статистику
            extendedStatsEnabled = false;
            
            // Очищаем данные
            if (statsCollector)
            {
                StatsCollector* stats = static_cast<StatsCollector*>(statsCollector);
                std::memset(stats->histogram, 0, sizeof(stats->histogram));
                stats->perThreadStats.clear();
            }
        }

        lock->writeUnlock();
    }

    bool GlobalAllocator::isExtendedStatsEnabled() const
    {
        lock->readLock();
        bool result = extendedStatsEnabled;
        lock->readUnock();
        return result;
    }

    bool GlobalAllocator::getHistogram(size_t* outBuckets) const
    {
        if (!outBuckets)
        {
            return false;
        }

        if (!extendedStatsEnabled || !statsCollector)
        {
            return false;
        }

        lock->readLock();

        StatsCollector* stats = static_cast<StatsCollector*>(statsCollector);
        std::memcpy(outBuckets, stats->histogram, sizeof(stats->histogram));

        lock->readUnock();
        return true;
    }

    void GlobalAllocator::dumpStats() const
    {
        lock->readLock();

        std::fprintf(stderr, "\n========================================\n");
        std::fprintf(stderr, "*** GLOBAL ALLOCATOR STATISTICS ***\n");
        std::fprintf(stderr, "========================================\n");

        // Базовая статистика
        std::fprintf(stderr, "\n--- Basic Statistics ---\n");
        std::fprintf(stderr, "Current allocated: %zu bytes (%.2f MB)\n", 
                    currentAllocated, currentAllocated / (1024.0 * 1024.0));
        std::fprintf(stderr, "Peak allocated: %zu bytes (%.2f MB)\n", 
                    peakAllocated, peakAllocated / (1024.0 * 1024.0));
        std::fprintf(stderr, "Active allocations: %zu\n", allocationCount);

        // Histogram (если включена расширенная статистика)
        if (extendedStatsEnabled && statsCollector)
        {
            StatsCollector* stats = static_cast<StatsCollector*>(statsCollector);

            std::fprintf(stderr, "\n--- Allocation Size Histogram ---\n");
            const char* bucketNames[] = {
                "0-64 bytes",
                "65-256 bytes",
                "257-1KB",
                "1KB-4KB",
                "4KB-16KB",
                "16KB-64KB",
                "64KB-256KB",
                "256KB-1MB",
                "1MB+"
            };

            for (size_t i = 0; i < StatsCollector::HISTOGRAM_BUCKETS; ++i)
            {
                std::fprintf(stderr, "  %-15s: %zu\n", bucketNames[i], stats->histogram[i]);
            }

            // Per-thread статистика
            if (!stats->perThreadStats.empty())
            {
                std::fprintf(stderr, "\n--- Per-Thread Statistics ---\n");
                std::fprintf(stderr, "Total threads: %zu\n", stats->perThreadStats.size());
                
                size_t threadIndex = 1;
                for (const auto& pair : stats->perThreadStats)
                {
                    const auto& threadStats = pair.second;
                    std::fprintf(stderr, "\nThread #%zu:\n", threadIndex++);
                    std::fprintf(stderr, "  Allocations: %zu\n", threadStats.allocations);
                    std::fprintf(stderr, "  Deallocations: %zu\n", threadStats.deallocations);
                    std::fprintf(stderr, "  Bytes allocated: %zu\n", threadStats.bytesAllocated);
                    std::fprintf(stderr, "  Bytes deallocated: %zu\n", threadStats.bytesDeallocated);
                    std::fprintf(stderr, "  Net bytes: %zd\n", 
                                static_cast<ptrdiff_t>(threadStats.bytesAllocated) - 
                                static_cast<ptrdiff_t>(threadStats.bytesDeallocated));
                }
            }
        }
        else
        {
            std::fprintf(stderr, "\n(Extended statistics disabled - enable with setExtendedStatsEnabled(true))\n");
        }

        std::fprintf(stderr, "\n========================================\n\n");
        std::fflush(stderr);

        lock->readUnock();

        // Если leak tracking включен - выводим leak report
        if (leakTrackingEnabled && leakTracker)
        {
            dumpLeaks();
        }
    }

} // namespace memory
} // namespace blib
