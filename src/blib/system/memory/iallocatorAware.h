#pragma once

#include <cstddef>

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/allocator.h>

namespace blib
{
namespace memory
{
	/**
	 * IAllocatorAware - интерфейс владения аллокатором.
	 *
	 * Назначение:
	 * - Базовый интерфейс для классов, которые владеют аллокатором
	 *   и обязаны проводить через него ВСЕ свои динамические аллокации
	 *   (контракт документальный: компилятор его не проверяет).
	 * - Позволяет внешнему владельцу (например, кешу ресурсов)
	 *   переопределить источник памяти объекта через setAllocator().
	 *
	 * Механика:
	 * - Член Allocator по значению (SBO, 64 байта). Конструктор по
	 *   умолчанию — DefaultAllocator (прокси к GlobalAllocator), поэтому
	 *   наследник без явной настройки ведёт себя как раньше.
	 * - setAllocator() — копирование = share(): обёртка разделяет
	 *   состояние impl через ref-counting (см. SYSTEM.md, «Allocator»).
	 *   Наследнику неважно, stateless или stateful у него аллокатор —
	 *   семантика едина.
	 *
	 * Контракт наследника:
	 * - Все динамические аллокации/освобождения — только через
	 *   защищённые allocate()/deallocate() (или getAllocator()).
	 * - setAllocator() вызывается ДО первой аллокации объекта:
	 *   deallocate обязан идти через тот же аллокатор, что и allocate.
	 * - Конструкторы наследников не должны аллоцировать динамическую
	 *   память: внешний владелец конструирует объект пустым, затем
	 *   настраивает аллокатор, затем наполняет объект (требование
	 *   кеша ресурсов — см. RESOURCE_MANAGER.md в blib-core).
	 */
	class __blib_system_api IAllocatorAware
	{
	protected:
		// Аллокатор объекта. Настройка — только через setAllocator();
		// доступ наследника к памяти — через allocate()/deallocate().
		blib::memory::Allocator allocator;

	public:
		// Виртуальный деструктор по конвенции интерфейсов blib.
		// Сам интерфейс ничем не владеет: уничтожение объектов
		// выполняется через конкретный тип (как у ITypeErased).
		virtual ~IAllocatorAware() {}

		/**
		 * Установить аллокатор объекта (копирование = share).
		 * Вызывать до первой аллокации.
		 */
		void setAllocator(_In_ const Allocator& newAllocator)
		{
			this->allocator = newAllocator;
		}

		/**
		 * Текущий аллокатор объекта (для адаптеров контейнеров).
		 */
		Allocator& getAllocator()
		{
			return this->allocator;
		}

		const Allocator& getAllocator() const
		{
			return this->allocator;
		}

	protected:
		/**
		 * Выделить блок через аллокатор объекта.
		 */
		void* allocate(size_t size)
		{
			return this->allocator.allocate(size);
		}

		/**
		 * Освободить блок через аллокатор объекта.
		 */
		void deallocate(_In_ void* ptr, size_t size)
		{
			this->allocator.deallocate(ptr, size);
		}
	};

} // namespace memory
} // namespace blib
