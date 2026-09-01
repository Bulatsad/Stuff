#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>

#include <blib/inline.h>

namespace blib
{
    template<typename T>
    struct LinkedListNode
    {
        T data;
        LinkedListNode* next;

        LinkedListNode(const T& obj)
            : data(obj)
            , next(nullptr)
        {
        }

        LinkedListNode(T&& obj)
            : data(std::move(obj))
            , next(nullptr)
        {
        }
    };

    template<typename T, typename AllocatorT = std::allocator<LinkedListNode<T> > >
    class LinkedList
    {
    public:
        // std-совместимый forward iterator для обхода списка.
        // Инвариант: итератор валиден пока список не модифицировался
        // (как и у любого двусвязного/односвязного списка).
        class Iterator
        {
        public:
            typedef std::forward_iterator_tag iterator_category;
            typedef T value_type;
            typedef std::ptrdiff_t difference_type;
            typedef T* pointer;
            typedef T& reference;

            Iterator()
                : current(nullptr)
            {
            }

            explicit Iterator(LinkedListNode<T>* node)
                : current(node)
            {
            }

            T& operator*() const { return this->current->data; }
            T* operator->() const { return &this->current->data; }

            Iterator& operator++()
            {
                this->current = this->current->next;
                return *this;
            }

            Iterator operator++(int)
            {
                Iterator tmp(*this);
                ++(*this);
                return tmp;
            }

            bool operator==(const Iterator& other) const { return this->current == other.current; }
            bool operator!=(const Iterator& other) const { return this->current != other.current; }

        private:
            LinkedListNode<T>* current;
        };

        // Константная версия итератора (возвращает const T&).
        class ConstIterator
        {
        public:
            typedef std::forward_iterator_tag iterator_category;
            typedef T value_type;
            typedef std::ptrdiff_t difference_type;
            typedef const T* pointer;
            typedef const T& reference;

            ConstIterator()
                : current(nullptr)
            {
            }

            explicit ConstIterator(LinkedListNode<T>* node)
                : current(node)
            {
            }

            const T& operator*() const { return this->current->data; }
            const T* operator->() const { return &this->current->data; }

            ConstIterator& operator++()
            {
                this->current = this->current->next;
                return *this;
            }

            ConstIterator operator++(int)
            {
                ConstIterator tmp(*this);
                ++(*this);
                return tmp;
            }

            bool operator==(const ConstIterator& other) const { return this->current == other.current; }
            bool operator!=(const ConstIterator& other) const { return this->current != other.current; }

        private:
            LinkedListNode<T>* current;
        };

    private:
        AllocatorT allocator;
        LinkedListNode<T>* head;
        LinkedListNode<T>* tail;
        size_t sz;

    public:
        LinkedList();
        ~LinkedList();

        size_t size() const;
        bool empty() const;

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
        T popBack();

        T& operator[](size_t pos);
        const T& operator[](size_t pos) const;

        Iterator begin() { return Iterator(this->head); }
        Iterator end() { return Iterator(nullptr); }

        ConstIterator begin() const { return ConstIterator(this->head); }
        ConstIterator end() const { return ConstIterator(nullptr); }

        ConstIterator cbegin() const { return ConstIterator(this->head); }
        ConstIterator cend() const { return ConstIterator(nullptr); }
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
        // Разрушаем и освобождаем все узлы (destroy + deallocate, размер обязателен).
        LinkedListNode<T>* pcurrent = this->head;
        while (pcurrent)
        {
            LinkedListNode<T>* pnext = pcurrent->next;
            this->allocator.destroy(pcurrent);
            this->allocator.deallocate(pcurrent, sizeof(LinkedListNode<T>));
            pcurrent = pnext;
        }
    }

    template<typename T, typename AllocatorT>
    __blib_inline size_t LinkedList<T, AllocatorT>::size() const
    {
        return this->sz;
    }

    template<typename T, typename AllocatorT>
    __blib_inline bool LinkedList<T, AllocatorT>::empty() const
    {
        return this->sz == 0;
    }

    template<typename T, typename AllocatorT>
    __blib_inline bool LinkedList<T, AllocatorT>::pushBack(const T& obj)
    {
        // Выделяем память под узел и конструируем его (протокол std::allocator:
        // allocate + construct(ptr, args...)).
        LinkedListNode<T>* pnode = this->allocator.allocate(sizeof(LinkedListNode<T>));
        if (!pnode)
            return false;
        this->allocator.construct(pnode, obj);

        // Новый узел всегда становится хвостом, его next - nullptr (конец списка).
        pnode->next = nullptr;
        if (!this->sz)
            this->head = pnode;
        else
            this->tail->next = pnode;
        this->tail = pnode;
        ++this->sz;
        return true;
    }

    template<typename T, typename AllocatorT>
    __blib_inline bool LinkedList<T, AllocatorT>::pushBack(T&& obj)
    {
        LinkedListNode<T>* pnode = this->allocator.allocate(sizeof(LinkedListNode<T>));
        if (!pnode)
            return false;
        this->allocator.construct(pnode, std::move(obj));

        pnode->next = nullptr;
        if (!this->sz)
            this->head = pnode;
        else
            this->tail->next = pnode;
        this->tail = pnode;
        ++this->sz;
        return true;
    }

    template<typename T, typename AllocatorT>
    __blib_inline bool LinkedList<T, AllocatorT>::pushFront(const T& obj)
    {
        LinkedListNode<T>* pnode = this->allocator.allocate(sizeof(LinkedListNode<T>));
        if (!pnode)
            return false;
        this->allocator.construct(pnode, obj);

        // Новый узел становится головой и указывает на прежнюю голову.
        pnode->next = this->head;
        this->head = pnode;
        if (!this->sz)
            this->tail = pnode;
        ++this->sz;
        return true;
    }

    template<typename T, typename AllocatorT>
    __blib_inline bool LinkedList<T, AllocatorT>::pushFront(T&& obj)
    {
        LinkedListNode<T>* pnode = this->allocator.allocate(sizeof(LinkedListNode<T>));
        if (!pnode)
            return false;
        this->allocator.construct(pnode, std::move(obj));

        pnode->next = this->head;
        this->head = pnode;
        if (!this->sz)
            this->tail = pnode;
        ++this->sz;
        return true;
    }

    template<typename T, typename AllocatorT>
    __blib_inline T& LinkedList<T, AllocatorT>::front()
    {
        return this->head->data;
    }

    template<typename T, typename AllocatorT>
    __blib_inline const T& LinkedList<T, AllocatorT>::front() const
    {
        return this->head->data;
    }

    template<typename T, typename AllocatorT>
    __blib_inline T& LinkedList<T, AllocatorT>::back()
    {
        return this->tail->data;
    }

    template<typename T, typename AllocatorT>
    __blib_inline const T& LinkedList<T, AllocatorT>::back() const
    {
        return this->tail->data;
    }

    template<typename T, typename AllocatorT>
    __blib_inline void LinkedList<T, AllocatorT>::pop(size_t pos)
    {
        // pos должен быть в пределах [0, sz), иначе - UB (согласовано с operator[]).
        LinkedListNode<T>* pprev = nullptr;
        LinkedListNode<T>* pcurrent = this->head;
        for (size_t i = 0; i < pos; ++i)
        {
            pprev = pcurrent;
            pcurrent = pcurrent->next;
        }

        // Вынимаем узел из цепочки: предыдущий пропускает его, либо двигается голова.
        if (pprev)
            pprev->next = pcurrent->next;
        else
            this->head = pcurrent->next;

        // Если удаляли хвост - новым хвостом становится предыдущий узел.
        if (pcurrent == this->tail)
            this->tail = pprev;

        this->allocator.destroy(pcurrent);
        this->allocator.deallocate(pcurrent, sizeof(LinkedListNode<T>));
        --this->sz;
    }

    template<typename T, typename AllocatorT>
    __blib_inline T LinkedList<T, AllocatorT>::popFront()
    {
        T res(std::move(this->front()));
        this->pop(0);
        return res;
    }

    template<typename T, typename AllocatorT>
    __blib_inline T LinkedList<T, AllocatorT>::popBack()
    {
        T res(std::move(this->back()));
        this->pop(this->sz - 1);
        return res;
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
        LinkedListNode<T>* res = this->head;
        for (size_t i = 0; i < pos; ++i)
            res = res->next;

        return res->data;
    }
}
