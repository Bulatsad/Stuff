#pragma once

#include "LinkedList.h"
#include "DefaultAllocator.h"
#include "Pair.h"

namespace blib
{
    template<typename T, size_t PreAllocatedBlockSize = 10000, typename Allocator = DefaultAllocator >
    class StaticObjectPool
    {
    public:
        template<typename ...Args>
        T* create(Args&&...args);
        void destroy(T* pobj);
    private:
        T pool[PreAllocatedBlockSize];
        StaticObjectPool* next;
        LinkedList<Pair<StaticObjectPool*, size_t> >trash;
        size_t allocated;
        size_t last;
    };
    template<typename T, size_t PreAllocatedBlockSize, typename Allocator>
    template<typename ...Args>
    inline T* StaticObjectPool<T, PreAllocatedBlockSize, Allocator>::create(Args&& ...args)
    {
        StaticObjectPool* pcurrentPool = this;
        if (this->trash.size() == 0)
        {
            StaticObjectPool* pprevPool = nullptr;
            while (pcurrentPool->allocated == PreAllocatedBlockSize)
            {
                pprevPool = pcurrentPool;
                pcurrentPool = pcurrentPool->next;
                if (!pcurrentPool)
                {
                    pcurrentPool = Allocator::template create<StaticObjectPool>();
                    pprevPool->next = pcurrentPool;
                }
            }

            pcurrentPool->pool[pcurrentPool->last] = std::move(T(std::forward<Args>(args)...));
            ++(pcurrentPool->last);
            ++(pcurrentPool->allocated);
            return &(pcurrentPool->pool[pcurrentPool->last - 1]);
        }
        else
        {
            auto freeSpace = trash.popFront();
            StaticObjectPool* pcurrentPool = freeSpace.first;
            size_t pos = freeSpace.second;

            ++(pcurrentPool->allocated);
            pcurrentPool->pool[pos] = std::move(T(std::forward<Args>(args)...));
            return &(pcurrentPool->pool[pos]);
        }
    }
    template<typename T, size_t PreAllocatedBlockSize, typename Allocator>
    inline void StaticObjectPool<T, PreAllocatedBlockSize, Allocator>::destroy(T* pobj)
    {
        StaticObjectPool* pcurrentPool = this;
        while (!(pobj >= (&(pcurrentPool->pool[0])) && pobj <= &(pcurrentPool->pool[PreAllocatedBlockSize - 1])))
            pcurrentPool = pcurrentPool->next;

        pobj->~T();

        size_t trashPos = (pobj - &(pcurrentPool->pool[0]));

        pcurrentPool->trash.pushBack(blib::make_pair(pcurrentPool, trashPos));
    }
}