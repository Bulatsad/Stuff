#pragma once

#include <cstddef>
#include <cstdio>
#include <type_traits>
#include <utility>

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/allocator.h>

namespace blib
{
namespace memory
{
	/**
	 * ITypeErased - базовый интерфейс type erasure.
	 *
	 * Назначение:
	 * - Фундамент для классов, прячущих конкретный тип объекта за общим API.
	 *   Наследники (например, будущий TypeErasedAllocator) расширяют этот
	 *   интерфейс своими операциями над erased-объектом.
	 * - Хранит erased-объект в куче (через член-Allocator) и управляет его
	 *   временем жизни: уничтожение конкретного типа — через не-захватывающую
	 *   функцию-деструктор (fn-ptr "vtable" без виртуальных вызовов на объект).
	 *
	 * Механика:
	 * - Наследник вызывает protected construct<T>(args...) в своём конструкторе:
	 *   allocate(sizeof(T)) -> placement new T -> установка fn-ptr операций
	 *   конкретного T (деструктор + copy/move конструкторы).
	 * - Виртуальный деструктор: производный dtor выполняется первым, затем
	 *   ITypeErased::~ITypeErased уничтожает erased-объект и возвращает память.
	 * - Копирование/перемещение содержимого — хелперы copyConstructFrom/
	 *   moveConstructFrom: свежая аллокация через СОБСТВЕННЫЙ аллокатор +
	 *   copy/move-ctor T в новый блок. Кража указателя недопустима:
	 *   deallocate обязан идти через аллокатор, выделивший память. База
	 *   реализует на этих хелперах свои copy/move ctor'ы и operator=;
	 *   наследники с дополнительным состоянием дополняют их своим.
	 *
	 * Ограничения:
	 * - Копирование/перемещение/присваивание аллоцируют (не noexcept);
	 *   исключений в проекте нет, поэтому при OOM объект становится пустым —
	 *   вызывающий проверяет isEmpty().
	 * - Если T не копируем (move-only) или не перемещаем — соответствующий
	 *   fn-ptr остаётся nullptr, и соответствующая операция даёт пустой
	 *   объект (при move источник не тронут).
	 * - Конструкторы T не должны бросать (исключения запрещены проектом).
	 * - Alignment erased-объекта определяется аллокатором (как и везде в модуле).
	 */
	class ITypeErased
	{
	public:
		/**
		 * Виртуальный деструктор: позволяет удалять наследников через
		 * указатель на базу. После dtor наследника уничтожает erased-объект
		 * и возвращает память аллокатору.
		 */
		virtual ~ITypeErased();

		/**
		 * Конструктор копирования: deep copy содержимого other.
		 *
		 * - Аллокатор НЕ копируется: копирование stateful-Allocator даёт
		 *   «мёртвый» аллокатор (известный дефект Allocator, см. SYSTEM.md) —
		 *   копия использует собственный DefaultAllocator.
		 * - Содержимое — свежая аллокация через copyConstructFrom.
		 * - При OOM или не-копируемом T объект остаётся пустым
		 *   (исключений в проекте нет — вызывающий проверяет isEmpty()).
		 */
		ITypeErased(_In_ const ITypeErased& other)
		{
			this->copyConstructFrom(other);
		}

		/**
		 * Присваивание копированием. Собственный аллокатор не меняется.
		 * Self-assign guard: без него destroyErased() обнулил бы содержимое
		 * до чтения источника.
		 */
		ITypeErased& operator=(_In_ const ITypeErased& other)
		{
			if (this != &other)
				this->copyConstructFrom(other);
			return *this;
		}

		/**
		 * Конструктор перемещения: содержимое переносится через
		 * moveConstructFrom, источник остаётся пустым.
		 *
		 * Аллокатор НЕ переносится и остаётся дефолтным: moveConstructFrom
		 * уничтожает источник через ЕГО аллокатор — чужой забирать нельзя
		 * (deallocate обязан идти через выделивший аллокатор).
		 */
		ITypeErased(_In_ ITypeErased&& other)
		{
			this->moveConstructFrom(other);
		}

		/**
		 * Присваивание перемещением. Self-assign guard, источник пуст.
		 */
		ITypeErased& operator=(_In_ ITypeErased&& other)
		{
			if (this != &other)
				this->moveConstructFrom(other);
			return *this;
		}

		/**
		 * Есть ли внутри erased-объект.
		 */
		bool isEmpty() const noexcept
		{
			return this->pdata == nullptr;
		}

	protected:
		/**
		 * Пустой объект: erased-объекта нет, аллокатор — DefaultAllocator.
		 */
		ITypeErased() noexcept = default;

		/**
		 * Пустой объект с заданным аллокатором для будущего construct().
		 */
		explicit ITypeErased(Allocator&& allocator) noexcept
			: allocator(std::move(allocator))
		{
		}

		// Не-захватывающие функции-операции конкретного T. Память
		// (allocate/deallocate) управляет сам ITypeErased: ему известны
		// аллокатор и размер, поэтому лямбды не захватывают состояние.
		using DestructorFn = void (*)(void*);

		// Копирующий конструктор T: from -> to, где to — уже выделенный
		// блок памяти (sizeof(T)), в котором объект placement-конструируется.
		using CopyConstructorFn = void (*)(const void* from, void* to);

		// Перемещающий конструктор T: from -> to. Исходный объект остаётся
		// жив (move-ctor сам ничего не разрушает) — источник уничтожает
		// хелпер moveConstructFrom уже после переноса.
		using MoveConstructorFn = void (*)(void* from, void* to);

		/**
		 * Сконструировать erased-объект T из аргументов и взять владение им.
		 *
		 * - Если объект уже был — старое содержимое уничтожается (destroyErased).
		 * - При нехватке памяти объект остаётся пустым, возвращается false.
		 */
		template<typename T, typename... TConstructArgs>
		bool construct(TConstructArgs&&... args)
		{
			static_assert(!std::is_base_of<ITypeErased, T>::value,
				"ITypeErased cannot erase an ITypeErased (base is non-copyable)");
			static_assert(!std::is_const<T>::value,
				"ITypeErased cannot erase a const-qualified type");

			// Повторный construct поверх занятого: сначала освобождаем старое.
			this->destroyErased();

			void* mem = this->allocator.allocate(sizeof(T));
			if (__blib_unlikely(mem == nullptr))
			{
				// blib-system не имеет Console — диагностика напрямую в stderr
				fprintf(stderr, "ITypeErased: failed to allocate %zu bytes for erased object\n",
					sizeof(T));
				return false;
			}

			// Placement new: память уже выделена аллокатором, здесь только
			// конструирование (допустимая форма new по правилам проекта).
			new (mem) T(std::forward<TConstructArgs>(args)...);

			this->pdata = mem;
			this->dataSize = sizeof(T);
			// Лямбда без захвата => преобразуется в обычный указатель на функцию.
			this->pDestructor = [](void* ptr)
			{
				static_cast<T*>(ptr)->~T();
			};

			// Copy/move-конструкторы есть не у каждого T (move-only,
			// non-movable типы) — для них fn-ptr остаётся nullptr, и хелперы
			// копирования/перемещения честно возвращают false.
			if constexpr (std::is_copy_constructible<T>::value)
			{
				this->pCopyConstructor = [](const void* from, void* to)
				{
					new (to) T(*static_cast<const T*>(from));
				};
			}
			else
			{
				this->pCopyConstructor = nullptr;
			}

			if constexpr (std::is_move_constructible<T>::value)
			{
				this->pMoveConstructor = [](void* from, void* to)
				{
					new (to) T(std::move(*static_cast<T*>(from)));
				};
			}
			else
			{
				this->pMoveConstructor = nullptr;
			}
			return true;
		}

		/**
		 * Уничтожить erased-объект и вернуть память. Объект становится пустым.
		 */
		void destroyErased() noexcept
		{
			if (this->pdata == nullptr)
				return;

			// Инвариант: pdata != nullptr <=> pDestructor != nullptr
			this->pDestructor(this->pdata);
			this->pDestructor = nullptr;
			this->pCopyConstructor = nullptr;
			this->pMoveConstructor = nullptr;

			this->allocator.deallocate(this->pdata, this->dataSize);
			this->pdata = nullptr;
			this->dataSize = 0;
		}

		/**
		 * Скопировать содержимое other в себя (deep copy).
		 *
		 * - Свежая аллокация через СОБСТВЕННЫЙ аллокатор + copy-ctor T.
		 *   Чужой pdata красть нельзя: deallocate обязан идти через
		 *   аллокатор, выделивший память.
		 * - Копия пустого объекта — валидная пустая копия (true).
		 * - При OOM или не-копируемом T (fn-ptr nullptr) объект остаётся
		 *   пустым, возвращается false.
		 */
		bool copyConstructFrom(_In_ const ITypeErased& other)
		{
			// Поверх занятого: сначала освобождаем своё содержимое.
			this->destroyErased();

			if (other.pdata == nullptr)
				return true;

			if (__blib_unlikely(other.pCopyConstructor == nullptr))
			{
				fprintf(stderr, "ITypeErased: erased type is not copy-constructible\n");
				return false;
			}

			void* mem = this->allocator.allocate(other.dataSize);
			if (__blib_unlikely(mem == nullptr))
			{
				fprintf(stderr, "ITypeErased: failed to allocate %zu bytes for copy\n",
					other.dataSize);
				return false;
			}

			other.pCopyConstructor(other.pdata, mem);

			this->pdata = mem;
			this->dataSize = other.dataSize;
			this->pDestructor = other.pDestructor;
			this->pCopyConstructor = other.pCopyConstructor;
			this->pMoveConstructor = other.pMoveConstructor;
			return true;
		}

		/**
		 * Перенести содержимое other в себя (move). Источник после
		 * переноса пуст.
		 *
		 * - Свежая аллокация через СОБСТВЕННЫЙ аллокатор + move-ctor T,
		 *   затем явное уничтожение источника (move-ctor его не разрушает).
		 * - Перемещение пустого — no-op (true).
		 * - При OOM или не-перемещаемом T (fn-ptr nullptr) объект остаётся
		 *   пустым, источник не тронут, возвращается false.
		 */
		bool moveConstructFrom(_In_ ITypeErased& other)
		{
			this->destroyErased();

			if (other.pdata == nullptr)
				return true;

			if (__blib_unlikely(other.pMoveConstructor == nullptr))
			{
				fprintf(stderr, "ITypeErased: erased type is not move-constructible\n");
				return false;
			}

			// Фиксируем состояние источника ДО переноса: destroyErased()
			// обнулит его размер и fn-ptr.
			const size_t size = other.dataSize;
			const DestructorFn destroyFn = other.pDestructor;
			const CopyConstructorFn copyFn = other.pCopyConstructor;
			const MoveConstructorFn moveFn = other.pMoveConstructor;

			void* mem = this->allocator.allocate(size);
			if (__blib_unlikely(mem == nullptr))
			{
				fprintf(stderr, "ITypeErased: failed to allocate %zu bytes for move\n", size);
				return false;
			}

			other.pMoveConstructor(other.pdata, mem);
			other.destroyErased();

			this->pdata = mem;
			this->dataSize = size;
			this->pDestructor = destroyFn;
			this->pCopyConstructor = copyFn;
			this->pMoveConstructor = moveFn;
			return true;
		}

		// Аллокатор erased-объекта. Хранится в базе, чтобы deallocate
		// выполнялся ровно тем же аллокатором, что и allocate.
		blib::memory::Allocator allocator;

		// Куча: блок sizeof(T), выделенный через allocator.
		void* pdata = nullptr;

		// Не-захватывающие операции конкретного T (nullptr, если объект
		// пуст или операция недоступна для T: move-only, non-movable).
		DestructorFn pDestructor = nullptr;
		CopyConstructorFn pCopyConstructor = nullptr;
		MoveConstructorFn pMoveConstructor = nullptr;

		// Размер блока для deallocate (требование API аллокаторов модуля).
		size_t dataSize = 0;
	};

	inline ITypeErased::~ITypeErased()
	{
		this->destroyErased();
	}

} // namespace memory
} // namespace blib
