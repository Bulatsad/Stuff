#include <blib/test/src/test.h>

// JSON: DOM-значение, парсер, writer
#include <blib/core/json/json.h>

// Общие хелперы группы json (разбор/запись/round-trip, тест-потоки)
#include <blib/test/src/impl/jsonTestUtils.h>

// Потоки для интеграционных проверок
#include <blib/core/memoryStream.h>

// Статистика аллокаций для проверки утечек
#include <blib/system/memory/globalAllocator.h>

#include <cstring>
#include <cmath>
#include <limits>
#include <string>

using namespace blib;
using namespace blib::core;
using namespace blib::core::json;
using namespace blib::memory;

// ============================================================
// Вспомогательные функции
// ============================================================

namespace
{
    /**
     * Поток, у которого любая запись проваливается (write == 0) —
     * для проверки JsonError::StreamError.
     */
    class FailingOutputStream : public IOutputStream
    {
    public:
        size_t write(_In const void* data, size_t size) __blib_override
        {
            (void)data;
            (void)size;
            return 0;
        }

        bool canSeek() const __blib_override { return false; }
        bool seek(bint64 offset, SeekOrigin origin) __blib_override { (void)offset; (void)origin; return false; }
        buint64 tell() const __blib_override { return 0; }
        buint64 size() const __blib_override { return 0; }
    };
}

// ============================================================
// Типы bfloat/bdouble
// ============================================================

BLIB_TEST_CASE("json_bfloat_bdouble_types")
{
    static_assert(sizeof(bfloat) == 4, "bfloat must be 4 bytes");
    static_assert(sizeof(bdouble) == 8, "bdouble must be 8 bytes");
    BLIB_TEST_CHECK(sizeof(bfloat) == sizeof(float));
    BLIB_TEST_CHECK(sizeof(bdouble) == sizeof(double));
}

// ============================================================
// Конструкторы JsonValue
// ============================================================

BLIB_TEST_CASE("json_value_constructors")
{
    // Null
    JsonValue n;
    BLIB_TEST_CHECK(n.isNull());
    BLIB_TEST_CHECK(n.type() == JsonType::Null);

    JsonValue nn(nullptr);
    BLIB_TEST_CHECK(nn.isNull());

    // Bool
    JsonValue b(true);
    BLIB_TEST_CHECK(b.isBool());
    BLIB_TEST_CHECK(b.asBool());

    JsonValue f(false);
    BLIB_TEST_CHECK(f.isBool());
    BLIB_TEST_CHECK(!f.asBool());

    // Целые числа
    JsonValue i(42);
    BLIB_TEST_CHECK(i.isNumber());
    BLIB_TEST_CHECK(i.asBint64() == 42);
    BLIB_TEST_CHECK(i.asBuint64() == 42u);
    BLIB_TEST_CHECK(i.asBdouble() == 42.0);

    JsonValue u(42u);
    BLIB_TEST_CHECK(u.asBuint64() == 42u);

    JsonValue neg(-5);
    BLIB_TEST_CHECK(neg.asBint64() == -5);

    JsonValue big(9223372036854775807LL);
    BLIB_TEST_CHECK(big.asBint64() == (std::numeric_limits<bint64>::max)());

    JsonValue ubig(18446744073709551615ULL);
    BLIB_TEST_CHECK(ubig.asBuint64() == (std::numeric_limits<buint64>::max)());

    // Вещественные числа
    JsonValue fl(0.5f);
    BLIB_TEST_CHECK(fl.asBdouble() == 0.5);
    BLIB_TEST_CHECK(fl.asBfloat() == 0.5f);

    JsonValue d(2.5);
    BLIB_TEST_CHECK(d.asBdouble() == 2.5);

    // Строки
    JsonValue s("hello");
    BLIB_TEST_CHECK(s.isString());
    BLIB_TEST_CHECK(s.asString() == "hello");

    JsonValue es("");
    BLIB_TEST_CHECK(es.asString().empty());

    // nullptr — защитно трактуется как пустая строка (задокументировано)
    JsonValue nullStr(static_cast<const char*>(nullptr));
    BLIB_TEST_CHECK(nullStr.isString());
    BLIB_TEST_CHECK(nullStr.asString().empty());

    // сравнение
    BLIB_TEST_CHECK(n == nn);
    BLIB_TEST_CHECK(!(n == b));
}

// ============================================================
// Конструктор из целочисленных типов (SFINAE: любой интегральный, кроме bool)
// ============================================================

BLIB_TEST_CASE("json_value_integral_template")
{
    // знаковые типы хранятся как bint64
    JsonValue i8(static_cast<bint8>(-7));
    BLIB_TEST_CHECK(i8.isNumber());
    BLIB_TEST_CHECK(i8.asBint64() == -7);
    BLIB_TEST_CHECK(i8.asBuint64() == static_cast<buint64>(-7));

    JsonValue i16(static_cast<bint16>(-300));
    BLIB_TEST_CHECK(i16.asBint64() == -300);

    JsonValue ch('A');
    BLIB_TEST_CHECK(ch.asBint64() == 65);

    JsonValue sh(static_cast<short>(-1));
    BLIB_TEST_CHECK(sh.asBint64() == -1);

    JsonValue lo(123456789L);
    BLIB_TEST_CHECK(lo.asBint64() == 123456789);

    // беззнаковые типы хранятся как buint64
    JsonValue u8(static_cast<buint8>(200));
    BLIB_TEST_CHECK(u8.asBuint64() == 200);

    JsonValue u16(static_cast<buint16>(60000));
    BLIB_TEST_CHECK(u16.asBuint64() == 60000);

    JsonValue ush(static_cast<unsigned short>(1));
    BLIB_TEST_CHECK(ush.asBuint64() == 1u);

    JsonValue ulo(42UL);
    BLIB_TEST_CHECK(ulo.asBuint64() == 42u);

    // bool не попадает в интегральный шаблон (отдельный конструктор)
    JsonValue truth(true);
    BLIB_TEST_CHECK(truth.isBool());
}

// ============================================================
// Доступ к скалярам: приведения и размер не-контейнеров
// ============================================================

BLIB_TEST_CASE("json_value_scalar_accessors")
{
    // приведение вещественных к целым (не-integer путь asBint64/asBuint64)
    JsonValue pos(2.5);
    BLIB_TEST_CHECK(pos.asBint64() == 2);
    BLIB_TEST_CHECK(pos.asBuint64() == 2u);
    BLIB_TEST_CHECK(pos.asBdouble() == 2.5);
    BLIB_TEST_CHECK(pos.asBfloat() == 2.5f);

    JsonValue neg(-2.5);
    BLIB_TEST_CHECK(neg.asBint64() == -2);

    // asBuint64 на отрицательном целом — битовое приведение
    JsonValue minusOne(-1);
    BLIB_TEST_CHECK(minusOne.asBuint64() == (std::numeric_limits<buint64>::max)());

    // asBint64/asBdouble на беззнаковом
    JsonValue ubig((std::numeric_limits<buint64>::max)());
    BLIB_TEST_CHECK(ubig.asBint64() == -1);
    BLIB_TEST_CHECK(ubig.asBdouble() == static_cast<bdouble>((std::numeric_limits<buint64>::max)()));

    // asBfloat на целом
    JsonValue i(7);
    BLIB_TEST_CHECK(i.asBfloat() == 7.0f);

    // size()/empty() у не-контейнерных типов — 0/true
    BLIB_TEST_CHECK(JsonValue(nullptr).size() == 0);
    BLIB_TEST_CHECK(JsonValue(true).empty());
    BLIB_TEST_CHECK(JsonValue(42).size() == 0);
    BLIB_TEST_CHECK(JsonValue("abc").empty());

    // ссылочные аксессоры
    JsonValue s("abc");
    const JsonValue::String& str = s.asString();
    BLIB_TEST_CHECK(str == "abc");

    JsonValue arr = JsonValue::makeArray();
    arr.pushBack(1);
    const JsonValue::Array& aref = arr.asArray();
    BLIB_TEST_CHECK(aref.size() == 1);

    JsonValue obj = JsonValue::makeObject();
    obj.set("k", 1);
    const JsonValue::Object& oref = obj.asObject();
    BLIB_TEST_CHECK(oref.size() == 1);
    BLIB_TEST_CHECK(oref[0].first == "k");
}

// ============================================================
// Копирование и перемещение
// ============================================================

BLIB_TEST_CASE("json_copy_move")
{
    JsonValue v1 = JsonValue::makeArray();
    v1.pushBack(1);
    v1.pushBack(2);

    // глубокое копирование: изменение копии не трогает оригинал
    JsonValue v2(v1);
    v2.pushBack(3);
    BLIB_TEST_CHECK(v1.size() == 2);
    BLIB_TEST_CHECK(v2.size() == 3);
    BLIB_TEST_CHECK(v2[0] == v1[0]);
    BLIB_TEST_CHECK(v2[1] == v1[1]);

    // copy-assign
    JsonValue v3;
    v3 = v1;
    BLIB_TEST_CHECK(v3 == v1);
    v3.pushBack(42);
    BLIB_TEST_CHECK(v1.size() == 2);

    // move: источник становится Null
    JsonValue v4(std::move(v3));
    BLIB_TEST_CHECK(v4.size() == 3);
    BLIB_TEST_CHECK(v3.isNull());

    // move-assign
    JsonValue v5;
    v5 = std::move(v4);
    BLIB_TEST_CHECK(v5.size() == 3);
    BLIB_TEST_CHECK(v4.isNull());

    // moved-from можно использовать заново (присваивание)
    v4 = v5;
    BLIB_TEST_CHECK(v4 == v5);

    // самоприсваивание копированием — no-op
    JsonValue self = JsonValue::makeArray();
    self.pushBack(7);
    JsonValue* selfPtr = &self;
    self = self;
    BLIB_TEST_CHECK(&self == selfPtr);
    BLIB_TEST_CHECK(self.size() == 1);

    // самоприсваивание перемещением — no-op
    JsonValue& selfRef = self;
    self = std::move(selfRef);
    BLIB_TEST_CHECK(self.size() == 1);
    BLIB_TEST_CHECK(self[0].asBint64() == 7);

    // глубокая копия всех шести типов значений
    JsonValue all = JsonValue::makeArray();
    all.pushBack(JsonValue(nullptr));
    all.pushBack(JsonValue(true));
    all.pushBack(JsonValue(42));
    all.pushBack(JsonValue(2.5));
    all.pushBack(JsonValue("str"));
    all.pushBack(JsonValue::makeObject());
    JsonValue allCopy(all);
    BLIB_TEST_CHECK(allCopy == all);

    // копия вложенного объекта независима
    JsonValue obj = JsonValue::makeObject();
    obj.set("x", 1);
    JsonValue objCopy(obj);
    objCopy.set("x", 2);
    BLIB_TEST_CHECK(obj.get("x").asBint64() == 1);
    BLIB_TEST_CHECK(objCopy.get("x").asBint64() == 2);

    // разбор в moved-from значение: старое содержимое уничтожается
    JsonValue src = JsonValue::makeObject();
    src.set("k", 1);
    JsonValue target(std::move(src));
    BLIB_TEST_CHECK(src.isNull());
    JsonParser parser;
    BLIB_TEST_REQUIRE(parser.parse("{\"a\":2}", 7, target) == JsonError::None);
    BLIB_TEST_CHECK(target.isObject());
    BLIB_TEST_CHECK(target.size() == 1);
    BLIB_TEST_CHECK(target.get("a").asBint64() == 2);
}

// ============================================================
// Мутация
// ============================================================

BLIB_TEST_CASE("json_mutation")
{
    // массив
    JsonValue arr = JsonValue::makeArray();
    arr.pushBack(1);
    arr.pushBack(2);
    arr.pushBack(3);
    BLIB_TEST_CHECK(arr.size() == 3);

    arr.remove(1);
    BLIB_TEST_CHECK(arr.size() == 2);
    BLIB_TEST_CHECK(arr[0].asBint64() == 1);
    BLIB_TEST_CHECK(arr[1].asBint64() == 3);

    arr[0] = JsonValue(10);
    BLIB_TEST_CHECK(arr[0].asBint64() == 10);

    arr.clear();
    BLIB_TEST_CHECK(arr.empty());

    // краевые remove: первый и последний элемент
    JsonValue arr2 = JsonValue::makeArray();
    arr2.pushBack(1);
    arr2.pushBack(2);
    arr2.pushBack(3);
    arr2.remove(0u); // 0u: литерал 0 неоднозначен (null pointer constant -> const char*)
    BLIB_TEST_CHECK(arr2.size() == 2);
    BLIB_TEST_CHECK(arr2[0].asBint64() == 2);
    arr2.remove(arr2.size() - 1);
    BLIB_TEST_CHECK(arr2.size() == 1);
    BLIB_TEST_CHECK(arr2[0].asBint64() == 2);

    // pushBack возвращает ссылку на добавленный элемент
    JsonValue arr3 = JsonValue::makeArray();
    JsonValue& added = arr3.pushBack(JsonValue(5));
    BLIB_TEST_CHECK(arr3.size() == 1);
    added = JsonValue(6);
    BLIB_TEST_CHECK(arr3[0].asBint64() == 6);

    // const operator[]
    const JsonValue& constArr = arr3;
    BLIB_TEST_CHECK(constArr[0].asBint64() == 6);

    // clear() на не-контейнере — no-op, тип и значение сохраняются
    JsonValue num(42);
    num.clear();
    BLIB_TEST_CHECK(num.isNumber());
    BLIB_TEST_CHECK(num.asBint64() == 42);
    JsonValue str("abc");
    str.clear();
    BLIB_TEST_CHECK(str.isString());
    BLIB_TEST_CHECK(str.asString() == "abc");

    // объект
    JsonValue obj = JsonValue::makeObject();
    obj.set("a", 1);
    obj.set("b", 2);
    BLIB_TEST_CHECK(obj.size() == 2);

    // порядок ключей — порядок вставки
    BLIB_TEST_CHECK(obj.asObject()[0].first == "a");
    BLIB_TEST_CHECK(obj.asObject()[1].first == "b");

    // set перезаписывает и возвращает ссылку на значение
    JsonValue& replaced = obj.set("a", 42);
    BLIB_TEST_CHECK(obj.size() == 2);
    BLIB_TEST_CHECK(obj.get("a").asBint64() == 42);
    replaced = JsonValue(43);
    BLIB_TEST_CHECK(obj.get("a").asBint64() == 43);

    // get возвращает ссылку на существующее значение
    JsonValue& existing = obj.get("b");
    BLIB_TEST_CHECK(existing.asBint64() == 2);

    // get вставляет Null при отсутствии ключа
    JsonValue& fresh = obj.get("c");
    BLIB_TEST_CHECK(fresh.isNull());
    BLIB_TEST_CHECK(obj.size() == 3);
    BLIB_TEST_CHECK(obj.has("c"));

    // пустой ключ — допустим
    obj.set("", 0);
    BLIB_TEST_CHECK(obj.has(""));
    BLIB_TEST_CHECK(obj.get("").asBint64() == 0);

    // remove
    obj.remove("b");
    BLIB_TEST_CHECK(!obj.has("b"));
    BLIB_TEST_CHECK(obj.size() == 3);
    obj.remove("no_such_key"); // отсутствующий ключ — no-op
    BLIB_TEST_CHECK(obj.size() == 3);

    obj.clear();
    BLIB_TEST_CHECK(obj.empty());
}

// ============================================================
// Сравнение
// ============================================================

BLIB_TEST_CASE("json_equality")
{
    // глубокая эквивалентность
    JsonValue a = JsonValue::makeArray();
    a.pushBack(1);
    a.pushBack(JsonValue("x"));
    JsonValue b = JsonValue::makeArray();
    b.pushBack(1);
    b.pushBack(JsonValue("x"));
    BLIB_TEST_CHECK(a == b);

    b[1] = JsonValue("y");
    BLIB_TEST_CHECK(a != b);

    // объекты равны без учёта порядка ключей
    JsonValue o1 = JsonValue::makeObject();
    o1.set("x", 1);
    o1.set("y", 2);
    JsonValue o2 = JsonValue::makeObject();
    o2.set("y", 2);
    o2.set("x", 1);
    BLIB_TEST_CHECK(o1 == o2);

    // числовое сравнение: 1 == 1.0
    BLIB_TEST_CHECK(JsonValue(1) == JsonValue(1.0));
    BLIB_TEST_CHECK(JsonValue(2) == JsonValue(2.0));
    BLIB_TEST_CHECK(JsonValue(-1) != JsonValue(1));
    BLIB_TEST_CHECK(JsonValue(1.5) == JsonValue(1.5));

    // смешанная знаковость целых — сравнение как bdouble
    BLIB_TEST_CHECK(JsonValue(1) == JsonValue(1u));
    BLIB_TEST_CHECK(JsonValue(0) == JsonValue(0u));
    BLIB_TEST_CHECK(JsonValue(-1) != JsonValue(1u));
    BLIB_TEST_CHECK(JsonValue(1) != JsonValue((std::numeric_limits<buint64>::max)()));

    // -0.0 == 0.0 (сравнение через bdouble)
    BLIB_TEST_CHECK(JsonValue(-0.0) == JsonValue(0.0));
    BLIB_TEST_CHECK(JsonValue(0) == JsonValue(0.0));

    // массивы разной длины
    JsonValue arr1 = JsonValue::makeArray();
    arr1.pushBack(1);
    arr1.pushBack(2);
    JsonValue arr2 = JsonValue::makeArray();
    arr2.pushBack(1);
    arr2.pushBack(2);
    arr2.pushBack(3);
    BLIB_TEST_CHECK(arr1 != arr2);

    // объекты разного размера / разные ключи / разные значения
    JsonValue oa = JsonValue::makeObject();
    oa.set("x", 1);
    JsonValue ob = JsonValue::makeObject();
    ob.set("y", 1);
    BLIB_TEST_CHECK(oa != ob);

    JsonValue oc = JsonValue::makeObject();
    oc.set("x", 1);
    oc.set("y", 2);
    BLIB_TEST_CHECK(oa != oc);

    JsonValue od = JsonValue::makeObject();
    od.set("x", 2);
    BLIB_TEST_CHECK(oa != od);

    // примитивы
    BLIB_TEST_CHECK(JsonValue(nullptr) == JsonValue());
    BLIB_TEST_CHECK(JsonValue(false) == JsonValue(false));
    BLIB_TEST_CHECK(JsonValue(true) != JsonValue(false));
    BLIB_TEST_CHECK(JsonValue("a") != JsonValue("b"));
    BLIB_TEST_CHECK(JsonValue("a") == JsonValue("a"));

    // разные типы не равны
    BLIB_TEST_CHECK(!(JsonValue(1) == JsonValue("1")));
    BLIB_TEST_CHECK(!(JsonValue(nullptr) == JsonValue(false)));
}

// ============================================================
// Round-trip и идемпотентность записи
// ============================================================

BLIB_TEST_CASE("json_round_trip")
{
    // разнотипное дерево
    JsonValue v = JsonValue::makeObject();
    v.set("name", JsonValue("test"));
    JsonValue arr = JsonValue::makeArray();
    arr.pushBack(1);
    arr.pushBack(-42);
    arr.pushBack(3.14);
    arr.pushBack(JsonValue(true));
    arr.pushBack(JsonValue(false));
    arr.pushBack(JsonValue(nullptr));
    arr.pushBack(JsonValue("line1\nline2"));
    v.set("values", std::move(arr));
    JsonValue nested = JsonValue::makeObject();
    nested.set("empty", JsonValue::makeArray());
    v.set("nested", std::move(nested));
    BLIB_TEST_CHECK(roundTrip(v));

    // точность bdouble
    const bdouble doubles[] = { 0.1, 1.0 / 3.0, 1e300, 1e-300, -1234.5678, 6.02e23 };
    for (bdouble d : doubles)
        BLIB_TEST_CHECK(roundTrip(JsonValue(d)));

    BLIB_TEST_CHECK(roundTrip(JsonValue(0.5f)));

    // границы целых
    BLIB_TEST_CHECK(roundTrip(JsonValue((std::numeric_limits<bint64>::max)())));
    BLIB_TEST_CHECK(roundTrip(JsonValue((std::numeric_limits<bint64>::min)())));
    BLIB_TEST_CHECK(roundTrip(JsonValue((std::numeric_limits<buint64>::max)())));

    // строки с эскейпами и юникодом
    BLIB_TEST_CHECK(roundTrip(JsonValue("a\"b\\c\nd\te")));
    BLIB_TEST_CHECK(roundTrip(JsonValue("текст на русском")));
    BLIB_TEST_CHECK(roundTrip(JsonValue("\xF0\x9F\x98\x80")));
}

BLIB_TEST_CASE("json_parse_write_idempotent")
{
    // text -> parse -> write -> parse -> write: второй вывод байт-в-байт равен первому
    const char* text = "{\"a\":[1,2.5,null,true,\"x\"],\"b\":{\"k\":-0.125},\"s\":\"строка\\u00E9\"}";
    JsonValue v;
    BLIB_TEST_REQUIRE(parseText(text, v) == JsonError::None);

    ByteArray w1;
    BLIB_TEST_REQUIRE(writeText(v, w1) == JsonError::None);

    JsonValue v2;
    JsonParser parser;
    BLIB_TEST_REQUIRE(parser.parse(reinterpret_cast<const char*>(w1.data()), w1.size(), v2) == JsonError::None);
    BLIB_TEST_CHECK(v2 == v);

    ByteArray w2;
    BLIB_TEST_REQUIRE(writeText(v2, w2) == JsonError::None);
    BLIB_TEST_CHECK(w1.size() == w2.size());
    BLIB_TEST_CHECK(std::memcmp(w1.data(), w2.data(), w1.size()) == 0);
}

// ============================================================
// Работа с потоками
// ============================================================

BLIB_TEST_CASE("json_stream_api")
{
    // запись в MemoryStream + разбор из него
    JsonValue v = JsonValue::makeObject();
    v.set("k", JsonValue(7));

    MemoryStream stream;
    BLIB_TEST_CHECK(v.writeTo(stream) == JsonError::None);
    stream.seek(0, SeekOrigin::Begin);

    JsonValue parsed;
    JsonParser parser;
    BLIB_TEST_CHECK(parser.parse(stream, parsed) == JsonError::None);
    BLIB_TEST_CHECK(parsed == v);

    // сбой записи в поток — StreamError
    FailingOutputStream failing;
    JsonWriter writer;
    BLIB_TEST_CHECK(writer.write(failing, v) == JsonError::StreamError);
    BLIB_TEST_CHECK(v.writeTo(failing) == JsonError::StreamError);

    // пустой вход
    BLIB_TEST_CHECK(parser.parse("", 0, parsed) == JsonError::UnexpectedEnd);
}

// ============================================================
// Утечки памяти
// ============================================================

BLIB_TEST_CASE("json_memory_no_leaks")
{
    const char* sample = "{\"name\":\"test\",\"values\":[1,2,3,4.5],\"nested\":{\"a\":[true,null,\"x\"]}}";
    const size_t sampleLen = std::strlen(sample);

    // прогрев: ленивые инициализации (консоль, статистика) до замера
    for (buint32 i = 0; i < 3; ++i)
    {
        JsonValue v;
        JsonParser parser;
        if (parser.parse(sample, sampleLen, v) != JsonError::None)
            return; // не должно случиться
    }

    // разбор + разрушение дерева
    size_t before = GlobalAllocator::instance().getAllocationCount();
    for (buint32 i = 0; i < 50; ++i)
    {
        JsonValue v;
        JsonParser parser;
        BLIB_TEST_REQUIRE(parser.parse(sample, sampleLen, v) == JsonError::None);
    }
    size_t afterParse = GlobalAllocator::instance().getAllocationCount();
    BLIB_TEST_CHECK(before == afterParse);

    // разбор + запись + разрушение
    for (buint32 i = 0; i < 50; ++i)
    {
        JsonValue v;
        JsonParser parser;
        BLIB_TEST_REQUIRE(parser.parse(sample, sampleLen, v) == JsonError::None);
        MemoryStream stream;
        JsonWriter writer;
        BLIB_TEST_REQUIRE(writer.write(stream, v) == JsonError::None);
    }
    size_t afterWrite = GlobalAllocator::instance().getAllocationCount();
    BLIB_TEST_CHECK(before == afterWrite);

    // глубокое копирование и перемещение
    for (buint32 i = 0; i < 50; ++i)
    {
        JsonValue v;
        JsonParser parser;
        BLIB_TEST_REQUIRE(parser.parse(sample, sampleLen, v) == JsonError::None);
        JsonValue copy(v);
        JsonValue moved(std::move(copy));
    }
    size_t afterCopyMove = GlobalAllocator::instance().getAllocationCount();
    BLIB_TEST_CHECK(before == afterCopyMove);

    // ошибка парсера посреди документа: частично построенное дерево
    // должно быть освобождено
    const char* broken = "[1,2,{\"a\":[true,null,\"x\"]";
    for (buint32 i = 0; i < 50; ++i)
    {
        JsonValue v;
        JsonParser parser;
        BLIB_TEST_CHECK(parser.parse(broken, std::strlen(broken), v) == JsonError::UnexpectedEnd);
    }
    size_t afterErrorParse = GlobalAllocator::instance().getAllocationCount();
    BLIB_TEST_CHECK(before == afterErrorParse);

    // ошибка записи (поток не принимает данные)
    for (buint32 i = 0; i < 50; ++i)
    {
        JsonValue v;
        JsonParser parser;
        BLIB_TEST_REQUIRE(parser.parse(sample, sampleLen, v) == JsonError::None);
        FailingOutputStream failing;
        JsonWriter writer;
        BLIB_TEST_CHECK(writer.write(failing, v) == JsonError::StreamError);
    }
    size_t afterErrorWrite = GlobalAllocator::instance().getAllocationCount();
    BLIB_TEST_CHECK(before == afterErrorWrite);
}

// ============================================================
// Средние объёмы данных
// ============================================================

BLIB_TEST_CASE("json_stress_medium")
{
    // массив из 10k целых
    constexpr buint32 arrayItems = 10000;
    JsonValue big = JsonValue::makeArray();
    for (buint32 i = 0; i < arrayItems; ++i)
        big.pushBack(JsonValue(i));
    BLIB_TEST_CHECK(big.size() == arrayItems);
    BLIB_TEST_CHECK(roundTrip(big));

    // объект с 1k ключей
    constexpr buint32 keyCount = 1000;
    JsonValue obj = JsonValue::makeObject();
    for (buint32 i = 0; i < keyCount; ++i)
    {
        std::string key = "key_" + std::to_string(i);
        obj.set(key.c_str(), JsonValue(i));
    }
    BLIB_TEST_CHECK(obj.size() == keyCount);
    BLIB_TEST_CHECK(roundTrip(obj));

    // глубокая вложенность 200 уровней (меньше jsonMaxDepth)
    constexpr buint32 nestLevels = 200;
    JsonValue deep = JsonValue::makeArray();
    JsonValue* cursor = &deep;
    for (buint32 i = 0; i < nestLevels; ++i)
    {
        cursor->pushBack(JsonValue::makeArray());
        cursor = &(*cursor)[0];
    }
    cursor->pushBack(JsonValue("bottom"));
    BLIB_TEST_CHECK(roundTrip(deep));
}
