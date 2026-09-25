#pragma once

#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/allocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>
#include <blib/core/istream.h>
#include <blib/core/console/console.h>
#include <blib/core/algorithm/hash/md5Hasher.h>
#include <blib/core/resource/resourceDigest.h>
#include <blib/core/resource/resourceEntry.h>
#include <blib/core/resource/resourceRef.h>

namespace blib
{
namespace resource
{
	/**
	 * ResourceManager — кеш владеемых ресурсов (ISaveLoadable) с
	 * dedup'ом по содержимому и refcount-доступом.
	 *
	 * Назначение:
	 * - Хранит загруженные объекты по строковым ключам; каждый объект
	 *   обязан быть ISaveLoadable + IAllocatorAware (см. ResourceEntry);
	 * - Dedup: одинаковое сериализованное содержимое (MD5 save()-формы)
	 *   одного типа под разными ключами разделяет один слот;
	 * - Время жизни: ключ или ResourceRef держат слот; слот живёт,
	 *   пока refCount > 0 («осиротевшие» слоты после unload живут до
	 *   последнего внешнего ref'а).
	 *
	 * Поток использования:
	 *   ResourceRef rf = rm.construct<Mesh>("tiles");
	 *   buildMesh(*rf.get<Mesh>());            // наполнение (native/процедурное)
	 *   rf = rm.commit(rf);                    // hash + dedup + byData
	 *   // либо сериализованный источник:
	 *   ResourceRef rf2 = rm.construct<Mesh>("tiles2");
	 *   rf2 = rm.preload(rf2, stream);         // load(is) + commit
	 *
	 * ВАЖНО: RM не трогает файлы и пути — загрузка контента целиком
	 * на стороне вызывающего (loadFromFile у типа или поток).
	 *
	 * Ограничения:
	 * - Не thread-safe (все операции — на одном потоке);
	 * - Некопируем/неперемещаем: контейнеры держат указатель на
	 *   containerAllocator-член (см. StdAllocatorAdapter);
	 * - Все ResourceRef'ы обязаны умереть раньше RM: деструктор
	 *   фаталит при слотах, переживших unloadAll (утечка ref'ов);
	 * - Ключи — std::string на входе (внутри — basic_string с
	 *   blib-аллокатором, канон scene.h).
	 */
	class __blib_core_api ResourceManager
	{
	public:
		// Строковый ключ с blib-аллокатором (канон scene.h:
		// ComponentTypeNameString) — ключ словаря entries
		using ResourceKeyString = std::basic_string<char, std::char_traits<char>,
			blib::memory::StdAllocatorAdapter<char>>;

		// Адаптер STL-контейнеров к blib-аллокатору
		template<typename U>
		using ContainerAllocator = blib::memory::StdAllocatorAdapter<U>;

		ResourceManager();
		~ResourceManager();

		ResourceManager(const ResourceManager&) = delete;
		ResourceManager& operator=(const ResourceManager&) = delete;
		ResourceManager(ResourceManager&&) = delete;
		ResourceManager& operator=(ResourceManager&&) = delete;

		/**
		 * Создать слот ресурса типа T под ключом key и вернуть ref.
		 *
		 * - Ключ уже занят — идемпотентно: warning + ref на
		 *   существующий слот (создание игнорируется);
		 * - Объект конструируется пустым в слоте (ctor'ы не аллоцируют),
		 *   затем получает аллокатор RM (setAllocator) и наполняется
		 *   вызывающим через get<T>(); завершение — commit()/preload();
		 * - При нехватке памяти — пустой ref + ошибка в консоли.
		 *
		 * @tparam T Тип ресурса: ISaveLoadable + IAllocatorAware +
		 *         статический тег T::resourceTypeName
		 */
		template<class T, class... TConstructArgs>
		ResourceRef construct(_In_ const std::string& key, TConstructArgs&&... args)
		{
			static_assert(std::is_base_of<blib::core::ISaveLoadable, T>::value,
				"ResourceManager: resource type must be blib::core::ISaveLoadable");
			static_assert(std::is_base_of<blib::memory::IAllocatorAware, T>::value,
				"ResourceManager: resource type must be blib::memory::IAllocatorAware");

			ResourceEntry* existing = this->findEntry(key);
			if (existing != nullptr)
			{
				__blib_log_warning("ResourceManager::construct: key '%s' already cached — returning existing slot", key.c_str());
				return ResourceRef(existing);
			}

			ResourceEntry* entry = this->allocateEntry();
			if (entry == nullptr)
			{
				return ResourceRef();
			}

			if (!entry->emplace<T>(std::forward<TConstructArgs>(args)...))
			{
				this->destroyEntry(entry);
				return ResourceRef();
			}

			// Аллокатор RM раздаётся объекту копией-share: все ресурсы
			// делят один impl (см. IAllocatorAware)
			entry->pAllocatorAware->setAllocator(this->resourceAllocator);

			ResourceKeyString storageKey(key.c_str(),
				blib::memory::StdAllocatorAdapter<char>(&this->containerAllocator));
			this->entries.emplace(std::move(storageKey), entry);
			entry->addRef(); // ref ключа

			return ResourceRef(entry);
		}

		/**
		 * Ref на слот по ключу (+1). Пустой ref — ключа нет.
		 * Не на горячем пути (объект кешируется вызывающим после
		 * первого получения).
		 */
		ResourceRef get(_In_ const std::string& key);

		/**
		 * Заполнить слот из сериализованного потока и «опечатать»
		 * его: load(is) + commit (hash + dedup + byData).
		 *
		 * - Неудача загрузки: слот снимается с кеша (ref вызывающего
		 *   держит его «осиротевшим»), возвращается пустой ref;
		 * - Дубликат: возвращается ref на канонический слот, ключ
		 *   перемапливается — паттерн `rf = rm.preload(rf, stream);`.
		 */
		ResourceRef preload(_In_ const ResourceRef& rf, _In_ blib::core::IInputStream& is);

		/**
		 * «Опечатать» слот: MD5 сериализованной формы → dedup по
		 * (digest, тег типа) → регистрация в byData.
		 *
		 * - Дубликат: ключ перемапливается на канонический слот,
		 *   возвращается ref на него (свежий слот умрёт после
		 *   переприсваивания ref'а вызывающим);
		 * - Идемпотентно: повторный commit возвращает тот же слот
		 *   без перехэширования;
		 * - Неудача save/hash: warning, слот остаётся не-«опечатанным»
		 *   (dedup не участвует), возвращается ref на него же.
		 */
		ResourceRef commit(_In_ const ResourceRef& rf);

		/**
		 * Снять ключ со слота. Внешние ResourceRef'ы продолжают
		 * держать объект. @return false, если ключа нет.
		 */
		bool unload(_In_ const std::string& key);

		/**
		 * Снять все ключи. Фатальная ошибка (abort), если какой-то
		 * слот пережил снятие ключей — утечка внешних ResourceRef'ов
		 * (баг вызывающего; ref'ы обязаны умереть раньше RM).
		 */
		void unloadAll();

	private:
		friend class ResourceEntry;

		// Выделить пустой слот из resourceAllocator (placement new)
		ResourceEntry* allocateEntry();

		// Уничтожить слот: стереть из byData, разрушить erased-объект,
		// вернуть память. Только при refCount == 0 (release)
		void destroyEntry(_In_ ResourceEntry* entry);

		// Снять ключ слота и release (провал preload): слот может
		// пережить как «осиротевший», если его держит внешний ref
		void unloadEntry(_In_ ResourceEntry* entry);

		// Dedup: перемапить ключ слота-источника на канонический слот
		void remapEntryKey(_In_ ResourceEntry* from, _In_ ResourceEntry* to);

		// MD5 сериализованной формы слота (save → MemoryStream → hash)
		bool computeDigest(_In_ ResourceEntry* entry, _Out_ ResourceDigest& outDigest);

		// Поиск слота по ключу (внутренняя версия для construct)
		ResourceEntry* findEntry(_In_ const std::string& key);

		// Аллокатор служебных контейнеров. Объявлен ПЕРЕД контейнерами:
		// они хранят указатель на него (StdAllocatorAdapter не владеет)
		blib::memory::Allocator containerAllocator;

		// Аллокатор ресурсов: раздаётся объектам (копии-share) и
		// используется для слотов. DefaultAllocator → GlobalAllocator
		blib::memory::Allocator resourceAllocator;

		// MD5-хэшер сериализованных форм (не аллоцирует, stack-only)
		blib::algorithm::hash::Md5Hasher hasher;

		// Ключ → слот. Узел стабилен: слот живёт по фиксированному
		// адресу (heap), map хранит только указатели
		std::unordered_map<ResourceKeyString, ResourceEntry*,
			std::hash<ResourceKeyString>, std::equal_to<ResourceKeyString>,
			ContainerAllocator<std::pair<const ResourceKeyString, ResourceEntry*>>> entries{
				ContainerAllocator<std::pair<const ResourceKeyString, ResourceEntry*>>(&this->containerAllocator) };

		// Dedup-индекс: (дайджест, тег типа) → канонический слот
		std::unordered_map<ResourceDataKey, ResourceEntry*,
			ResourceDataKeyHasher, std::equal_to<ResourceDataKey>,
			ContainerAllocator<std::pair<const ResourceDataKey, ResourceEntry*>>> byData{
				ContainerAllocator<std::pair<const ResourceDataKey, ResourceEntry*>>(&this->containerAllocator) };
	};

} // namespace resource
} // namespace blib
