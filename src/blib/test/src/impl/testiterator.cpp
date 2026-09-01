#include <blib/test/src/test.h>

// Итераторы ядра: AnyIterator (type-erased), Range/makeRange, LinkedList
#include <blib/core/iterator.h>
#include <blib/core/linkedList.h>

#include <vector>
#include <string>
#include <cstring>

using namespace blib::core;

// ============================================================
// 1. AnyIterator: базовые операции (inline-ветка, std::vector)
// ============================================================

BLIB_TEST_CASE("AnyIterator: deref/increment/equality over std::vector")
{
	std::vector<int> v;
	v.push_back(10);
	v.push_back(20);
	v.push_back(30);

	AnyIterator<int> it(v.begin());
	AnyIterator<int> end(v.end());

	BLIB_TEST_CHECK(it != end);
	BLIB_TEST_CHECK(*it == 10);
	++it;
	BLIB_TEST_CHECK(*it == 20);

	// Постфиксный ++ (внутри copy+clone и move - регрессия memcpy-move,
	// который ломал debug STL итераторы: ITERATOR LIST CORRUPTED!)
	AnyIterator<int> it2 = it++;
	BLIB_TEST_CHECK(*it2 == 20);
	BLIB_TEST_CHECK(*it == 30);

	++it;
	BLIB_TEST_CHECK(it == end);
}

BLIB_TEST_CASE("AnyIterator: operator->")
{
	struct Item
	{
		int x;
	};

	std::vector<Item> v;
	v.push_back(Item{42});

	AnyIterator<Item> it(v.begin());
	BLIB_TEST_CHECK(it->x == 42);
}

BLIB_TEST_CASE("AnyIterator: copy and move semantics")
{
	std::vector<int> v;
	v.push_back(7);

	// Копирование
	AnyIterator<int> orig(v.begin());
	AnyIterator<int> copy(orig);
	BLIB_TEST_CHECK(*copy == 7);

	// Move-конструктор: orig теряет владение, copy2 работает
	AnyIterator<int> moved(std::move(orig));
	BLIB_TEST_CHECK(*moved == 7);

	// Копирующее присваивание
	AnyIterator<int> assigned;
	assigned = copy;
	BLIB_TEST_CHECK(*assigned == 7);

	// Move-присваивание
	AnyIterator<int> movedAssigned;
	movedAssigned = std::move(copy);
	BLIB_TEST_CHECK(*movedAssigned == 7);
}

BLIB_TEST_CASE("AnyIterator: default-constructed (null) iterators are equal")
{
	AnyIterator<int> null1;
	AnyIterator<int> null2;

	BLIB_TEST_CHECK(null1 == null2);
	BLIB_TEST_CHECK(!(null1 != null2));

	// Пустой != непустой
	int arr[] = {1};
	AnyIterator<int> notNull(arr);
	BLIB_TEST_CHECK(null1 != notNull);
	BLIB_TEST_CHECK(notNull != null1);
}

BLIB_TEST_CASE("AnyIterator: raw pointer and const iteration")
{
	int arr[] = {1, 2, 3, 4};
	AnyIterator<int> it(arr);
	AnyIterator<int> end(arr + 4);

	int sum = 0;
	for (; it != end; ++it)
		sum += *it;
	BLIB_TEST_CHECK(sum == 10);

	// Константная итерация: AnyIterator<const int>
	const int carr[] = {5, 6};
	AnyIterator<const int> cit(carr);
	AnyIterator<const int> cend(carr + 2);

	sum = 0;
	for (; cit != cend; ++cit)
		sum += *cit;
	BLIB_TEST_CHECK(sum == 11);
}

BLIB_TEST_CASE("AnyIterator: different wrapped types are not equal")
{
	int arr1[] = {1, 2};
	std::vector<int> v;
	v.push_back(1);
	v.push_back(2);

	// Разные обёрнутые типы (int* vs std::vector::iterator),
	// даже при одинаковых элементах - не равны
	AnyIterator<int> a(arr1);
	AnyIterator<int> b(v.begin());
	BLIB_TEST_CHECK(a != b);
}

// ============================================================
// 2. AnyIterator: heap-ветка (обёртка не влезает в inline буфер)
// ============================================================

namespace
{
	/**
	 * Тестовый forward итератор с большим телом (> SBO_SIZE = 48),
	 * чтобы проверить heap-путь хранения обёртки в AnyIterator.
	 */
	struct BigIterator
	{
		typedef std::forward_iterator_tag iterator_category;
		typedef int value_type;
		typedef std::ptrdiff_t difference_type;
		typedef int* pointer;
		typedef int& reference;

		int* p;
		char padding[200];

		explicit BigIterator(int* ptr)
			: p(ptr)
		{
		}

		int& operator*() const { return *p; }
		int* operator->() const { return p; }
		BigIterator& operator++() { ++p; return *this; }
		BigIterator operator++(int) { BigIterator t(*this); ++p; return t; }
		bool operator==(const BigIterator& o) const { return p == o.p; }
		bool operator!=(const BigIterator& o) const { return p != o.p; }
	};
}

BLIB_TEST_CASE("AnyIterator: heap path for iterator larger than SBO")
{
	auto& ga = blib::memory::GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();

	int arr[] = {100, 200, 300};
	{
		BigIterator bi1(arr);
		BigIterator bi2(arr + 3);

		// sizeof(AnyIteratorImplWrapper<int, BigIterator>) > 48 - heap-путь:
		// каждая обёртка = +1 аллокация через GlobalAllocator
		AnyIterator<int> it(bi1);
		AnyIterator<int> end(bi2);

		int sum = 0;
		for (; it != end; ++it)
			sum += *it;
		BLIB_TEST_CHECK(sum == 600);

		// Move heap-объекта: указатель переносится, новых аллокаций нет
		AnyIterator<int> copy(bi1);
		size_t countBeforeMove = ga.getAllocationCount();
		AnyIterator<int> moved(std::move(copy));
		BLIB_TEST_CHECK(ga.getAllocationCount() == countBeforeMove);
		BLIB_TEST_CHECK(*moved == 100);

		// Копия heap-объекта: +1 аллокация
		AnyIterator<int> copied(moved);
		BLIB_TEST_CHECK(ga.getAllocationCount() == countBeforeMove + 1);
		BLIB_TEST_CHECK(*copied == 100);

		// Присваивание с заменой: старый heap освобождается, новый выделяется
		AnyIterator<int> assigned(copied);
		BLIB_TEST_CHECK(*assigned == 100);
	}

	// Все heap-обёртки освобождены деструкторами
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
}

BLIB_TEST_CASE("AnyIterator: inline path allocates nothing in heap")
{
	auto& ga = blib::memory::GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();

	{
		// Указатель и std::vector::iterator - маленькие: обёртка влезает
		// в inline буфер, heap-аллокаций быть не должно
		int arr[] = {1, 2};
		AnyIterator<int> a(arr);
		AnyIterator<int> b(arr + 2);
		AnyIterator<int> c(a);
		AnyIterator<int> d(std::move(c));

		int sum = 0;
		for (; a != b; ++a)
			sum += *a;
		BLIB_TEST_CHECK(sum == 3);
		BLIB_TEST_CHECK(*d == 1);
	}

	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
}

BLIB_TEST_CASE("AnyIterator: works with std algorithm (iterator_traits)")
{
	std::vector<int> v;
	v.push_back(3);
	v.push_back(1);
	v.push_back(2);

	// std::copy через AnyIterator (специализация std::iterator_traits)
	std::vector<int> out;
	for (AnyIterator<int> it(v.begin()); it != AnyIterator<int>(v.end()); ++it)
		out.push_back(*it);

	BLIB_TEST_CHECK(out.size() == 3);
	BLIB_TEST_CHECK(out[0] == 3 && out[1] == 1 && out[2] == 2);
}

// ============================================================
// 3. Range и makeRange
// ============================================================

BLIB_TEST_CASE("Range: empty/size/iteration")
{
	std::vector<int> v;
	v.push_back(1);
	v.push_back(2);
	v.push_back(3);

	Range<std::vector<int>::iterator> r(v.begin(), v.end());

	BLIB_TEST_CHECK(!r.empty());
	BLIB_TEST_CHECK(r.size() == 3);

	int sum = 0;
	for (int& x : r)
		sum += x;
	BLIB_TEST_CHECK(sum == 6);

	// Пустой диапазон
	Range<std::vector<int>::iterator> empty(v.begin(), v.begin());
	BLIB_TEST_CHECK(empty.empty());
	BLIB_TEST_CHECK(empty.size() == 0);
}

BLIB_TEST_CASE("makeRange: container, C array and LinkedList")
{
	std::vector<int> v;
	v.push_back(4);
	v.push_back(5);

	auto rv = makeRange(v);
	BLIB_TEST_CHECK(rv.size() == 2);

	int arr[4] = {7, 8, 9, 10};
	auto ra = makeRange(arr);
	int sum = 0;
	for (int x : ra)
		sum += x;
	BLIB_TEST_CHECK(sum == 34);
	BLIB_TEST_CHECK(ra.size() == 4);

	blib::LinkedList<int> ll;
	ll.pushBack(1);
	ll.pushBack(2);
	auto rl = makeRange(ll);
	sum = 0;
	for (int x : rl)
		sum += x;
	BLIB_TEST_CHECK(sum == 3);
}

// ============================================================
// 4. LinkedList: операции, итерация, владение памятью
// ============================================================

namespace
{
	/**
	 * Тип-счётчик живых объектов: ловит утечки (destroy без deallocate)
	 * и двойные разрушения в LinkedList.
	 */
	struct Tracked
	{
		static int alive;

		int value;

		explicit Tracked(int v) : value(v) { ++alive; }
		Tracked(const Tracked& other) : value(other.value) { ++alive; }
		Tracked(Tracked&& other) : value(other.value) { ++alive; }
		~Tracked() { --alive; }
	};

	int Tracked::alive = 0;
}

BLIB_TEST_CASE("LinkedList: pushBack/pushFront/iteration")
{
	blib::LinkedList<int> ll;
	for (int i = 0; i < 5; ++i)
		ll.pushBack(i);

	// Обе перегрузки pushBack
	ll.pushBack(42);

	// pushFront (объявлен и реализован в ходе переработки LinkedList)
	ll.pushFront(-1);

	BLIB_TEST_CHECK(ll.size() == 7);
	BLIB_TEST_CHECK(!ll.empty());
	BLIB_TEST_CHECK(ll.front() == -1);
	BLIB_TEST_CHECK(ll.back() == 42);
	BLIB_TEST_CHECK(ll[1] == 0);
	BLIB_TEST_CHECK(ll[6] == 42);

	// Прямой обход итераторами
	int expected[] = {-1, 0, 1, 2, 3, 4, 42};
	size_t idx = 0;
	for (blib::LinkedList<int>::Iterator it = ll.begin(); it != ll.end(); ++it, ++idx)
	{
		BLIB_TEST_CHECK(idx < 7);
		BLIB_TEST_CHECK(*it == expected[idx]);
	}
	BLIB_TEST_CHECK(idx == 7);
}

BLIB_TEST_CASE("LinkedList: single element iteration (self-loop regression)")
{
	// Регрессия: раньше первый pushBack замыкал head->next на head,
	// и итерация по списку из 1 элемента была бесконечным циклом
	blib::LinkedList<int> single;
	single.pushBack(7);

	size_t count = 0;
	for (int v : single)
	{
		(void)v;
		++count;
	}
	BLIB_TEST_CHECK(count == 1);
}

BLIB_TEST_CASE("LinkedList: const iteration and range-based for")
{
	blib::LinkedList<int> ll;
	ll.pushBack(1);
	ll.pushBack(2);
	ll.pushBack(3);

	const blib::LinkedList<int>& cll = ll;

	int sum = 0;
	for (blib::LinkedList<int>::ConstIterator it = cll.begin(); it != cll.end(); ++it)
		sum += *it;
	BLIB_TEST_CHECK(sum == 6);

	sum = 0;
	for (int v : cll)
		sum += v;
	BLIB_TEST_CHECK(sum == 6);

	sum = 0;
	for (auto it = cll.cbegin(); it != cll.cend(); ++it)
		sum += *it;
	BLIB_TEST_CHECK(sum == 6);
}

BLIB_TEST_CASE("LinkedList: pop operations")
{
	blib::LinkedList<int> ll;
	for (int i = 0; i < 6; ++i)
		ll.pushBack(i);

	BLIB_TEST_CHECK(ll.popFront() == 0);
	BLIB_TEST_CHECK(ll.popBack() == 5);
	BLIB_TEST_CHECK(ll.size() == 4);

	ll.pop(1); // удаляем 2
	BLIB_TEST_CHECK(ll.size() == 3);
	BLIB_TEST_CHECK(ll[0] == 1);
	BLIB_TEST_CHECK(ll[1] == 3);
	BLIB_TEST_CHECK(ll[2] == 4);

	// pop(0) - крашился в старой реализации
	ll.pop(0);
	BLIB_TEST_CHECK(ll.size() == 2);
	BLIB_TEST_CHECK(ll.front() == 3);
}

BLIB_TEST_CASE("LinkedList: destructor releases all nodes (no leaks/double destroy)")
{
	{
		blib::LinkedList<Tracked> lt;
		lt.pushBack(Tracked(1));
		lt.pushBack(Tracked(2));
		lt.pushBack(Tracked(3));
		BLIB_TEST_CHECK(Tracked::alive == 3);

		lt.pop(1);
		BLIB_TEST_CHECK(Tracked::alive == 2);

		lt.pushFront(Tracked(9));
		BLIB_TEST_CHECK(Tracked::alive == 3);
	}

	// Деструктор списка разрушил и освободил все узлы
	// (регрессия: раньше был только destroy без deallocate)
	BLIB_TEST_CHECK(Tracked::alive == 0);
}
