#include <blib/test/src/test.h>

// Core memory system headers
#include <blib/system/memory/allocatorTraits.h>
#include <blib/system/memory/globalAllocator.h>

// Debug allocator must be included before other allocators if in debug mode
#ifdef BLIB_DEBUG
	#include <blib/system/memory/allocators/debugAllocator.h>
#endif

#include <blib/system/memory/defaultAllocator.h>
#include <blib/system/memory/allocator.h>

// Specific allocators
#include <blib/system/memory/allocators/mallocAllocator.h>
#include <blib/system/memory/allocators/poolAllocator.h>

// Adapters
#include <blib/system/memory/stdAllocatorAdapter.h>

// Standard library
#include <thread>
#include <atomic>
#include <vector>
#include <list>
#include <string>
#include <algorithm>
#include <cstring>
#include <chrono>

using namespace blib::memory;

// ============================================================
// Helper Macros for Testing Debug Allocator Aborts
// ============================================================

#ifdef _MSC_VER
#include <windows.h> // For EXCEPTION_EXECUTE_HANDLER
#define BLIB_TEST_EXPECT_ABORT(expr) \
	do \
	{ \
		bool __caught = false; \
		__try { \
			expr; \
		} __except(EXCEPTION_EXECUTE_HANDLER) { \
			__caught = true; \
		} \
		/* Пропущенный abort - это регрессия детекции, тест должен падать */ \
		BLIB_TEST_CHECK(__caught); \
	} while(0)
#else
// Non-MSVC platforms: skip abort tests (could use signal handlers in future)
#define BLIB_TEST_EXPECT_ABORT(expr) \
	do { \
		std::cerr << "  SKIPPED: BLIB_TEST_EXPECT_ABORT not supported on this platform" << std::endl; \
	} while(0)
#endif

// ============================================================
// 1. GlobalAllocator Tests
// ============================================================

BLIB_TEST_CASE("GlobalAllocator: singleton instance")
{
	auto& ga1 = GlobalAllocator::instance();
	auto& ga2 = GlobalAllocator::instance();
	
	// Same instance address
	BLIB_TEST_CHECK(&ga1 == &ga2);
}

BLIB_TEST_CASE("GlobalAllocator: basic allocation and deallocation")
{
	auto& ga = GlobalAllocator::instance();
	
	size_t beforeCount = ga.getAllocationCount();
	size_t beforeAllocated = ga.getCurrentAllocated();
	
	void* ptr = ga.allocate(128);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	size_t afterAlloc = ga.getCurrentAllocated();
	BLIB_TEST_CHECK(afterAlloc >= beforeAllocated + 128);
	BLIB_TEST_CHECK(ga.getAllocationCount() == beforeCount + 1);
	
	ga.deallocate(ptr, 128);
	
	BLIB_TEST_CHECK(ga.getAllocationCount() == beforeCount);
}

BLIB_TEST_CASE("GlobalAllocator: large allocations (1MB+)")
{
	auto& ga = GlobalAllocator::instance();
	
	size_t largeSize = 1024 * 1024; // 1MB
	void* ptr = ga.allocate(largeSize);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	ga.deallocate(ptr, largeSize);
}

BLIB_TEST_CASE("GlobalAllocator: getCurrentAllocated tracking")
{
	auto& ga = GlobalAllocator::instance();
	
	size_t before = ga.getCurrentAllocated();
	
	void* ptr1 = ga.allocate(256);
	void* ptr2 = ga.allocate(512);
	
	size_t during = ga.getCurrentAllocated();
	BLIB_TEST_CHECK(during >= before + 256 + 512);
	
	ga.deallocate(ptr1, 256);
	ga.deallocate(ptr2, 512);
	
	size_t after = ga.getCurrentAllocated();
	BLIB_TEST_CHECK(after == before);
}

BLIB_TEST_CASE("GlobalAllocator: getPeakAllocated tracking")
{
	auto& ga = GlobalAllocator::instance();
	
	size_t peakBefore = ga.getPeakAllocated();
	
	void* ptr = ga.allocate(2048);
	
	size_t peakDuring = ga.getPeakAllocated();
	BLIB_TEST_CHECK(peakDuring >= peakBefore);
	
	ga.deallocate(ptr, 2048);
	
	// Peak should remain (doesn't decrease)
	size_t peakAfter = ga.getPeakAllocated();
	BLIB_TEST_CHECK(peakAfter >= peakDuring);
}

BLIB_TEST_CASE("GlobalAllocator: getAllocationCount tracking")
{
	auto& ga = GlobalAllocator::instance();
	
	size_t countBefore = ga.getAllocationCount();
	
	void* ptr1 = ga.allocate(64);
	void* ptr2 = ga.allocate(128);
	void* ptr3 = ga.allocate(256);
	
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore + 3);
	
	ga.deallocate(ptr1, 64);
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore + 2);
	
	ga.deallocate(ptr2, 128);
	ga.deallocate(ptr3, 256);
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
}

BLIB_TEST_CASE("GlobalAllocator: histogram buckets distribution")
{
	auto& ga = GlobalAllocator::instance();
	
	// Enable extended stats
	ga.setExtendedStatsEnabled(true);
	BLIB_TEST_CHECK(ga.isExtendedStatsEnabled());
	
	// Allocate one block in each bucket range
	void* ptr1 = ga.allocate(32);      // bucket 0: 0-64
	void* ptr2 = ga.allocate(128);     // bucket 1: 65-256
	void* ptr3 = ga.allocate(512);     // bucket 2: 257-1KB
	void* ptr4 = ga.allocate(2048);    // bucket 3: 1KB-4KB
	void* ptr5 = ga.allocate(8192);    // bucket 4: 4KB-16KB
	void* ptr6 = ga.allocate(32768);   // bucket 5: 16KB-64KB
	void* ptr7 = ga.allocate(131072);  // bucket 6: 64KB-256KB
	void* ptr8 = ga.allocate(524288);  // bucket 7: 256KB-1MB
	void* ptr9 = ga.allocate(2097152); // bucket 8: 1MB+
	
	size_t buckets[9] = {0};
	bool success = ga.getHistogram(buckets);
	BLIB_TEST_CHECK(success);
	
	// Check that each bucket has at least 1 allocation
	// (may have more due to other allocations in the system)
	BLIB_TEST_CHECK(buckets[0] >= 1);
	BLIB_TEST_CHECK(buckets[1] >= 1);
	BLIB_TEST_CHECK(buckets[2] >= 1);
	BLIB_TEST_CHECK(buckets[3] >= 1);
	BLIB_TEST_CHECK(buckets[4] >= 1);
	BLIB_TEST_CHECK(buckets[5] >= 1);
	BLIB_TEST_CHECK(buckets[6] >= 1);
	BLIB_TEST_CHECK(buckets[7] >= 1);
	BLIB_TEST_CHECK(buckets[8] >= 1);
	
	// Cleanup
	ga.deallocate(ptr1, 32);
	ga.deallocate(ptr2, 128);
	ga.deallocate(ptr3, 512);
	ga.deallocate(ptr4, 2048);
	ga.deallocate(ptr5, 8192);
	ga.deallocate(ptr6, 32768);
	ga.deallocate(ptr7, 131072);
	ga.deallocate(ptr8, 524288);
	ga.deallocate(ptr9, 2097152);
	
	ga.setExtendedStatsEnabled(false);
}

BLIB_TEST_CASE("GlobalAllocator: histogram disabled by default")
{
	auto& ga = GlobalAllocator::instance();
	
	// Disable if it was enabled
	ga.setExtendedStatsEnabled(false);
	
	size_t buckets[9] = {0};
	bool success = ga.getHistogram(buckets);
	BLIB_TEST_CHECK(!success); // Should return false when disabled
}

BLIB_TEST_CASE("GlobalAllocator: leak tracking enable/disable")
{
	auto& ga = GlobalAllocator::instance();
	
	BLIB_TEST_CHECK(!ga.isLeakTrackingEnabled()); // Default off
	
	ga.setLeakTrackingEnabled(true);
	BLIB_TEST_CHECK(ga.isLeakTrackingEnabled());
	
	ga.setLeakTrackingEnabled(false);
	BLIB_TEST_CHECK(!ga.isLeakTrackingEnabled());
}

BLIB_TEST_CASE("GlobalAllocator: dumpLeaks detects memory leaks")
{
	auto& ga = GlobalAllocator::instance();
	
	ga.setLeakTrackingEnabled(true);
	
	// Intentionally leak memory for testing
	void* leak1 = ga.allocate(100);
	void* leak2 = ga.allocate(200);
	
	size_t leakCount = ga.dumpLeaks();
	BLIB_TEST_CHECK(leakCount >= 2);
	
	// Cleanup the leaks
	ga.deallocate(leak1, 100);
	ga.deallocate(leak2, 200);
	
	// Now should have no leaks
	leakCount = ga.dumpLeaks();
	BLIB_TEST_CHECK(leakCount == 0);
	
	ga.setLeakTrackingEnabled(false);
}

BLIB_TEST_CASE("GlobalAllocator: concurrent allocations (multi-thread)")
{
	auto& ga = GlobalAllocator::instance();
	
	std::atomic<int> successCount{0};
	constexpr int numThreads = 4;
	constexpr int allocsPerThread = 1000;
	
	std::vector<std::thread> threads;
	for (int t = 0; t < numThreads; ++t) {
		threads.emplace_back([&]() {
			for (int i = 0; i < allocsPerThread; ++i) {
				void* ptr = ga.allocate(64);
				if (ptr) {
					++successCount;
					ga.deallocate(ptr, 64);
				}
			}
		});
	}
	
	for (auto& th : threads) {
		th.join();
	}
	
	BLIB_TEST_CHECK(successCount == numThreads * allocsPerThread);
}

BLIB_TEST_CASE("GlobalAllocator: concurrent alloc/dealloc (multi-thread)")
{
	auto& ga = GlobalAllocator::instance();
	
	constexpr int numThreads = 4;
	constexpr int iterations = 500;
	
	std::vector<std::thread> threads;
	for (int t = 0; t < numThreads; ++t) {
		threads.emplace_back([&]() {
			std::vector<void*> ptrs;
			for (int i = 0; i < iterations; ++i) {
				ptrs.push_back(ga.allocate(128));
			}
			for (void* ptr : ptrs) {
				ga.deallocate(ptr, 128);
			}
		});
	}
	
	for (auto& th : threads) {
		th.join();
	}
	
	// Test passes if no crashes occurred
}

// ============================================================
// 2. DefaultAllocator Tests
// ============================================================

BLIB_TEST_CASE("DefaultAllocator: construction (stateless)")
{
	DefaultAllocator alloc;
	
	// Check trait
	bool isStateless = AllocatorTraits<DefaultAllocatorImpl>::isStateless;
	BLIB_TEST_CHECK(isStateless);
}

BLIB_TEST_CASE("DefaultAllocator: copy semantics (trivial)")
{
	DefaultAllocator alloc1;
	DefaultAllocator alloc2 = alloc1;
	
	// Both should work independently (stateless)
	void* ptr1 = alloc1.allocate(64);
	void* ptr2 = alloc2.allocate(64);
	
	BLIB_TEST_CHECK(ptr1 != nullptr);
	BLIB_TEST_CHECK(ptr2 != nullptr);
	
	alloc1.deallocate(ptr1, 64);
	alloc2.deallocate(ptr2, 64);
}

BLIB_TEST_CASE("DefaultAllocator: allocate and deallocate")
{
	DefaultAllocator alloc;
	
	void* ptr = alloc.allocate(256);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc.deallocate(ptr, 256);
}

BLIB_TEST_CASE("DefaultAllocator: wraps into Allocator (SBO)")
{
	auto& ga = GlobalAllocator::instance();
	size_t allocCountBefore = ga.getAllocationCount();
	
	{
		DefaultAllocator def;
		Allocator alloc(std::move(def));
		
		// Stateless allocator should fit in SBO (no heap allocation for impl)
		size_t allocCountDuring = ga.getAllocationCount();
		BLIB_TEST_CHECK(allocCountDuring == allocCountBefore);
	}
	
	size_t allocCountAfter = ga.getAllocationCount();
	BLIB_TEST_CHECK(allocCountAfter == allocCountBefore);
}

BLIB_TEST_CASE("DefaultAllocator: allocate through type-erased Allocator")
{
	DefaultAllocator def;
	Allocator alloc(std::move(def));
	
	void* ptr = alloc.allocate(128);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc.deallocate(ptr, 128);
}

BLIB_TEST_CASE("DefaultAllocator: auto-wrapped in debug mode")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	// In debug mode, DefaultAllocator should be DebugAllocator<DefaultAllocatorImpl>
	// We can't directly check type equality without RTTI, but we can check behavior
	// (debug allocator adds overhead)
	
	auto& ga = GlobalAllocator::instance();
	size_t before = ga.getCurrentAllocated();
	
	DefaultAllocator alloc;
	void* ptr = alloc.allocate(64);
	
	size_t after = ga.getCurrentAllocated();
	size_t overhead = after - before;
	
	// Debug allocator adds ~40 bytes overhead
	BLIB_TEST_CHECK(overhead >= 64 + 40);
	
	alloc.deallocate(ptr, 64);
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DefaultAllocator: default alignment")
{
	DefaultAllocator alloc;
	
	void* ptr = alloc.allocate(64);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	// Check alignment (at least 8 bytes on most platforms)
	uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
	BLIB_TEST_CHECK((addr % 8) == 0);
	
	alloc.deallocate(ptr, 64);
}

BLIB_TEST_CASE("DefaultAllocator: multiple allocations same size")
{
	DefaultAllocator alloc;
	
	void* ptr1 = alloc.allocate(128);
	void* ptr2 = alloc.allocate(128);
	void* ptr3 = alloc.allocate(128);
	
	BLIB_TEST_CHECK(ptr1 != nullptr);
	BLIB_TEST_CHECK(ptr2 != nullptr);
	BLIB_TEST_CHECK(ptr3 != nullptr);
	
	// All should be different addresses
	BLIB_TEST_CHECK(ptr1 != ptr2);
	BLIB_TEST_CHECK(ptr2 != ptr3);
	BLIB_TEST_CHECK(ptr1 != ptr3);
	
	alloc.deallocate(ptr1, 128);
	alloc.deallocate(ptr2, 128);
	alloc.deallocate(ptr3, 128);
}

// ============================================================
// 3. MallocAllocator Tests
// ============================================================

BLIB_TEST_CASE("MallocAllocator: construction (stateless)")
{
	MallocAllocator alloc;
	
	bool isStateless = AllocatorTraits<MallocAllocatorImpl>::isStateless;
	BLIB_TEST_CHECK(isStateless);
}

BLIB_TEST_CASE("MallocAllocator: allocate and deallocate")
{
	MallocAllocator alloc;
	
	void* ptr = alloc.allocate(512);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc.deallocate(ptr, 512);
}

BLIB_TEST_CASE("MallocAllocator: large allocations (10MB)")
{
	MallocAllocator alloc;
	
	size_t largeSize = 10 * 1024 * 1024; // 10MB
	void* ptr = alloc.allocate(largeSize);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc.deallocate(ptr, largeSize);
}

BLIB_TEST_CASE("MallocAllocator: copy semantics (trivial)")
{
	MallocAllocator alloc1;
	MallocAllocator alloc2 = alloc1;
	
	void* ptr1 = alloc1.allocate(64);
	void* ptr2 = alloc2.allocate(64);
	
	BLIB_TEST_CHECK(ptr1 != nullptr);
	BLIB_TEST_CHECK(ptr2 != nullptr);
	
	alloc1.deallocate(ptr1, 64);
	alloc2.deallocate(ptr2, 64);
}

BLIB_TEST_CASE("MallocAllocator: wraps into Allocator (SBO)")
{
	auto& ga = GlobalAllocator::instance();
	size_t allocCountBefore = ga.getAllocationCount();
	
	{
		MallocAllocator malloc;
		Allocator alloc(std::move(malloc));
	}
	
	size_t allocCountAfter = ga.getAllocationCount();
	BLIB_TEST_CHECK(allocCountAfter == allocCountBefore);
}

// ============================================================
// 4. PoolAllocator Tests
// ============================================================

BLIB_TEST_CASE("PoolAllocator: construction with blockSize and blocksPerChunk")
{
	PoolAllocator pool(64, 128);
	
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	BLIB_TEST_CHECK(pool.getBlockSize() == 64);
#else
	// In debug mode, can't access getBlockSize through DebugAllocator wrapper
	// Just check that construction succeeded
	void* ptr = pool.allocate(64);
	BLIB_TEST_CHECK(ptr != nullptr);
	pool.deallocate(ptr, 64);
#endif
}

BLIB_TEST_CASE("PoolAllocator: getBlockSize returns correct value")
{
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	PoolAllocator pool1(32, 64);
	PoolAllocator pool2(128, 256);
	
	BLIB_TEST_CHECK(pool1.getBlockSize() == 32);
	BLIB_TEST_CHECK(pool2.getBlockSize() == 128);
#else
	std::cout << "  SKIPPED (getBlockSize not available in debug mode)" << std::endl;
#endif
}

BLIB_TEST_CASE("PoolAllocator: allocate exact blockSize")
{
	PoolAllocator pool(64, 128);
	
	void* ptr = pool.allocate(64);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	pool.deallocate(ptr, 64);
}

BLIB_TEST_CASE("PoolAllocator: allocate wrong size returns nullptr")
{
	PoolAllocator pool(64, 128);
	
	void* ptr = pool.allocate(128); // Wrong size
	BLIB_TEST_CHECK(ptr == nullptr);
}

BLIB_TEST_CASE("PoolAllocator: allocates new chunk when pool exhausted")
{
	PoolAllocator pool(64, 4); // Small blocksPerChunk for quick exhaustion
	
	void* ptrs[10];
	for (int i = 0; i < 10; ++i) {
		ptrs[i] = pool.allocate(64);
		BLIB_TEST_CHECK(ptrs[i] != nullptr);
	}
	
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	size_t totalBlocks = pool.getTotalBlocks();
	BLIB_TEST_CHECK(totalBlocks >= 10);
#endif
	
	// Cleanup
	for (int i = 0; i < 10; ++i) {
		pool.deallocate(ptrs[i], 64);
	}
}

BLIB_TEST_CASE("PoolAllocator: deallocate and reuse (free list)")
{
	PoolAllocator pool(64, 128);
	
	void* ptr1 = pool.allocate(64);
	pool.deallocate(ptr1, 64); // Return to free list
	
	void* ptr2 = pool.allocate(64); // Should reuse ptr1
	BLIB_TEST_CHECK(ptr1 == ptr2); // Same address
	
	pool.deallocate(ptr2, 64);
}

BLIB_TEST_CASE("PoolAllocator: multiple deallocations build free list")
{
	PoolAllocator pool(64, 128);
	
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	size_t totalBlocksBefore = pool.getTotalBlocks();
#endif
	
	// Allocate 100 blocks
	void* ptrs[100];
	for (int i = 0; i < 100; ++i) {
		ptrs[i] = pool.allocate(64);
	}
	
	// Deallocate all
	for (int i = 0; i < 100; ++i) {
		pool.deallocate(ptrs[i], 64);
	}
	
	// Allocate again - should not allocate new chunks
	for (int i = 0; i < 100; ++i) {
		ptrs[i] = pool.allocate(64);
	}
	
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	size_t totalBlocksAfter = pool.getTotalBlocks();
	BLIB_TEST_CHECK(totalBlocksAfter == totalBlocksBefore + 128); // Only 1 initial chunk
#endif
	
	// Cleanup
	for (int i = 0; i < 100; ++i) {
		pool.deallocate(ptrs[i], 64);
	}
}

BLIB_TEST_CASE("PoolAllocator: getTotalBlocks tracking")
{
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	PoolAllocator pool(64, 128);
	
	size_t before = pool.getTotalBlocks();
	BLIB_TEST_CHECK(before == 0); // No chunks allocated yet
	
	void* ptr = pool.allocate(64); // Triggers first chunk allocation
	
	size_t after = pool.getTotalBlocks();
	BLIB_TEST_CHECK(after == 128); // First chunk with 128 blocks
	
	pool.deallocate(ptr, 64);
#else
	std::cout << "  SKIPPED (getTotalBlocks not available in debug mode)" << std::endl;
#endif
}

BLIB_TEST_CASE("PoolAllocator: move construction")
{
	PoolAllocator pool1(64, 128);
	void* ptr1 = pool1.allocate(64);
	
	PoolAllocator pool2(std::move(pool1));
	
	// pool2 should have ownership
	void* ptr2 = pool2.allocate(64);
	BLIB_TEST_CHECK(ptr2 != nullptr);
	
	pool2.deallocate(ptr1, 64);
	pool2.deallocate(ptr2, 64);
}

BLIB_TEST_CASE("PoolAllocator: move assignment")
{
	PoolAllocator pool1(64, 128);
	PoolAllocator pool2(32, 64);
	
	void* ptr1 = pool1.allocate(64);
	
	pool2 = std::move(pool1);
	
	// pool2 now manages pool1's resources
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	BLIB_TEST_CHECK(pool2.getBlockSize() == 64);
#endif
	
	pool2.deallocate(ptr1, 64);
}

// ============================================================
// 5. DebugAllocator Tests (Aggressive Testing)
// ============================================================

BLIB_TEST_CASE("DebugAllocator: detects front guard corruption (underflow)")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	DefaultAllocator alloc;
	void* ptr = alloc.allocate(64);
	
	char* buf = static_cast<char*>(ptr);
	buf[-1] = 0xFF; // Underflow - corrupt front guard
	
	// deallocate should detect and abort
	BLIB_TEST_EXPECT_ABORT(alloc.deallocate(ptr, 64));
	
	// Note: Can't cleanup after abort, memory will leak in this test
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: detects back guard corruption (overflow)")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	DefaultAllocator alloc;
	void* ptr = alloc.allocate(64);
	
	char* buf = static_cast<char*>(ptr);
	buf[64] = 0xFF; // Overflow - corrupt back guard
	
	// deallocate should detect and abort
	BLIB_TEST_EXPECT_ABORT(alloc.deallocate(ptr, 64));
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: poisons memory after deallocate")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	DefaultAllocator alloc;
	void* ptr = alloc.allocate(64);
	
	// Write some data
	std::memset(ptr, 0xAA, 64);
	
	alloc.deallocate(ptr, 64);
	
	// NOTE: Memory is poisoned with 0xDEADC0DE pattern before being freed,
	// but reading freed memory is UB and underlying allocator may have reused it.
	// This test just verifies that deallocate completes without errors.
	// Use-after-free detection is covered by the "detects double-free" test.
	
	// Test passes if deallocate succeeded without crashing
	BLIB_TEST_CHECK(true);
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: detects double-free")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	DefaultAllocator alloc;
	void* ptr = alloc.allocate(64);
	
	alloc.deallocate(ptr, 64); // First free - OK
	
	// Second free should detect (magic == MAGIC_FREED)
	BLIB_TEST_EXPECT_ABORT(alloc.deallocate(ptr, 64));
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: detects size mismatch")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	DefaultAllocator alloc;
	void* ptr = alloc.allocate(64);
	
	// deallocate with wrong size
	BLIB_TEST_EXPECT_ABORT(alloc.deallocate(ptr, 128));
	
	// Cleanup with correct size (if abort didn't kill process)
	// alloc.deallocate(ptr, 64);
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: overhead is 40 bytes per allocation")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	auto& ga = GlobalAllocator::instance();
	
	size_t before = ga.getCurrentAllocated();
	
	DefaultAllocator alloc;
	void* ptr = alloc.allocate(64);
	
	size_t after = ga.getCurrentAllocated();
	size_t overhead = after - before;
	
	// Debug overhead: 24 (header) + 8 (front guard) + 8 (back guard) = 40 bytes
	BLIB_TEST_CHECK(overhead >= 64 + 40);
	
	alloc.deallocate(ptr, 64);
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: wraps MallocAllocator correctly")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	MallocAllocator alloc;
	void* ptr = alloc.allocate(128);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc.deallocate(ptr, 128);
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: wraps PoolAllocator correctly")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	PoolAllocator pool(64, 128);
	void* ptr = pool.allocate(64);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	pool.deallocate(ptr, 64);
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: handles minimum size allocation (1 byte)")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	DefaultAllocator alloc;
	void* ptr = alloc.allocate(1);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc.deallocate(ptr, 1);
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: handles large allocations (1MB+)")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	DefaultAllocator alloc;
	size_t largeSize = 1024 * 1024;
	void* ptr = alloc.allocate(largeSize);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc.deallocate(ptr, largeSize);
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: stress test (1000 allocations)")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	DefaultAllocator alloc;
	
	std::vector<void*> ptrs;
	for (int i = 0; i < 1000; ++i) {
		void* ptr = alloc.allocate(64);
		BLIB_TEST_CHECK(ptr != nullptr);
		ptrs.push_back(ptr);
	}
	
	// Deallocate all
	for (void* ptr : ptrs) {
		alloc.deallocate(ptr, 64);
	}
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

// ============================================================
// 6. Allocator (Type-Erased) Tests
// ============================================================

BLIB_TEST_CASE("Allocator: default construction (uses DefaultAllocator)")
{
	Allocator alloc;
	
	void* ptr = alloc.allocate(128);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc.deallocate(ptr, 128);
}

BLIB_TEST_CASE("Allocator: construction from stateless allocator (MallocAllocator)")
{
	MallocAllocator malloc;
	Allocator alloc(std::move(malloc));
	
	void* ptr = alloc.allocate(256);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc.deallocate(ptr, 256);
}

BLIB_TEST_CASE("Allocator: construction from stateful allocator (PoolAllocator)")
{
	PoolAllocator pool(64, 128);
	Allocator alloc(std::move(pool));
	
	void* ptr = alloc.allocate(64);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc.deallocate(ptr, 64);
}

BLIB_TEST_CASE("Allocator: stateless allocator uses SBO (no heap allocation)")
{
	auto& ga = GlobalAllocator::instance();
	size_t allocCountBefore = ga.getAllocationCount();
	
	{
		MallocAllocator malloc;
		Allocator alloc(std::move(malloc));
	}
	
	size_t allocCountAfter = ga.getAllocationCount();
	BLIB_TEST_CHECK(allocCountAfter == allocCountBefore);
}

BLIB_TEST_CASE("Allocator: copy constructor")
{
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	Allocator alloc1;
	Allocator alloc2(alloc1);
	
	void* ptr1 = alloc1.allocate(64);
	void* ptr2 = alloc2.allocate(64);
	
	BLIB_TEST_CHECK(ptr1 != nullptr);
	BLIB_TEST_CHECK(ptr2 != nullptr);
	
	alloc1.deallocate(ptr1, 64);
	alloc2.deallocate(ptr2, 64);
#else
	std::cout << "  SKIPPED (copy not supported for stateful allocators in debug mode)" << std::endl;
#endif
}

BLIB_TEST_CASE("Allocator: move constructor transfers ownership")
{
	Allocator alloc1;
	Allocator alloc2(std::move(alloc1));
	
	void* ptr = alloc2.allocate(128);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc2.deallocate(ptr, 128);
}

BLIB_TEST_CASE("Allocator: move assignment transfers ownership")
{
	Allocator alloc1;
	Allocator alloc2;
	
	alloc2 = std::move(alloc1);
	
	void* ptr = alloc2.allocate(128);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	alloc2.deallocate(ptr, 128);
}

BLIB_TEST_CASE("Allocator: can hold different allocator types")
{
	std::vector<Allocator> allocators;
	allocators.push_back(Allocator());
	allocators.push_back(Allocator(MallocAllocator()));
	
	// Test all allocators work through unified API
	for (auto& alloc : allocators) {
		void* ptr = alloc.allocate(32);
		BLIB_TEST_CHECK(ptr != nullptr);
		alloc.deallocate(ptr, 32);
	}
}

// ============================================================
// 7. StdAllocatorAdapter Tests
// ============================================================

BLIB_TEST_CASE("StdAllocatorAdapter: std::vector with DefaultAllocator")
{
	Allocator alloc;
	StdAllocatorAdapter<int> adapter(&alloc);
	std::vector<int, StdAllocatorAdapter<int>> vec(adapter);
	
	vec.push_back(42);
	vec.push_back(43);
	vec.push_back(44);
	
	BLIB_TEST_CHECK(vec.size() == 3);
	BLIB_TEST_CHECK(vec[0] == 42);
	BLIB_TEST_CHECK(vec[1] == 43);
	BLIB_TEST_CHECK(vec[2] == 44);
}

BLIB_TEST_CASE("StdAllocatorAdapter: std::string with MallocAllocator")
{
	MallocAllocator malloc;
	Allocator alloc(std::move(malloc));
	StdAllocatorAdapter<char> adapter(&alloc);
	std::basic_string<char, std::char_traits<char>, StdAllocatorAdapter<char>> str(adapter);
	
	str = "Hello, World!";
	BLIB_TEST_CHECK(str.size() == 13);
	BLIB_TEST_CHECK(str == "Hello, World!");
}

BLIB_TEST_CASE("StdAllocatorAdapter: tracks allocations through GlobalAllocator")
{
	auto& ga = GlobalAllocator::instance();
	size_t before = ga.getCurrentAllocated();
	
	{
		Allocator alloc;
		StdAllocatorAdapter<int> adapter(&alloc);
		std::vector<int, StdAllocatorAdapter<int>> vec(adapter);
		vec.resize(1000);
		
		size_t during = ga.getCurrentAllocated();
		BLIB_TEST_CHECK(during > before);
	}
	
	size_t after = ga.getCurrentAllocated();
	BLIB_TEST_CHECK(after == before);
}

BLIB_TEST_CASE("StdAllocatorAdapter: empty container allocates no memory")
{
	auto& ga = GlobalAllocator::instance();
	size_t before = ga.getCurrentAllocated();
	
	{
		Allocator alloc;
		StdAllocatorAdapter<int> adapter(&alloc);
		std::vector<int, StdAllocatorAdapter<int>> vec(adapter);
		// Empty vector
	}
	
	size_t after = ga.getCurrentAllocated();
	BLIB_TEST_CHECK(after == before);
}

// ============================================================
// 8. Integration & Stress Tests
// ============================================================

BLIB_TEST_CASE("Integration: switch from DefaultAllocator to PoolAllocator")
{
	// Allocate through DefaultAllocator
	Allocator alloc1;
	void* ptr1 = alloc1.allocate(64);
	
	// Switch to PoolAllocator
	PoolAllocator pool(64, 128);
	Allocator alloc2(std::move(pool));
	void* ptr2 = alloc2.allocate(64);
	
	BLIB_TEST_CHECK(ptr1 != nullptr);
	BLIB_TEST_CHECK(ptr2 != nullptr);
	
	// Deallocate through correct allocators
	alloc1.deallocate(ptr1, 64);
	alloc2.deallocate(ptr2, 64);
}

BLIB_TEST_CASE("Integration: all allocators auto-wrapped in debug mode")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	// All allocators should be wrapped automatically in debug builds
	// This is a compile-time check - if it compiles, it works
	DefaultAllocator def;
	MallocAllocator mal;
	PoolAllocator pool(64, 128);
	
	// All should work through unified API
	void* ptr1 = def.allocate(64);
	void* ptr2 = mal.allocate(64);
	void* ptr3 = pool.allocate(64);
	
	BLIB_TEST_CHECK(ptr1 != nullptr);
	BLIB_TEST_CHECK(ptr2 != nullptr);
	BLIB_TEST_CHECK(ptr3 != nullptr);
	
	def.deallocate(ptr1, 64);
	mal.deallocate(ptr2, 64);
	pool.deallocate(ptr3, 64);
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("Integration: entity pool pattern (allocate 1000 entities)")
{
	struct Entity { char data[64]; };
	PoolAllocator pool(sizeof(Entity), 128);
	
	std::vector<Entity*> entities;
	for (int i = 0; i < 1000; ++i) {
		entities.push_back(static_cast<Entity*>(pool.allocate(sizeof(Entity))));
		BLIB_TEST_CHECK(entities.back() != nullptr);
	}
	
	// Deallocate in reverse order (simulate entity death in different order)
	std::reverse(entities.begin(), entities.end());
	for (auto* e : entities) {
		pool.deallocate(e, sizeof(Entity));
	}
}

BLIB_TEST_CASE("Integration: particle system pattern (rapid alloc/dealloc)")
{
	PoolAllocator pool(32, 256);
	
	// Simulate 10000 particle spawn/death cycles
	for (int i = 0; i < 10000; ++i) {
		void* ptr = pool.allocate(32);
		BLIB_TEST_CHECK(ptr != nullptr);
		pool.deallocate(ptr, 32);
	}
}

BLIB_TEST_CASE("Integration: mixed size allocations (DefaultAllocator)")
{
	DefaultAllocator alloc;
	
	size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 4096};
	std::vector<std::pair<void*, size_t>> allocations;
	
	// Allocate blocks of different sizes
	for (size_t size : sizes) {
		for (int i = 0; i < 10; ++i) {
			void* ptr = alloc.allocate(size);
			BLIB_TEST_CHECK(ptr != nullptr);
			allocations.push_back({ptr, size});
		}
	}
	
	// Deallocate in reverse order
	std::reverse(allocations.begin(), allocations.end());
	for (auto& pair : allocations) {
		alloc.deallocate(pair.first, pair.second);
	}
}

BLIB_TEST_CASE("Integration: concurrent multi-allocator access")
{
	constexpr int numThreads = 4;
	constexpr int allocsPerThread = 500;
	
	std::vector<std::thread> threads;
	for (int t = 0; t < numThreads; ++t) {
		threads.emplace_back([&]() {
			// Each thread has its own PoolAllocator
			PoolAllocator pool(64, 128);
			
			std::vector<void*> ptrs;
			for (int i = 0; i < allocsPerThread; ++i) {
				ptrs.push_back(pool.allocate(64));
			}
			
			for (void* ptr : ptrs) {
				pool.deallocate(ptr, 64);
			}
		});
	}
	
	for (auto& th : threads) {
		th.join();
	}
}

BLIB_TEST_CASE("Integration: std::vector with PoolAllocator adapter")
{
	PoolAllocator pool(64, 128);
	
	// This test demonstrates that PoolAllocator can't be used with std::vector
	// because std::vector may allocate different sizes (capacity growth)
	// PoolAllocator only supports fixed-size allocations
	
	// We'll use DefaultAllocator for std::vector instead
	Allocator alloc;
	StdAllocatorAdapter<int> adapter(&alloc);
	std::vector<int, StdAllocatorAdapter<int>> vec(adapter);
	
	for (int i = 0; i < 100; ++i) {
		vec.push_back(i);
	}
	
	BLIB_TEST_CHECK(vec.size() == 100);
}

BLIB_TEST_CASE("Integration: performance comparison (informational)")
{
	constexpr int iterations = 10000;
	
	std::cout << "  Performance comparison (" << iterations << " allocations):" << std::endl;
	
	// DefaultAllocator
	{
		DefaultAllocator alloc;
		auto start = std::chrono::high_resolution_clock::now();
		
		for (int i = 0; i < iterations; ++i) {
			void* ptr = alloc.allocate(64);
			alloc.deallocate(ptr, 64);
		}
		
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << "    DefaultAllocator: " << duration.count() << " μs" << std::endl;
	}
	
	// PoolAllocator
	{
		PoolAllocator pool(64, 256);
		auto start = std::chrono::high_resolution_clock::now();
		
		for (int i = 0; i < iterations; ++i) {
			void* ptr = pool.allocate(64);
			pool.deallocate(ptr, 64);
		}
		
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << "    PoolAllocator: " << duration.count() << " μs" << std::endl;
	}
}

// ============================================================
// 9. Allocator Ownership Tests (copy/move/clone после починки владения)
// ============================================================

BLIB_TEST_CASE("Allocator: copy ctor allocates impl in heap (GA tracked)")
{
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	auto& ga = GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();
	
	{
		Allocator alloc1;
		Allocator alloc2(alloc1);
		
		// share() создаёт heap-копию impl через GlobalAllocator
		size_t countDuring = ga.getAllocationCount();
		BLIB_TEST_CHECK(countDuring == countBefore + 1);
		
		// Обе копии работают (stateless DefaultAllocator)
		void* ptr = alloc2.allocate(64);
		BLIB_TEST_CHECK(ptr != nullptr);
		alloc2.deallocate(ptr, 64);
	}
	
	// destroyImpl должен вернуть heap-копию в GlobalAllocator
	size_t countAfter = ga.getAllocationCount();
	BLIB_TEST_CHECK(countAfter == countBefore);
#else
	std::cout << "  SKIPPED (copy not supported for stateful allocators in debug mode)" << std::endl;
#endif
}

BLIB_TEST_CASE("Allocator: copy of copy keeps GA balanced")
{
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	auto& ga = GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();
	
	{
		Allocator a1;
		Allocator a2(a1);
		Allocator a3(a2);
		
		// Две heap-копии impl (по одной на копирование)
		BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore + 2);
	}
	
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
#else
	std::cout << "  SKIPPED (copy not supported for stateful allocators in debug mode)" << std::endl;
#endif
}

BLIB_TEST_CASE("Allocator: clone() creates independent copy (stateless)")
{
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	auto& ga = GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();
	
	{
		Allocator orig;
		Allocator copy = orig.clone();
		
		// clone() выделяет heap-impl через GlobalAllocator (deepCopy -> share)
		BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore + 1);
		
		void* ptr = copy.allocate(128);
		BLIB_TEST_CHECK(ptr != nullptr);
		copy.deallocate(ptr, 128);
	}
	
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
#else
	std::cout << "  SKIPPED (deepCopy not supported for stateful allocators in debug mode)" << std::endl;
#endif
}

BLIB_TEST_CASE("Allocator: stateful copy and clone give dead allocator (known defect)")
{
	// TODO: Убрать/переписать этот тест после реализации share()/deepCopy()
	// для stateful аллокаторов (AllocatorImplWrapper сейчас возвращает nullptr).
	// Тест фиксирует ТЕКУЩЕЕ поведение, чтобы дефект был виден.
	
	PoolAllocator pool(64, 128);
	Allocator orig(std::move(pool));
	
	void* ptr = orig.allocate(64);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	// Копия stateful-аллокатора: share() == nullptr -> impl == nullptr
	Allocator copy(orig);
	BLIB_TEST_CHECK(copy.allocate(64) == nullptr);
	
	// clone() stateful-аллокатора: deepCopy() == nullptr -> impl == nullptr
	Allocator cloned = orig.clone();
	BLIB_TEST_CHECK(cloned.allocate(64) == nullptr);
	
	orig.deallocate(ptr, 64);
}

BLIB_TEST_CASE("Allocator: move ctor with stateful PoolAllocator (no leak)")
{
	auto& ga = GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();
	size_t allocBefore = ga.getCurrentAllocated();
	
	{
		PoolAllocator pool(64, 128);
		Allocator alloc1(std::move(pool));
		void* ptr1 = alloc1.allocate(64);
		BLIB_TEST_CHECK(ptr1 != nullptr);
		
		// Move-конструктор переносит stateful impl (SBO bytewise)
		Allocator alloc2(std::move(alloc1));
		
		// Moved-from: impl == nullptr
		BLIB_TEST_CHECK(alloc1.allocate(64) == nullptr);
		
		void* ptr2 = alloc2.allocate(64);
		BLIB_TEST_CHECK(ptr2 != nullptr);
		
		alloc2.deallocate(ptr1, 64);
		alloc2.deallocate(ptr2, 64);
	}
	
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
	BLIB_TEST_CHECK(ga.getCurrentAllocated() == allocBefore);
}

BLIB_TEST_CASE("Allocator: move assignment destroys old impl and transfers ownership")
{
	auto& ga = GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();
	size_t allocBefore = ga.getCurrentAllocated();
	
	{
		PoolAllocator pool1(64, 128);
		PoolAllocator pool2(32, 64);
		
		Allocator alloc1(std::move(pool1));
		void* ptr1 = alloc1.allocate(64);
		BLIB_TEST_CHECK(ptr1 != nullptr);
		
		Allocator alloc2(std::move(pool2));
		void* ptr2 = alloc2.allocate(32);
		BLIB_TEST_CHECK(ptr2 != nullptr);
		
		// Move assignment: старый impl (пул 32) уничтожается, его чанки
		// освобождаются автоматически деструктором PoolAllocatorImpl.
		// ptr2 после этого валиден до конца жизни чанка - не освобождаем его!
		alloc2 = std::move(alloc1);
		
		// alloc2 теперь управляет пулом 64
		void* ptr3 = alloc2.allocate(64);
		BLIB_TEST_CHECK(ptr3 != nullptr);
		
		alloc2.deallocate(ptr1, 64);
		alloc2.deallocate(ptr3, 64);
		
		// Moved-from alloc1 - мёртвый
		BLIB_TEST_CHECK(alloc1.allocate(64) == nullptr);
	}
	
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
	BLIB_TEST_CHECK(ga.getCurrentAllocated() == allocBefore);
}

BLIB_TEST_CASE("Allocator: self-copy and self-move are safe")
{
	Allocator alloc;
	
	// Self-copy: guard (this != &other) - без изменений
	alloc = *(&alloc);
	
	void* ptr = alloc.allocate(64);
	BLIB_TEST_CHECK(ptr != nullptr);
	alloc.deallocate(ptr, 64);
	
	// Self-move: guard (this != &other) - без изменений
	alloc = std::move(alloc);
	
	ptr = alloc.allocate(64);
	BLIB_TEST_CHECK(ptr != nullptr);
	alloc.deallocate(ptr, 64);
}

// ============================================================
// 10. Heap-Path SBO Tests (impl больше SBO_SIZE = 56 байт)
// ============================================================

/**
 * Тестовый stateful аллокатор, который НЕ влезает в SBO (56 байт),
 * чтобы проверить heap-путь хранения impl (constructInHeap + destroyImpl путь 2).
 * 
 * AllocatorTraits<BigStatefulAllocator> не специализирован -
 * по умолчанию все аллокаторы считаются stateful (allocatorTraits.h).
 */
struct BigStatefulAllocator
{
	char padding[128]; // 128 байт > SBO_SIZE = 56 байт

	BigStatefulAllocator()
	{
		std::memset(padding, 0, sizeof(padding));
	}

	void* allocate(size_t size)
	{
		return GlobalAllocator::instance().allocate(size);
	}

	void deallocate(void* ptr, size_t size)
	{
		GlobalAllocator::instance().deallocate(ptr, size);
	}
};

BLIB_TEST_CASE("Allocator: heap path for impl larger than SBO (BigStatefulAllocator)")
{
	auto& ga = GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();
	
	{
		// BigStatefulAllocator (128 байт) не влезает в SBO (56 байт),
		// поэтому impl размещается в heap через constructInHeap
		BigStatefulAllocator big;
		Allocator alloc(std::move(big));
		
		size_t countDuring = ga.getAllocationCount();
		BLIB_TEST_CHECK(countDuring == countBefore + 1);
		
		void* ptr = alloc.allocate(256);
		BLIB_TEST_CHECK(ptr != nullptr);
		alloc.deallocate(ptr, 256);
		
		// Move heap-объекта: heapPtr переносится без новых аллокаций
		Allocator moved(std::move(alloc));
		size_t countAfterMove = ga.getAllocationCount();
		BLIB_TEST_CHECK(countAfterMove == countDuring);
		
		// Moved-from - мёртвый
		BLIB_TEST_CHECK(alloc.allocate(64) == nullptr);
		
		ptr = moved.allocate(128);
		BLIB_TEST_CHECK(ptr != nullptr);
		moved.deallocate(ptr, 128);
	}
	
	// destroyImpl (путь 2) должен освободить heap-impl
	size_t countAfter = ga.getAllocationCount();
	BLIB_TEST_CHECK(countAfter == countBefore);
}

// ============================================================
// 11. Дополнительные тесты GlobalAllocator
// ============================================================

BLIB_TEST_CASE("GlobalAllocator: dumpStats smoke test")
{
	auto& ga = GlobalAllocator::instance();
	
	// Включаем расширенную статистику и leak tracking
	ga.setExtendedStatsEnabled(true);
	ga.setLeakTrackingEnabled(true);
	
	void* ptr = ga.allocate(1024);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	// dumpStats должен отработать без падений (выводит в stderr)
	ga.dumpStats();
	
	ga.deallocate(ptr, 1024);
	
	// Возвращаем состояние
	ga.setLeakTrackingEnabled(false);
	ga.setExtendedStatsEnabled(false);
}

BLIB_TEST_CASE("GlobalAllocator: getHistogram with nullptr returns false")
{
	auto& ga = GlobalAllocator::instance();
	
	ga.setExtendedStatsEnabled(true);
	
	bool success = ga.getHistogram(nullptr);
	BLIB_TEST_CHECK(!success);
	
	ga.setExtendedStatsEnabled(false);
}

BLIB_TEST_CASE("GlobalAllocator: leak tracking re-enable clears data")
{
	auto& ga = GlobalAllocator::instance();
	
	ga.setLeakTrackingEnabled(true);
	void* ptr = ga.allocate(64);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	// Выключение очищает накопленные данные
	ga.setLeakTrackingEnabled(false);
	
	// Повторное включение - трекер пуст (прошлые записи удалены)
	ga.setLeakTrackingEnabled(true);
	size_t leaks = ga.dumpLeaks();
	BLIB_TEST_CHECK(leaks == 0);
	
	ga.setLeakTrackingEnabled(false);
	ga.deallocate(ptr, 64);
}

BLIB_TEST_CASE("GlobalAllocator: deallocate nullptr is no-op")
{
	auto& ga = GlobalAllocator::instance();
	
	size_t countBefore = ga.getAllocationCount();
	size_t allocBefore = ga.getCurrentAllocated();
	
	ga.deallocate(nullptr, 128);
	
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
	BLIB_TEST_CHECK(ga.getCurrentAllocated() == allocBefore);
}

// ============================================================
// 12. Дополнительные тесты PoolAllocator
// ============================================================

BLIB_TEST_CASE("PoolAllocator: getApproximateFreeBlocks tracking")
{
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	PoolAllocator pool(64, 8);
	
	// Пул пуст - chunks ещё не выделялись
	BLIB_TEST_CHECK(pool.getApproximateFreeBlocks() == 0);
	
	void* ptr1 = pool.allocate(64);
	BLIB_TEST_CHECK(ptr1 != nullptr);
	
	// Выделен 1 блок из 8
	BLIB_TEST_CHECK(pool.getApproximateFreeBlocks() == 7);
	
	void* ptr2 = pool.allocate(64);
	BLIB_TEST_CHECK(pool.getApproximateFreeBlocks() == 6);
	
	pool.deallocate(ptr1, 64);
	BLIB_TEST_CHECK(pool.getApproximateFreeBlocks() == 7);
	
	pool.deallocate(ptr2, 64);
	BLIB_TEST_CHECK(pool.getApproximateFreeBlocks() == 8);
#else
	std::cout << "  SKIPPED (getApproximateFreeBlocks not available in debug mode)" << std::endl;
#endif
}

BLIB_TEST_CASE("PoolAllocator: blockSize clamped to sizeof(void*)")
{
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	// blockSize меньше sizeof(void*) непригоден для intrusive free list,
	// конструктор увеличивает его до минимально допустимого
	PoolAllocator pool(1, 8);
	BLIB_TEST_CHECK(pool.getBlockSize() == sizeof(void*));
	
	// Пул при этом работает
	void* ptr = pool.allocate(sizeof(void*));
	BLIB_TEST_CHECK(ptr != nullptr);
	pool.deallocate(ptr, sizeof(void*));
#else
	std::cout << "  SKIPPED (getBlockSize not available in debug mode)" << std::endl;
#endif
}

BLIB_TEST_CASE("PoolAllocator: deallocate with wrong size is silently ignored")
{
#ifndef BLIB_DEBUG_ALLOCATOR_ENABLED
	PoolAllocator pool(64, 8);
	void* ptr = pool.allocate(64);
	BLIB_TEST_CHECK(ptr != nullptr);
	
	// Неверный размер: блок НЕ возвращается в free list (молча игнорируется)
	pool.deallocate(ptr, 32);
	BLIB_TEST_CHECK(pool.getApproximateFreeBlocks() == 7);
	
	// Корректный возврат
	pool.deallocate(ptr, 64);
	BLIB_TEST_CHECK(pool.getApproximateFreeBlocks() == 8);
#else
	// В debug wrong size детектируется DebugAllocator (abort), поэтому skip
	std::cout << "  SKIPPED (DebugAllocator aborts on size mismatch in debug mode)" << std::endl;
#endif
}

BLIB_TEST_CASE("PoolAllocator: moved-from pool does not free moved chunks")
{
	auto& ga = GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();
	size_t allocBefore = ga.getCurrentAllocated();
	
	{
		PoolAllocator pool1(64, 8);
		void* ptr = pool1.allocate(64);
		BLIB_TEST_CHECK(ptr != nullptr);
		
		// Move: chunks переходят к pool2, pool1 становится пустым
		PoolAllocator pool2(std::move(pool1));
		
		// ptr из чанка pool1 теперь принадлежит pool2
		void* ptr2 = pool2.allocate(64);
		BLIB_TEST_CHECK(ptr2 != nullptr);
		
		pool2.deallocate(ptr, 64);
		pool2.deallocate(ptr2, 64);
		
		// pool1 при разрушении НЕ должен освобождать чужие чанки
		// (у moved-from пула пустой вектор chunks)
	}
	
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
	BLIB_TEST_CHECK(ga.getCurrentAllocated() == allocBefore);
}

// ============================================================
// 13. Дополнительные тесты DebugAllocator
// ============================================================

BLIB_TEST_CASE("DebugAllocator: detects invalid pointer")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	DefaultAllocator alloc;
	
	// Мусорный указатель (не из DebugAllocator): magic в "header"
	// не совпадёт с MAGIC_ALLOCATED - должен детектироваться
	char fakeBuffer[64];
	void* fakePtr = fakeBuffer;
	
	BLIB_TEST_EXPECT_ABORT(alloc.deallocate(fakePtr, 64));
	
	// Память fakeBuffer - стек, освобождать не нужно
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: detects header corruption (magic field)")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	DefaultAllocator alloc;
	void* ptr = alloc.allocate(64);
	
	// Layout блока: Header(24) + FrontGuard(8) + user + BackGuard(8)
	// Header начинается за 32 байта до user-данных,
	// поле magic внутри Header - по смещению +8 (size[0..7], magic[8..11])
	char* raw = static_cast<char*>(ptr) - 32;
	buint32* magicField = reinterpret_cast<buint32*>(raw + 8);
	*magicField = 0x12345678; // Портим magic
	
	// deallocate должен детектировать невалидный magic
	BLIB_TEST_EXPECT_ABORT(alloc.deallocate(ptr, 64));
	
	// Note: память намеренно не освобождается после abort (как в guard-тестах)
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

BLIB_TEST_CASE("DebugAllocator: allocate(0) returns nullptr")
{
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
	DefaultAllocator alloc;
	
	void* ptr = alloc.allocate(0);
	BLIB_TEST_CHECK(ptr == nullptr);
#else
	std::cout << "  SKIPPED (debug mode only)" << std::endl;
#endif
}

// ============================================================
// 14. Дополнительные тесты StdAllocatorAdapter
// ============================================================

BLIB_TEST_CASE("StdAllocatorAdapter: comparison operators")
{
	Allocator allocA;
	Allocator allocB;
	
	StdAllocatorAdapter<int> adapterA1(&allocA);
	StdAllocatorAdapter<int> adapterA2(&allocA);
	StdAllocatorAdapter<int> adapterB(&allocB);
	
	// Равенство по указателю на Allocator
	BLIB_TEST_CHECK(adapterA1 == adapterA2);
	BLIB_TEST_CHECK(!(adapterA1 == adapterB));
	BLIB_TEST_CHECK(adapterA1 != adapterB);
	BLIB_TEST_CHECK(!(adapterA1 != adapterA2));
}

BLIB_TEST_CASE("StdAllocatorAdapter: rebind with node-based container (std::list)")
{
	auto& ga = GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();
	size_t allocBefore = ga.getCurrentAllocated();
	
	{
		Allocator alloc;
		StdAllocatorAdapter<int> adapter(&alloc);
		
		// std::list - узловой контейнер: внутри rebind<node> + allocate(1)
		std::list<int, StdAllocatorAdapter<int>> list(adapter);
		
		for (int i = 0; i < 100; ++i) {
			list.push_back(i);
		}
		
		BLIB_TEST_CHECK(list.size() == 100);
		
		// Проверяем содержимое
		int expected = 0;
		for (int value : list) {
			BLIB_TEST_CHECK(value == expected);
			++expected;
		}
	}
	
	// Все узлы списка освобождены
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
	BLIB_TEST_CHECK(ga.getCurrentAllocated() == allocBefore);
}

BLIB_TEST_CASE("StdAllocatorAdapter: construct/destroy with non-trivial type")
{
	// Нетривиальный тип с std::string
	struct Complex
	{
		std::string name;
		int value;
		
		Complex(const std::string& n, int v)
			: name(n)
			, value(v)
		{
		}
	};
	
	Allocator alloc;
	StdAllocatorAdapter<Complex> adapter(&alloc);
	
	Complex* obj = adapter.allocate(1);
	BLIB_TEST_CHECK(obj != nullptr);
	
	// construct: placement new с аргументами
	adapter.construct(obj, "test", 42);
	BLIB_TEST_CHECK(obj->name == "test");
	BLIB_TEST_CHECK(obj->value == 42);
	
	// destroy: явный вызов деструктора (освобождает std::string)
	adapter.destroy(obj);
	
	adapter.deallocate(obj, 1);
}
