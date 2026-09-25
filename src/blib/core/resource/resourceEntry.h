#pragma once

#include <cstring>
#include <type_traits>
#include <utility>

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/itypeErased.h>
#include <blib/system/memory/iallocatorAware.h>
#include <blib/core/isaveloadable.h>
#include <blib/core/resource/resourceDigest.h>

namespace blib
{
namespace resource
{
	class ResourceManager;

	/**
	 * ResourceEntry — слот кеша ресурсов: один загруженный объект
	 * (type-erased через ITypeErased) + служебные поля.
	 *
	 * Назначение:
	 * - Хранит erased-объект конкретного типа T (требования к T:
	 *   ISaveLoadable + IAllocatorAware + статический тег
	 *   T::resourceTypeName);
	 * - Сохраняет базовые указатели на интерфейсы ISaveLoadable и
	 *   IAllocatorAware (не-шаблонные операции RM: save/load/hash/
	 *   setAllocator);
	 * - Intrusive refcount: refCount = число ключей в entries +
	 *   число живых ResourceRef. При достижении нуля слот
	 *   уничтожается владельцем (ResourceManager::destroyEntry).
	 *
	 * Владение:
	 * - Слот выделяется ResourceManager'ом из его resourceAllocator
	 *   (placement new) и уничтожается только через release() при
	 *   refCount == 0;
	 * - ~ITypeErased уничтожает erased-объект и возвращает его память.
	 *
	 * Некопируем и неперемещаем: живёт по фиксированному адресу,
	 * ResourceRef'ы и оба индекса держат сырой указатель на слот.
	 */
	class __blib_core_api ResourceEntry : public blib::memory::ITypeErased
	{
	public:
		/**
		 * Создать пустой слот с аллокатором erased-объекта.
		 */
		explicit ResourceEntry(blib::memory::Allocator&& allocator)
			: ITypeErased(std::move(allocator))
		{
		}

		/**
		 * Сконструировать объект T внутри слота и взять владение.
		 * Тег типа и базовые указатели ставятся атомарно с
		 * конструированием; при неудаче слот остаётся пустым.
		 */
		template<class T, class... TConstructArgs>
		bool emplace(TConstructArgs&&... args)
		{
			static_assert(std::is_base_of<blib::core::ISaveLoadable, T>::value,
				"ResourceManager: resource type must be blib::core::ISaveLoadable");
			static_assert(std::is_base_of<blib::memory::IAllocatorAware, T>::value,
				"ResourceManager: resource type must be blib::memory::IAllocatorAware");

			this->typeName = T::resourceTypeName;
			if (!this->construct<T>(std::forward<TConstructArgs>(args)...))
			{
				this->typeName = nullptr;
				return false;
			}

			T* pObject = static_cast<T*>(this->pdata);
			this->pSaveLoadable = static_cast<blib::core::ISaveLoadable*>(pObject);
			this->pAllocatorAware = static_cast<blib::memory::IAllocatorAware*>(pObject);
			return true;
		}

		/**
		 * Типизированный доступ к объекту. nullptr при пустом слоте
		 * или несовпадении тега типа (сравнение по содержимому).
		 */
		template<class T>
		T* get() const
		{
			if (this->typeName == nullptr
				|| std::strcmp(this->typeName, T::resourceTypeName) != 0)
			{
				return nullptr;
			}
			return static_cast<T*>(this->pdata);
		}

		/**
		 * +1 к счётчику владельцев (ключ в entries или ResourceRef).
		 */
		void addRef() noexcept
		{
			++this->refCount;
		}

		/**
		 * -1 к счётчику владельцев; при нуле слот уничтожается
		 * владельцем (определение — impl/resourceManager.cpp).
		 */
		void release() noexcept;

		// Стабильное имя типа ресурса (литерал T::resourceTypeName)
		const char* typeName = nullptr;

		// Базовые указатели erased-объекта (не-шаблонные операции)
		blib::core::ISaveLoadable* pSaveLoadable = nullptr;
		blib::memory::IAllocatorAware* pAllocatorAware = nullptr;

		// MD5 сериализованного содержимого; валиден после commit()
		ResourceDigest datahash = {};

		// Слот прошёл commit: зарегистрирован в byData (или признан
		// дубликатом) — повторный commit не перехэширует
		bool committed = false;

		// Ключи в entries + живые ResourceRef'ы
		buint32 refCount = 0;

		// Владелец слота (уничтожение при refCount == 0).
		// Инвариант: ResourceManager переживает все свои слоты
		ResourceManager* owner = nullptr;
	};

} // namespace resource
} // namespace blib
