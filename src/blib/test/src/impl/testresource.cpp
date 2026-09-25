#include <blib/test/src/test.h>

// ResourceManager: кеш ISaveLoadable-ресурсов — construct/commit/preload,
// dedup по содержимому (MD5 сериализованной формы), refcount-доступ
// (ResourceRef), unload/unloadAll, теги типов
#include <blib/blibint.h>
#include <blib/core/resource/resourceManager.h>
#include <blib/core/memoryStream.h>
#include <blib/core/binaryWriter.h>
#include <blib/core/binaryReader.h>
#include <blib/core/verifyHelper.h>
#include <blib/system/memory/iallocatorAware.h>

using namespace blib;

// ---------------------------------------------------------------------
// Фикстура: минимальный ресурс — ISaveLoadable + IAllocatorAware.
// Сериализуемая форма — одно buint32 (BinaryWriter LE); статический
// счётчик живых объектов для проверки отсутствия утечек
// ---------------------------------------------------------------------
struct TestAsset : public blib::core::ISaveLoadable, public blib::memory::IAllocatorAware
{
	static constexpr const char* resourceTypeName = "test.Asset";

	static buint32 g_liveCount;

	buint32 value;

	TestAsset()
		: value(0)
	{
		++g_liveCount;
	}

	~TestAsset() override
	{
		--g_liveCount;
	}

	// Не прятать 1-аргументную точку входа строгого сравнения
	using blib::core::IStrongComparable::strongCompare;

	blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const override
	{
		blib::core::BinaryWriter writer(os);
		if (!writer.writeU32LE(this->value))
		{
			return blib::core::SaveStatus::WriteFailed;
		}
		return blib::core::SaveStatus::None;
	}

	blib::core::LoadStatus load(_In blib::core::IInputStream& is) override
	{
		blib::core::BinaryReader reader(is);
		buint32 loadedValue = 0;
		if (!reader.readU32LE(loadedValue))
		{
			return blib::core::LoadStatus::ReadFailed;
		}
		this->value = loadedValue;
		return blib::core::LoadStatus::None;
	}

	bool strongCompare(_In const blib::core::IStrongComparable& other,
		_In blib::core::CompareSession& session) const override
	{
		if (!session.enter(this, &other))
		{
			return true;
		}
		const TestAsset& rhs = static_cast<const TestAsset&>(other);
		return this->value == rhs.value;
	}

	bool verify() const override
	{
		return blib::core::verifyRoundTrip(*this);
	}
};

buint32 TestAsset::g_liveCount = 0;

// Второй тип с другим тегом — проверка несовпадения типа в get<T>()
struct TestAssetOther : public blib::core::ISaveLoadable, public blib::memory::IAllocatorAware
{
	static constexpr const char* resourceTypeName = "test.AssetOther";

	buint32 value = 0;

	// Не прятать 1-аргументную точку входа строгого сравнения
	using blib::core::IStrongComparable::strongCompare;

	blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const override
	{
		blib::core::BinaryWriter writer(os);
		return writer.writeU32LE(this->value)
			? blib::core::SaveStatus::None
			: blib::core::SaveStatus::WriteFailed;
	}

	blib::core::LoadStatus load(_In blib::core::IInputStream& is) override
	{
		blib::core::BinaryReader reader(is);
		return reader.readU32LE(this->value)
			? blib::core::LoadStatus::None
			: blib::core::LoadStatus::ReadFailed;
	}

	bool strongCompare(_In const blib::core::IStrongComparable& other,
		_In blib::core::CompareSession& session) const override
	{
		if (!session.enter(this, &other))
		{
			return true;
		}
		const TestAssetOther& rhs = static_cast<const TestAssetOther&>(other);
		return this->value == rhs.value;
	}

	bool verify() const override
	{
		return blib::core::verifyRoundTrip(*this);
	}
};

BLIB_TEST_CASE("ResourceManager: construct + commit + get roundtrip")
{
	{
		blib::resource::ResourceManager rm;

		blib::resource::ResourceRef rf = rm.construct<TestAsset>("asset.a");
		BLIB_TEST_CHECK(!rf.isEmpty());

		TestAsset* pAsset = rf.get<TestAsset>();
		BLIB_TEST_REQUIRE(pAsset != nullptr);
		pAsset->value = 42;

		blib::resource::ResourceRef committed = rm.commit(rf);
		BLIB_TEST_CHECK(!committed.isEmpty());
		BLIB_TEST_CHECK(committed.get<TestAsset>() == pAsset);

		// Повторное получение по ключу — тот же объект и содержимое
		blib::resource::ResourceRef again = rm.get("asset.a");
		BLIB_TEST_CHECK(!again.isEmpty());
		BLIB_TEST_CHECK(again.get<TestAsset>() == pAsset);
		BLIB_TEST_CHECK(again.get<TestAsset>()->value == 42);

		// Идемпотентность commit: повторный вызов не перехэширует
		blib::resource::ResourceRef reCommit = rm.commit(rf);
		BLIB_TEST_CHECK(reCommit.get<TestAsset>() == pAsset);
	}

	// RM уничтожен — слот и payload разрушены без утечек
	BLIB_TEST_CHECK(TestAsset::g_liveCount == 0);
}

BLIB_TEST_CASE("ResourceManager: dedup одинакового содержимого под разными ключами")
{
	{
		blib::resource::ResourceManager rm;

		blib::resource::ResourceRef first = rm.construct<TestAsset>("a");
		first.get<TestAsset>()->value = 7;
		first = rm.commit(first);

		// Второй ключ, то же содержимое: commit вернёт канонический слот
		blib::resource::ResourceRef second = rm.construct<TestAsset>("b");
		second.get<TestAsset>()->value = 7;
		second = rm.commit(second);

		BLIB_TEST_CHECK(!first.isEmpty());
		BLIB_TEST_CHECK(!second.isEmpty());
		BLIB_TEST_CHECK(first.get<TestAsset>() == second.get<TestAsset>());

		// Оба ключа ведут на один объект
		BLIB_TEST_CHECK(rm.get("a").get<TestAsset>() == rm.get("b").get<TestAsset>());

		// Осиротевший свежий слот уничтожается после переприсваивания ref'а
		BLIB_TEST_CHECK(TestAsset::g_liveCount == 1);
	}

	BLIB_TEST_CHECK(TestAsset::g_liveCount == 0);
}

BLIB_TEST_CASE("ResourceManager: разное содержимое — разные слоты")
{
	blib::resource::ResourceManager rm;

	blib::resource::ResourceRef first = rm.construct<TestAsset>("a");
	first.get<TestAsset>()->value = 1;
	first = rm.commit(first);

	blib::resource::ResourceRef second = rm.construct<TestAsset>("b");
	second.get<TestAsset>()->value = 2;
	second = rm.commit(second);

	BLIB_TEST_CHECK(!first.isEmpty());
	BLIB_TEST_CHECK(!second.isEmpty());
	BLIB_TEST_CHECK(first.get<TestAsset>() != second.get<TestAsset>());
}

BLIB_TEST_CASE("ResourceManager: get отсутствующего ключа — пустой ref")
{
	blib::resource::ResourceManager rm;

	blib::resource::ResourceRef missing = rm.get("no.such.key");
	BLIB_TEST_CHECK(missing.isEmpty());
	BLIB_TEST_CHECK(missing.get<TestAsset>() == nullptr);
}

BLIB_TEST_CASE("ResourceManager: construct по занятому ключу идемпотентен")
{
	blib::resource::ResourceManager rm;

	blib::resource::ResourceRef first = rm.construct<TestAsset>("x");
	blib::resource::ResourceRef second = rm.construct<TestAsset>("x");

	BLIB_TEST_CHECK(!first.isEmpty());
	BLIB_TEST_CHECK(!second.isEmpty());
	BLIB_TEST_CHECK(first.get<TestAsset>() == second.get<TestAsset>());
	BLIB_TEST_CHECK(TestAsset::g_liveCount == 1);
}

BLIB_TEST_CASE("ResourceManager: unload снимает ключ, внешний ref держит слот")
{
	{
		blib::resource::ResourceManager rm;

		blib::resource::ResourceRef rf = rm.construct<TestAsset>("k");
		rf.get<TestAsset>()->value = 9;
		rf = rm.commit(rf);

		BLIB_TEST_CHECK(rm.unload("k"));
		BLIB_TEST_CHECK(rm.get("k").isEmpty());

		// «Осиротевший» слот живёт внешним ref'ом
		BLIB_TEST_CHECK(!rf.isEmpty());
		BLIB_TEST_CHECK(rf.get<TestAsset>()->value == 9);

		// Последний ref — слот уничтожается
		rf = blib::resource::ResourceRef();
		BLIB_TEST_CHECK(TestAsset::g_liveCount == 0);
	}

	BLIB_TEST_CHECK(TestAsset::g_liveCount == 0);
}

BLIB_TEST_CASE("ResourceManager: unload несуществующего ключа — false")
{
	blib::resource::ResourceManager rm;
	BLIB_TEST_CHECK(!rm.unload("missing"));
}

BLIB_TEST_CASE("ResourceManager: preload из сериализованного потока")
{
	{
		// Эталон сериализуем в память
		TestAsset reference;
		reference.value = 1234;

		blib::core::MemoryStream stream;
		BLIB_TEST_REQUIRE(reference.save(stream) == blib::core::SaveStatus::None);
		stream.seek(0, blib::core::SeekOrigin::Begin);

		blib::resource::ResourceManager rm;
		blib::resource::ResourceRef rf = rm.construct<TestAsset>("streamed");
		rf = rm.preload(rf, stream);

		BLIB_TEST_CHECK(!rf.isEmpty());
		BLIB_TEST_REQUIRE(rf.get<TestAsset>() != nullptr);
		BLIB_TEST_CHECK(rf.get<TestAsset>()->value == 1234);
	}

	BLIB_TEST_CHECK(TestAsset::g_liveCount == 0);
}

BLIB_TEST_CASE("ResourceManager: preload при битом потоке — слот снят с кеша")
{
	{
		blib::core::MemoryStream emptyStream; // пусто: readU32LE провалится

		blib::resource::ResourceManager rm;
		blib::resource::ResourceRef rf = rm.construct<TestAsset>("broken");

		blib::resource::ResourceRef result = rm.preload(rf, emptyStream);
		BLIB_TEST_CHECK(result.isEmpty());
		BLIB_TEST_CHECK(rm.get("broken").isEmpty());

		// rf держит «осиротевший» слот до своего освобождения
		rf = blib::resource::ResourceRef();
	}

	BLIB_TEST_CHECK(TestAsset::g_liveCount == 0);
}

BLIB_TEST_CASE("ResourceManager: несовпадение тега типа — get<T> даёт nullptr")
{
	blib::resource::ResourceManager rm;

	blib::resource::ResourceRef rf = rm.construct<TestAsset>("tagged");
	BLIB_TEST_REQUIRE(rf.get<TestAsset>() != nullptr);
	BLIB_TEST_CHECK(rf.get<TestAssetOther>() == nullptr);
}

BLIB_TEST_CASE("ResourceManager: unloadAll очищает кеш, RM пригоден к повторному использованию")
{
	{
		blib::resource::ResourceManager rm;

		blib::resource::ResourceRef first = rm.construct<TestAsset>("a");
		first.get<TestAsset>()->value = 1;
		first = rm.commit(first);

		blib::resource::ResourceRef second = rm.construct<TestAsset>("b");
		second.get<TestAsset>()->value = 2;
		second = rm.commit(second);

		// Все внешние ref'ы сняты до unloadAll — утечек быть не должно
		first = blib::resource::ResourceRef();
		second = blib::resource::ResourceRef();

		rm.unloadAll();
		BLIB_TEST_CHECK(rm.get("a").isEmpty());
		BLIB_TEST_CHECK(rm.get("b").isEmpty());
		BLIB_TEST_CHECK(TestAsset::g_liveCount == 0);

		// Кеш снова пригоден
		blib::resource::ResourceRef fresh = rm.construct<TestAsset>("a");
		BLIB_TEST_CHECK(!fresh.isEmpty());
	}

	BLIB_TEST_CHECK(TestAsset::g_liveCount == 0);
}
