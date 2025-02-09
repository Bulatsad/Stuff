#pragma once

#include <memory>

#include <blib/inline.h>

namespace blib
{
    template<typename T>
    struct LinkedListNode
    {
        T data;
        LinkedListNode* next;

        LinkedListNode(const T&obj)
        {
            new (const_cast<void*>(static_cast<const volatile void*>(&data))) T(obj);
            next = nullptr;
        }
        LinkedListNode(T&& obj)
        {
            new (const_cast<void*>(static_cast<const volatile void*>(&this->data))) T(std::move(obj));
            next = nullptr;
        }

        //template<typename ...Args>
        //LinkedListNode(Args&&... args)
        //{
        //    new (const_cast<void*>(static_cast<const volatile void*>(&data))) T(std::forward<Args>(args)...);
        //    next = nullptr;
        //}
        LinkedListNode() = delete;
        //{
        //    data = T();
        //    next = nullptr;
        //}
    };
    template<typename T, typename AllocatorT = std::allocator<LinkedListNode<T> > >
    class LinkedList
    {
    private:
        AllocatorT allocator;
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
    __blib_inline LinkedList<T, AllocatorT>::LinkedList()
    {
        this->sz = 0;
        this->head = nullptr;
        this->tail = nullptr;
    }

    template<typename T, typename AllocatorT>
    __blib_inline LinkedList<T, AllocatorT>::~LinkedList()
    {
        LinkedListNode<T>* pcurrent = this->head;
        LinkedListNode<T>* pnext = pcurrent ? pcurrent->next : nullptr;

        for (size_t i = 0; i < this->sz; ++i)
        {
            this->allocator.destroy(pcurrent);
            pcurrent = pnext;
            pnext = pcurrent ? pcurrent->next : nullptr;
        }
    }

    template<typename T, typename AllocatorT>
    __blib_inline size_t LinkedList<T, AllocatorT>::size() const
    {
        return this->sz;
    }

    template<typename T, typename AllocatorT>
    __blib_inline bool LinkedList<T, AllocatorT>::pushBack(const T& obj)
    {
        bool res = false;
        if (!sz)
        {
            this->tail = this->allocator.construct(obj);
            if (!this->tail)
                return false;
            this->head = this->tail;
            this->head->next = this->tail;
        }
        else
        {
            this->tail->next = this->allocator.construct(obj);
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
    __blib_inline bool LinkedList<T, AllocatorT>::pushBack(T&& obj)
    {
        bool res = false;
        if (!sz)
        {
            this->tail = this->allocator.allocate(sizeof(LinkedListNode<T>));
            this->allocator.construct(this->tail, std::move(obj));
            
            if (!this->tail)
                return false;
            this->head = this->tail;
            this->head->next = this->tail;
        }
        else
        {
            this->tail->next = this->allocator.allocate(sizeof(LinkedListNode<T>));
            this->allocator.construct(this->tail->next, std::move(obj));
            if (!this->tail->next)
                return false;
        }

        this->tail = this->tail->next;
        ++sz;
        return true;
    }

    template<typename T, typename AllocatorT>
    __blib_inline T& LinkedList<T, AllocatorT>::front()
    {
        return head->data;
    }

    template<typename T, typename AllocatorT>
    __blib_inline const T& LinkedList<T, AllocatorT>::front() const
    {
        return this->front();
    }

    template<typename T, typename AllocatorT>
    __blib_inline T& LinkedList<T, AllocatorT>::back()
    {
        return tail->data;
    }

    template<typename T, typename AllocatorT>
    __blib_inline const T& LinkedList<T, AllocatorT>::back() const
    {
        return this->back();
    }

    template<typename T, typename AllocatorT>
    __blib_inline void LinkedList<T, AllocatorT>::pop(size_t pos)
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

        this->allocator.destroy(pdeleting);
        this->allocator.deallocate(pdeleting, sizeof(LinkedListNode<T>));

        --sz;
    }

    template<typename T, typename AllocatorT>
    __blib_inline T LinkedList<T, AllocatorT>::popFront()
    {
        T res(std::move(this->operator[](0)));
        this->pop(0);
        return res;
    }

    template<typename T, typename AllocatorT>
    __blib_inline const T LinkedList<T, AllocatorT>::popFront() const
    {
        this->popFront();
    }

    template<typename T, typename AllocatorT>
    __blib_inline T LinkedList<T, AllocatorT>::popBack()
    {
        T res(std::move(this->operator[](this->sz - 1)));
        this->pop(this->sz - 1);
        return res;
    }

    template<typename T, typename AllocatorT>
    __blib_inline const T LinkedList<T, AllocatorT>::popBack() const
    {
        this->popBack();
    }

    template<typename T, typename AllocatorT>
    __blib_inline T& LinkedList<T, AllocatorT>::operator[](size_t pos)
    {
        LinkedListNode<T>* res = this->head;
        for (size_t i = 0; i < pos; ++i)
            res = res->next;

        return res->data;
    }

    template<typename T, typename AllocatorT>
    __blib_inline const T& LinkedList<T, AllocatorT>::operator[](size_t pos) const
    {
        return this->operator[](pos);
    }
}
