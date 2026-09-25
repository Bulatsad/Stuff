#pragma once

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/utilmacro.h>
#include <blib/core/resource/resourceEntry.h>

namespace blib
{
namespace resource
{
	/**
	 * ResourceRef — невладеющий хендл на слот кеша ресурсов
	 * (собственный intrusive refcount-указатель; std-смарты запрещены
	 * правилами проекта).
	 *
	 * Семантика:
	 * - Копирование — +1 к счётчику слота; разрушение/переприсваивание
	 *   — release(); при нуле слот уничтожается владельцем.
	 * - Перемещение — перенос указателя без изменения счётчика,
	 *   источник становится пустым.
	 * - Пустой ref — nullptr-слот: isEmpty(), get<T>() → nullptr.
	 *
	 * Инварианты:
	 * - ref'ы обязаны умереть раньше своего ResourceManager
	 *   (деструктор RM фаталит при живых слотах без ключей);
	 * - ref на слот, снятый с кеша (unload), продолжает держать
	 *   объект живым («осиротевший» слот) — память честно
	 *   освобождается при последнем release.
	 */
	class __blib_core_api ResourceRef
	{
	public:
		// Пустой ref (нет слота)
		ResourceRef() = default;

		~ResourceRef()
		{
			this->reset();
		}

		ResourceRef(_In_ const ResourceRef& other)
			: entry(other.entry)
		{
			if (this->entry != nullptr)
			{
				this->entry->addRef();
			}
		}

		ResourceRef& operator=(_In_ const ResourceRef& other)
		{
			if (this != &other)
			{
				this->reset();
				this->entry = other.entry;
				if (this->entry != nullptr)
				{
					this->entry->addRef();
				}
			}
			return *this;
		}

		// Move: перенос указателя, счётчик не трогается
		ResourceRef(ResourceRef&& other) noexcept
			: entry(other.entry)
		{
			other.entry = nullptr;
		}

		ResourceRef& operator=(ResourceRef&& other) noexcept
		{
			if (this != &other)
			{
				this->reset();
				this->entry = other.entry;
				other.entry = nullptr;
			}
			return *this;
		}

		/**
		 * Пуст ли ref (нет слота).
		 */
		bool isEmpty() const noexcept
		{
			return this->entry == nullptr;
		}

		/**
		 * Типизированный доступ к объекту ресурса.
		 * nullptr при пустом ref или несовпадении тега типа.
		 */
		template<class T>
		T* get() const
		{
			if (this->entry == nullptr)
			{
				return nullptr;
			}
			return this->entry->get<T>();
		}

	private:
		friend class ResourceManager;

		// Создать ref на слот (+1). Только ResourceManager.
		explicit ResourceRef(_In_ ResourceEntry* sourceEntry)
			: entry(sourceEntry)
		{
			if (this->entry != nullptr)
			{
				this->entry->addRef();
			}
		}

		// Снять свой ref: при нуле слот уничтожается владельцем
		void reset() noexcept
		{
			if (this->entry != nullptr)
			{
				this->entry->release();
				this->entry = nullptr;
			}
		}

		ResourceEntry* entry = nullptr;
	};

} // namespace resource
} // namespace blib
