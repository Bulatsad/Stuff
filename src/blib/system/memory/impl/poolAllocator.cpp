#include <blib/system/memory/allocators/poolAllocator.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace blib
{
namespace memory
{
    namespace
    {
        // Начальный резерв под чанки: типичный пул использует ~4 чанка,
        // резерв избавляет от реаллокаций вектора chunks на старте.
        constexpr size_t initialChunkReserve = 4;

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
        chunks.reserve(initialChunkReserve);
    }

    PoolAllocatorImpl::~PoolAllocatorImpl()
    {
        // Освобождаем все выделенные chunks через GlobalAllocator
        for (void* chunk : chunks)
        {
            if (chunk)
            {
                size_t chunkSize = this->getChunkTotalBytes();
                GlobalAllocator::instance().deallocate(chunk, chunkSize);
            }
        }
        
        // Очищаем вектор (не обязательно, но для ясности)
        chunks.clear();
        freeList = nullptr;
    }

    PoolAllocatorImpl::PoolAllocatorImpl(_In _Out PoolAllocatorImpl&& other) noexcept
        : blockSize(other.blockSize)
        , blocksPerChunk(other.blocksPerChunk)
        , freeList(other.freeList)
    {
        // ВАЖНО: chunks НЕ перемещается в mem-init списке. Vector move-ctor
        // скопировал бы указатель адаптера на other.chunkAllocator (висячий
        // после смерти other). Вместо этого chunks инициализирован NSDMI
        // с адаптером на this->chunkAllocator, а move-assign ниже сохраняет
        // наш аллокатор (propagate_on_container_move_assignment == false
        // у StdAllocatorAdapter).
        //
        // ГРАБЛИ MSVC: при move-assign с неравными аллокаторами вектор
        // memmove'ит элементы в *this, но НЕ опустошает источник — other.chunks
        // сохраняет size и указатели на чанки. Поэтому обязателен явный
        // clear(): иначе деструктор moved-from пула освободит чанки второй
        // раз (double free → повреждение кучи).
        chunks = std::move(other.chunks);
        other.chunks.clear();

        // Обнуляем other чтобы он не освобождал chunks в деструкторе
        other.freeList = nullptr;
        other.blockSize = 0;
        other.blocksPerChunk = 0;
    }

    PoolAllocatorImpl& PoolAllocatorImpl::operator=(_In _Out PoolAllocatorImpl&& other) noexcept
    {
        if (this != &other)
        {
            // Освобождаем старые chunks (размер считается по СТАРЫМ полям -
            // перезапись blockSize/blocksPerChunk идёт ниже).
            for (void* chunk : chunks)
            {
                if (chunk)
                {
                    size_t chunkSize = this->getChunkTotalBytes();
                    GlobalAllocator::instance().deallocate(chunk, chunkSize);
                }
            }

            // Перемещаем данные от other
            blockSize = other.blockSize;
            blocksPerChunk = other.blocksPerChunk;

            // ГРАБЛИ MSVC: move-assign с неравными аллокаторами не опустошает
            // источник — поэтому явный clear() обязателен (иначе деструктор
            // other освободит чанки второй раз → double free).
            chunks = std::move(other.chunks);
            other.chunks.clear();
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

        // Снимаем бит "свободен" в битмапе чанка. Поиск чанка - O(C),
        // скан с конца (LIFO: блоки чаще из свежих чанков).
        size_t chunkI = this->findChunkIndex(block);
        if (chunkI == this->chunks.size())
        {
            // Внутренний инвариант: блок из free list обязан принадлежать
            // какому-то чанку. Если нет - повреждено состояние пула.
            // blib-system не имеет Console - диагностика в stderr (см. SYSTEM.md).
            std::fprintf(stderr, "PoolAllocator internal error: free block outside of any chunk!\n");
            std::fflush(stderr);
            std::abort();
        }

        const buint64 chunkBase = reinterpret_cast<buint64>(this->chunks[chunkI]);
        const buint64 blockAddr = reinterpret_cast<buint64>(block);
        const size_t blockJ = (blockAddr - chunkBase - this->getBitmapBytes()) / this->blockSize;

        buint64* bitmap = static_cast<buint64*>(this->chunks[chunkI]);
        bitmap[blockJ / bitsPerWord] &= ~(static_cast<buint64>(1) << (blockJ % bitsPerWord));

        // Возвращаем указатель на блок
        // Важно: не инициализируем память, пользователь сам должен construct объект
        return static_cast<void*>(block);
    }

    size_t PoolAllocatorImpl::findChunkIndex(_In const void* ptr) const
    {
        // Каждый чанк - отдельный буфер фиксированного размера, поэтому
        // принадлежность проверяется диапазоном адресов. Скан с конца:
        // deallocate чаще приходит на свежие чанки (LIFO free list).
        const size_t totalBytes = this->getChunkTotalBytes();
        const buint64 addr = reinterpret_cast<buint64>(ptr);

        for (size_t i = this->chunks.size(); i-- > 0; )
        {
            const buint64 base = reinterpret_cast<buint64>(this->chunks[i]);
            if (addr >= base && addr < base + totalBytes)
            {
                return i;
            }
        }

        // Указатель не принадлежит ни одному чанку
        return this->chunks.size();
    }

    void PoolAllocatorImpl::deallocate(_In void* ptr, size_t size)
    {
        // Проверяем валидность входных данных
        if (!ptr)
        {
            return;
        }

        if (size != blockSize)
        {
            // Некорректный размер - игнорируем
            return;
        }

        // Чужой указатель (не из наших чанков): раньше здесь было молчаливое
        // повреждение free list (UB), теперь - warning + no-op. В debug сборке
        // до нас это ловит DebugAllocator (abort).
        const size_t chunkI = this->findChunkIndex(ptr);
        if (chunkI == this->chunks.size())
        {
            std::fprintf(stderr, "PoolAllocator: deallocate of foreign pointer %p ignored!\n", ptr);
            std::fflush(stderr);
            return;
        }

        // Вычисляем смещение и индекс блока внутри чанка. Проверка диапазона
        // выше гарантирует offset < totalBytes, но указатель может попасть
        // в область битмапа или между блоками - такой тоже игнорируем
        // (иначе испортим битмап/чужие блоки).
        const buint64 chunkBase = reinterpret_cast<buint64>(this->chunks[chunkI]);
        const size_t offset = reinterpret_cast<buint64>(ptr) - chunkBase;
        if (offset < this->getBitmapBytes() || (offset - this->getBitmapBytes()) % this->blockSize != 0)
        {
            std::fprintf(stderr, "PoolAllocator: deallocate of misaligned pointer %p ignored!\n", ptr);
            std::fflush(stderr);
            return;
        }

        const size_t blockJ = (offset - this->getBitmapBytes()) / this->blockSize;

        buint64* bitmap = static_cast<buint64*>(this->chunks[chunkI]);
        const size_t wordIndex = blockJ / bitsPerWord;
        const buint64 bitMask = static_cast<buint64>(1) << (blockJ % bitsPerWord);

        // Бит уже взведён - double-free (в release DebugAllocator не ловит).
        // warning + no-op: повторный возврат блока в free list зациклил бы его.
        if ((bitmap[wordIndex] & bitMask) != 0)
        {
            std::fprintf(stderr, "PoolAllocator: double-free of block %p ignored!\n", ptr);
            std::fflush(stderr);
            return;
        }

        // Взводим бит "свободен"
        bitmap[wordIndex] |= bitMask;

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
        // Полный размер буфера чанка: инлайн-битмап (1 бит на блок) + сами блоки
        const size_t chunkSize = this->getChunkTotalBytes();
        const size_t bitmapBytes = this->getBitmapBytes();

        // Выделяем память для чанка через GlobalAllocator
        void* chunk = GlobalAllocator::instance().allocate(chunkSize);
        
        if (!chunk)
        {
            // Аллокация не удалась
            return false;
        }

        // Сохраняем указатель на chunk для освобождения в деструкторе
        chunks.push_back(chunk);

        // Битмап в начале чанка: 1 = блок свободен. Свежий чанк весь свободен
        // (неиспользуемые хвостовые биты последнего слова тоже взведены - их
        // никто не читает, т.к. blockJ < blocksPerChunk всегда).
        std::memset(chunk, 0xFF, bitmapBytes);

        // Блоки стартуют сразу за битмапом. Каждый блок кладём в голову
        // free list (LIFO - allocate() забирает блоки с конца чанка).
        unsigned char* blockPtr = static_cast<unsigned char*>(chunk) + bitmapBytes;
        
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
