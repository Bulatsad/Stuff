#pragma once

#include<stdint.h>
#include<stdlib.h>

#include<blib/align.h>
#include<blib/inline.h>

namespace blib
{
	template<class T, size_t align>
	struct AlignedAllocator
	{
		T* allocate(size_t blockSize);
		void deallocate(T* pBlock, size_t blockSize);

		template<class ...Args>
		void construct(T* pObj, Args&&...args);

		void destroy(T* pObj);

		using type = T;
	};

	template<class T, size_t align>
	__blib_inline T* AlignedAllocator<T, align>::allocate(size_t blockSize)
	{
		return reinterpret_cast<T*>(_aligned_malloc(blockSize, align));
	}
	template<class T, size_t align>
	__blib_inline void AlignedAllocator<T, align>::deallocate(T* pBlock, size_t blockSize)
	{
		free(pBlock);
	}

	template<class T, size_t align>
	__blib_inline void AlignedAllocator<T, align>::destroy(T* pObj)
	{
		pObj->~T();
	}

	template<class T, size_t align>
	template<class ...Args>
	__blib_inline void AlignedAllocator<T, align>::construct(T* pObj, Args && ...args)
	{
		pObj->T(std::forward<Args>(args)...);
	}

	template<class T>
	using CacheAlignedAllocator = AlignedAllocator<T, __blib_cache_size>;
}
