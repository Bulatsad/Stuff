#include <blib/core/resource/resourceManager.h>

#include <blib/core/memoryStream.h>

#include <vector>

namespace blib
{
namespace resource
{
	ResourceManager::ResourceManager()
	{
		// Контейнеры инициализированы in-class с адаптером на
		// containerAllocator (объявлен раньше них — см. заголовок)
	}

	ResourceManager::~ResourceManager()
	{
		// Фаталит при слотах, переживших снятие ключей, — утечка
		// внешних ResourceRef'ов (баг вызывающего)
		this->unloadAll();
	}

	ResourceRef ResourceManager::get(_In_ const std::string& key)
	{
		ResourceEntry* entry = this->findEntry(key);
		if (entry == nullptr)
		{
			return ResourceRef();
		}
		return ResourceRef(entry);
	}

	ResourceRef ResourceManager::preload(_In_ const ResourceRef& rf, _In_ blib::core::IInputStream& is)
	{
		ResourceEntry* entry = rf.entry;
		if (entry == nullptr)
		{
			return ResourceRef();
		}

		const blib::core::LoadStatus status = entry->pSaveLoadable->load(is);
		if (status != blib::core::LoadStatus::None)
		{
			__blib_log_error("ResourceManager::preload: load failed (status %u) — slot removed from cache",
				static_cast<buint32>(status));
			// Ref вызывающего держит слот «осиротевшим» до своего
			// освобождения — память не утекает
			this->unloadEntry(entry);
			return ResourceRef();
		}

		return this->commit(rf);
	}

	ResourceRef ResourceManager::commit(_In_ const ResourceRef& rf)
	{
		ResourceEntry* entry = rf.entry;
		if (entry == nullptr)
		{
			return ResourceRef();
		}
		if (entry->committed)
		{
			// Идемпотентность: повторный commit не перехэширует
			return ResourceRef(entry);
		}

		ResourceDigest digest = {};
		if (!this->computeDigest(entry, digest))
		{
			__blib_log_warning("ResourceManager::commit: digest computation failed — slot stays uncommitted");
			return ResourceRef(entry);
		}
		entry->datahash = digest;
		entry->committed = true;

		const ResourceDataKey dataKey{ entry->datahash, entry->typeName };
		const auto it = this->byData.find(dataKey);
		if (it != this->byData.end() && it->second != entry)
		{
			// Дубликат содержимого: ключ источника перемапливается на
			// канонический слот; свежий слот живёт только внешними
			// ref'ами (см. ResourceRef) и умрёт при последнем release
			ResourceEntry* canonical = it->second;
			this->remapEntryKey(entry, canonical);
			__blib_log_warning("ResourceManager::commit: duplicate content — sharing existing slot");
			return ResourceRef(canonical);
		}

		this->byData.emplace(dataKey, entry);
		return ResourceRef(entry);
	}

	ResourceRef ResourceManager::reCommit(_In_ const ResourceRef& rf)
	{
		ResourceEntry* entry = rf.entry;
		if (entry == nullptr)
		{
			return ResourceRef();
		}
		if (!entry->committed)
		{
			// Не-«опечатанный» слот — обычный commit
			return this->commit(rf);
		}

		// Свежий дайджест ВЫЧИСЛЯЕТСЯ ПЕРВЫМ: при неудаче старый
		// индекс не трогаем (остаётся устаревшим, слот цел)
		ResourceDigest newDigest = {};
		if (!this->computeDigest(entry, newDigest))
		{
			__blib_log_warning("ResourceManager::reCommit: digest computation failed — dedup index stays stale");
			return ResourceRef(entry);
		}

		// Снять СТАРУЮ запись индекса только если она указывает на этот
		// слот: слот-дубликат в byData не регистрировался, а по его
		// старому ключу может жить канонический слот (см. destroyEntry)
		const ResourceDataKey oldKey{ entry->datahash, entry->typeName };
		const auto oldIt = this->byData.find(oldKey);
		if (oldIt != this->byData.end() && oldIt->second == entry)
		{
			this->byData.erase(oldIt);
		}

		entry->datahash = newDigest;

		// Новое содержимое совпало с существующим каноническим слотом —
		// перемапить ключ и отпустить этот слот (паттерн commit)
		const ResourceDataKey newKey{ entry->datahash, entry->typeName };
		const auto it = this->byData.find(newKey);
		if (it != this->byData.end() && it->second != entry)
		{
			ResourceEntry* canonical = it->second;
			this->remapEntryKey(entry, canonical);
			__blib_log_warning("ResourceManager::reCommit: new content matches existing slot — sharing it");
			return ResourceRef(canonical);
		}

		this->byData.emplace(newKey, entry);
		return ResourceRef(entry);
	}

	bool ResourceManager::unload(_In_ const std::string& key)
	{
		ResourceKeyString storageKey(key.c_str(),
			blib::memory::StdAllocatorAdapter<char>(&this->containerAllocator));

		const auto it = this->entries.find(storageKey);
		if (it == this->entries.end())
		{
			return false;
		}

		ResourceEntry* entry = it->second;
		this->entries.erase(it);
		entry->release(); // при refCount == 0 уничтожит слот
		return true;
	}

	void ResourceManager::unloadAll()
	{
		// Служебный вектор: по одному элементу на ключ (слот может
		// встречаться несколько раз — алиасы dedup'а). Аллокатор —
		// containerAllocator (правило STL)
		std::vector<ResourceEntry*, blib::memory::StdAllocatorAdapter<ResourceEntry*>> slotPerKey{
			blib::memory::StdAllocatorAdapter<ResourceEntry*>(&this->containerAllocator) };

		// Пасс 1: собрать слоты всех ключей и снять отображения.
		// release() пока не зовём: ключевые ref'ы снимаются пассом 2
		// ПОСЛЕ проверки утечки внешних ref'ов (release при 0
		// уничтожил бы слот раньше проверки)
		for (const auto& kv : this->entries)
		{
			slotPerKey.push_back(kv.second);
		}
		this->entries.clear();

		// Пасс 2: проверка утечки + снятие ключевых ref'ов
		for (size_t i = 0; i < slotPerKey.size(); ++i)
		{
			ResourceEntry* entry = slotPerKey[i];
			if (entry == nullptr)
			{
				continue; // уже обработан как алиас другого ключа
			}

			// Число ключей этого слота (остальные вхождения — алиасы)
			buint32 keyCount = 1;
			for (size_t j = i + 1; j < slotPerKey.size(); ++j)
			{
				if (slotPerKey[j] == entry)
				{
					++keyCount;
					slotPerKey[j] = nullptr;
				}
			}

			// Инвариант: refCount == ключи + внешние ref'ы. Внешние —
			// утечка ref'ов (баг вызывающего; ref'ы обязаны умереть
			// раньше RM)
			const buint32 externalRefs = entry->refCount - keyCount;
			if (externalRefs > 0)
			{
				__blib_fatal("ResourceManager::unloadAll: slot '%s' still held by %u external refs",
					entry->typeName, externalRefs);
			}

			for (buint32 k = 0; k < keyCount; ++k)
			{
				entry->release(); // при нуле уничтожит слот (destroyEntry)
			}
		}
	}

	ResourceEntry* ResourceManager::allocateEntry()
	{
		void* mem = this->resourceAllocator.allocate(sizeof(ResourceEntry));
		if (mem == nullptr)
		{
			__blib_log_error("ResourceManager: failed to allocate resource slot");
			return nullptr;
		}

		// Placement new: блок уже выделен аллокатором (допустимая форма)
		ResourceEntry* entry = new (mem) ResourceEntry(blib::memory::Allocator(this->resourceAllocator));
		entry->owner = this;
		return entry;
	}

	void ResourceManager::destroyEntry(_In_ ResourceEntry* entry)
	{
		if (entry->committed)
		{
			// Снять регистрацию индекса ТОЛЬКО если она указывает на
			// этот слот: у слота-дубликата (remap при commit/reCommit)
			// по дайджесту живёт КАНОНИЧЕСКИЙ слот — безусловный erase
			// потерял бы его запись из dedup-индекса
			const ResourceDataKey dataKey{ entry->datahash, entry->typeName };
			const auto it = this->byData.find(dataKey);
			if (it != this->byData.end() && it->second == entry)
			{
				this->byData.erase(it);
			}
		}

		entry->~ResourceEntry(); // уничтожает erased-объект и его память
		this->resourceAllocator.deallocate(entry, sizeof(ResourceEntry));
	}

	void ResourceManager::unloadEntry(_In_ ResourceEntry* entry)
	{
		// Линейный скан: у слота не более одного ключа (алиасы
		// бывают только у канонических слотов). Операция редкая —
		// unload и провал preload
		for (auto it = this->entries.begin(); it != this->entries.end(); ++it)
		{
			if (it->second == entry)
			{
				this->entries.erase(it);
				entry->release();
				return;
			}
		}
	}

	void ResourceManager::remapEntryKey(_In_ ResourceEntry* from, _In_ ResourceEntry* to)
	{
		for (auto& kv : this->entries)
		{
			if (kv.second == from)
			{
				kv.second = to;
				to->addRef();
				from->release();
				return;
			}
		}
	}

	bool ResourceManager::computeDigest(_In_ ResourceEntry* entry, _Out_ ResourceDigest& outDigest)
	{
		blib::core::MemoryStream serialized;
		const blib::core::SaveStatus status = entry->pSaveLoadable->save(serialized);
		if (status != blib::core::SaveStatus::None)
		{
			__blib_log_warning("ResourceManager::commit: save() failed (status %u)",
				static_cast<buint32>(status));
			return false;
		}

		// hash() читает от текущей позиции до EOF — вернуть в начало
		serialized.seek(0, blib::core::SeekOrigin::Begin);

		blib::core::MemoryStream digestOut;
		if (!this->hasher.hash(serialized, digestOut))
		{
			__blib_log_warning("ResourceManager::commit: MD5 hashing failed");
			return false;
		}

		digestOut.seek(0, blib::core::SeekOrigin::Begin);
		const size_t readBytes = digestOut.read(outDigest.bytes, resourceDigestSize);
		return readBytes == resourceDigestSize;
	}

	ResourceEntry* ResourceManager::findEntry(_In_ const std::string& key)
	{
		ResourceKeyString storageKey(key.c_str(),
			blib::memory::StdAllocatorAdapter<char>(&this->containerAllocator));

		const auto it = this->entries.find(storageKey);
		if (it == this->entries.end())
		{
			return nullptr;
		}
		return it->second;
	}

	void ResourceEntry::release() noexcept
	{
		if (this->refCount == 0)
		{
			// Защита от повторного release (нарушение инварианта —
			// не должно случаться)
			return;
		}

		--this->refCount;
		if (this->refCount == 0 && this->owner != nullptr)
		{
			this->owner->destroyEntry(this);
		}
	}

} // namespace resource
} // namespace blib
