#pragma once

#include <blib/config.h>
#include <blib/blibint.h>

namespace blib
{
    namespace core
    {
        // Utill class for array slicing
        template<class Type>
        class __blib_core_api UnsafeSlicer
        {
        public:
            UnsafeSlicer(Type* pBase, size_t aStrideElem = 1);
            Type& operator[](size_t index) __blib_unsafe;
            const Type& operator[](size_t index) const __blib_unsafe;
            
        private:
            buint8* base;
            size_t stride;
        };

        template<class Type>
        class __blib_core_api ConstUnsafeSlicer
        {
        public:
            ConstUnsafeSlicer(const Type* pBase, size_t aStrideElem = 1);
            const Type& operator[](size_t index) const __blib_unsafe;

        private:
            const buint8* base;
            size_t stride;
        };

        //UnsafeSlicer
        template<class Type>
        inline UnsafeSlicer<Type>::UnsafeSlicer(Type* pBase, size_t aStrideElem)
        {
            this->base = reinterpret_cast<decltype(this->base)>(pBase);
            this->stride = aStrideElem * sizeof(Type);
        }
        template<class Type>
        inline Type& UnsafeSlicer<Type>::operator[](size_t index)
        {
            return *(reinterpret_cast<Type*>(this->base + (index * this->stride)));
        }
        template<class Type>
        inline const Type& UnsafeSlicer<Type>::operator[](size_t index) const
        {
            return (*this)[index];
        }
        
        //ConstUnsafeSlicer
        template<class Type>
        inline ConstUnsafeSlicer<Type>::ConstUnsafeSlicer(const Type* pBase, size_t aStrideElem)
        {
            this->base = reinterpret_cast<decltype(this->base)>(pBase);
            this->stride = aStrideElem * sizeof(Type);
        }
        template<class Type>
        inline const Type& ConstUnsafeSlicer<Type>::operator[](size_t index) const
        {
            return *(reinterpret_cast<Type*>(this->base + (index * this->stride)));
        }
    }
}