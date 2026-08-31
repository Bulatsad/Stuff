#pragma once

#include <new>
#include <type_traits>
#include <utility>

#include <blib/memory/globalAllocator.h>

namespace blib
{
namespace memory
{
    /**
     * SBO (Small Buffer Optimization) - шаблонный класс для оптимизации хранения маленьких объектов.
     * 
     * Назначение:
     * - Если объект влезает в буфер (sizeof(T) <= BufferSize && alignof(T) <= Alignment) - хранится inline
     * - Если не влезает - выделяется через GlobalAllocator в heap
     * - Устраняет heap allocation для маленьких stateful аллокаторов
     * 
     * Использование:
     *   SBO<64> storage;
     *   MyAllocator* alloc = storage.construct<MyAllocator>(args...);
     *   storage.destroy();
     * 
     * Ограничения:
     * - Может хранить только один объект за раз
     * - Объект должен удовлетворять alignment требованиям
     * - Не thread-safe (синхронизация должна быть снаружи)
     * 
     * @tparam BufferSize Размер inline буфера в байтах
     * @tparam Alignment Выравнивание буфера (по умолчанию - void*)
     */
    template<size_t BufferSize, size_t Alignment = alignof(void*)>
    class SBO
    {
    public:
        /**
         * Конструктор - инициализирует буфер как пустой.
         */
        SBO()
            : heapPtr(nullptr)
        {
        }

        /**
         * Деструктор - НЕ вызывает destroy() автоматически!
         * Пользователь должен явно вызвать destroy() перед уничтожением SBO.
         * 
         * Обоснование: SBO не знает тип хранимого объекта, поэтому не может вызвать правильный деструктор.
         */
        ~SBO()
        {
            // Ничего не делаем - пользователь должен вызвать destroy() явно
        }

        // Запрет копирования и перемещения
        SBO(const SBO&) = delete;
        SBO(SBO&&) = delete;
        SBO& operator=(const SBO&) = delete;
        SBO& operator=(SBO&&) = delete;

        /**
         * Размещает объект типа T в буфере (если влезает) или в heap (если нет).
         * 
         * @tparam T Тип создаваемого объекта
         * @tparam Args Типы аргументов конструктора
         * @param args Аргументы для конструктора T
         * @return Указатель на созданный объект
         * 
         * Инварианты:
         * - Должен вызываться на пустом SBO (heapPtr == nullptr && буфер не занят)
         * - После вызова объект считается активным до destroy()
         * 
         * Compile-time проверки:
         * - sizeof(T) <= BufferSize - объект влезает по размеру
         * - alignof(T) <= Alignment - выравнивание корректно
         */
        template<typename T, typename... Args>
        T* construct(Args&&... args)
        {
            // Compile-time проверки размера и выравнивания
            static_assert(sizeof(T) <= BufferSize, "Object is too large for SBO buffer");
            static_assert(alignof(T) <= Alignment, "Object alignment is too strict for SBO buffer");

            // Размещаем объект в inline буфере через placement new
            T* ptr = new (buffer) T(std::forward<Args>(args)...);
            
            // heapPtr остаётся nullptr, значит используется inline буфер
            heapPtr = nullptr;

            return ptr;
        }

        /**
         * Размещает объект в heap если он не влезает в inline буфер.
         * Эта перегрузка используется когда compile-time проверки в construct() не проходят.
         * 
         * ВАЖНО: В текущей реализации эта функция НЕ используется, так как construct()
         * имеет static_assert и не скомпилируется для больших объектов.
         * Оставлена для будущего расширения функционала.
         * 
         * @tparam T Тип создаваемого объекта
         * @tparam Args Типы аргументов конструктора
         * @param args Аргументы для конструктора T
         * @return Указатель на созданный объект в heap
         */
        template<typename T, typename... Args>
        T* constructInHeap(Args&&... args)
        {
            // Выделяем память через GlobalAllocator
            void* memory = GlobalAllocator::instance().allocate(sizeof(T));
            
            if (!memory)
            {
                // Аллокация не удалась
                return nullptr;
            }

            // Создаём объект через placement new
            T* ptr = new (memory) T(std::forward<Args>(args)...);
            
            // Сохраняем указатель на heap
            heapPtr = ptr;

            return ptr;
        }

        /**
         * Уничтожает хранимый объект и освобождает память (если была в heap).
         * 
         * @tparam T Тип уничтожаемого объекта (должен совпадать с тем, что был в construct)
         * 
         * Инварианты:
         * - T должен быть тем же типом, что был передан в construct()
         * - После вызова SBO считается пустым
         * 
         * ВАЖНО: Пользователь ДОЛЖЕН знать тип T, так как это header-only template.
         * В type-erased Allocator это будет обёрнуто в виртуальную функцию.
         */
        template<typename T>
        void destroy()
        {
            if (heapPtr)
            {
                // Объект был в heap
                T* ptr = static_cast<T*>(heapPtr);
                
                // Явно вызываем деструктор
                ptr->~T();
                
                // Освобождаем память через GlobalAllocator
                GlobalAllocator::instance().deallocate(heapPtr, sizeof(T));
                
                heapPtr = nullptr;
            }
            else
            {
                // Объект был в inline буфере
                T* ptr = reinterpret_cast<T*>(buffer);
                
                // Явно вызываем деструктор
                ptr->~T();
                
                // Память не освобождается - это stack buffer
            }
        }

        /**
         * Получить указатель на хранимый объект.
         * 
         * @return Указатель на объект (inline или heap)
         * 
         * ВАЖНО: Пользователь должен знать реальный тип для корректного приведения.
         */
        void* get()
        {
            return heapPtr ? heapPtr : static_cast<void*>(buffer);
        }

        /**
         * Получить константный указатель на хранимый объект.
         */
        const void* get() const
        {
            return heapPtr ? heapPtr : static_cast<const void*>(buffer);
        }

        /**
         * Проверить, хранится ли объект inline (в буфере) или в heap.
         * 
         * @return true если объект в inline буфере, false если в heap
         */
        bool isInline() const
        {
            return heapPtr == nullptr;
        }

    private:
        // Inline буфер с выравниванием
        alignas(Alignment) unsigned char buffer[BufferSize];
        
        // Указатель на heap (nullptr если используется inline буфер)
        void* heapPtr;
    };

} // namespace memory
} // namespace blib
