// Реализация JSON-модуля: DOM-значение JsonValue, строгий парсер
// (RFC 8259) и writer. Публичный API — blib/core/json/json.h.

#include <blib/core/json/json.h>

#include <new>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <charconv>
#include <system_error>

#include <blib/core/sliceStream.h>
#include <blib/core/console/console.h>
#include <blib/system/memory/globalAllocator.h>

namespace blib
{
namespace core
{
namespace json
{

// ============================================================================
// Константы (именованные — правило проекта: без вшитых литералов)
// ============================================================================

namespace
{
    // Максимальная длина токена числа при разборе (защита от обрезки)
    constexpr buint32 jsonMaxNumberTokenLength = 128;

    // Буфер форматирования числа при записи: int64 — 20 знаков,
    // double — до 24 ("-1.7976931348623157e+308"), с запасом
    constexpr buint32 jsonNumberBufferSize = 64;

    // 2^63 == |INT64_MIN| — предел накопления модуля отрицательных целых
    constexpr buint64 jsonInt64MagnitudeLimit = 9223372036854775808ULL;

    // UINT64_MAX — предел накопления беззнаковых целых
    constexpr buint64 jsonUint64Limit = 18446744073709551615ULL;

    // Предельный размер единичного куска отступа в pretty-выводе
    constexpr buint32 jsonMaxIndentChunk = 16;

    // Литералы вывода (длины — sizeof - 1: нуль-терминаторы не пишем)
    const char jsonNullText[]             = "null";
    const char jsonTrueText[]             = "true";
    const char jsonFalseText[]            = "false";
    const char jsonOpenBraceText[]        = "{";
    const char jsonCloseBraceText[]       = "}";
    const char jsonOpenBracketText[]      = "[";
    const char jsonCloseBracketText[]     = "]";
    const char jsonCommaText[]            = ",";
    const char jsonColonText[]            = ":";
    const char jsonColonSpaceText[]       = ": ";
    const char jsonNewlineText[]          = "\n";
    const char jsonQuoteText[]            = "\"";
    const char jsonEscapedQuoteText[]     = "\\\"";
    const char jsonEscapedBackslashText[] = "\\\\";
    const char jsonEscapedBackspaceText[] = "\\b";
    const char jsonEscapedFormFeedText[]  = "\\f";
    const char jsonEscapedNewlineText[]   = "\\n";
    const char jsonEscapedCarriageText[]  = "\\r";
    const char jsonEscapedTabText[]       = "\\t";

    // Единичный кусок отступа pretty-вывода (jsonMaxIndentChunk пробелов)
    const char jsonIndentUnit[] = "                ";

    // Шестнадцатеричные цифры для \u00XX-эскейпов управляющих символов
    const char jsonHexDigits[] = "0123456789ABCDEF";
}

// ============================================================================
// Общие вспомогательные функции (anonymous namespace)
// ============================================================================

namespace
{
    /**
     * Выделить контейнер T через аллокатор и сконструировать его
     * с StdAllocatorAdapter, указывающим на тот же аллокатор.
     *
     * Инвариант: адрес аллокатора стабилен (heap, владеет корень
     * поддерева), поэтому адаптеры переживают перемещения контейнеров.
     *
     * @param alloc Аллокатор документа (не nullptr)
     * @return Указатель на контейнер или nullptr при неудаче выделения
     */
    template<typename T>
    T* allocContainer(_In blib::memory::Allocator* alloc)
    {
        void* mem = alloc->allocate(sizeof(T));
        if (__blib_unlikely(!mem))
            return nullptr;
        return new (mem) T(blib::memory::StdAllocatorAdapter<typename T::value_type>(alloc));
    }

    /**
     * Определить длину UTF-8-последовательности по ведущему байту
     * и начать накопление кодпойнта.
     *
     * @param lead      Ведущий байт (>= 0x80)
     * @param seqLen    [out] Длина последовательности в байтах
     * @param codePoint [out] Начальное значение кодпойнта (биты ведущего байта)
     * @return false, если байт не может быть ведущим (0x80-0xC1, 0xF5-0xFF)
     */
    bool utf8SequenceInfo(buint8 lead, _Out buint32& seqLen, _Out buint32& codePoint)
    {
        if (lead >= 0xC2 && lead <= 0xDF) { seqLen = 2; codePoint = lead & 0x1F; return true; }
        if (lead >= 0xE0 && lead <= 0xEF) { seqLen = 3; codePoint = lead & 0x0F; return true; }
        if (lead >= 0xF0 && lead <= 0xF4) { seqLen = 4; codePoint = lead & 0x07; return true; }
        return false;
    }

    /**
     * Финальная проверка кодпойнта UTF-8-последовательности:
     * отклоняет overlong-формы, суррогаты (U+D800-U+DFFF — в UTF-8
     * недопустимы, они кодируются только \u-парами) и значения
     * за пределами U+10FFFF.
     */
    bool utf8CodePointValid(buint32 seqLen, buint32 codePoint)
    {
        if (seqLen == 3 && codePoint < 0x800)
            return false; // overlong
        if (seqLen == 3 && codePoint >= 0xD800 && codePoint <= 0xDFFF)
            return false; // суррогат в UTF-8
        if (seqLen == 4 && codePoint < 0x10000)
            return false; // overlong
        if (codePoint > 0x10FFFF)
            return false;
        return true;
    }

    /**
     * Закодировать кодпойнт в UTF-8 и дописать в строку.
     */
    void utf8Encode(_Out JsonValue::String& out, buint32 codePoint)
    {
        if (codePoint <= 0x7F)
        {
            out.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint <= 0x7FF)
        {
            out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else if (codePoint <= 0xFFFF)
        {
            out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    bool isHexDigit(buint8 c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    buint32 hexValue(buint8 c)
    {
        if (c >= '0' && c <= '9')
            return static_cast<buint32>(c - '0');
        if (c >= 'a' && c <= 'f')
            return static_cast<buint32>(c - 'a' + 10);
        return static_cast<buint32>(c - 'A' + 10);
    }

    /**
     * Строгая проверка грамматики числа RFC 8259:
     *   -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
     * Токен собран парсером и гарантированно состоит только из
     * символов [0-9\-+.eE].
     */
    bool validateNumberGrammar(_In const char* token, buint32 len)
    {
        buint32 i = 0;

        if (i < len && token[i] == '-')
            ++i;

        if (i >= len)
            return false;

        if (token[i] == '0')
        {
            ++i;
        }
        else if (token[i] >= '1' && token[i] <= '9')
        {
            while (i < len && token[i] >= '0' && token[i] <= '9')
                ++i;
        }
        else
        {
            return false;
        }

        if (i < len && token[i] == '.')
        {
            ++i;
            if (i >= len || token[i] < '0' || token[i] > '9')
                return false;
            while (i < len && token[i] >= '0' && token[i] <= '9')
                ++i;
        }

        if (i < len && (token[i] == 'e' || token[i] == 'E'))
        {
            ++i;
            if (i < len && (token[i] == '+' || token[i] == '-'))
                ++i;
            if (i >= len || token[i] < '0' || token[i] > '9')
                return false;
            while (i < len && token[i] >= '0' && token[i] <= '9')
                ++i;
        }

        return i == len;
    }
}

// ============================================================================
// JsonValue
// ============================================================================

blib::memory::Allocator* JsonValue::acquireDocumentAllocator()
{
    // Аллокатор документа всегда в heap: его адрес должен быть стабильным
    // на всё время жизни дерева (адаптеры контейнеров хранят указатель).
    void* mem = blib::memory::GlobalAllocator::instance().allocate(sizeof(blib::memory::Allocator));
    if (__blib_unlikely(!mem))
        __blib_fatal("JsonValue: failed to allocate document allocator");
    return new (mem) blib::memory::Allocator();
}

JsonValue::JsonValue()
    : nodeType(JsonType::Null)
    , isInteger(0)
    , numUnsigned(0)
    , alloc(nullptr)
    , ownsAlloc(0)
{
    this->alloc = acquireDocumentAllocator();
    this->ownsAlloc = 1;
}

JsonValue::JsonValue(std::nullptr_t)
    : JsonValue()
{
}

JsonValue::JsonValue(bool value)
    : JsonValue()
{
    this->nodeType = JsonType::Bool;
    this->boolValue = value;
}

JsonValue::JsonValue(bfloat value)
    : JsonValue()
{
    this->nodeType = JsonType::Number;
    this->realValue = static_cast<bdouble>(value);
}

JsonValue::JsonValue(bdouble value)
    : JsonValue()
{
    this->nodeType = JsonType::Number;
    this->realValue = value;
}

JsonValue::JsonValue(_In const char* value)
    : JsonValue()
{
    this->nodeType = JsonType::String;
    this->stringValue = allocContainer<JsonValue::String>(this->alloc);
    if (__blib_unlikely(!this->stringValue))
        __blib_fatal("JsonValue: failed to allocate string node");
    // nullptr — недопустимый аргумент (задокументировано); защитно трактуем как пустую строку
    this->stringValue->assign(value ? value : "");
}

JsonValue::JsonValue(_In blib::memory::Allocator* sharedAlloc, JsonType nodeType)
    : nodeType(nodeType)
    , isInteger(0)
    , numUnsigned(0)
    , alloc(sharedAlloc)
    , ownsAlloc(0)
{
    // Контейнерные типы сразу выделяют пустой контейнер через общий аллокатор.
    // Неудача оставляет указатель нулевым — проверка в makeNode.
    switch (nodeType)
    {
        case JsonType::String: this->stringValue = allocContainer<JsonValue::String>(sharedAlloc); break;
        case JsonType::Array:  this->arrayValue  = allocContainer<JsonValue::Array>(sharedAlloc);  break;
        case JsonType::Object: this->objectValue = allocContainer<JsonValue::Object>(sharedAlloc); break;
        default: break;
    }
}

JsonValue JsonValue::makeNode(_In blib::memory::Allocator* sharedAlloc, JsonType nodeType, _Out bool& allocationOk)
{
    allocationOk = true;
    JsonValue node(sharedAlloc, nodeType);
    switch (nodeType)
    {
        case JsonType::String: if (!node.stringValue) allocationOk = false; break;
        case JsonType::Array:  if (!node.arrayValue)  allocationOk = false; break;
        case JsonType::Object: if (!node.objectValue) allocationOk = false; break;
        default: break;
    }
    return node;
}

JsonValue JsonValue::makeArray()
{
    JsonValue value; // корень — владеет аллокатором документа
    value.nodeType = JsonType::Array;
    value.arrayValue = allocContainer<JsonValue::Array>(value.alloc);
    if (__blib_unlikely(!value.arrayValue))
        __blib_fatal("JsonValue: failed to allocate array node");
    return value;
}

JsonValue JsonValue::makeObject()
{
    JsonValue value; // корень — владеет аллокатором документа
    value.nodeType = JsonType::Object;
    value.objectValue = allocContainer<JsonValue::Object>(value.alloc);
    if (__blib_unlikely(!value.objectValue))
        __blib_fatal("JsonValue: failed to allocate object node");
    return value;
}

JsonValue::JsonValue(_In const JsonValue& other)
    : nodeType(JsonType::Null)
    , isInteger(0)
    , numUnsigned(0)
    , alloc(nullptr)
    , ownsAlloc(0)
{
    // Глубокое копирование: ОДИН свежий аллокатор на всё новое дерево
    this->alloc = acquireDocumentAllocator();
    this->ownsAlloc = 1;
    if (__blib_unlikely(this->copyFrom(other) != JsonError::None))
        __blib_fatal("JsonValue: allocation failed during deep copy");
}

JsonValue& JsonValue::operator=(_In const JsonValue& other)
{
    if (this != &other)
    {
        // copy-and-swap: копия владеет своим аллокатором, обмен — переносом указателей
        JsonValue copy(other);
        *this = std::move(copy);
    }
    return *this;
}

JsonValue::JsonValue(JsonValue&& other) noexcept
    : nodeType(other.nodeType)
    , isInteger(other.isInteger)
    , numUnsigned(other.numUnsigned)
    , alloc(other.alloc)
    , ownsAlloc(other.ownsAlloc)
{
    // Union-члены тривиально копируемы; переносим побайтово (8 байт покрывают
    // все варианты: bool занимает 1 байт, остальные члены — 8)
    std::memcpy(&this->boolValue, &other.boolValue, sizeof(buint64));

    // Источник — пустой Null: его деструктор станет no-op
    other.nodeType = JsonType::Null;
    other.isInteger = 0;
    other.numUnsigned = 0;
    other.alloc = nullptr;
    other.ownsAlloc = 0;
}

JsonValue& JsonValue::operator=(JsonValue&& other) noexcept
{
    if (this != &other)
    {
        this->destroy();
        new (this) JsonValue(std::move(other));
    }
    return *this;
}

JsonValue::~JsonValue()
{
    this->destroy();
}

void JsonValue::destroy()
{
    switch (this->nodeType)
    {
        case JsonType::String:
            if (this->stringValue)
            {
                this->stringValue->~String();
                this->alloc->deallocate(this->stringValue, sizeof(JsonValue::String));
            }
            break;

        case JsonType::Array:
            if (this->arrayValue)
            {
                this->arrayValue->~Array();
                this->alloc->deallocate(this->arrayValue, sizeof(JsonValue::Array));
            }
            break;

        case JsonType::Object:
            if (this->objectValue)
            {
                this->objectValue->~Object();
                this->alloc->deallocate(this->objectValue, sizeof(JsonValue::Object));
            }
            break;

        default:
            break;
    }

    // Порядок важен: сначала контейнеры (через аллокатор), потом сам аллокатор
    if (this->ownsAlloc && this->alloc)
    {
        this->alloc->~Allocator();
        blib::memory::GlobalAllocator::instance().deallocate(this->alloc, sizeof(blib::memory::Allocator));
    }

    this->nodeType = JsonType::Null;
    this->isInteger = 0;
    this->numUnsigned = 0;
    this->alloc = nullptr;
    this->ownsAlloc = 0;
}

JsonError JsonValue::copyFrom(_In const JsonValue& other)
{
    this->nodeType = other.nodeType;
    this->isInteger = other.isInteger;
    this->numUnsigned = other.numUnsigned;

    switch (other.nodeType)
    {
        case JsonType::Null:
            break;

        case JsonType::Bool:
            this->boolValue = other.boolValue;
            break;

        case JsonType::Number:
            // union — переносим байты (все числовые члены — 8 байт)
            std::memcpy(&this->realValue, &other.realValue, sizeof(buint64));
            break;

        case JsonType::String:
        {
            this->stringValue = allocContainer<JsonValue::String>(this->alloc);
            if (__blib_unlikely(!this->stringValue))
                return JsonError::AllocationFailed;
            *this->stringValue = *other.stringValue;
            break;
        }

        case JsonType::Array:
        {
            this->arrayValue = allocContainer<JsonValue::Array>(this->alloc);
            if (__blib_unlikely(!this->arrayValue))
                return JsonError::AllocationFailed;
            this->arrayValue->reserve(other.arrayValue->size());
            for (const JsonValue& element : *other.arrayValue)
            {
                // Копия-нода разделяет аллокатор нового дерева (не владеет)
                JsonValue copy(this->alloc, JsonType::Null);
                JsonError err = copy.copyFrom(element);
                if (__blib_unlikely(err != JsonError::None))
                    return err;
                this->arrayValue->push_back(std::move(copy));
            }
            break;
        }

        case JsonType::Object:
        {
            this->objectValue = allocContainer<JsonValue::Object>(this->alloc);
            if (__blib_unlikely(!this->objectValue))
                return JsonError::AllocationFailed;
            this->objectValue->reserve(other.objectValue->size());
            for (const JsonValue::Member& member : *other.objectValue)
            {
                JsonValue::String key(blib::memory::StdAllocatorAdapter<char>(this->alloc));
                key = member.first;

                JsonValue copy(this->alloc, JsonType::Null);
                JsonError err = copy.copyFrom(member.second);
                if (__blib_unlikely(err != JsonError::None))
                    return err;
                this->objectValue->emplace_back(std::move(key), std::move(copy));
            }
            break;
        }
    }

    return JsonError::None;
}

JsonType JsonValue::type() const
{
    return this->nodeType;
}

bool JsonValue::isNull() const
{
    return this->nodeType == JsonType::Null;
}

bool JsonValue::isBool() const
{
    return this->nodeType == JsonType::Bool;
}

bool JsonValue::isNumber() const
{
    return this->nodeType == JsonType::Number;
}

bool JsonValue::isString() const
{
    return this->nodeType == JsonType::String;
}

bool JsonValue::isArray() const
{
    return this->nodeType == JsonType::Array;
}

bool JsonValue::isObject() const
{
    return this->nodeType == JsonType::Object;
}

bool JsonValue::asBool() const
{
    return this->boolValue;
}

bint64 JsonValue::asBint64() const
{
    if (this->isInteger)
        return this->numUnsigned ? static_cast<bint64>(this->uintValue) : this->intValue;
    return static_cast<bint64>(this->realValue);
}

buint64 JsonValue::asBuint64() const
{
    if (this->isInteger)
        return this->numUnsigned ? this->uintValue : static_cast<buint64>(this->intValue);
    return static_cast<buint64>(this->realValue);
}

bfloat JsonValue::asBfloat() const
{
    return static_cast<bfloat>(this->numberToDouble());
}

bdouble JsonValue::asBdouble() const
{
    return this->numberToDouble();
}

bdouble JsonValue::numberToDouble() const
{
    if (this->isInteger)
        return this->numUnsigned ? static_cast<bdouble>(this->uintValue) : static_cast<bdouble>(this->intValue);
    return this->realValue;
}

const JsonValue::String& JsonValue::asString() const
{
    return *this->stringValue;
}

const JsonValue::Array& JsonValue::asArray() const
{
    return *this->arrayValue;
}

const JsonValue::Object& JsonValue::asObject() const
{
    return *this->objectValue;
}

buint32 JsonValue::size() const
{
    if (this->nodeType == JsonType::Array)
        return static_cast<buint32>(this->arrayValue->size());
    if (this->nodeType == JsonType::Object)
        return static_cast<buint32>(this->objectValue->size());
    return 0;
}

bool JsonValue::empty() const
{
    return this->size() == 0;
}

JsonValue& JsonValue::operator[](_In buint32 index)
{
    return (*this->arrayValue)[index];
}

const JsonValue& JsonValue::operator[](_In buint32 index) const
{
    return (*this->arrayValue)[index];
}

JsonValue& JsonValue::pushBack(JsonValue value)
{
    this->arrayValue->push_back(std::move(value));
    return this->arrayValue->back();
}

void JsonValue::remove(_In buint32 index)
{
    this->arrayValue->erase(this->arrayValue->begin() + index);
}

void JsonValue::clear()
{
    if (this->nodeType == JsonType::Array)
        this->arrayValue->clear();
    else if (this->nodeType == JsonType::Object)
        this->objectValue->clear();
}

bool JsonValue::has(_In const char* key) const
{
    for (const JsonValue::Member& member : *this->objectValue)
    {
        if (member.first == key)
            return true;
    }
    return false;
}

JsonValue& JsonValue::get(_In const char* key)
{
    for (JsonValue::Member& member : *this->objectValue)
    {
        if (member.first == key)
            return member.second;
    }
    // Ключа нет — вставляем Null (нода разделяет аллокатор документа)
    JsonValue::String newKey(blib::memory::StdAllocatorAdapter<char>(this->alloc));
    newKey = key;
    this->objectValue->emplace_back(std::move(newKey), JsonValue(this->alloc, JsonType::Null));
    return this->objectValue->back().second;
}

const JsonValue& JsonValue::get(_In const char* key) const
{
    for (const JsonValue::Member& member : *this->objectValue)
    {
        if (member.first == key)
            return member.second;
    }
    // Предусловие has(key) нарушено (документированное UB) — логируем и
    // возвращаем формальную ссылку (код недостижим по контракту)
    __blib_log_error("JsonValue: get() called with missing key '%s'", key);
    return this->objectValue->front().second;
}

JsonValue& JsonValue::set(_In const char* key, JsonValue value)
{
    for (JsonValue::Member& member : *this->objectValue)
    {
        if (member.first == key)
        {
            member.second = std::move(value);
            return member.second;
        }
    }
    JsonValue::String newKey(blib::memory::StdAllocatorAdapter<char>(this->alloc));
    newKey = key;
    this->objectValue->emplace_back(std::move(newKey), std::move(value));
    return this->objectValue->back().second;
}

void JsonValue::remove(_In const char* key)
{
    for (buint32 i = 0; i < this->objectValue->size(); ++i)
    {
        if ((*this->objectValue)[i].first == key)
        {
            this->objectValue->erase(this->objectValue->begin() + i);
            return;
        }
    }
}

bool JsonValue::operator==(_In const JsonValue& other) const
{
    return this->equalValue(other);
}

bool JsonValue::operator!=(_In const JsonValue& other) const
{
    return !this->equalValue(other);
}

bool JsonValue::equalValue(_In const JsonValue& other) const
{
    if (this->nodeType != other.nodeType)
        return false;

    switch (this->nodeType)
    {
        case JsonType::Null:
            return true;

        case JsonType::Bool:
            return this->boolValue == other.boolValue;

        case JsonType::Number:
            if (this->isInteger && other.isInteger)
            {
                if (this->numUnsigned == other.numUnsigned)
                    return this->numUnsigned
                        ? this->uintValue == other.uintValue
                        : this->intValue == other.intValue;
                // смешанная знаковость — сравниваем как bdouble
                return this->numberToDouble() == other.numberToDouble();
            }
            return this->numberToDouble() == other.numberToDouble();

        case JsonType::String:
            return *this->stringValue == *other.stringValue;

        case JsonType::Array:
        {
            if (this->arrayValue->size() != other.arrayValue->size())
                return false;
            for (buint32 i = 0; i < this->arrayValue->size(); ++i)
            {
                if (!(*this->arrayValue)[i].equalValue((*other.arrayValue)[i]))
                    return false;
            }
            return true;
        }

        case JsonType::Object:
        {
            // Объекты равны без учёта порядка ключей
            if (this->objectValue->size() != other.objectValue->size())
                return false;
            for (const JsonValue::Member& member : *this->objectValue)
            {
                const JsonValue* otherValue = nullptr;
                for (const JsonValue::Member& otherMember : *other.objectValue)
                {
                    if (otherMember.first == member.first)
                    {
                        otherValue = &otherMember.second;
                        break;
                    }
                }
                if (!otherValue || !member.second.equalValue(*otherValue))
                    return false;
            }
            return true;
        }
    }

    return false;
}

JsonError JsonValue::writeTo(_In blib::core::IOutputStream& out, _In const JsonWriteOptions& options) const
{
    JsonWriter writer;
    return writer.write(out, *this, options);
}

// ============================================================================
// JsonParser
// ============================================================================

// Состояние одного разбора
struct JsonParser::ParseContext
{
    blib::core::IInputStream* in;   // входной поток
    buint64 offset;                 // прочитано байт от начала потока
    buint32 depth;                  // текущая глубина вложенности
    blib::memory::Allocator* alloc; // аллокатор документа (владеет корневой JsonValue)
    buint8 hasPushback;             // 1 — прочитанный вперёд байт возвращён в pushbackByte
    buint8 pushbackByte;
};

JsonParser::JsonParser()
    : errorOffset(0)
#ifdef BLIB_BUILD_TESTS
    , testDocumentAllocator(nullptr)
#endif
{
}

JsonError JsonParser::parse(_In blib::core::IInputStream& in, _Out JsonValue& out)
{
    this->errorOffset = 0;

    ParseContext ctx;
    ctx.in = &in;
    ctx.offset = 0;
    ctx.depth = 0;
    ctx.alloc = nullptr;
    ctx.hasPushback = 0;
    ctx.pushbackByte = 0;

    // doc — временный владелец аллокатора документа: дерево строится
    // с общим аллокатором, а по завершении владение передаётся результату
    JsonValue doc;
    ctx.alloc = doc.alloc;
#ifdef BLIB_BUILD_TESTS
    if (this->testDocumentAllocator)
        ctx.alloc = this->testDocumentAllocator;
#endif

    bool hasByte = false;
    buint8 b = 0;
    skipWhitespace(ctx, hasByte, b);
    if (__blib_unlikely(!hasByte))
    {
        this->errorOffset = ctx.offset;
        __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: empty input");
    }

    // parsed — невладеющая корневая нода: move-присваивания узлов в неё
    // не уничтожают аллокатор (ownsAlloc == 0), он живёт в doc
    JsonValue parsed(ctx.alloc, JsonType::Null);
    JsonError err = parseValue(ctx, b, parsed);
    if (__blib_unlikely(err != JsonError::None))
    {
        this->errorOffset = ctx.offset;
        return err;
    }

    skipWhitespace(ctx, hasByte, b);
    if (__blib_unlikely(hasByte))
    {
        this->errorOffset = ctx.offset;
        __blib_return_error(JsonError::TrailingData, "JsonParser: trailing data at byte %llu", ctx.offset);
    }

    // Успех: дерево уходит в out, владение аллокатором — следом
    out = std::move(parsed);
#ifdef BLIB_BUILD_TESTS
    if (this->testDocumentAllocator)
    {
        // Инъецированный тестом аллокатор остаётся во владении вызывающего:
        // out ссылается на него без владения
        out.ownsAlloc = 0;
    }
    else
#endif
    {
        out.ownsAlloc = 1;
    }
    doc.alloc = nullptr;
    doc.ownsAlloc = 0;

    return JsonError::None;
}

JsonError JsonParser::parse(_In const char* data, size_t size, _Out JsonValue& out)
{
    blib::core::SliceStream stream(data, size);
    return this->parse(stream, out);
}

buint64 JsonParser::getErrorOffset() const
{
    return this->errorOffset;
}

#ifdef BLIB_BUILD_TESTS
void JsonParser::setDocumentAllocatorForTests(_In_opt blib::memory::Allocator* alloc)
{
    this->testDocumentAllocator = alloc;
}
#endif

bool JsonParser::readByte(_In ParseContext& ctx, _Out buint8& b)
{
    if (ctx.hasPushback)
    {
        ctx.hasPushback = 0;
        b = ctx.pushbackByte;
        return true;
    }
    size_t n = ctx.in->read(&b, 1);
    if (n == 1)
    {
        ++ctx.offset;
        return true;
    }
    return false; // контракт IInputStream: 0 == EOF
}

void JsonParser::skipWhitespace(_In ParseContext& ctx, _Out bool& hasByte, _Out buint8& b)
{
    hasByte = false;
    while (readByte(ctx, b))
    {
        if (!(b == ' ' || b == '\t' || b == '\r' || b == '\n'))
        {
            hasByte = true;
            return;
        }
    }
}

JsonError JsonParser::parseValue(_In ParseContext& ctx, buint8 firstByte, _Out JsonValue& out)
{
    // Единственная точка рекурсии: глубина считается здесь, снимается на выходе
    ++ctx.depth;

    JsonError err = JsonError::None;

    if (__blib_unlikely(ctx.depth > jsonMaxDepth))
    {
        __blib_log_error("JsonParser: max depth %u exceeded at byte %llu", jsonMaxDepth, ctx.offset);
        err = JsonError::MaxDepthExceeded;
    }
    else
    {
        switch (firstByte)
        {
            case '{':
            {
                bool ok = false;
                out = JsonValue::makeNode(ctx.alloc, JsonType::Object, ok);
                if (__blib_unlikely(!ok))
                {
                    __blib_log_error("JsonParser: object allocation failed at byte %llu", ctx.offset);
                    err = JsonError::AllocationFailed;
                }
                else
                {
                    err = parseObject(ctx, out);
                }
                break;
            }

            case '[':
            {
                bool ok = false;
                out = JsonValue::makeNode(ctx.alloc, JsonType::Array, ok);
                if (__blib_unlikely(!ok))
                {
                    __blib_log_error("JsonParser: array allocation failed at byte %llu", ctx.offset);
                    err = JsonError::AllocationFailed;
                }
                else
                {
                    err = parseArray(ctx, out);
                }
                break;
            }

            case '"':
            {
                bool ok = false;
                out = JsonValue::makeNode(ctx.alloc, JsonType::String, ok);
                if (__blib_unlikely(!ok))
                {
                    __blib_log_error("JsonParser: string allocation failed at byte %llu", ctx.offset);
                    err = JsonError::AllocationFailed;
                }
                else
                {
                    err = parseStringBody(ctx, *out.stringValue);
                }
                break;
            }

            case 't':
            {
                err = parseLiteral(ctx, "rue");
                if (err == JsonError::None)
                {
                    bool ok = false;
                    out = JsonValue::makeNode(ctx.alloc, JsonType::Bool, ok);
                    out.boolValue = true;
                }
                break;
            }

            case 'f':
            {
                err = parseLiteral(ctx, "alse");
                if (err == JsonError::None)
                {
                    bool ok = false;
                    out = JsonValue::makeNode(ctx.alloc, JsonType::Bool, ok);
                    out.boolValue = false;
                }
                break;
            }

            case 'n':
            {
                err = parseLiteral(ctx, "ull");
                if (err == JsonError::None)
                {
                    bool ok = false;
                    out = JsonValue::makeNode(ctx.alloc, JsonType::Null, ok);
                }
                break;
            }

            default:
                if (firstByte == '-' || (firstByte >= '0' && firstByte <= '9'))
                {
                    bool ok = false;
                    out = JsonValue::makeNode(ctx.alloc, JsonType::Number, ok);
                    err = parseNumber(ctx, firstByte, out);
                }
                else
                {
                    __blib_log_error("JsonParser: unexpected byte 0x%02X at byte %llu", firstByte, ctx.offset);
                    err = JsonError::InvalidSyntax;
                }
                break;
        }
    }

    --ctx.depth;
    return err;
}

JsonError JsonParser::parseLiteral(_In ParseContext& ctx, _In const char* rest)
{
    for (const char* p = rest; *p != '\0'; ++p)
    {
        buint8 b = 0;
        if (__blib_unlikely(!readByte(ctx, b)))
            __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: unexpected end of literal at byte %llu", ctx.offset);
        if (__blib_unlikely(b != static_cast<buint8>(*p)))
            __blib_return_error(JsonError::InvalidSyntax, "JsonParser: invalid literal at byte %llu", ctx.offset);
    }
    return JsonError::None;
}

JsonError JsonParser::parseArray(_In ParseContext& ctx, _Out JsonValue& out)
{
    // '[' уже прочитан
    bool hasByte = false;
    buint8 b = 0;
    skipWhitespace(ctx, hasByte, b);
    if (__blib_unlikely(!hasByte))
        __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: unexpected end of array at byte %llu", ctx.offset);
    if (b == ']')
        return JsonError::None;

    while (true)
    {
        bool ok = false;
        JsonValue element = JsonValue::makeNode(ctx.alloc, JsonType::Null, ok);
        JsonError err = parseValue(ctx, b, element);
        if (__blib_unlikely(err != JsonError::None))
            return err;
        out.arrayValue->push_back(std::move(element));

        skipWhitespace(ctx, hasByte, b);
        if (__blib_unlikely(!hasByte))
            __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: unexpected end of array at byte %llu", ctx.offset);
        if (b == ']')
            return JsonError::None;
        if (__blib_unlikely(b != ','))
            __blib_return_error(JsonError::InvalidSyntax, "JsonParser: expected ',' or ']' at byte %llu", ctx.offset);
        skipWhitespace(ctx, hasByte, b);
        if (__blib_unlikely(!hasByte))
            __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: unexpected end of array at byte %llu", ctx.offset);
    }
}

JsonError JsonParser::parseObject(_In ParseContext& ctx, _Out JsonValue& out)
{
    // '{' уже прочитан
    bool hasByte = false;
    buint8 b = 0;
    skipWhitespace(ctx, hasByte, b);
    if (__blib_unlikely(!hasByte))
        __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: unexpected end of object at byte %llu", ctx.offset);
    if (b == '}')
        return JsonError::None;

    while (true)
    {
        if (__blib_unlikely(b != '"'))
            __blib_return_error(JsonError::InvalidSyntax, "JsonParser: expected '\"' at byte %llu", ctx.offset);

        JsonValue::String key(blib::memory::StdAllocatorAdapter<char>(ctx.alloc));
        JsonError err = parseStringBody(ctx, key);
        if (__blib_unlikely(err != JsonError::None))
            return err;

        skipWhitespace(ctx, hasByte, b);
        if (__blib_unlikely(!hasByte))
            __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: unexpected end of object at byte %llu", ctx.offset);
        if (__blib_unlikely(b != ':'))
            __blib_return_error(JsonError::InvalidSyntax, "JsonParser: expected ':' at byte %llu", ctx.offset);

        skipWhitespace(ctx, hasByte, b);
        if (__blib_unlikely(!hasByte))
            __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: unexpected end of object at byte %llu", ctx.offset);

        bool ok = false;
        JsonValue member = JsonValue::makeNode(ctx.alloc, JsonType::Null, ok);
        err = parseValue(ctx, b, member);
        if (__blib_unlikely(err != JsonError::None))
            return err;

        out.objectValue->emplace_back(std::move(key), std::move(member));

        skipWhitespace(ctx, hasByte, b);
        if (__blib_unlikely(!hasByte))
            __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: unexpected end of object at byte %llu", ctx.offset);
        if (b == '}')
            return JsonError::None;
        if (__blib_unlikely(b != ','))
            __blib_return_error(JsonError::InvalidSyntax, "JsonParser: expected ',' or '}' at byte %llu", ctx.offset);
        skipWhitespace(ctx, hasByte, b);
        if (__blib_unlikely(!hasByte))
            __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: unexpected end of object at byte %llu", ctx.offset);
    }
}

JsonError JsonParser::parseStringBody(_In ParseContext& ctx, _Out JsonValue::String& out)
{
    // Открывающая '"' уже прочитана
    while (true)
    {
        buint8 b = 0;
        if (__blib_unlikely(!readByte(ctx, b)))
            __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: unterminated string at byte %llu", ctx.offset);

        if (b == '"')
            return JsonError::None;

        if (b == '\\')
        {
            buint8 e = 0;
            if (__blib_unlikely(!readByte(ctx, e)))
                __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: unterminated escape at byte %llu", ctx.offset);
            switch (e)
            {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u':
                {
                    JsonError err = parseUnicodeEscape(ctx, out);
                    if (__blib_unlikely(err != JsonError::None))
                        return err;
                    break;
                }
                default:
                    __blib_return_error(JsonError::InvalidEscape, "JsonParser: invalid escape '\\%c' at byte %llu", e, ctx.offset);
            }
            continue;
        }

        if (__blib_unlikely(b < 0x20))
            __blib_return_error(JsonError::InvalidSyntax, "JsonParser: unescaped control byte 0x%02X at byte %llu", b, ctx.offset);

        if (b < 0x80)
        {
            out.push_back(static_cast<char>(b));
            continue;
        }

        // Сырая UTF-8-последовательность: валидируем по мере чтения
        buint32 seqLen = 0;
        buint32 cp = 0;
        if (__blib_unlikely(!utf8SequenceInfo(b, seqLen, cp)))
            __blib_return_error(JsonError::InvalidUtf8, "JsonParser: invalid UTF-8 lead byte 0x%02X at byte %llu", b, ctx.offset);
        out.push_back(static_cast<char>(b));
        for (buint32 i = 1; i < seqLen; ++i)
        {
            buint8 c = 0;
            if (__blib_unlikely(!readByte(ctx, c)))
                __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: truncated UTF-8 sequence at byte %llu", ctx.offset);
            if (__blib_unlikely(c < 0x80 || c > 0xBF))
                __blib_return_error(JsonError::InvalidUtf8, "JsonParser: invalid UTF-8 continuation at byte %llu", ctx.offset);
            cp = (cp << 6) | (c & 0x3F);
            out.push_back(static_cast<char>(c));
        }
        if (__blib_unlikely(!utf8CodePointValid(seqLen, cp)))
            __blib_return_error(JsonError::InvalidUtf8, "JsonParser: invalid UTF-8 code point at byte %llu", ctx.offset);
    }
}

JsonError JsonParser::parseUnicodeEscape(_In ParseContext& ctx, _Out JsonValue::String& out)
{
    // "\u" уже прочитан
    buint8 digits[4];
    for (buint32 i = 0; i < 4; ++i)
    {
        if (__blib_unlikely(!readByte(ctx, digits[i])))
            __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: truncated \\u escape at byte %llu", ctx.offset);
        if (__blib_unlikely(!isHexDigit(digits[i])))
            __blib_return_error(JsonError::InvalidUnicode, "JsonParser: invalid hex digit in \\u escape at byte %llu", ctx.offset);
    }
    buint32 codeUnit = (hexValue(digits[0]) << 12) | (hexValue(digits[1]) << 8)
                     | (hexValue(digits[2]) << 4) | hexValue(digits[3]);

    if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF)
    {
        // Верхний суррогат: обязательна пара "\uDC00..DFFF"
        buint8 b1 = 0;
        buint8 b2 = 0;
        if (__blib_unlikely(!readByte(ctx, b1) || b1 != '\\'))
            __blib_return_error(JsonError::InvalidUnicode, "JsonParser: high surrogate without low at byte %llu", ctx.offset);
        if (__blib_unlikely(!readByte(ctx, b2) || b2 != 'u'))
            __blib_return_error(JsonError::InvalidUnicode, "JsonParser: high surrogate without low at byte %llu", ctx.offset);

        buint8 lowDigits[4];
        for (buint32 i = 0; i < 4; ++i)
        {
            if (__blib_unlikely(!readByte(ctx, lowDigits[i])))
                __blib_return_error(JsonError::UnexpectedEnd, "JsonParser: truncated low surrogate at byte %llu", ctx.offset);
            if (__blib_unlikely(!isHexDigit(lowDigits[i])))
                __blib_return_error(JsonError::InvalidUnicode, "JsonParser: invalid hex digit in low surrogate at byte %llu", ctx.offset);
        }
        buint32 lowUnit = (hexValue(lowDigits[0]) << 12) | (hexValue(lowDigits[1]) << 8)
                        | (hexValue(lowDigits[2]) << 4) | hexValue(lowDigits[3]);
        if (__blib_unlikely(lowUnit < 0xDC00 || lowUnit > 0xDFFF))
            __blib_return_error(JsonError::InvalidUnicode, "JsonParser: invalid low surrogate at byte %llu", ctx.offset);

        buint32 cp = 0x10000 + ((codeUnit - 0xD800) << 10) + (lowUnit - 0xDC00);
        utf8Encode(out, cp);
        return JsonError::None;
    }

    if (__blib_unlikely(codeUnit >= 0xDC00 && codeUnit <= 0xDFFF))
        __blib_return_error(JsonError::InvalidUnicode, "JsonParser: low surrogate without high at byte %llu", ctx.offset);

    utf8Encode(out, codeUnit);
    return JsonError::None;
}

JsonError JsonParser::parseNumber(_In ParseContext& ctx, buint8 firstByte, _Out JsonValue& out)
{
    // Первый символ числа уже прочитан
    char token[jsonMaxNumberTokenLength];
    buint32 len = 0;
    buint8 hasFrac = 0;
    buint8 hasExp = 0;

    token[len++] = static_cast<char>(firstByte);

    while (len < jsonMaxNumberTokenLength)
    {
        buint8 b = 0;
        if (!readByte(ctx, b))
            break; // EOF — токен завершён, валидность решит грамматика

        char c = static_cast<char>(b);
        if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E'))
        {
            // Разделитель: возвращаем байт и завершаем токен
            ctx.hasPushback = 1;
            ctx.pushbackByte = b;
            break;
        }
        if (c == '.')
            hasFrac = 1;
        if (c == 'e' || c == 'E')
            hasExp = 1;
        token[len++] = c;
    }

    // Токен не поместился: если число продолжается — отклоняем (защита от обрезки)
    if (__blib_unlikely(len == jsonMaxNumberTokenLength))
    {
        buint8 b = 0;
        if (readByte(ctx, b))
        {
            char c = static_cast<char>(b);
            if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E')
                __blib_return_error(JsonError::NumberOutOfRange, "JsonParser: number token too long at byte %llu", ctx.offset);
            ctx.hasPushback = 1;
            ctx.pushbackByte = b;
        }
    }

    if (__blib_unlikely(!validateNumberGrammar(token, len)))
        __blib_return_error(JsonError::InvalidSyntax, "JsonParser: invalid number at byte %llu", ctx.offset);

    // "-0" сохраняем как вещественный -0.0 (round-trip знака)
    if (len == 2 && token[0] == '-' && token[1] == '0')
    {
        out.realValue = -0.0;
        out.isInteger = 0;
        return JsonError::None;
    }

    if (!hasFrac && !hasExp)
    {
        // Целое: накопление с проверкой переполнения; при переполнении — bdouble
        bool neg = token[0] == '-';
        buint64 magnitude = 0;
        buint8 overflow = 0;
        const buint64 limit = neg ? jsonInt64MagnitudeLimit : jsonUint64Limit;
        for (buint32 i = neg ? 1 : 0; i < len; ++i)
        {
            buint64 digit = static_cast<buint64>(token[i] - '0');
            if (magnitude > (limit - digit) / 10)
            {
                overflow = 1;
                break;
            }
            magnitude = magnitude * 10 + digit;
        }
        if (!overflow)
        {
            if (neg)
            {
                if (magnitude == jsonInt64MagnitudeLimit)
                    out.intValue = (std::numeric_limits<bint64>::min)();
                else
                    out.intValue = -static_cast<bint64>(magnitude);
                out.numUnsigned = 0;
            }
            else
            {
                out.uintValue = magnitude;
                out.numUnsigned = 1;
            }
            out.isInteger = 1;
            return JsonError::None;
        }
        __blib_log_warning("JsonParser: integer overflow at byte %llu, parsing as double", ctx.offset);
    }

    bdouble value = 0.0;
    auto result = std::from_chars(token, token + len, value, std::chars_format::general);
    if (__blib_unlikely(result.ec != std::errc()))
        __blib_return_error(JsonError::NumberOutOfRange, "JsonParser: number out of range at byte %llu", ctx.offset);
    out.realValue = value;
    out.isInteger = 0;
    return JsonError::None;
}

// ============================================================================
// JsonWriter
// ============================================================================

struct JsonWriter::WriteContext
{
    blib::core::IOutputStream* out; // выходной поток
    buint32 indentSpaces;           // 0 — компактный вывод, иначе pretty
};

JsonWriter::JsonWriter()
{
}

JsonError JsonWriter::write(_In blib::core::IOutputStream& out, _In const JsonValue& value, _In const JsonWriteOptions& options) const
{
    WriteContext ctx;
    ctx.out = &out;
    ctx.indentSpaces = options.indentSpaces;

    return this->writeValue(ctx, value, 0);
}

JsonError JsonWriter::writeValue(_In WriteContext& ctx, _In const JsonValue& value, buint32 level) const
{
    switch (value.type())
    {
        case JsonType::Null:
            return this->writeRaw(ctx, jsonNullText, static_cast<buint32>(sizeof(jsonNullText) - 1));

        case JsonType::Bool:
            if (value.asBool())
                return this->writeRaw(ctx, jsonTrueText, static_cast<buint32>(sizeof(jsonTrueText) - 1));
            return this->writeRaw(ctx, jsonFalseText, static_cast<buint32>(sizeof(jsonFalseText) - 1));

        case JsonType::Number:
            return this->writeNumber(ctx, value);

        case JsonType::String:
            return this->writeString(ctx, value.asString());

        case JsonType::Array:
            return this->writeArray(ctx, value.asArray(), level);

        case JsonType::Object:
            return this->writeObject(ctx, value.asObject(), level);
    }

    __blib_return_error(JsonError::InvalidSyntax, "JsonWriter: unknown value type");
}

JsonError JsonWriter::writeArray(_In WriteContext& ctx, _In const JsonValue::Array& array, buint32 level) const
{
    JsonError err = this->writeRaw(ctx, jsonOpenBracketText, static_cast<buint32>(sizeof(jsonOpenBracketText) - 1));
    if (__blib_unlikely(err != JsonError::None))
        return err;

    // Пустые контейнеры — всегда [] / {} (без внутренних переносов)
    if (array.empty())
        return this->writeRaw(ctx, jsonCloseBracketText, static_cast<buint32>(sizeof(jsonCloseBracketText) - 1));

    for (buint32 i = 0; i < array.size(); ++i)
    {
        if (i > 0)
        {
            err = this->writeRaw(ctx, jsonCommaText, static_cast<buint32>(sizeof(jsonCommaText) - 1));
            if (__blib_unlikely(err != JsonError::None))
                return err;
        }
        if (ctx.indentSpaces > 0)
        {
            err = this->writeRaw(ctx, jsonNewlineText, static_cast<buint32>(sizeof(jsonNewlineText) - 1));
            if (__blib_unlikely(err != JsonError::None))
                return err;
            err = this->writeIndent(ctx, level + 1);
            if (__blib_unlikely(err != JsonError::None))
                return err;
        }
        err = this->writeValue(ctx, array[i], level + 1);
        if (__blib_unlikely(err != JsonError::None))
            return err;
    }

    if (ctx.indentSpaces > 0)
    {
        err = this->writeRaw(ctx, jsonNewlineText, static_cast<buint32>(sizeof(jsonNewlineText) - 1));
        if (__blib_unlikely(err != JsonError::None))
            return err;
        err = this->writeIndent(ctx, level);
        if (__blib_unlikely(err != JsonError::None))
            return err;
    }

    return this->writeRaw(ctx, jsonCloseBracketText, static_cast<buint32>(sizeof(jsonCloseBracketText) - 1));
}

JsonError JsonWriter::writeObject(_In WriteContext& ctx, _In const JsonValue::Object& object, buint32 level) const
{
    JsonError err = this->writeRaw(ctx, jsonOpenBraceText, static_cast<buint32>(sizeof(jsonOpenBraceText) - 1));
    if (__blib_unlikely(err != JsonError::None))
        return err;

    if (object.empty())
        return this->writeRaw(ctx, jsonCloseBraceText, static_cast<buint32>(sizeof(jsonCloseBraceText) - 1));

    for (buint32 i = 0; i < object.size(); ++i)
    {
        if (i > 0)
        {
            err = this->writeRaw(ctx, jsonCommaText, static_cast<buint32>(sizeof(jsonCommaText) - 1));
            if (__blib_unlikely(err != JsonError::None))
                return err;
        }
        if (ctx.indentSpaces > 0)
        {
            err = this->writeRaw(ctx, jsonNewlineText, static_cast<buint32>(sizeof(jsonNewlineText) - 1));
            if (__blib_unlikely(err != JsonError::None))
                return err;
            err = this->writeIndent(ctx, level + 1);
            if (__blib_unlikely(err != JsonError::None))
                return err;
        }

        err = this->writeString(ctx, object[i].first);
        if (__blib_unlikely(err != JsonError::None))
            return err;
        err = this->writeRaw(ctx, ctx.indentSpaces > 0 ? jsonColonSpaceText : jsonColonText,
                             static_cast<buint32>(ctx.indentSpaces > 0 ? sizeof(jsonColonSpaceText) - 1 : sizeof(jsonColonText) - 1));
        if (__blib_unlikely(err != JsonError::None))
            return err;

        err = this->writeValue(ctx, object[i].second, level + 1);
        if (__blib_unlikely(err != JsonError::None))
            return err;
    }

    if (ctx.indentSpaces > 0)
    {
        err = this->writeRaw(ctx, jsonNewlineText, static_cast<buint32>(sizeof(jsonNewlineText) - 1));
        if (__blib_unlikely(err != JsonError::None))
            return err;
        err = this->writeIndent(ctx, level);
        if (__blib_unlikely(err != JsonError::None))
            return err;
    }

    return this->writeRaw(ctx, jsonCloseBraceText, static_cast<buint32>(sizeof(jsonCloseBraceText) - 1));
}

JsonError JsonWriter::writeString(_In WriteContext& ctx, _In const JsonValue::String& string) const
{
    JsonError err = this->writeRaw(ctx, jsonQuoteText, static_cast<buint32>(sizeof(jsonQuoteText) - 1));
    if (__blib_unlikely(err != JsonError::None))
        return err;

    const char* data = string.data();
    size_t size = string.size();
    size_t i = 0;
    while (i < size)
    {
        buint8 c = static_cast<buint8>(data[i]);

        if (c == '"')
            err = this->writeRaw(ctx, jsonEscapedQuoteText, static_cast<buint32>(sizeof(jsonEscapedQuoteText) - 1));
        else if (c == '\\')
            err = this->writeRaw(ctx, jsonEscapedBackslashText, static_cast<buint32>(sizeof(jsonEscapedBackslashText) - 1));
        else if (c == '\b')
            err = this->writeRaw(ctx, jsonEscapedBackspaceText, static_cast<buint32>(sizeof(jsonEscapedBackspaceText) - 1));
        else if (c == '\f')
            err = this->writeRaw(ctx, jsonEscapedFormFeedText, static_cast<buint32>(sizeof(jsonEscapedFormFeedText) - 1));
        else if (c == '\n')
            err = this->writeRaw(ctx, jsonEscapedNewlineText, static_cast<buint32>(sizeof(jsonEscapedNewlineText) - 1));
        else if (c == '\r')
            err = this->writeRaw(ctx, jsonEscapedCarriageText, static_cast<buint32>(sizeof(jsonEscapedCarriageText) - 1));
        else if (c == '\t')
            err = this->writeRaw(ctx, jsonEscapedTabText, static_cast<buint32>(sizeof(jsonEscapedTabText) - 1));
        else if (c < 0x20)
        {
            // Управляющий символ — \u00XX
            char escape[6];
            escape[0] = '\\';
            escape[1] = 'u';
            escape[2] = '0';
            escape[3] = '0';
            escape[4] = jsonHexDigits[(c >> 4) & 0x0F];
            escape[5] = jsonHexDigits[c & 0x0F];
            err = this->writeRaw(ctx, escape, 6);
        }
        else if (c < 0x80)
        {
            err = this->writeRaw(ctx, data + i, 1);
        }
        else
        {
            // UTF-8: валидируем последовательность целиком, затем пишем
            buint32 seqLen = 0;
            buint32 cp = 0;
            if (__blib_unlikely(!utf8SequenceInfo(c, seqLen, cp)))
                __blib_return_error(JsonError::InvalidUtf8, "JsonWriter: invalid UTF-8 lead byte 0x%02X", c);
            if (__blib_unlikely(i + seqLen > size))
                __blib_return_error(JsonError::InvalidUtf8, "JsonWriter: truncated UTF-8 sequence");
            for (buint32 j = 1; j < seqLen; ++j)
            {
                buint8 cj = static_cast<buint8>(data[i + j]);
                if (__blib_unlikely(cj < 0x80 || cj > 0xBF))
                    __blib_return_error(JsonError::InvalidUtf8, "JsonWriter: invalid UTF-8 continuation byte 0x%02X", cj);
                cp = (cp << 6) | (cj & 0x3F);
            }
            if (__blib_unlikely(!utf8CodePointValid(seqLen, cp)))
                __blib_return_error(JsonError::InvalidUtf8, "JsonWriter: invalid UTF-8 code point");

            err = this->writeRaw(ctx, data + i, seqLen);
            i += seqLen;
            continue;
        }

        if (__blib_unlikely(err != JsonError::None))
            return err;
        ++i;
    }

    return this->writeRaw(ctx, jsonQuoteText, static_cast<buint32>(sizeof(jsonQuoteText) - 1));
}

JsonError JsonWriter::writeNumber(_In WriteContext& ctx, _In const JsonValue& value) const
{
    char buffer[jsonNumberBufferSize];

    std::to_chars_result result;
    if (value.isInteger)
    {
        result = value.numUnsigned
            ? std::to_chars(buffer, buffer + jsonNumberBufferSize, value.uintValue)
            : std::to_chars(buffer, buffer + jsonNumberBufferSize, value.intValue);
    }
    else
    {
        // RFC 8259 не допускает неконечных чисел: to_chars выдал бы
        // "nan"/"inf" — невалидный JSON со сломанным round-trip
        if (__blib_unlikely(!std::isfinite(value.realValue)))
            __blib_return_error(JsonError::NumberOutOfRange, "JsonWriter: non-finite number is not representable in JSON");
        result = std::to_chars(buffer, buffer + jsonNumberBufferSize, value.realValue);
    }

    if (__blib_unlikely(result.ec != std::errc()))
        __blib_return_error(JsonError::NumberOutOfRange, "JsonWriter: failed to format number");

    return this->writeRaw(ctx, buffer, static_cast<buint32>(result.ptr - buffer));
}

JsonError JsonWriter::writeRaw(_In WriteContext& ctx, _In const char* text, buint32 len) const
{
    size_t written = 0;
    while (written < len)
    {
        size_t n = ctx.out->write(text + written, len - written);
        if (__blib_unlikely(n == 0))
            __blib_return_error(JsonError::StreamError, "JsonWriter: output stream write failed");
        written += n;
    }
    return JsonError::None;
}

JsonError JsonWriter::writeIndent(_In WriteContext& ctx, buint32 level) const
{
    // level уровней по ctx.indentSpaces пробелов, кусками до jsonMaxIndentChunk
    for (buint32 l = 0; l < level; ++l)
    {
        buint32 remaining = ctx.indentSpaces;
        while (remaining > 0)
        {
            buint32 chunk = remaining > jsonMaxIndentChunk ? jsonMaxIndentChunk : remaining;
            JsonError err = this->writeRaw(ctx, jsonIndentUnit, chunk);
            if (__blib_unlikely(err != JsonError::None))
                return err;
            remaining -= chunk;
        }
    }
    return JsonError::None;
}

} // namespace json
} // namespace core
} // namespace blib
