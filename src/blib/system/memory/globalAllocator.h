#pragma once

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/utilmacro.h>

namespace thread
{
    class RWLocker;
}

namespace blib
{
namespace memory
{
    /**
     * GlobalAllocator - глобальный синглтон для управления всеми аллокациями памяти в системе.
     * 
     * Назначение:
     * - Центральная точка для всех аллокаций (god allocator)
     * - Сбор статистики использования памяти (текущая, пиковая, количество)
     * - Прокси к системным new/delete с возможностью замены реализации
     * - Thread-safe через RWLocker (read для статистики, write для аллокаций)
     * 
     * Использование:
     *   auto& global = GlobalAllocator::instance();
     *   void* ptr = global.allocate(1024);
     *   global.deallocate(ptr, 1024);
     * 
     * Ограничения:
     * - Singleton - единственный экземпляр на процесс
     * - Потокобезопасен, но блокировки могут снижать производительность в высоконагруженных сценариях
     * - Базовая статистика, расширенная (histogram, per-thread, leak tracking) планируется позже
     */
    class __blib_system_api GlobalAllocator
    {
    public:
        /**
         * Получить единственный экземпляр GlobalAllocator.
         * Thread-safe инициализация через magic static (C++11).
         */
        static GlobalAllocator& instance();

        /**
         * Выделить блок памяти заданного размера.
         * 
         * @param size Размер блока в байтах (должен быть > 0)
         * @return Указатель на выделенный блок или nullptr при ошибке
         * 
         * Потокобезопасность: write lock
         * Инварианты:
         * - Если size == 0, поведение не определено (зависит от реализации new)
         * - Увеличивает currentAllocated и allocationCount
         * - Обновляет peakAllocated если необходимо
         */
        void* allocate(size_t size);

        /**
         * Освободить ранее выделенный блок памяти.
         * 
         * @param ptr Указатель на блок (должен быть получен из allocate)
         * @param size Размер блока в байтах (должен совпадать с allocate)
         * 
         * Потокобезопасность: write lock
         * Инварианты:
         * - ptr должен быть получен через allocate этого же аллокатора
         * - size должен точно совпадать с размером при аллокации
         * - Уменьшает currentAllocated и allocationCount
         * - Двойной delete вызовет undefined behavior
         */
        void deallocate(_In void* ptr, size_t size);

        /**
         * Получить текущий объем выделенной памяти в байтах.
         * Потокобезопасность: read lock
         */
        size_t getCurrentAllocated() const;

        /**
         * Получить пиковый объем выделенной памяти за всё время работы программы.
         * Потокобезопасность: read lock
         */
        size_t getPeakAllocated() const;

        /**
         * Получить количество активных аллокаций (allocate без соответствующих deallocate).
         * Потокобезопасность: read lock
         */
        size_t getAllocationCount() const;

        /**
         * Включить leak tracking - сохранение информации о каждой аллокации.
         * 
         * ВАЖНО: Значительно увеличивает overhead (память и производительность)!
         * Использовать только для debug/поиска утечек.
         * 
         * После включения будут сохраняться адреса и размеры всех аллокаций.
         * При выключении - очищается вся накопленная информация.
         * 
         * @param enabled true для включения, false для выключения
         */
        void setLeakTrackingEnabled(bool enabled);

        /**
         * Проверить включен ли leak tracking.
         * @return true если включен
         */
        bool isLeakTrackingEnabled() const;

        /**
         * Вывести отчёт об утечках памяти (не освобождённых аллокациях).
         * Работает только если leak tracking включен.
         * 
         * Выводит в stderr список всех активных аллокаций:
         * - Адрес блока
         * - Размер блока
         * - Общее количество утечек
         * - Общий размер утечек
         * 
         * @return Количество обнаруженных утечек
         */
        size_t dumpLeaks() const;

        /**
         * Включить сбор расширенной статистики (histogram, per-thread, etc).
         * 
         * ВАЖНО: Увеличивает overhead!
         * - Histogram: дополнительная память для bucket'ов
         * - Per-thread: TLS storage для каждого потока
         * 
         * @param enabled true для включения, false для выключения
         */
        void setExtendedStatsEnabled(bool enabled);

        /**
         * Проверить включена ли расширенная статистика.
         * @return true если включена
         */
        bool isExtendedStatsEnabled() const;

        /**
         * Получить histogram распределения размеров аллокаций.
         * Работает только если расширенная статистика включена.
         * 
         * Histogram разбит на bucket'ы:
         * - 0-64 bytes
         * - 65-256 bytes
         * - 257-1KB
         * - 1KB-4KB
         * - 4KB-16KB
         * - 16KB-64KB
         * - 64KB-256KB
         * - 256KB-1MB
         * - 1MB+
         * 
         * @param outBuckets Указатель на массив из 9 элементов для записи результата
         * @return true если данные записаны, false если статистика выключена
         */
        bool getHistogram(size_t* outBuckets) const;

        /**
         * Вывести полный отчёт статистики в stderr.
         * Включает:
         * - Базовая статистика (current/peak/count)
         * - Histogram (если включена расширенная статистика)
         * - Per-thread статистика (если включена)
         * - Leak report (если включен leak tracking)
         */
        void dumpStats() const;

    private:
        // Приватный конструктор/деструктор для singleton
        GlobalAllocator();
        ~GlobalAllocator();

        // Запрет копирования и перемещения
        GlobalAllocator(const GlobalAllocator&) = delete;
        GlobalAllocator(GlobalAllocator&&) = delete;
        GlobalAllocator& operator=(const GlobalAllocator&) = delete;
        GlobalAllocator& operator=(GlobalAllocator&&) = delete;

        // Статистика
        size_t currentAllocated;   // Текущий объём выделенной памяти (в байтах)
        size_t peakAllocated;      // Максимальный объём за всё время
        size_t allocationCount;    // Количество активных аллокаций

        // Leak tracking
        bool leakTrackingEnabled;  // Включен ли leak tracking
        void* leakTracker;         // Указатель на LeakTracker (pimpl для скрытия std::unordered_map)

        // Extended statistics
        bool extendedStatsEnabled; // Включена ли расширенная статистика
        void* statsCollector;      // Указатель на StatsCollector (pimpl)

        // Синхронизация для thread-safety
        thread::RWLocker* lock;
    };

} // namespace memory
} // namespace blib
