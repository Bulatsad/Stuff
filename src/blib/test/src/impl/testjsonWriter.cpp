#include <blib/test/src/test.h>

// JSON: writer (компактный и pretty), эскейпинг, UTF-8-валидация на выходе,
// числа, потоки с троттлингом/сбоями, NaN/Inf, AllocationFailed через тест-хук
#include <blib/core/json/json.h>

// Общие хелперы группы json (writeText, equalsText, тест-потоки, FailingAllocator)
#include <blib/test/src/impl/jsonTestUtils.h>

// Потоки и статистика аллокаций
#include <blib/core/memoryStream.h>
#include <blib/system/memory/globalAllocator.h>

#include <cstring>
#include <limits>
#include <string>

using namespace blib;
using namespace blib::core;
using namespace blib::core::json;
using namespace blib::memory;

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
// Эскейпинг строк на выходе
// ============================================================

BLIB_TEST_CASE("json_write_string_escapes")
{
    // \b \f \r — короткие эскейпы (каждая ветка writeString отдельно)
    BLIB_TEST_CHECK(equalsText(JsonValue("a\bb\fc\rd"), "\"a\\bb\\fc\\rd\""));

    // управляющие символы — \u00XX (0x01 и 0x1F)
    BLIB_TEST_CHECK(equalsText(JsonValue("x\x01y"), "\"x\\u0001y\""));
    BLIB_TEST_CHECK(equalsText(JsonValue("x\x1Fy"), "\"x\\u001Fy\""));

    // NUL — \u0000. Строка строится парсером: C-строки обрезаются по NUL,
    // поэтому JsonValue(const char*) такой случай не воспроизводит
    JsonValue nulStr;
    BLIB_TEST_REQUIRE(parseText("\"\\u0000\"", nulStr) == JsonError::None);
    BLIB_TEST_CHECK(equalsText(nulStr, "\"\\u0000\""));

    // 0x7F (DEL) — не эскейпится, пишется как есть
    BLIB_TEST_CHECK(equalsText(JsonValue("x\x7Fy"), "\"x\x7Fy\""));

    // '/' не экранируется на выходе (RFC 8259 этого не требует)
    BLIB_TEST_CHECK(equalsText(JsonValue("a/b"), "\"a/b\""));

    // ключ объекта с эскейпами
    JsonValue obj = JsonValue::makeObject();
    obj.set("a\"b\\c", 1);
    BLIB_TEST_CHECK(equalsText(obj, "{\"a\\\"b\\\\c\":1}"));
}

// ============================================================
// Валидный UTF-8 на выходе пишется как есть
// ============================================================

BLIB_TEST_CASE("json_write_utf8_valid")
{
    // сырые 2/3/4-байтные последовательности
    BLIB_TEST_CHECK(equalsText(JsonValue("привет"), "\"привет\""));
    BLIB_TEST_CHECK(equalsText(JsonValue("\xC3\xA9"), "\"\xC3\xA9\""));
    BLIB_TEST_CHECK(equalsText(JsonValue("\xE2\x82\xAC"), "\"\xE2\x82\xAC\""));
    BLIB_TEST_CHECK(equalsText(JsonValue("\xF0\x9F\x98\x80"), "\"\xF0\x9F\x98\x80\""));
}

// ============================================================
// Невалидный UTF-8 на выходе отклоняется
// ============================================================

BLIB_TEST_CASE("json_write_utf8_invalid")
{
    ByteArray data;

    // недопустимые ведущие байты (0x80-0xC1 и 0xF5-0xFF)
    BLIB_TEST_CHECK(writeText(JsonValue("\x80"), data) == JsonError::InvalidUtf8);
    BLIB_TEST_CHECK(writeText(JsonValue("\xFF"), data) == JsonError::InvalidUtf8);

    // overlong 2/3/4-байтные последовательности
    BLIB_TEST_CHECK(writeText(JsonValue("\xC0\xAF"), data) == JsonError::InvalidUtf8);
    BLIB_TEST_CHECK(writeText(JsonValue("\xE0\x81\x81"), data) == JsonError::InvalidUtf8);
    BLIB_TEST_CHECK(writeText(JsonValue("\xF0\x80\x80\x80"), data) == JsonError::InvalidUtf8);

    // суррогат в UTF-8 (U+D800-U+DFFF)
    BLIB_TEST_CHECK(writeText(JsonValue("\xED\xA0\x80"), data) == JsonError::InvalidUtf8);

    // кодпойнт за пределами U+10FFFF
    BLIB_TEST_CHECK(writeText(JsonValue("\xF4\x90\x80\x80"), data) == JsonError::InvalidUtf8);

    // обрезанная последовательность (3 байта ожидалось, 2 получено)
    BLIB_TEST_CHECK(writeText(JsonValue("\xE2\x82"), data) == JsonError::InvalidUtf8);

    // неверный continuation-байт
    BLIB_TEST_CHECK(writeText(JsonValue("\xE2\x28"), data) == JsonError::InvalidUtf8);
}

// ============================================================
// Точные представления чисел
// ============================================================

BLIB_TEST_CASE("json_write_number_goldens")
{
    BLIB_TEST_CHECK(equalsText(JsonValue((std::numeric_limits<bint64>::max)()), "9223372036854775807"));
    BLIB_TEST_CHECK(equalsText(JsonValue((std::numeric_limits<bint64>::min)()), "-9223372036854775808"));
    BLIB_TEST_CHECK(equalsText(JsonValue((std::numeric_limits<buint64>::max)()), "18446744073709551615"));
    BLIB_TEST_CHECK(equalsText(JsonValue(0.1), "0.1"));
    BLIB_TEST_CHECK(equalsText(JsonValue(2.5), "2.5"));
    BLIB_TEST_CHECK(equalsText(JsonValue(-0.0), "-0"));
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
// Pretty: chunk-границы отступа (jsonMaxIndentChunk == 16) и вложенные объекты
// ============================================================

BLIB_TEST_CASE("json_write_indent_chunking")
{
    JsonValue v = JsonValue::makeObject();
    v.set("a", 1);
    v.set("b", 2);

    // 16 пробелов — ровно один chunk в writeIndent
    std::string indent16(16, ' ');
    std::string expected16 = "{\n" + indent16 + "\"a\": 1,\n" + indent16 + "\"b\": 2\n}";
    BLIB_TEST_CHECK(equalsText(v, expected16.c_str(), 16));

    // 17 пробелов — два chunk'а (16 + 1)
    std::string indent17(17, ' ');
    std::string expected17 = "{\n" + indent17 + "\"a\": 1,\n" + indent17 + "\"b\": 2\n}";
    BLIB_TEST_CHECK(equalsText(v, expected17.c_str(), 17));

    // 20 пробелов — chunk'и 16 + 4
    std::string indent20(20, ' ');
    std::string expected20 = "{\n" + indent20 + "\"a\": 1,\n" + indent20 + "\"b\": 2\n}";
    BLIB_TEST_CHECK(equalsText(v, expected20.c_str(), 20));

    // корневой скаляр с pretty — без отступов и переносов
    BLIB_TEST_CHECK(equalsText(JsonValue(42), "42", 4));
    BLIB_TEST_CHECK(equalsText(JsonValue("s"), "\"s\"", 4));

    // массив объектов с вложенным массивом — полный golden
    JsonValue arrObj = JsonValue::makeArray();
    JsonValue item = JsonValue::makeObject();
    item.set("x", 1);
    arrObj.pushBack(std::move(item));
    JsonValue item2 = JsonValue::makeObject();
    JsonValue inner = JsonValue::makeArray();
    inner.pushBack(2);
    inner.pushBack(3);
    item2.set("y", std::move(inner));
    arrObj.pushBack(std::move(item2));

    const char* expectedArr =
        "[\n"
        "    {\n"
        "        \"x\": 1\n"
        "    },\n"
        "    {\n"
        "        \"y\": [\n"
        "            2,\n"
        "            3\n"
        "        ]\n"
        "    }\n"
        "]";
    BLIB_TEST_CHECK(equalsText(arrObj, expectedArr, 4));
}

// ============================================================
// Запись в поток с троттлингом и сбоями
// ============================================================

BLIB_TEST_CASE("json_write_throttled_stream")
{
    JsonValue v = JsonValue::makeObject();
    v.set("a", JsonValue::makeArray());
    v.get("a").pushBack(1);
    v.get("a").pushBack(2.5);
    v.set("b", JsonValue("x"));

    ByteArray reference;
    BLIB_TEST_REQUIRE(writeText(v, reference) == JsonError::None);

    // запись по 1 байту за вызов — результат идентичен MemoryStream
    // (цикл дописывания в writeRaw)
    JsonWriter writer;
    ThrottledOutputStream throttled(1);
    BLIB_TEST_REQUIRE(writer.write(throttled, v) == JsonError::None);
    BLIB_TEST_CHECK(throttled.getData().size() == reference.size());
    BLIB_TEST_CHECK(std::memcmp(throttled.getData().data(), reference.data(), reference.size()) == 0);

    // pretty по 3 байта за вызов
    ByteArray prettyRef;
    BLIB_TEST_REQUIRE(writeText(v, prettyRef, 2) == JsonError::None);
    JsonWriteOptions options;
    options.indentSpaces = 2;
    ThrottledOutputStream throttled3(3);
    BLIB_TEST_REQUIRE(writer.write(throttled3, v, options) == JsonError::None);
    BLIB_TEST_CHECK(throttled3.getData().size() == prettyRef.size());
    BLIB_TEST_CHECK(std::memcmp(throttled3.getData().data(), prettyRef.data(), prettyRef.size()) == 0);

    // сбой посреди документа: принято 5 байт, дальше StreamError
    FailingAfterBytesOutputStream failing(5);
    BLIB_TEST_CHECK(writer.write(failing, v) == JsonError::StreamError);
    // частичный вывод — корректный префикс сериализации
    BLIB_TEST_CHECK(failing.getData().size() == 5);
    BLIB_TEST_CHECK(std::memcmp(failing.getData().data(), reference.data(), 5) == 0);
}

// ============================================================
// writeTo с опциями и повторное использование writer'а
// ============================================================

BLIB_TEST_CASE("json_write_to_options")
{
    JsonValue v = JsonValue::makeObject();
    v.set("k", JsonValue(7));

    // writeTo с pretty-опциями эквивалентен JsonWriter::write
    MemoryStream stream;
    JsonWriteOptions options;
    options.indentSpaces = 2;
    BLIB_TEST_CHECK(v.writeTo(stream, options) == JsonError::None);

    ByteArray viaWriter;
    BLIB_TEST_REQUIRE(writeText(v, viaWriter, 2) == JsonError::None);
    const ByteArray& viaWriteTo = stream.getData();
    BLIB_TEST_CHECK(viaWriteTo.size() == viaWriter.size());
    BLIB_TEST_CHECK(std::memcmp(viaWriteTo.data(), viaWriter.data(), viaWriter.size()) == 0);

    // повторное использование одного writer'а
    JsonWriter writer;
    MemoryStream s1;
    MemoryStream s2;
    JsonValue a = JsonValue::makeArray();
    a.pushBack(1);
    BLIB_TEST_REQUIRE(writer.write(s1, a) == JsonError::None);
    BLIB_TEST_REQUIRE(writer.write(s2, v) == JsonError::None);
    BLIB_TEST_CHECK(s1.getData().size() == 3); // "[1]"

    // второй документ — компактный вывод, эквивалентный JsonWriter без опций
    ByteArray compactRef;
    BLIB_TEST_REQUIRE(writeText(v, compactRef) == JsonError::None);
    BLIB_TEST_CHECK(s2.getData().size() == compactRef.size());
    BLIB_TEST_CHECK(std::memcmp(s2.getData().data(), compactRef.data(), compactRef.size()) == 0);
}

// ============================================================
// NaN/Inf: неконечные числа отклоняются (RFC 8259)
// ============================================================

BLIB_TEST_CASE("json_write_nan_inf")
{
    ByteArray data;
    const bdouble nan = (std::numeric_limits<bdouble>::quiet_NaN)();
    const bdouble inf = (std::numeric_limits<bdouble>::infinity)();

    BLIB_TEST_CHECK(writeText(JsonValue(nan), data) == JsonError::NumberOutOfRange);
    BLIB_TEST_CHECK(writeText(JsonValue(inf), data) == JsonError::NumberOutOfRange);
    BLIB_TEST_CHECK(writeText(JsonValue(-inf), data) == JsonError::NumberOutOfRange);

    // writeTo — тот же путь проверки
    MemoryStream stream;
    BLIB_TEST_CHECK(JsonValue(nan).writeTo(stream) == JsonError::NumberOutOfRange);

    // неконечное число в недрах дерева тоже отклоняется
    JsonValue arr = JsonValue::makeArray();
    arr.pushBack(1);
    arr.pushBack(JsonValue(nan));
    BLIB_TEST_CHECK(writeText(arr, data) == JsonError::NumberOutOfRange);

    // парсер не производит NaN: "nan" — невалидный литерал
    JsonValue v;
    BLIB_TEST_CHECK(parseText("nan", v) == JsonError::InvalidSyntax);
}

// ============================================================
// AllocationFailed через тест-хук (инъекция сбоящего аллокатора)
// ============================================================

BLIB_TEST_CASE("json_parser_allocation_failed")
{
    blib::memory::Allocator failing(FailingAllocator{});
    JsonParser parser;
    JsonValue out = JsonValue::makeObject();

    parser.setDocumentAllocatorForTests(&failing);

    size_t before = GlobalAllocator::instance().getAllocationCount();

    // аллокация узла-контейнера проваливается -> AllocationFailed
    BLIB_TEST_CHECK(parser.parse("{", 1, out) == JsonError::AllocationFailed);
    BLIB_TEST_CHECK(parser.getErrorOffset() == 1);
    BLIB_TEST_CHECK(out.isObject()); // при ошибке out не изменён
    BLIB_TEST_CHECK(out.size() == 0);

    BLIB_TEST_CHECK(parser.parse("[", 1, out) == JsonError::AllocationFailed);
    BLIB_TEST_CHECK(parser.parse("\"abc\"", 5, out) == JsonError::AllocationFailed);
    BLIB_TEST_CHECK(parser.parse("[1,2,3]", 7, out) == JsonError::AllocationFailed);

    // отказ происходит на первой аллокации: ничего не выделено, утечек нет
    size_t after = GlobalAllocator::instance().getAllocationCount();
    BLIB_TEST_CHECK(before == after);

    // сброс хука возвращает парсер в норму
    parser.setDocumentAllocatorForTests(nullptr);
    BLIB_TEST_REQUIRE(parser.parse("{\"a\":1}", 7, out) == JsonError::None);
    BLIB_TEST_CHECK(out.get("a").asBint64() == 1);

    // рабочий инъецированный аллокатор: парсинг успешен, а out НЕ владеет
    // аллокатором (тест владеет им сам — порядок разрушения безопасен)
    blib::memory::Allocator working;
    JsonParser parser2;
    parser2.setDocumentAllocatorForTests(&working);
    JsonValue out2;
    BLIB_TEST_REQUIRE(parser2.parse("{\"a\":[1,2]}", 11, out2) == JsonError::None);
    BLIB_TEST_CHECK(out2.isObject());
    BLIB_TEST_CHECK(out2.get("a").size() == 2);
    BLIB_TEST_CHECK(out2.get("a")[1].asBint64() == 2);
}
