#pragma once

#include <utility>
#include "DefaultAllocator.h"

namespace blib
{
    template<typename T>
    struct LinkedListNode
    {
        T data;
        LinkedListNode* next;

        template<typename ...Args>
        LinkedListNode(Args&&... args)
        {
            data = T(std::forward<Args>(args)...);
            next = nullptr;
        }
        LinkedListNode()
        {
            data = T();
            next = nullptr;
        }
    };
    template<typename T, typename AllocatorT = DefaultAllocator>
    class LinkedList
    {
    private:
        LinkedListNode<T>* head;
        LinkedListNode<T>* tail;
        size_t sz;
    public:
        LinkedList();
        ~LinkedList();
        size_t size() const;

        bool pushBack(const T& obj);
        bool pushBack(T&& obj);
        bool pushFront(const T& obj);
        bool pushFront(T&& obj);

        T& front();
        const T& front() const;

        T& back();
        const T& back() const;

        void pop(size_t pos);

        T popFront();
        const T popFront() const;

        T popBack();
        const T popBack() const;

        T& operator[](size_t pos);
        const T& operator[](size_t pos) const;
    };
    template<typename T, typename AllocatorT>
    INLINE LinkedList<T, AllocatorT>::LinkedList()
    {
        this->sz = 0;
        this->head = nullptr;
        this->tail = nullptr;
    }
    template<typename T, typename AllocatorT>
    INLINE LinkedList<T, AllocatorT>::~LinkedList()
    {
        LinkedListNode<T>* pcurrent = this->head;
        LinkedListNode<T>* pnext = pcurrent ? pcurrent->next : nullptr;

        for (size_t i = 0; i < this->sz; ++i)
        {
            AllocatorT::destroy(pcurrent);
            pcurrent = pnext;
            pnext = pcurrent ? pcurrent->next : nullptr;
        }
    }
    template<typename T, typename AllocatorT>
    INLINE size_t LinkedList<T, AllocatorT>::size() const
    {
        return this->sz;
    }
    template<typename T, typename AllocatorT>
    INLINE bool LinkedList<T, AllocatorT>::pushBack(const T& obj)
    {
        bool res = false;
        if (!sz)
        {
            this->tail = AllocatorT::create(obj);
            if (!this->tail)
                return false;
            this->head = this->tail;
            this->head->next = this->tail;
        }
        else
        {
            this->tail->next = AllocatorT::create(obj);
            if (!this->tail->next)
                return false;
        }

        if (this->tail.next)
            return false;

        this->tail = this->tail->next;
        ++sz;
        return true;
    }
    template<typename T, typename AllocatorT>
    INLINE bool LinkedList<T, AllocatorT>::pushBack(T&& obj)
    {
        bool res = false;
        if (!sz)
        {
            this->tail = AllocatorT::template create<LinkedListNode<T> >(std::move(obj));
            if (!this->tail)
                return false;
            this->head = this->tail;
            this->head->next = this->tail;
        }
        else
        {
            this->tail->next = AllocatorT::template create<LinkedListNode<T> >(std::move(obj));
            if (!this->tail->next)
                return false;
        }

        this->tail = this->tail->next;
        ++sz;
        return true;
    }
    template<typename T, typename AllocatorT>
    INLINE T& LinkedList<T, AllocatorT>::front()
    {
        return head->data;
    }
    template<typename T, typename AllocatorT>
    INLINE const T& LinkedList<T, AllocatorT>::front() const
    {
        return this->front();
    }
    template<typename T, typename AllocatorT>
    INLINE T& LinkedList<T, AllocatorT>::back()
    {
        return tail->data;
    }
    template<typename T, typename AllocatorT>
    INLINE const T& LinkedList<T, AllocatorT>::back() const
    {
        return this->back();
    }
    template<typename T, typename AllocatorT>
    INLINE void LinkedList<T, AllocatorT>::pop(size_t pos)
    {
        LinkedListNode<T>* pprev = nullptr;
        LinkedListNode<T>* pcurrent = this->head;
        LinkedListNode<T>* pdeleting;
        LinkedListNode<T>* pretarget;

        for (size_t i = 0; i < pos; ++i)
        {
            pprev = pcurrent;
            pcurrent = pcurrent->next;
        }

        if (!pprev)
            this->head = this->head->next;
        else if (pcurrent == this->tail)
            this->tail = pprev;

        pdeleting = pprev ? pcurrent : nullptr;
        pretarget = pprev ? pprev : pcurrent;

        pretarget->next = pdeleting ? pdeleting->next : nullptr;
        
        AllocatorT::destroy(pdeleting);
            
        --sz;
    }
    template<typename T, typename AllocatorT>
    INLINE T LinkedList<T, AllocatorT>::popFront()
    {
        T res(std::move(this->operator[](0)));
        this->pop(0);
        return res;
    }
    template<typename T, typename AllocatorT>
    INLINE const T LinkedList<T, AllocatorT>::popFront() const
    {
        this->popFront();
    }
    template<typename T, typename AllocatorT>
    INLINE T LinkedList<T, AllocatorT>::popBack()
    {
        T res(std::move(this->operator[](this->sz - 1)));
        this->pop(this->sz - 1);
        return res;
    }
    template<typename T, typename AllocatorT>
    INLINE const T LinkedList<T, AllocatorT>::popBack() const
    {
        this->popBack();
    }
    template<typename T, typename AllocatorT>
    INLINE T& LinkedList<T, AllocatorT>::operator[](size_t pos)
    {
        LinkedListNode<T>* res = this->head;
        for (size_t i = 0; i < pos; ++i)
            res = res->next;

        return res->data;
    }

    template<typename T, typename AllocatorT>
    INLINE const T& LinkedList<T, AllocatorT>::operator[](size_t pos) const
    {
        return this->operator[](pos);
    }
}
