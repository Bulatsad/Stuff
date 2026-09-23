#include <blib/test/src/test.h>

// Type erasure: базовый интерфейс ITypeErased + примеры-наследники
#include <blib/system/memory/itypeErased.h>
#include <blib/system/memory/globalAllocator.h>
#include <blib/system/memory/allocators/mallocAllocator.h>

#include <typeinfo>
#include <type_traits>

using namespace blib::memory;

// Компиляционные контракты: база — полноценный copy/move value-тип
// (семантика на хелперах copyConstructFrom/moveConstructFrom).
static_assert(std::is_copy_constructible<ITypeErased>::value, "ITypeErased must be copy-constructible");
static_assert(std::is_move_constructible<ITypeErased>::value, "ITypeErased must be move-constructible");
static_assert(std::is_copy_assignable<ITypeErased>::value, "ITypeErased must be copy-assignable");
static_assert(std::is_move_assignable<ITypeErased>::value, "ITypeErased must be move-assignable");

// Счётчик вызовов деструктора: проверка, что erased-объект действительно
// уничтожается (и ровно один раз).
static int g_dtorCount = 0;

/**
 * Простая структура с деструктором-счётчиком и двумя конструкторами
 * (один — с несколькими аргументами, для проверки проброса параметров).
 */
struct Tracked
{
	int value;

	explicit Tracked(int v)
		: value(v)
	{
	}

	Tracked(int a, int b)
		: value(a + b)
	{
	}

	~Tracked()
	{
		++g_dtorCount;
	}
};

/**
 * Move-only тип: копирование запрещено, перемещение разрешено.
 * Проверяет условную установку pCopyConstructor (if constexpr).
 */
struct MoveOnly
{
	int value;

	explicit MoveOnly(int v)
		: value(v)
	{
	}

	MoveOnly(const MoveOnly&) = delete;
	MoveOnly& operator=(const MoveOnly&) = delete;

	MoveOnly(MoveOnly&& other) noexcept
		: value(other.value)
	{
	}
};

/**
 * Non-movable тип: ни копирования, ни перемещения.
 * Проверяет условную установку обоих fn-ptr (if constexpr).
 */
struct Immovable
{
	int value;

	explicit Immovable(int v)
		: value(v)
	{
	}

	Immovable(const Immovable&) = delete;
	Immovable& operator=(const Immovable&) = delete;
	Immovable(Immovable&&) = delete;
	Immovable& operator=(Immovable&&) = delete;
};

/**
 * Минимальный наследник, открывающий защищённый API базы: тесты
 * проверяют механику construct напрямую, без собственного API.
 */
class RawTypeErased : public ITypeErased
{
public:
	// Наследуемые конструкторы сохраняют protected-доступ базы
	// (using-декларация для ctor'ов доступ не меняет) — объявляем явно.
	RawTypeErased() = default;

	explicit RawTypeErased(Allocator&& allocator)
		: ITypeErased(std::move(allocator))
	{
	}

	using ITypeErased::construct;
};

/**
 * Пример расширения базового интерфейса: владеющий type-erased value
 * с typed-доступом. Демонстрирует паттерн, по которому позже будет
 * построен TypeErasedAllocator: наследник добавляет свой API поверх
 * механики ITypeErased (construct + хранение type_info у себя).
 */
class TypeErasedValue : public ITypeErased
{
public:
	template<typename T, typename... TConstructArgs>
	bool emplace(TConstructArgs&&... args)
	{
		if (!this->construct<T>(std::forward<TConstructArgs>(args)...))
		{
			// При неудаче объект пуст — type_info тоже сбрасываем.
			this->typeInfo = nullptr;
			return false;
		}
		this->typeInfo = &typeid(T);
		return true;
	}

	template<typename T>
	T* get() noexcept
	{
		if (this->isEmpty() || this->typeInfo != &typeid(T))
			return nullptr;
		return static_cast<T*>(this->pdata);
	}

	bool hasValue() const noexcept
	{
		return !this->isEmpty();
	}

private:
	const std::type_info* typeInfo = nullptr;
};

/**
 * Наследник с копированием и перемещением содержимого: copy/move ctor'ы
 * и operator= построены на protected-хелперах базы (copyConstructFrom/
 * moveConstructFrom) + хранение собственного состояния (type_info).
 * Демонстрирует каноничный паттерн value-типа поверх ITypeErased.
 */
class CopyableValue : public ITypeErased
{
public:
	CopyableValue() = default;

	CopyableValue(const CopyableValue& other)
	{
		// При неудаче (OOM / не-копируемый T) объект остаётся пустым —
		// copy-ctor не может вернуть код ошибки, вызывающий проверяет hasValue().
		if (this->copyConstructFrom(other))
			this->typeInfo = other.typeInfo;
	}

	CopyableValue(CopyableValue&& other)
	{
		if (this->moveConstructFrom(other))
		{
			this->typeInfo = other.typeInfo;
			// Источник пуст — его type_info сбрасываем (база о нём не знает).
			other.typeInfo = nullptr;
		}
	}

	// operator= объявляем явно: неявный move-assign не сгенерируется
	// (объявлены copy/move ctor'ы) и выродился бы в copy-семантику;
	// плюс держим инвариант typeInfo: пустой <=> nullptr.
	CopyableValue& operator=(const CopyableValue& other)
	{
		if (this != &other)
		{
			if (this->copyConstructFrom(other))
				this->typeInfo = other.typeInfo;
			else
				this->typeInfo = nullptr;
		}
		return *this;
	}

	CopyableValue& operator=(CopyableValue&& other)
	{
		if (this != &other)
		{
			if (this->moveConstructFrom(other))
			{
				this->typeInfo = other.typeInfo;
				other.typeInfo = nullptr;
			}
			else
			{
				this->typeInfo = nullptr;
			}
		}
		return *this;
	}

	template<typename T, typename... TConstructArgs>
	bool emplace(TConstructArgs&&... args)
	{
		if (!this->construct<T>(std::forward<TConstructArgs>(args)...))
		{
			this->typeInfo = nullptr;
			return false;
		}
		this->typeInfo = &typeid(T);
		return true;
	}

	template<typename T>
	T* get() noexcept
	{
		if (this->isEmpty() || this->typeInfo != &typeid(T))
			return nullptr;
		return static_cast<T*>(this->pdata);
	}

	bool hasValue() const noexcept
	{
		return !this->isEmpty();
	}

private:
	const std::type_info* typeInfo = nullptr;
};

// ============================================================
// База: пустое состояние
// ============================================================

BLIB_TEST_CASE("ITypeErased: пустой объект по умолчанию")
{
	RawTypeErased obj;
	BLIB_TEST_CHECK(obj.isEmpty());
}

// ============================================================
// База: construct + уничтожение при выходе из области видимости
// ============================================================

BLIB_TEST_CASE("ITypeErased: construct и уничтожение через деструктор")
{
	const int before = g_dtorCount;

	{
		RawTypeErased obj;
		BLIB_TEST_CHECK(obj.isEmpty());
		BLIB_TEST_CHECK(obj.construct<Tracked>(42));
		BLIB_TEST_CHECK(!obj.isEmpty());
	}

	// Деструктор erased-объекта вызван ровно один раз при выходе из scope.
	BLIB_TEST_CHECK(g_dtorCount == before + 1);
}

// ============================================================
// База: проброс нескольких аргументов конструктора
// ============================================================

BLIB_TEST_CASE("ITypeErased: construct с несколькими аргументами конструктора")
{
	RawTypeErased obj;
	BLIB_TEST_CHECK(obj.construct<Tracked>(20, 22));

	// Значение суммы доступно только через наследников (база type-agnostic),
	// поэтому факт конструирования проверяем непустотой.
	BLIB_TEST_CHECK(!obj.isEmpty());
}

// ============================================================
// База: construct поверх занятого объекта
// ============================================================

BLIB_TEST_CASE("ITypeErased: construct поверх занятого уничтожает старое содержимое")
{
	const int before = g_dtorCount;

	RawTypeErased obj;
	BLIB_TEST_CHECK(obj.construct<Tracked>(1));
	// Второй construct: старое содержимое уничтожается, объект остаётся валидным.
	BLIB_TEST_CHECK(obj.construct<Tracked>(2));
	BLIB_TEST_CHECK(!obj.isEmpty());

	// Старый Tracked уничтожен сразу при перезаписи.
	BLIB_TEST_CHECK(g_dtorCount == before + 1);
}

// ============================================================
// База: кастомный аллокатор
// ============================================================

BLIB_TEST_CASE("ITypeErased: кастомный аллокатор (MallocAllocator)")
{
	// MallocAllocator работает мимо статистики GlobalAllocator (std::malloc).
	// В debug-сборке он автоматически оборачивается в DebugAllocator
	// (stateful) — Allocator корректно инкапсулирует его через SBO.
	// Фигурные скобки — против most vexing parse.
	Allocator alloc{MallocAllocator{}};
	RawTypeErased obj{std::move(alloc)};

	BLIB_TEST_CHECK(obj.construct<Tracked>(7));
	BLIB_TEST_CHECK(!obj.isEmpty());
}

// ============================================================
// База: отсутствие утечек (статистика GlobalAllocator)
// ============================================================

BLIB_TEST_CASE("ITypeErased: нет утечки памяти через GlobalAllocator")
{
	auto& ga = GlobalAllocator::instance();
	const size_t before = ga.getCurrentAllocated();

	{
		RawTypeErased obj;
		BLIB_TEST_CHECK(obj.construct<Tracked>(5));
		BLIB_TEST_CHECK(ga.getCurrentAllocated() > before);
	}

	// Память erased-объекта возвращена аллокатору.
	BLIB_TEST_CHECK(ga.getCurrentAllocated() == before);
}

// ============================================================
// Наследник: typed-доступ
// ============================================================

BLIB_TEST_CASE("TypeErasedValue: emplace/get и несовпадение типа")
{
	TypeErasedValue value;
	BLIB_TEST_CHECK(!value.hasValue());

	BLIB_TEST_CHECK(value.emplace<int>(123));
	BLIB_TEST_CHECK(value.hasValue());

	int* pInt = value.get<int>();
	BLIB_TEST_REQUIRE(pInt != nullptr);
	BLIB_TEST_CHECK(*pInt == 123);

	// Несовпадение типа — nullptr, а не ошибка доступа.
	BLIB_TEST_CHECK(value.get<double>() == nullptr);
	BLIB_TEST_CHECK(value.get<Tracked>() == nullptr);
}

BLIB_TEST_CASE("TypeErasedValue: get на пустом объекте")
{
	TypeErasedValue value;
	BLIB_TEST_CHECK(value.get<int>() == nullptr);
	BLIB_TEST_CHECK(value.get<Tracked>() == nullptr);
}

// ============================================================
// Наследник: виртуальный деструктор при удалении через базу
// ============================================================

BLIB_TEST_CASE("ITypeErased: удаление наследника через указатель на базу")
{
	const int before = g_dtorCount;

	// Память под наследника — через GlobalAllocator + placement new
	// (raw new запрещён; delete для placement-new блока неприменим,
	// поэтому явный dtor + deallocate).
	void* mem = GlobalAllocator::instance().allocate(sizeof(TypeErasedValue));
	BLIB_TEST_REQUIRE(mem != nullptr);

	ITypeErased* pBase = new (mem) TypeErasedValue();
	BLIB_TEST_REQUIRE(static_cast<TypeErasedValue*>(pBase)->emplace<Tracked>(9));

	// Внутренний erased-объект жив, пока не уничтожен держатель.
	BLIB_TEST_CHECK(g_dtorCount == before);

	// Вызов виртуального деструктора через базу: сначала dtor наследника,
	// затем ITypeErased::~ITypeErased уничтожает erased-объект.
	pBase->~ITypeErased();
	BLIB_TEST_CHECK(g_dtorCount == before + 1);

	GlobalAllocator::instance().deallocate(mem, sizeof(TypeErasedValue));
}

// ============================================================
// Наследник: копирование содержимого (deep copy)
// ============================================================

BLIB_TEST_CASE("CopyableValue: копия независима от оригинала")
{
	const int before = g_dtorCount;

	{
		CopyableValue src;
		BLIB_TEST_REQUIRE(src.emplace<Tracked>(42));

		CopyableValue copy{src};
		BLIB_TEST_REQUIRE(copy.hasValue());
		BLIB_TEST_REQUIRE(src.hasValue());

		Tracked* pSrc = src.get<Tracked>();
		Tracked* pCopy = copy.get<Tracked>();
		BLIB_TEST_REQUIRE(pSrc != nullptr);
		BLIB_TEST_REQUIRE(pCopy != nullptr);

		// Значения совпадают, хранилища независимы.
		BLIB_TEST_CHECK(pSrc->value == 42);
		BLIB_TEST_CHECK(pCopy->value == 42);
		BLIB_TEST_CHECK(pSrc != pCopy);
	}

	// Каждый экземпляр уничтожил свой Tracked.
	BLIB_TEST_CHECK(g_dtorCount == before + 2);
}

// ============================================================
// Наследник: перемещение содержимого
// ============================================================

BLIB_TEST_CASE("CopyableValue: move опустошает источник")
{
	const int before = g_dtorCount;

	{
		CopyableValue src;
		BLIB_TEST_REQUIRE(src.emplace<Tracked>(7));

		CopyableValue dst{std::move(src)};
		BLIB_TEST_REQUIRE(dst.hasValue());

		// Источник пуст после переноса.
		BLIB_TEST_CHECK(!src.hasValue());
		BLIB_TEST_CHECK(src.get<Tracked>() == nullptr);

		Tracked* pDst = dst.get<Tracked>();
		BLIB_TEST_REQUIRE(pDst != nullptr);
		BLIB_TEST_CHECK(pDst->value == 7);
	}

	// Erased-объект уничтожен дважды: исходный — при переносе
	// (destroyErased источника), перенесённый — при выходе из scope.
	BLIB_TEST_CHECK(g_dtorCount == before + 2);
}

// ============================================================
// Наследник: копирование/перемещение пустого объекта
// ============================================================

BLIB_TEST_CASE("CopyableValue: copy/move пустого объекта")
{
	CopyableValue src;

	CopyableValue copy{src};
	BLIB_TEST_CHECK(!copy.hasValue());

	CopyableValue dst{std::move(src)};
	BLIB_TEST_CHECK(!dst.hasValue());
	BLIB_TEST_CHECK(!src.hasValue());
}

// ============================================================
// Наследник: move-only тип (pCopyConstructor == nullptr)
// ============================================================

BLIB_TEST_CASE("CopyableValue: move-only тип — копирование недоступно, move работает")
{
	CopyableValue src;
	BLIB_TEST_REQUIRE(src.emplace<MoveOnly>(3));

	// Копирование: fn-ptr отсутствует -> пустая копия, оригинал цел.
	CopyableValue copy{src};
	BLIB_TEST_CHECK(!copy.hasValue());
	BLIB_TEST_CHECK(src.hasValue());

	// Перемещение работает.
	CopyableValue dst{std::move(src)};
	BLIB_TEST_REQUIRE(dst.hasValue());
	BLIB_TEST_CHECK(!src.hasValue());

	MoveOnly* pDst = dst.get<MoveOnly>();
	BLIB_TEST_REQUIRE(pDst != nullptr);
	BLIB_TEST_CHECK(pDst->value == 3);
}

// ============================================================
// Наследник: non-movable тип (оба fn-ptr == nullptr)
// ============================================================

BLIB_TEST_CASE("CopyableValue: non-movable тип — copy и move недоступны")
{
	CopyableValue src;
	BLIB_TEST_REQUIRE(src.emplace<Immovable>(5));

	// Копирование недоступно: пустая копия, оригинал цел.
	CopyableValue copy{src};
	BLIB_TEST_CHECK(!copy.hasValue());
	BLIB_TEST_CHECK(src.hasValue());

	// Перемещение недоступно: приёмник пуст, источник не тронут.
	CopyableValue dst{std::move(src)};
	BLIB_TEST_CHECK(!dst.hasValue());
	BLIB_TEST_CHECK(src.hasValue());

	Immovable* pSrc = src.get<Immovable>();
	BLIB_TEST_REQUIRE(pSrc != nullptr);
	BLIB_TEST_CHECK(pSrc->value == 5);
}

// ============================================================
// Наследник: отсутствие утечек при copy/move
// ============================================================

BLIB_TEST_CASE("CopyableValue: нет утечки памяти при copy/move")
{
	auto& ga = GlobalAllocator::instance();
	const size_t before = ga.getCurrentAllocated();

	{
		CopyableValue src;
		BLIB_TEST_REQUIRE(src.emplace<Tracked>(1));

		CopyableValue copy{src};
		CopyableValue dst{std::move(src)};
		BLIB_TEST_CHECK(copy.hasValue());
		BLIB_TEST_CHECK(dst.hasValue());
	}

	// Память всех экземпляров (оригинал + копия + перенесённый) возвращена.
	BLIB_TEST_CHECK(ga.getCurrentAllocated() == before);
}

// ============================================================
// База: copy/move ctor'ы и operator= самой базы
// ============================================================

BLIB_TEST_CASE("ITypeErased: copy-ctor и copy-assign базы")
{
	const int before = g_dtorCount;

	{
		RawTypeErased src;
		BLIB_TEST_REQUIRE(src.construct<Tracked>(42));

		// Копия — независимый экземпляр.
		RawTypeErased copy{src};
		BLIB_TEST_CHECK(copy.isEmpty() == false);
		BLIB_TEST_CHECK(src.isEmpty() == false);

		// Присваивание копированием.
		RawTypeErased dst;
		dst = src;
		BLIB_TEST_CHECK(dst.isEmpty() == false);
		BLIB_TEST_CHECK(src.isEmpty() == false);
	}

	// Каждый из трёх экземпляров уничтожил свой Tracked.
	BLIB_TEST_CHECK(g_dtorCount == before + 3);
}

BLIB_TEST_CASE("ITypeErased: move-ctor и move-assign базы")
{
	const int before = g_dtorCount;

	{
		RawTypeErased src;
		BLIB_TEST_REQUIRE(src.construct<Tracked>(7));

		RawTypeErased dst{std::move(src)};
		BLIB_TEST_CHECK(dst.isEmpty() == false);
		BLIB_TEST_CHECK(src.isEmpty());  // источник пуст

		// Присваивание перемещением: dst занят -> его содержимое уничтожается.
		RawTypeErased dst2;
		dst2 = std::move(dst);
		BLIB_TEST_CHECK(dst2.isEmpty() == false);
		BLIB_TEST_CHECK(dst.isEmpty());
	}

	// Fresh-аллокация: каждый перенос уничтожает исходный объект
	// (2 переноса = 2 деструктора) + финальное уничтожение в dst2.
	BLIB_TEST_CHECK(g_dtorCount == before + 3);
}

// ============================================================
// Наследник: присваивание и self-assign
// ============================================================

BLIB_TEST_CASE("CopyableValue: copy-assign и move-assign")
{
	CopyableValue a;
	CopyableValue b;
	BLIB_TEST_REQUIRE(a.emplace<Tracked>(10));
	BLIB_TEST_REQUIRE(b.emplace<Tracked>(20));

	// copy-assign поверх занятого: старое содержимое уничтожается.
	b = a;
	BLIB_TEST_REQUIRE(b.hasValue());
	BLIB_TEST_CHECK(b.get<Tracked>()->value == 10);
	BLIB_TEST_CHECK(a.hasValue());

	// move-assign: источник пуст, приёмник держит значение.
	CopyableValue c;
	c = std::move(a);
	BLIB_TEST_REQUIRE(c.hasValue());
	BLIB_TEST_CHECK(c.get<Tracked>()->value == 10);
	BLIB_TEST_CHECK(!a.hasValue());
}

BLIB_TEST_CASE("CopyableValue: self-assign")
{
	CopyableValue src;
	BLIB_TEST_REQUIRE(src.emplace<Tracked>(33));

	// Self-assign guard: без него destroyErased() обнулил бы источник
	// до чтения, и значение бы потерялось.
	src = src;
	BLIB_TEST_REQUIRE(src.hasValue());
	BLIB_TEST_CHECK(src.get<Tracked>()->value == 33);

	src = std::move(src);
	BLIB_TEST_REQUIRE(src.hasValue());
	BLIB_TEST_CHECK(src.get<Tracked>()->value == 33);
}
