#pragma once

#include <cstddef>
#include <cstring>

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/utilmacro.h>

namespace blib
{
namespace resource
{
	// Размер MD5-дайджеста в байтах (Md5Hasher::digestSize() == 16)
	constexpr buint32 resourceDigestSize = 16;

	// FNV-1a 64-битные константы (public domain) — хэш дайджестов
	// содержимого и тегов типов для ключей unordered_map
	constexpr size_t resourceFnvOffsetBasis = 14695981039346656037ull;
	constexpr size_t resourceFnvPrime = 1099511628211ull;

	// Константы комбинирования хэша дайджеста и хэша тега типа
	// (boost-приём, как comparePairHashShift/FoldShift в icomparable.h)
	constexpr size_t resourceHashCombineShift = 6;
	constexpr size_t resourceHashCombineFoldShift = 2;

	/**
	 * ResourceDigest — MD5-дайджест сериализованного содержимого
	 * ресурса (T::save → MemoryStream → Md5Hasher). Идентичность
	 * содержимого для dedup-индекса кеша ресурсов.
	 */
	struct ResourceDigest
	{
		buint8 bytes[resourceDigestSize];

		bool operator==(_In_ const ResourceDigest& other) const
		{
			return std::memcmp(this->bytes, other.bytes, resourceDigestSize) == 0;
		}

		bool operator!=(_In_ const ResourceDigest& other) const
		{
			return !(*this == other);
		}
	};

	// Хэш дайджеста: FNV-1a над 16 байтами
	struct ResourceDigestHasher
	{
		size_t operator()(_In_ const ResourceDigest& digest) const noexcept
		{
			size_t hash = resourceFnvOffsetBasis;
			for (buint32 i = 0; i < resourceDigestSize; ++i)
			{
				hash ^= static_cast<size_t>(digest.bytes[i]);
				hash *= resourceFnvPrime;
			}
			return hash;
		}
	};

	// FNV-1a над C-строкой тега типа (без учёта адреса литерала)
	inline size_t resourceHashTypeName(_In_ const char* typeName) noexcept
	{
		size_t hash = resourceFnvOffsetBasis;
		while (*typeName != '\0')
		{
			hash ^= static_cast<size_t>(static_cast<unsigned char>(*typeName));
			hash *= resourceFnvPrime;
			++typeName;
		}
		return hash;
	}

	/**
	 * ResourceDataKey — ключ dedup-индекса: дайджест содержимого +
	 * стабильное имя типа. Тег сравнивается ПО СОДЕРЖИМОМУ (strcmp):
	 * литералы T::resourceTypeName из разных модулей (shared DLL)
	 * могут иметь разные адреса — сравнение адресов недопустимо.
	 */
	struct ResourceDataKey
	{
		ResourceDigest digest;
		const char* typeName;

		bool operator==(_In_ const ResourceDataKey& other) const
		{
			return this->digest == other.digest
				&& std::strcmp(this->typeName, other.typeName) == 0;
		}
	};

	struct ResourceDataKeyHasher
	{
		size_t operator()(_In_ const ResourceDataKey& key) const noexcept
		{
			size_t hash = ResourceDigestHasher()(key.digest);
			hash ^= resourceHashTypeName(key.typeName) + resourceFnvPrime
				+ (hash << resourceHashCombineShift) + (hash >> resourceHashCombineFoldShift);
			return hash;
		}
	};

} // namespace resource
} // namespace blib
