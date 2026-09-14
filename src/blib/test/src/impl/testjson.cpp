#include <blib/test/src/test.h>

// JSON: DOM-значение, парсер, writer
#include <blib/core/json/json.h>

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

    /**
     * Разобрать JSON из C-строки (SliceStream поверх буфера, без копирования).
     */
    JsonError parseText(_In const char* text, _Out JsonValue& out)
    {
        JsonParser parser;
        return parser.parse(text, std::strlen(text), out);
    }

    /**
     * Сериализовать значение в ByteArray.
     */
    JsonError writeText(_In const JsonValue& value, _Out ByteArray& out, buint32 indentSpaces = 0)
    {
        MemoryStream stream;
        JsonWriter writer;
        JsonWriteOptions options;
        options.indentSpaces = indentSpaces;
        JsonError err = writer.write(stream, value, options);
        if (err != JsonError::None)
            return err;
        out = stream.getData();
        return JsonError::None;
    }

    /**
     * Точное совпадение сериализованного текста с ожидаемой строкой.
     */
    bool equalsText(_In const JsonValue& value, _In const char* expected, buint32 indentSpaces = 0)
    {
        ByteArray data;
        if (writeText(value, data, indentSpaces) != JsonError::None)
            return false;
        size_t expectedLen = std::strlen(expected);
        if (data.size() != expectedLen)
            return false;
        return std::memcmp(data.data(), expected, expectedLen) == 0;
    }

    /**
     * Round-trip: значение -> компактный текст -> разбор -> глубокая эквивалентность.
     */
    bool roundTrip(_In const JsonValue& value)
    {
        ByteArray data;
        if (writeText(value, data) != JsonError::None)
            return false;
        JsonParser parser;
        JsonValue parsed;
        if (parser.parse(reinterpret_cast<const char*>(data.data()), data.size(), parsed) != JsonError::None)
            return false;
        return parsed == value;
    }
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

    // сравнение
    BLIB_TEST_CHECK(n == nn);
    BLIB_TEST_CHECK(!(n == b));
}

// ============================================================
// Разбор примитивов
// ============================================================

BLIB_TEST_CASE("json_parse_primitives")
{
    JsonValue v;

    BLIB_TEST_REQUIRE(parseText("null", v) == JsonError::None);
    BLIB_TEST_CHECK(v.isNull());

    BLIB_TEST_REQUIRE(parseText("true", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBool());

    BLIB_TEST_REQUIRE(parseText("false", v) == JsonError::None);
    BLIB_TEST_CHECK(!v.asBool());

    // пробельные символы вокруг значения
    BLIB_TEST_REQUIRE(parseText("  null  ", v) == JsonError::None);
    BLIB_TEST_CHECK(v.isNull());

    BLIB_TEST_REQUIRE(parseText("\t\r\n true \n", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBool());
}

// ============================================================
// Разбор чисел
// ============================================================

BLIB_TEST_CASE("json_parse_numbers")
{
    JsonValue v;

    BLIB_TEST_REQUIRE(parseText("0", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBint64() == 0);

    BLIB_TEST_REQUIRE(parseText("-17", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBint64() == -17);

    // границы целых
    BLIB_TEST_REQUIRE(parseText("9223372036854775807", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBint64() == (std::numeric_limits<bint64>::max)());

    BLIB_TEST_REQUIRE(parseText("-9223372036854775808", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBint64() == (std::numeric_limits<bint64>::min)());

    BLIB_TEST_REQUIRE(parseText("18446744073709551615", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBuint64() == (std::numeric_limits<buint64>::max)());

    // целое за пределами int64, но в диапазоне uint64
    BLIB_TEST_REQUIRE(parseText("9223372036854775808", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBuint64() == 9223372036854775808ULL);

    // "-0" сохраняет знак (round-trip)
    BLIB_TEST_REQUIRE(parseText("-0", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() == 0.0);
    BLIB_TEST_CHECK(std::signbit(v.asBdouble()));

    // вещественные
    BLIB_TEST_REQUIRE(parseText("3.14", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() == 3.14);

    BLIB_TEST_REQUIRE(parseText("-2.5e-3", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() == -0.0025);

    BLIB_TEST_REQUIRE(parseText("1E+2", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() == 100.0);

    BLIB_TEST_REQUIRE(parseText("6.02e23", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() == 6.02e23);

    // нормализация при записи: "1.50" -> "1.5"
    BLIB_TEST_REQUIRE(parseText("1.50", v) == JsonError::None);
    BLIB_TEST_CHECK(equalsText(v, "1.5"));
}

// ============================================================
// Разбор строк
// ============================================================

BLIB_TEST_CASE("json_parse_strings")
{
    JsonValue v;

    BLIB_TEST_REQUIRE(parseText("\"\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString().empty());

    BLIB_TEST_REQUIRE(parseText("\"hello\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString() == "hello");

    // все короткие эскейпы: \" \\ \/ \b \f \n \r \t
    BLIB_TEST_REQUIRE(parseText("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString() == "\"\\/\b\f\n\r\t");

    // \uXXXX
    BLIB_TEST_REQUIRE(parseText("\"\\u0041\\u0062\\u0043\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString() == "AbC");

    // \u00E9 -> UTF-8 C3 A9 (é)
    BLIB_TEST_REQUIRE(parseText("\"\\u00E9\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString() == "\xC3\xA9");

    // \u20AC -> UTF-8 E2 82 AC (€)
    BLIB_TEST_REQUIRE(parseText("\"\\u20AC\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString() == "\xE2\x82\xAC");

    // суррогатная пара \uD83D\uDE00 -> UTF-8 F0 9F 98 80
    BLIB_TEST_REQUIRE(parseText("\"\\uD83D\\uDE00\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString() == "\xF0\x9F\x98\x80");

    // сырой UTF-8 проходит без изменений
    BLIB_TEST_REQUIRE(parseText("\"привет\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString() == "привет");
}

// ============================================================
// Разбор массивов и объектов
// ============================================================

BLIB_TEST_CASE("json_parse_containers")
{
    JsonValue v;

    BLIB_TEST_REQUIRE(parseText("[]", v) == JsonError::None);
    BLIB_TEST_CHECK(v.isArray());
    BLIB_TEST_CHECK(v.empty());
    BLIB_TEST_CHECK(v.size() == 0);

    BLIB_TEST_REQUIRE(parseText("{}", v) == JsonError::None);
    BLIB_TEST_CHECK(v.isObject());
    BLIB_TEST_CHECK(v.empty());

    BLIB_TEST_REQUIRE(parseText("[1,2,3]", v) == JsonError::None);
    BLIB_TEST_CHECK(v.size() == 3);
    BLIB_TEST_CHECK(v[0].asBint64() == 1);
    BLIB_TEST_CHECK(v[1].asBint64() == 2);
    BLIB_TEST_CHECK(v[2].asBint64() == 3);

    // вложенность
    BLIB_TEST_REQUIRE(parseText("[1, [2, [3]], 4]", v) == JsonError::None);
    BLIB_TEST_CHECK(v.size() == 3);
    BLIB_TEST_CHECK(v[1].isArray());
    BLIB_TEST_CHECK(v[1][0].asBint64() == 2);
    BLIB_TEST_CHECK(v[1][1][0].asBint64() == 3);

    // объект с разнотипными значениями
    BLIB_TEST_REQUIRE(parseText("{\"a\": 1, \"b\": [true, null, \"x\"]}", v) == JsonError::None);
    BLIB_TEST_CHECK(v.isObject());
    BLIB_TEST_CHECK(v.size() == 2);
    BLIB_TEST_CHECK(v.has("a"));
    BLIB_TEST_CHECK(!v.has("c"));
    BLIB_TEST_CHECK(v.get("a").asBint64() == 1);
    BLIB_TEST_CHECK(v.get("b").isArray());
    BLIB_TEST_CHECK(v.get("b").size() == 3);
    BLIB_TEST_CHECK(v.get("b")[0].asBool());
    BLIB_TEST_CHECK(v.get("b")[1].isNull());
    BLIB_TEST_CHECK(v.get("b")[2].asString() == "x");

    // пробельные вариации
    BLIB_TEST_REQUIRE(parseText(" { \"a\" : [ 1 , 2 ] } ", v) == JsonError::None);
    BLIB_TEST_CHECK(v.get("a").size() == 2);
}

// ============================================================
// Компактная запись
// ============================================================

BLIB_TEST_CASE("json_write_compact")
{
    // скалярные значения
    BLIB_TEST_CHECK(equalsText(JsonValue(nullptr), "null"));
    BLIB_TEST_CHECK(equalsText(JsonValue(), "null"));
    BLIB_TEST_CHECK(equalsText(JsonValue(true), "true"));
    BLIB_TEST_CHECK(equalsText(JsonValue(false), "false"));
    BLIB_TEST_CHECK(equalsText(JsonValue(42), "42"));
    BLIB_TEST_CHECK(equalsText(JsonValue(-42), "-42"));
    BLIB_TEST_CHECK(equalsText(JsonValue(2.5), "2.5"));
    BLIB_TEST_CHECK(equalsText(JsonValue(""), "\"\""));
    BLIB_TEST_CHECK(equalsText(JsonValue::makeArray(), "[]"));
    BLIB_TEST_CHECK(equalsText(JsonValue::makeObject(), "{}"));

    // строки с эскейпами
    BLIB_TEST_CHECK(equalsText(JsonValue("he said \"hi\""), "\"he said \\\"hi\\\"\""));
    BLIB_TEST_CHECK(equalsText(JsonValue("a\\b"), "\"a\\\\b\""));
    BLIB_TEST_CHECK(equalsText(JsonValue("a\nb\tc"), "\"a\\nb\\tc\""));
    BLIB_TEST_CHECK(equalsText(JsonValue("x\x01y"), "\"x\\u0001y\"")); // управляющий символ

    // объект с массивом
    JsonValue v = JsonValue::makeObject();
    v.set("a", 1);
    JsonValue arr = JsonValue::makeArray();
    arr.pushBack(1);
    arr.pushBack(2.5);
    arr.pushBack(JsonValue(nullptr));
    arr.pushBack(JsonValue(true));
    arr.pushBack(JsonValue("x"));
    v.set("b", std::move(arr));
    BLIB_TEST_CHECK(equalsText(v, "{\"a\":1,\"b\":[1,2.5,null,true,\"x\"]}"));

    // вложенный объект
    JsonValue nested = JsonValue::makeObject();
    JsonValue inner = JsonValue::makeObject();
    inner.set("k", JsonValue("v"));
    nested.set("inner", std::move(inner));
    BLIB_TEST_CHECK(equalsText(nested, "{\"inner\":{\"k\":\"v\"}}"));
}

// ============================================================
// Pretty-запись
// ============================================================

BLIB_TEST_CASE("json_write_pretty")
{
    JsonValue v = JsonValue::makeObject();
    v.set("a", 1);
    JsonValue arr = JsonValue::makeArray();
    arr.pushBack(1);
    arr.pushBack(2);
    v.set("b", std::move(arr));

    const char* expected =
        "{\n"
        "  \"a\": 1,\n"
        "  \"b\": [\n"
        "    1,\n"
        "    2\n"
        "  ]\n"
        "}";
    BLIB_TEST_CHECK(equalsText(v, expected, 2));

    // пустые контейнеры — без внутренних переносов
    JsonValue e = JsonValue::makeObject();
    e.set("a", JsonValue::makeArray());
    e.set("b", JsonValue::makeObject());
    const char* expectedEmpty =
        "{\n"
        "  \"a\": [],\n"
        "  \"b\": {}\n"
        "}";
    BLIB_TEST_CHECK(equalsText(e, expectedEmpty, 2));
}

// ============================================================
// Round-trip
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

// ============================================================
// Ошибки разбора
// ============================================================

BLIB_TEST_CASE("json_parse_errors")
{
    JsonValue v;

    // UnexpectedEnd
    BLIB_TEST_CHECK(parseText("", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("{", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("[1,", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("{\"a\":1", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("\"abc", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("tr", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("tru", v) == JsonError::UnexpectedEnd); // литерал обрезан EOF
    BLIB_TEST_CHECK(parseText("nul", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("fals", v) == JsonError::UnexpectedEnd);

    // InvalidSyntax
    BLIB_TEST_CHECK(parseText("trx", v) == JsonError::InvalidSyntax); // неверный символ литерала
    BLIB_TEST_CHECK(parseText("nux", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("falsy", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("+1", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText(".5", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("1.", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("1e", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("--1", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("01", v) == JsonError::InvalidSyntax); // ведущие нули запрещены
    BLIB_TEST_CHECK(parseText("[1,]", v) == JsonError::InvalidSyntax); // trailing comma
    BLIB_TEST_CHECK(parseText("[,]", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("{\"a\":1,}", v) == JsonError::InvalidSyntax); // trailing comma
    BLIB_TEST_CHECK(parseText("{a:1}", v) == JsonError::InvalidSyntax); // ключ без кавычек
    BLIB_TEST_CHECK(parseText("{\"a\" 1}", v) == JsonError::InvalidSyntax); // нет ':'
    BLIB_TEST_CHECK(parseText("{'a':1}", v) == JsonError::InvalidSyntax); // одинарные кавычки
    BLIB_TEST_CHECK(parseText("[1 2]", v) == JsonError::InvalidSyntax); // нет ','
    BLIB_TEST_CHECK(parseText("{\"a\":1 \"b\":2}", v) == JsonError::InvalidSyntax); // нет ','
    BLIB_TEST_CHECK(parseText("x", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("\"a\nb\"", v) == JsonError::InvalidSyntax); // контрольный символ без эскейпа

    // TrailingData
    BLIB_TEST_CHECK(parseText("1 2", v) == JsonError::TrailingData);
    BLIB_TEST_CHECK(parseText("truex", v) == JsonError::TrailingData);
    BLIB_TEST_CHECK(parseText("{} {}", v) == JsonError::TrailingData);

    // InvalidEscape
    BLIB_TEST_CHECK(parseText("\"a\\x\"", v) == JsonError::InvalidEscape);
    BLIB_TEST_CHECK(parseText("\"\\q\"", v) == JsonError::InvalidEscape);

    // InvalidUnicode
    BLIB_TEST_CHECK(parseText("\"\\u12G4\"", v) == JsonError::InvalidUnicode); // не hex-цифра
    BLIB_TEST_CHECK(parseText("\"\\uD800\"", v) == JsonError::InvalidUnicode); // high без пары
    BLIB_TEST_CHECK(parseText("\"\\uDC00\"", v) == JsonError::InvalidUnicode); // low без пары
    BLIB_TEST_CHECK(parseText("\"\\uD800x\"", v) == JsonError::InvalidUnicode); // high + не-\u
    BLIB_TEST_CHECK(parseText("\"\\uD800\\u0041\"", v) == JsonError::InvalidUnicode); // high + не-low

    // NumberOutOfRange
    BLIB_TEST_CHECK(parseText("1e9999", v) == JsonError::NumberOutOfRange);

    // InvalidUtf8: сырые байты вне допустимых последовательностей
    BLIB_TEST_CHECK(parseText("\"\xFF\"", v) == JsonError::InvalidUtf8);
    BLIB_TEST_CHECK(parseText("\"\xC0\xAF\"", v) == JsonError::InvalidUtf8); // overlong
    BLIB_TEST_CHECK(parseText("\"\xED\xA0\x80\"", v) == JsonError::InvalidUtf8); // суррогат в UTF-8
    BLIB_TEST_CHECK(parseText("\"\xF5\x80\x80\x80\"", v) == JsonError::InvalidUtf8); // > U+10FFFF
    BLIB_TEST_CHECK(parseText("\"\xE2\x82\"", v) == JsonError::InvalidUtf8); // '"' вместо продолжения
    BLIB_TEST_CHECK(parseText("\"\xE2\x82", v) == JsonError::UnexpectedEnd); // EOF посреди последовательности

    // глубина: 512 допустимо, больше — MaxDepthExceeded
    std::string deep(jsonMaxDepth, '[');
    deep += std::string(jsonMaxDepth, ']');
    BLIB_TEST_CHECK(parseText(deep.c_str(), v) == JsonError::None);

    std::string tooDeep(jsonMaxDepth + 100, '[');
    tooDeep += std::string(jsonMaxDepth + 100, ']');
    BLIB_TEST_CHECK(parseText(tooDeep.c_str(), v) == JsonError::MaxDepthExceeded);
}

// ============================================================
// Смещение ошибки
// ============================================================

BLIB_TEST_CASE("json_error_offset")
{
    JsonValue v;
    JsonParser parser;

    BLIB_TEST_CHECK(parser.parse("{bad}", 5, v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 2); // 'b' — второй прочитанный байт

    BLIB_TEST_CHECK(parser.parse("[1, x]", 6, v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 5); // 'x' — пятый прочитанный байт

    BLIB_TEST_CHECK(parser.parse("[1,", 3, v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 3); // конец ввода

    // успешный разбор сбрасывает смещение
    BLIB_TEST_CHECK(parser.parse("[]", 2, v) == JsonError::None);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 0);
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

    // копия вложенного объекта независима
    JsonValue obj = JsonValue::makeObject();
    obj.set("x", 1);
    JsonValue objCopy(obj);
    objCopy.set("x", 2);
    BLIB_TEST_CHECK(obj.get("x").asBint64() == 1);
    BLIB_TEST_CHECK(objCopy.get("x").asBint64() == 2);
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

    // объект
    JsonValue obj = JsonValue::makeObject();
    obj.set("a", 1);
    obj.set("b", 2);
    BLIB_TEST_CHECK(obj.size() == 2);

    // порядок ключей — порядок вставки
    BLIB_TEST_CHECK(obj.asObject()[0].first == "a");
    BLIB_TEST_CHECK(obj.asObject()[1].first == "b");

    // set перезаписывает
    obj.set("a", 42);
    BLIB_TEST_CHECK(obj.size() == 2);
    BLIB_TEST_CHECK(obj.get("a").asBint64() == 42);

    // get вставляет Null при отсутствии ключа
    JsonValue& fresh = obj.get("c");
    BLIB_TEST_CHECK(fresh.isNull());
    BLIB_TEST_CHECK(obj.size() == 3);
    BLIB_TEST_CHECK(obj.has("c"));

    // remove
    obj.remove("b");
    BLIB_TEST_CHECK(!obj.has("b"));
    BLIB_TEST_CHECK(obj.size() == 2);
    obj.remove("no_such_key"); // отсутствующий ключ — no-op
    BLIB_TEST_CHECK(obj.size() == 2);

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
    BLIB_TEST_CHECK(JsonValue(-1) != JsonValue(1));
    BLIB_TEST_CHECK(JsonValue(1.5) == JsonValue(1.5));

    // разные типы не равны
    BLIB_TEST_CHECK(!(JsonValue(1) == JsonValue("1")));
    BLIB_TEST_CHECK(!(JsonValue(nullptr) == JsonValue(false)));
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

    // прогрев: ленивые инициализации (консоль, статистика) до замера
    for (buint32 i = 0; i < 3; ++i)
    {
        JsonValue v;
        JsonParser parser;
        if (parser.parse(sample, std::strlen(sample), v) != JsonError::None)
            return; // не должно случиться
    }

    // разбор + разрушение дерева не должны оставлять активных аллокаций
    size_t before = GlobalAllocator::instance().getAllocationCount();
    for (buint32 i = 0; i < 50; ++i)
    {
        JsonValue v;
        JsonParser parser;
        BLIB_TEST_REQUIRE(parser.parse(sample, std::strlen(sample), v) == JsonError::None);
        // v уничтожается на выходе из итерации
    }
    size_t after = GlobalAllocator::instance().getAllocationCount();

    BLIB_TEST_CHECK(before == after);
}
