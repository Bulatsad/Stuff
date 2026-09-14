#pragma once

#include <string>
#include <vector>
#include <utility>
#include <cstddef>
#include <type_traits>

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/inline.h>
#include <blib/utilmacro.h>
#include <blib/core/istream.h>
#include <blib/core/ostream.h>
#include <blib/system/memory/allocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>

namespace blib
{
namespace core
{
namespace json
{
    // Максимальная глубина вложенности документа при разборе.
    // Документы глубже отклоняются с JsonError::MaxDepthExceeded
    // (защита от переполнения стека рекурсивного спуска).
    constexpr buint32 jsonMaxDepth = 512;

    // Тип узла JSON-дерева (RFC 8259: шесть видов значений)
    enum class JsonType : buint8
    {
        Null = 0,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    // Коды ошибок разбора и записи JSON (см. AGENTS.md, «Обработка ошибок»:
    // None = 0 — всегда успех)
    enum class JsonError : buint32
    {
        None = 0,          // успех
        InvalidSyntax,     // неожиданный символ / нарушение структуры
        UnexpectedEnd,     // конец потока посреди значения
        InvalidEscape,     // недопустимый \x-эскейп в строке
        InvalidUnicode,    // невалидный \uXXXX или суррогатная пара
        InvalidUtf8,       // невалидная UTF-8-последовательность
        NumberOutOfRange,  // число не представимо в bint64/buint64/bdouble
        TrailingData,      // данные после корневого значения (строгий режим)
        MaxDepthExceeded,  // превышена jsonMaxDepth
        AllocationFailed,  // не удалось выделить память под узел
        StreamError        // null-поток или сбой чтения/записи
    };

    // Параметры записи документа
    struct JsonWriteOptions
    {
        // 0 — компактный вывод (без пробелов и переносов);
        // иначе pretty-вывод: по indentSpaces пробелов на уровень вложенности
        buint32 indentSpaces = 0;
    };

    class JsonParser;
    class JsonWriter;

    /**
     * JsonValue - узел JSON-дерева (DOM) с value-семантикой.
     *
     * Назначение:
     * - Хранение и ручная сборка JSON-документов (makeArray/makeObject,
     *   pushBack/set/remove)
     * - Результат работы JsonParser, вход JsonWriter
     *
     * Память:
     * - ВСЕ динамические аллокации дерева идут через blib::memory::Allocator
     *   (DefaultAllocator -> GlobalAllocator), включая строки и контейнеры
     *   (std::basic_string/std::vector с blib::memory::StdAllocatorAdapter)
     * - Инвариант стабильного адреса: аллокатор живёт в heap; корень
     *   поддерева владеет им (ownsAlloc), все узлы поддерева держат
     *   невладеющий указатель. Перемещение узлов (реаллокация векторов)
     *   безопасно, т.к. адрес аллокатора не меняется
     * - Глубокое копирование использует ОДИН свежий аллокатор на всё
     *   новое дерево; перенесённые (move) поддеревья сохраняют свои
     *
     * Числа:
     * - Хранятся как bint64 ИЛИ buint64 (целые) ИЛИ bdouble (вещественные)
     *   с флагом isInteger - точный round-trip в обоих случаях
     * - asBint64/asBuint64/asBdouble приводят число к нужному типу
     *
     * Поведение:
     * - Методы as* и операторы доступа требуют подходящего type()
     *   (документированное предусловие, как у AnyIterator)
     * - operator== сравнивает глубоко; объекты — без учёта порядка ключей;
     *   числа — целые как целые, иначе как bdouble
     * - Не thread-safe (как весь blib-core)
     * - std-контейнеры внутри могут бросить std::bad_alloc при нехватке
     *   памяти (исключения не ловятся; аллокация аллокатора/узлов через
     *   GlobalAllocator при неудаче — __blib_fatal либо JsonError::AllocationFailed)
     */
    class __blib_core_api JsonValue
    {
    public:
        // ---- контейнерные типы (аллокации через аллокатор документа) ----

        typedef std::basic_string<char, std::char_traits<char>, blib::memory::StdAllocatorAdapter<char>> String;
        typedef std::vector<JsonValue, blib::memory::StdAllocatorAdapter<JsonValue>> Array;
        typedef std::pair<String, JsonValue> Member;
        // Объект: вектор пар в порядке вставки (O(n) поиск по ключу)
        typedef std::vector<Member, blib::memory::StdAllocatorAdapter<Member>> Object;

        // ---- конструкторы ----

        /** Null-узел (корень: владеет аллокатором документа). */
        JsonValue();

        /** Null-узел. */
        JsonValue(std::nullptr_t);

        /** Bool-узел. */
        explicit JsonValue(bool value);

        /**
         * Число (целое). Принимает любые целые типы, кроме bool.
         * Знаковые значения хранятся как bint64, беззнаковые — как buint64.
         */
        template<typename T,
                 typename std::enable_if<std::is_integral<typename std::remove_cv<T>::type>::value &&
                                         !std::is_same<typename std::remove_cv<T>::type, bool>::value, int>::type = 0>
        JsonValue(T value);

        /** Число (вещественное). */
        JsonValue(bfloat value);

        /** Число (вещественное). */
        JsonValue(bdouble value);

        /** Строковый узел (копия данных; nullptr недопустим). */
        explicit JsonValue(_In const char* value);

        // Глубокое копирование и перемещение
        JsonValue(_In const JsonValue& other);
        JsonValue& operator=(_In const JsonValue& other);
        JsonValue(JsonValue&& other) noexcept;
        JsonValue& operator=(JsonValue&& other) noexcept;

        ~JsonValue();

        // ---- фабрики ----

        /** Пустой массив (корень). */
        static JsonValue makeArray();

        /** Пустой объект (корень). */
        static JsonValue makeObject();

        // ---- тип ----

        JsonType type() const;
        bool isNull() const;
        bool isBool() const;
        bool isNumber() const;
        bool isString() const;
        bool isArray() const;
        bool isObject() const;

        // ---- доступ к скалярам (предусловие: соответствующий type()) ----

        bool asBool() const;
        bint64 asBint64() const;   // Number (целое), иначе приведение
        buint64 asBuint64() const; // Number (целое), иначе приведение
        bfloat asBfloat() const;   // Number (приведение к float)
        bdouble asBdouble() const; // Number (приведение к double)
        const String& asString() const;
        const Array& asArray() const;
        const Object& asObject() const;

        // ---- размер (Array и Object; у прочих — 0) ----

        buint32 size() const;
        bool empty() const;

        // ---- массив (предусловие: isArray()) ----

        JsonValue& operator[](_In buint32 index);
        const JsonValue& operator[](_In buint32 index) const;
        JsonValue& pushBack(JsonValue value); // возвращает ссылку на добавленный элемент
        void remove(_In buint32 index);       // предусловие: index < size()
        void clear();                         // Array/Object — удалить все элементы

        // ---- объект (предусловие: isObject()) ----

        bool has(_In const char* key) const;
        /** Вставить Null при отсутствии ключа и вернуть ссылку на значение. */
        JsonValue& get(_In const char* key);
        /** Предусловие: has(key). */
        const JsonValue& get(_In const char* key) const;
        /** Вставить новый ключ или заменить значение существующего; возвращает ссылку на значение. */
        JsonValue& set(_In const char* key, JsonValue value);
        void remove(_In const char* key); // отсутствующий ключ — no-op

        // ---- сравнение ----

        bool operator==(_In const JsonValue& other) const;
        bool operator!=(_In const JsonValue& other) const;

        // ---- запись ----

        /** Сериализовать этот узел в поток (эквивалент JsonWriter::write). */
        JsonError writeTo(_In blib::core::IOutputStream& out, _In const JsonWriteOptions& options = JsonWriteOptions()) const;

    private:
        // Конструктор узла с общим аллокатором документа (не владеет им).
        // Для String/Array/Object сразу выделяет контейнер через sharedAlloc;
        // при неудаче оставляет указатель нулевым (проверка — в makeNode).
        JsonValue(_In blib::memory::Allocator* sharedAlloc, JsonType nodeType);

        // Фабрика узла для парсера/внутреннего использования:
        // allocationOk = false, если контейнер выделить не удалось
        static JsonValue makeNode(_In blib::memory::Allocator* sharedAlloc, JsonType nodeType, _Out bool& allocationOk);

        // Выделить аллокатор документа в heap через GlobalAllocator.
        // Неудача — __blib_fatal (конструкторы не могут вернуть ошибку).
        static blib::memory::Allocator* acquireDocumentAllocator();

        void destroy(); // освободить union-член и (если ownsAlloc) аллокатор
        JsonError copyFrom(_In const JsonValue& other); // глубокое копирование в this (аллокатор this)
        bdouble numberToDouble() const;
        bool equalValue(_In const JsonValue& other) const;

        JsonType nodeType; // тип узла (не назван type — конфликт с методом type())

        // Активный член определяется type. Все члены тривиально копируемы
        // (8 байт), копирование union — побайтовое (std::memcpy).
        union
        {
            bool boolValue;      // Bool
            bint64 intValue;     // Number: isInteger == 1, numUnsigned == 0
            buint64 uintValue;   // Number: isInteger == 1, numUnsigned == 1
            bdouble realValue;   // Number: isInteger == 0
            String* stringValue; // String (контейнер в heap)
            Array* arrayValue;   // Array (контейнер в heap)
            Object* objectValue; // Object (контейнер в heap)
        };

        buint8 isInteger;   // Number: 1 — целое (intValue/uintValue), 0 — realValue
        buint8 numUnsigned; // Number: 1 — uintValue, 0 — intValue (только при isInteger == 1)

        blib::memory::Allocator* alloc; // общий аллокатор (не владеем; адрес стабилен — heap)
        buint8 ownsAlloc;               // 1 — this владеет *alloc (корень поддерева)

        friend class JsonParser;
        friend class JsonWriter;
    };

    // ------------------------------------------------------------------------
    // Реализация интегрального конструктора JsonValue
    // ------------------------------------------------------------------------

    template<typename T, typename std::enable_if<std::is_integral<typename std::remove_cv<T>::type>::value &&
                                                 !std::is_same<typename std::remove_cv<T>::type, bool>::value, int>::type>
    __blib_inline JsonValue::JsonValue(T value)
        : nodeType(JsonType::Null)
        , isInteger(0)
        , numUnsigned(0)
        , alloc(nullptr)
        , ownsAlloc(0)
    {
        // Корень: выделяем аллокатор документа в heap (неудача — fatal)
        this->alloc = acquireDocumentAllocator();
        this->ownsAlloc = 1;

        this->nodeType = JsonType::Number;
        this->isInteger = 1;
        if (std::is_signed<T>::value)
        {
            this->numUnsigned = 0;
            this->intValue = static_cast<bint64>(value);
        }
        else
        {
            this->numUnsigned = 1;
            this->uintValue = static_cast<buint64>(value);
        }
    }

    /**
     * JsonParser - строгий разбор JSON (RFC 8259) из потока в JsonValue.
     *
     * Поведение:
     * - Читает поток от текущей позиции до конца; trailing-данные — ошибка
     * - Строки: \uXXXX (с суррогатными парами) декодируются в UTF-8;
     *   сырые байты >= 0x80 валидируются как UTF-8
     * - Числа: целые без дробной части/экспоненты — bint64/buint64
     *   (переполнение int64 -> попытка bdouble), иначе bdouble
     * - Глубина вложенности ограничена jsonMaxDepth
     * - Читает поток побайтово (простота и non-seekable поддержка,
     *   без внутренней буферизации)
     * - Не thread-safe; один экземпляр — последовательное использование
     *
     * Использование:
     *   JsonParser parser;
     *   JsonValue doc;
     *   JsonError err = parser.parse(stream, doc);
     *   if (err != JsonError::None) { buint64 off = parser.getErrorOffset(); }
     */
    class __blib_core_api JsonParser
    {
    public:
        JsonParser();

        /**
         * Разобрать документ из потока. При успехе out получает дерево
         * (старое содержимое out уничтожается), при ошибке out не меняется.
         *
         * @param in  Входной поток (валидная ссылка); чтение от текущей позиции
         * @param out Результат
         * @return JsonError::None при успехе
         */
        JsonError parse(_In blib::core::IInputStream& in, _Out JsonValue& out);

        /**
         * Разобрать документ из буфера (окно SliceStream, без копирования).
         *
         * @param data Указатель на данные (не nullptr при size > 0)
         * @param size Размер данных в байтах
         * @param out  Результат
         */
        JsonError parse(_In const char* data, size_t size, _Out JsonValue& out);

        /**
         * Смещение последней ошибки: количество байтов, прочитанных из
         * потока к моменту обнаружения ошибки (1-based позиция проблемного
         * байта; для EOF-ошибок — размер прочитанного). 0 при отсутствии ошибок.
         */
        buint64 getErrorOffset() const;

    private:
        // Состояние разбора; определён в impl/json.cpp
        struct ParseContext;

        // Рекурсивный спуск. firstByte — уже прочитанный первый байт значения.
        JsonError parseValue(_In ParseContext& ctx, buint8 firstByte, _Out JsonValue& out);

        // Разбор контейнеров (открывающая скобка уже прочитана, out — готовая нода)
        JsonError parseArray(_In ParseContext& ctx, _Out JsonValue& out);
        JsonError parseObject(_In ParseContext& ctx, _Out JsonValue& out);

        // Разбор тела строки (открывающая кавычка уже прочитана)
        JsonError parseStringBody(_In ParseContext& ctx, _Out JsonValue::String& out);
        JsonError parseUnicodeEscape(_In ParseContext& ctx, _Out JsonValue::String& out);

        // Разбор числа (firstByte — уже прочитанный первый символ)
        JsonError parseNumber(_In ParseContext& ctx, buint8 firstByte, _Out JsonValue& out);

        // Литералы true/false/null (остаток литерала после первого символа)
        JsonError parseLiteral(_In ParseContext& ctx, _In const char* rest);

        // Чтение одного байта (с учётом pushback) и пропуск пробельных символов
        bool readByte(_In ParseContext& ctx, _Out buint8& b);
        void skipWhitespace(_In ParseContext& ctx, _Out bool& hasByte, _Out buint8& b);

        buint64 errorOffset; // смещение последней ошибки (0 — успех)
    };

    /**
     * JsonWriter - сериализация JsonValue в поток (RFC 8259).
     *
     * Поведение:
     * - Компактный вывод при JsonWriteOptions::indentSpaces == 0,
     *   иначе pretty с заданным отступом
     * - Строки: эскейпинг ", \, управляющих символов (< 0x20 — \uXXXX),
     *   сырые байты >= 0x80 валидируются как UTF-8
     * - Числа: std::to_chars (shortest round-trip, locale-independent)
     * - Пустые контейнеры: [] / {} (без внутренних переносов)
     * - Не thread-safe
     */
    class __blib_core_api JsonWriter
    {
    public:
        JsonWriter();

        /**
         * Записать документ в поток.
         *
         * @param out     Выходной поток (валидная ссылка)
         * @param value   Корень документа
         * @param options Параметры форматирования
         * @return JsonError::None при успехе
         */
        JsonError write(_In blib::core::IOutputStream& out, _In const JsonValue& value, _In const JsonWriteOptions& options = JsonWriteOptions()) const;

    private:
        // Состояние записи; определён в impl/json.cpp
        struct WriteContext;

        JsonError writeValue(_In WriteContext& ctx, _In const JsonValue& value, buint32 level) const;
        JsonError writeArray(_In WriteContext& ctx, _In const JsonValue::Array& array, buint32 level) const;
        JsonError writeObject(_In WriteContext& ctx, _In const JsonValue::Object& object, buint32 level) const;
        JsonError writeString(_In WriteContext& ctx, _In const JsonValue::String& string) const;
        JsonError writeNumber(_In WriteContext& ctx, _In const JsonValue& value) const;
        JsonError writeRaw(_In WriteContext& ctx, _In const char* text, buint32 len) const;
        JsonError writeIndent(_In WriteContext& ctx, buint32 level) const;
    };

} // namespace json
} // namespace core
} // namespace blib
