#include <blib/test/src/test.h>

// IAllocatorAware: владение аллокатором, setAllocator (share-семантика),
// роутинг аллокаций наследника через аллокатор интерфейса
#include <blib/blibint.h>
#include <blib/system/memory/iallocatorAware.h>
#include <blib/system/memory/allocator.h>
#include <blib/system/memory/allocators/mallocAllocator.h>

#include <type_traits>

using namespace blib::memory;

// Компиляционные контракты: интерфейс — копируемый/перемещаемый
// value-тип (Allocator-член копируется share-семантикой)
static_assert(std::is_copy_constructible<IAllocatorAware>::value, "IAllocatorAware must be copy-constructible");
static_assert(std::is_move_constructible<IAllocatorAware>::value, "IAllocatorAware must be move-constructible");

// Stateful-аллокатор со счётчиком: проверка, что setAllocator
// действительно подменяет источник памяти объекта. Память идёт через
// MallocAllocator (мимо статистики GlobalAllocator — детерминированные
// проверки без гонки с фоновыми аллокациями консоли)
struct CountingAllocator
{
	buint32* pCount;
	MallocAllocator backend;

	void* allocate(size_t size)
	{
		++(*pCount);
		return this->backend.allocate(size);
	}

	void deallocate(_In_ void* ptr, size_t size)
	{
		this->backend.deallocate(ptr, size);
	}
};

// Наследник, открывающий защищённый API интерфейса: тесты гоняют
// роутинг через публичные обёртки над allocate/deallocate
class Allocating : public IAllocatorAware
{
public:
	Allocating() = default;

	void* makeAlloc(size_t size)
	{
		return this->allocate(size);
	}

	void freeAlloc(_In_ void* ptr, size_t size)
	{
		this->deallocate(ptr, size);
	}
};

BLIB_TEST_CASE("IAllocatorAware: дефолтный аллокатор работоспособен")
{
	Allocating obj;

	// Без setAllocator — DefaultAllocator (прокси к GlobalAllocator):
	// аллокация обязана пройти успешно
	void* p = obj.makeAlloc(64);
	BLIB_TEST_CHECK(p != nullptr);

	obj.freeAlloc(p, 64);
}

BLIB_TEST_CASE("IAllocatorAware: setAllocator подменяет источник памяти")
{
	buint32 allocations = 0;
	CountingAllocator counter{ &allocations };
	Allocator counting(std::move(counter));

	Allocating obj;
	BLIB_TEST_CHECK(allocations == 0);

	obj.setAllocator(counting);
	void* p = obj.makeAlloc(128);
	BLIB_TEST_CHECK(p != nullptr);
	BLIB_TEST_CHECK(allocations == 1);

	obj.freeAlloc(p, 128);
}

BLIB_TEST_CASE("IAllocatorAware: setAllocator копирует share-семантикой — объекты делят состояние impl")
{
	buint32 allocations = 0;
	Allocator sharedSource(CountingAllocator{ &allocations });

	Allocating first;
	Allocating second;
	first.setAllocator(sharedSource);
	second.setAllocator(sharedSource);

	void* p = first.makeAlloc(32);
	BLIB_TEST_CHECK(p != nullptr);
	BLIB_TEST_CHECK(allocations == 1);

	// Второй объект аллоцирует через ТОТ ЖЕ общий stateful impl
	void* q = second.makeAlloc(32);
	BLIB_TEST_CHECK(q != nullptr);
	BLIB_TEST_CHECK(allocations == 2);

	first.freeAlloc(p, 32);
	second.freeAlloc(q, 32);
}

BLIB_TEST_CASE("IAllocatorAware: getAllocator даёт живой аллокатор объекта")
{
	buint32 allocations = 0;

	Allocating obj;
	// Присваивание через ссылку из getAllocator — тот же эффект,
	// что у setAllocator (Allocator::operator= = share)
	obj.getAllocator() = Allocator(CountingAllocator{ &allocations });

	void* p = obj.makeAlloc(16);
	BLIB_TEST_CHECK(p != nullptr);
	BLIB_TEST_CHECK(allocations == 1);

	obj.freeAlloc(p, 16);
}
