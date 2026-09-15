#include <blib/test/src/test.h>

// JSON: парсер (RFC 8259) — позитивные и негативные кейсы,
// границы, смещение ошибки, работа с потоками
#include <blib/core/json/json.h>

// Общие хелперы группы json (parseText, тест-потоки)
#include <blib/test/src/impl/jsonTestUtils.h>

#include <cstring>
#include <cmath>
#include <limits>
#include <string>

using namespace blib;
using namespace blib::core;
using namespace blib::core::json;

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
// Только пробельные символы и обрезанные литералы
// ============================================================

BLIB_TEST_CASE("json_parse_whitespace_literals")
{
    JsonValue v;

    // пустой и whitespace-only вход — UnexpectedEnd
    BLIB_TEST_CHECK(parseText("", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("   ", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("\t\r\n ", v) == JsonError::UnexpectedEnd);

    // литералы, обрезанные после первого символа (EOF посреди parseLiteral)
    BLIB_TEST_CHECK(parseText("n", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("t", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("f", v) == JsonError::UnexpectedEnd);
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
// Расширенные числа: переполнение -> double, длина токена,
// грамматика, экспоненциальные формы, границы bdouble
// ============================================================

BLIB_TEST_CASE("json_parse_numbers_extended")
{
    JsonValue v;

    // переполнение целого -> вещественное (bdouble): UINT64_MAX + 1
    BLIB_TEST_REQUIRE(parseText("18446744073709551616", v) == JsonError::None);
    BLIB_TEST_CHECK(v.isNumber());
    BLIB_TEST_CHECK(v.asBdouble() == 18446744073709551616.0);

    // INT64_MIN - 1 -> bdouble (округляется до -2^63)
    BLIB_TEST_REQUIRE(parseText("-9223372036854775809", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() == -9223372036854775808.0);

    // токен ровно 128 символов — граница буфера, парсится как double
    std::string digits128(128, '1');
    BLIB_TEST_REQUIRE(parseText(digits128.c_str(), v) == JsonError::None);
    BLIB_TEST_CHECK(v.isNumber());
    BLIB_TEST_CHECK(v.asBdouble() > 1e127);
    BLIB_TEST_CHECK(v.asBdouble() < 1.2e127);

    // токен длиннее 128 — NumberOutOfRange (защита от обрезки)
    std::string digits129(129, '1');
    BLIB_TEST_CHECK(parseText(digits129.c_str(), v) == JsonError::NumberOutOfRange);
    std::string digits200(200, '1');
    BLIB_TEST_CHECK(parseText(digits200.c_str(), v) == JsonError::NumberOutOfRange);

    // грамматика числа: неполные/лишние части
    BLIB_TEST_CHECK(parseText("-", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("-01", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("1.e5", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("1e5e5", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("1e+", v) == JsonError::InvalidSyntax);
    BLIB_TEST_CHECK(parseText("1e-", v) == JsonError::InvalidSyntax);

    // 'x' — не символ числа: pushback, далее trailing-данные
    BLIB_TEST_CHECK(parseText("0x1", v) == JsonError::TrailingData);

    // экспоненциальные формы нуля (в т.ч. знак -0)
    BLIB_TEST_REQUIRE(parseText("0e0", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() == 0.0);
    BLIB_TEST_REQUIRE(parseText("0E+0", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() == 0.0);
    BLIB_TEST_REQUIRE(parseText("-0e0", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() == 0.0);
    BLIB_TEST_CHECK(std::signbit(v.asBdouble()));

    // границы bdouble: максимум и денормал
    BLIB_TEST_REQUIRE(parseText("1e308", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() == 1e308);
    BLIB_TEST_REQUIRE(parseText("1.7976931348623157e308", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() == (std::numeric_limits<bdouble>::max)());
    BLIB_TEST_REQUIRE(parseText("5e-324", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asBdouble() > 0.0);
    BLIB_TEST_CHECK(v.asBdouble() < 1e-320);

    // underflow — out of range
    BLIB_TEST_CHECK(parseText("1e-9999", v) == JsonError::NumberOutOfRange);
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
// Расширенные строки: NUL, границы \u, суррогатные пары,
// сырой UTF-8, недопустимые последовательности, ключи
// ============================================================

BLIB_TEST_CASE("json_parse_strings_extended")
{
    JsonValue v;

    // \u0000 -> NUL внутри строки (длина 1, первый байт 0)
    BLIB_TEST_REQUIRE(parseText("\"\\u0000\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString().size() == 1);
    BLIB_TEST_CHECK(v.asString()[0] == '\0');

    // \uFFFF -> UTF-8 EF BF BF
    BLIB_TEST_REQUIRE(parseText("\"\\uFFFF\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString() == "\xEF\xBF\xBF");

    // границы суррогатных пар: U+10000 и U+10FFFF
    BLIB_TEST_REQUIRE(parseText("\"\\uD800\\uDC00\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString() == "\xF0\x90\x80\x80");

    BLIB_TEST_REQUIRE(parseText("\"\\uDBFF\\uDFFF\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString() == "\xF4\x8F\xBF\xBF");

    // сырой 4-байтный UTF-8
    BLIB_TEST_REQUIRE(parseText("\"\xF0\x9F\x98\x80\"", v) == JsonError::None);
    BLIB_TEST_CHECK(v.asString() == "\xF0\x9F\x98\x80");

    // truncated \u: EOF в hex-цифрах и в low-суррогате
    BLIB_TEST_CHECK(parseText("\"\\u12", v) == JsonError::UnexpectedEnd);
    BLIB_TEST_CHECK(parseText("\"\\uD800\\u12", v) == JsonError::UnexpectedEnd);

    // не-hex в low-суррогате; high + high
    BLIB_TEST_CHECK(parseText("\"\\uD800\\u12G4\"", v) == JsonError::InvalidUnicode);
    BLIB_TEST_CHECK(parseText("\"\\uD800\\uDBFF\"", v) == JsonError::InvalidUnicode);

    // недопустимые ведущие байты UTF-8 (0x80-0xC1 и 0xF5-0xFF)
    BLIB_TEST_CHECK(parseText("\"\x80\"", v) == JsonError::InvalidUtf8);
    BLIB_TEST_CHECK(parseText("\"\xC1\x80\"", v) == JsonError::InvalidUtf8);
    BLIB_TEST_CHECK(parseText("\"\xF5\x80\x80\x80\"", v) == JsonError::InvalidUtf8);

    // overlong 3-байтная и 4-байтная последовательности
    BLIB_TEST_CHECK(parseText("\"\xE0\x81\x81\"", v) == JsonError::InvalidUtf8);
    BLIB_TEST_CHECK(parseText("\"\xF0\x80\x80\x80\"", v) == JsonError::InvalidUtf8);

    // сырой управляющий символ без эскейпа
    BLIB_TEST_CHECK(parseText("\"\x01\"", v) == JsonError::InvalidSyntax);

    // \u-эскейп в ключе объекта
    BLIB_TEST_REQUIRE(parseText("{\"\\u0061\":1}", v) == JsonError::None);
    BLIB_TEST_CHECK(v.has("a"));
    BLIB_TEST_CHECK(v.get("a").asBint64() == 1);

    // пустой ключ
    BLIB_TEST_REQUIRE(parseText("{\"\":1}", v) == JsonError::None);
    BLIB_TEST_CHECK(v.has(""));
    BLIB_TEST_CHECK(v.get("").asBint64() == 1);
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
// Незавершённые контейнеры
// ============================================================

BLIB_TEST_CASE("json_parse_container_errors")
{
    JsonValue v;

    BLIB_TEST_CHECK(parseText("[1", v) == JsonError::UnexpectedEnd);         // EOF после элемента
    BLIB_TEST_CHECK(parseText("{\"a\"", v) == JsonError::UnexpectedEnd);     // EOF после ключа
    BLIB_TEST_CHECK(parseText("{\"a\":", v) == JsonError::UnexpectedEnd);    // EOF после двоеточия
    BLIB_TEST_CHECK(parseText("{\"a\":1, ", v) == JsonError::UnexpectedEnd); // EOF после запятой
    BLIB_TEST_CHECK(parseText("[\"a\":1]", v) == JsonError::InvalidSyntax);  // ':' вместо ','/']'
}

// ============================================================
// Дублирующиеся ключи
// ============================================================

BLIB_TEST_CASE("json_parse_duplicate_keys")
{
    JsonValue v;

    // парсер не отклоняет дубликаты: оба члена сохраняются в порядке
    // вставки, get/has работают с первым вхождением
    BLIB_TEST_REQUIRE(parseText("{\"a\":1,\"a\":2}", v) == JsonError::None);
    BLIB_TEST_CHECK(v.size() == 2);
    BLIB_TEST_CHECK(v.has("a"));
    BLIB_TEST_CHECK(v.get("a").asBint64() == 1);
    BLIB_TEST_CHECK(v.asObject()[1].second.asBint64() == 2);
}

// ============================================================
// Границы глубины вложенности
// ============================================================

BLIB_TEST_CASE("json_parse_depth_boundary")
{
    JsonValue v;

    // ровно 513 уровней массива — MaxDepthExceeded
    // (512 допустимо — см. json_parse_errors)
    std::string depth513(513, '[');
    depth513 += std::string(513, ']');
    BLIB_TEST_CHECK(parseText(depth513.c_str(), v) == JsonError::MaxDepthExceeded);

    // глубина считается для ВСЕХ значений, включая скаляры:
    // N объектов + null на глубине N+1
    std::string objOk;
    for (buint32 i = 0; i < 511; ++i)
        objOk += "{\"a\":";
    objOk += "null";
    for (buint32 i = 0; i < 511; ++i)
        objOk += "}";
    BLIB_TEST_REQUIRE(parseText(objOk.c_str(), v) == JsonError::None);
    BLIB_TEST_CHECK(v.isObject());

    std::string objDeep;
    for (buint32 i = 0; i < 512; ++i)
        objDeep += "{\"a\":";
    objDeep += "null";
    for (buint32 i = 0; i < 512; ++i)
        objDeep += "}";
    BLIB_TEST_CHECK(parseText(objDeep.c_str(), v) == JsonError::MaxDepthExceeded);
}

// ============================================================
// UTF-8 BOM
// ============================================================

BLIB_TEST_CASE("json_parse_bom")
{
    JsonValue v;

    // UTF-8 BOM отклоняется как InvalidSyntax. RFC 8259 допускает игнор BOM,
    // но парсер строже. TODO: обсудить, стоит ли пропускать BOM явно
    BLIB_TEST_CHECK(parseText("\xEF\xBB\xBF{}", v) == JsonError::InvalidSyntax);
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
// Смещение ошибки: все классы ошибок и повторное использование
// ============================================================

BLIB_TEST_CASE("json_error_offset_extended")
{
    JsonValue v;
    JsonParser parser;

    // TrailingData: "1 2" — проблемный байт '2' на смещении 3
    BLIB_TEST_CHECK(parser.parse("1 2", 3, v) == JsonError::TrailingData);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 3);

    // InvalidEscape: 'q' в "\a\q" на смещении 4
    BLIB_TEST_CHECK(parser.parse("\"a\\q\"", 5, v) == JsonError::InvalidEscape);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 4);

    // InvalidUnicode: 'G' в "\u12G4" на смещении 6
    BLIB_TEST_CHECK(parser.parse("\"\\u12G4\"", 8, v) == JsonError::InvalidUnicode);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 6);

    // InvalidUtf8: 0xFF на смещении 2
    BLIB_TEST_CHECK(parser.parse("\"\xFF\"", 3, v) == JsonError::InvalidUtf8);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 2);

    // MaxDepthExceeded: 513-я открывающая скобка
    std::string tooDeep(513, '[');
    tooDeep += std::string(513, ']');
    BLIB_TEST_CHECK(parser.parse(tooDeep.c_str(), tooDeep.size(), v) == JsonError::MaxDepthExceeded);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 513);

    // повторное использование: ошибка -> успех -> смещение сброшено
    BLIB_TEST_CHECK(parser.parse("[]", 2, v) == JsonError::None);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 0);
    BLIB_TEST_CHECK(parser.parse("null", 4, v) == JsonError::None);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 0);
}

// ============================================================
// Разбор из потока с троттлингом (1 байт за вызов)
// ============================================================

BLIB_TEST_CASE("json_parse_throttled_stream")
{
    const char* sample = "{\"a\":[1,2.5,null,true,\"x\"],\"b\":{\"k\":-0.125},\"s\":\"строка\\u00E9\"}";
    JsonValue reference;
    BLIB_TEST_REQUIRE(parseText(sample, reference) == JsonError::None);

    // поток, выдающий по 1 байту за вызов: результат идентичен SliceStream
    ThrottledInputStream throttled(sample, std::strlen(sample), 1);
    JsonValue v;
    JsonParser parser;
    BLIB_TEST_REQUIRE(parser.parse(throttled, v) == JsonError::None);
    BLIB_TEST_CHECK(v == reference);

    // кусками по 2 байта — тоже идентично
    ThrottledInputStream throttled2(sample, std::strlen(sample), 2);
    JsonValue v2;
    JsonParser parser2;
    BLIB_TEST_REQUIRE(parser2.parse(throttled2, v2) == JsonError::None);
    BLIB_TEST_CHECK(v2 == reference);

    // ошибка в троттлинг-режиме: pushback + trailing-данные
    ThrottledInputStream broken("12 34", 5, 1);
    JsonValue v3;
    BLIB_TEST_CHECK(parser2.parse(broken, v3) == JsonError::TrailingData);
    BLIB_TEST_CHECK(parser2.getErrorOffset() == 4);
}
