#include <blib/system/memory/allocators/poolAllocator.h>
#include <algorithm>
#include <cstring>

namespace blib
{
namespace memory
{
    namespace
    {
        /**
         * Вспомогательная функция для корректировки размера блока в debug режиме.
         * 
         * В debug сборках PoolAllocatorImpl оборачивается в DebugAllocator, который добавляет
         * метаданные (header + guard bytes). Эта функция автоматически увеличивает blockSize
         * на размер этих метаданных, чтобы пользователь мог продолжать передавать обычный
         * размер объекта без учёта debug overhead.
         * 
         * @param userBlockSize Размер блока, запрошенный пользователем
         * @return Скорректированный размер блока (с учётом debug overhead в debug режиме)
         */
        size_t getAdjustedBlockSize(size_t userBlockSize)
        {
            #ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
                // В debug режиме добавляем overhead от DebugAllocator
                // Размер вычисляется автоматически из структуры DebugAllocator
                return userBlockSize + DebugAllocator<PoolAllocatorImpl>::getDebugOverhead();
            #else
                // В release режиме используем размер как есть
                return userBlockSize;
            #endif
        }
    } // anonymous namespace

    PoolAllocatorImpl::PoolAllocatorImpl(size_t blockSize, size_t blocksPerChunk)
        : blockSize(getAdjustedBlockSize(blockSize))
        , blocksPerChunk(blocksPerChunk)
        , freeList(nullptr)
    {
        // Проверяем что blockSize достаточен для хранения указателя (для free list)
        // Минимальный размер блока должен быть sizeof(FreeBlock) == sizeof(void*)
        if (this->blockSize < sizeof(FreeBlock))
        {
            this->blockSize = sizeof(FreeBlock);
        }

        // Резервируем место для chunks чтобы избежать реаллокаций
        // Предполагаем что в среднем понадобится ~4 чанка
        chunks.reserve(4);
    }

    PoolAllocatorImpl::~PoolAllocatorImpl()
    {
        // Освобождаем все выделенные chunks через GlobalAllocator
        for (void* chunk : chunks)
        {
            if (chunk)
            {
                size_t chunkSize = blockSize * blocksPerChunk;
                GlobalAllocator::instance().deallocate(chunk, chunkSize);
            }
        }
        
        // Очищаем вектор (не обязательно, но для ясности)
        chunks.clear();
        freeList = nullptr;
    }

    PoolAllocatorImpl::PoolAllocatorImpl(PoolAllocatorImpl&& other) noexcept
        : blockSize(other.blockSize)
        , blocksPerChunk(other.blocksPerChunk)
        , chunks(std::move(other.chunks))
        , freeList(other.freeList)
    {
        // Обнуляем other чтобы он не освобождал chunks в деструкторе
        other.freeList = nullptr;
        other.blockSize = 0;
        other.blocksPerChunk = 0;
    }

    PoolAllocatorImpl& PoolAllocatorImpl::operator=(PoolAllocatorImpl&& other) noexcept
    {
        if (this != &other)
        {
            // Освобождаем старые chunks
            for (void* chunk : chunks)
            {
                if (chunk)
                {
                    size_t chunkSize = blockSize * blocksPerChunk;
                    GlobalAllocator::instance().deallocate(chunk, chunkSize);
                }
            }

            // Перемещаем данные от other
            blockSize = other.blockSize;
            blocksPerChunk = other.blocksPerChunk;
            chunks = std::move(other.chunks);
            freeList = other.freeList;

            // Обнуляем other
            other.freeList = nullptr;
            other.blockSize = 0;
            other.blocksPerChunk = 0;
        }
        return *this;
    }

    void* PoolAllocatorImpl::allocate(size_t size)
    {
        // Проверяем что запрашиваемый размер соответствует blockSize
        if (size != blockSize)
        {
            // PoolAllocator поддерживает только фиксированный размер
            return nullptr;
        }

        // Если free list пуст - выделяем новый chunk
        if (!freeList)
        {
            if (!allocateNewChunk())
            {
                // Не удалось выделить новый chunk
                return nullptr;
            }
        }

        // Берём первый блок из free list
        FreeBlock* block = freeList;
        freeList = freeList->next;

        // Возвращаем указатель на блок
        // Важно: не инициализируем память, пользователь сам должен construct объект
        return static_cast<void*>(block);
    }

    void PoolAllocatorImpl::deallocate(void* ptr, size_t size)
    {
        // Проверяем валидность входных данных
        if (!ptr)
        {
            return;
        }

        if (size != blockSize)
        {
            // Некорректный размер - игнорируем (или можно добавить assert в debug)
            return;
        }

        // Преобразуем указатель в FreeBlock и добавляем в голову free list
        FreeBlock* block = static_cast<FreeBlock*>(ptr);
        block->next = freeList;
        freeList = block;

        // Важно: НЕ вызываем деструктор объекта - это обязанность пользователя
        // PoolAllocator работает только с raw memory
    }

    size_t PoolAllocatorImpl::getApproximateFreeBlocks() const
    {
        // Обходим free list и считаем блоки
        // TODO: Кешировать это значение для O(1) доступа
        size_t count = 0;
        FreeBlock* current = freeList;
        
        while (current)
        {
            count++;
            current = current->next;
        }

        return count;
    }

    bool PoolAllocatorImpl::allocateNewChunk()
    {
        // Вычисляем размер нового чанка
        size_t chunkSize = blockSize * blocksPerChunk;

        // Выделяем память для чанка через GlobalAllocator
        void* chunk = GlobalAllocator::instance().allocate(chunkSize);
        
        if (!chunk)
        {
            // Аллокация не удалась
            return false;
        }

        // Сохраняем указатель на chunk для освобождения в деструкторе
        chunks.push_back(chunk);

        // Разбиваем chunk на блоки и добавляем их в free list
        // Идём с конца чтобы первый блок оказался в начале free list
        unsigned char* blockPtr = static_cast<unsigned char*>(chunk);
        
        for (size_t i = 0; i < blocksPerChunk; ++i)
        {
            // Преобразуем текущий блок в FreeBlock
            FreeBlock* freeBlock = reinterpret_cast<FreeBlock*>(blockPtr);
            
            // Добавляем в голову free list
            freeBlock->next = freeList;
            freeList = freeBlock;

            // Переходим к следующему блоку
            blockPtr += blockSize;
        }

        return true;
    }

} // namespace memory
} // namespace blib
