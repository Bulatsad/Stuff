#pragma once

#include <beng/config.h>
#include <beng/core/icomponent.h>

#include <blib/core/math/vector.h>

#include <type_traits>

namespace beng
{
    /**
     * FieldValue — type-erased значение поля компонента.
     *
     * Рефлексия работает без RTTI и typeid: поле описывается типом
     * `FieldValue::Kind` и набором значений-членов (активно то, на
     * которое указывает kind). Набор типов намеренно узкий — его
     * хватает Inspector'у (числа, флаги, вектора); сложные типы
     * (меши, анимации) правятся не рефлексией, а специализированными
     * панелями.
     */
    struct __beng_api FieldValue
    {
        /**
         * Тип значения поля.
         */
        enum class Kind : buint8
        {
            Unset = 0,  // значение не установлено (дефолт)
            Float,      // floatValue
            Int,        // intValue
            Bool,       // boolValue
            Vector3     // vector3Value
        };

        Kind kind = Kind::Unset;
        bfloat floatValue = 0.0f;
        bint32 intValue = 0;
        bool boolValue = false;
        blib::math::Vector<float, 3> vector3Value{};

        // Фабрики (для заполнения из геттеров полей)
        static FieldValue fromFloat(bfloat value)
        {
            FieldValue v;
            v.kind = Kind::Float;
            v.floatValue = value;
            return v;
        }

        static FieldValue fromInt(bint32 value)
        {
            FieldValue v;
            v.kind = Kind::Int;
            v.intValue = value;
            return v;
        }

        static FieldValue fromBool(bool value)
        {
            FieldValue v;
            v.kind = Kind::Bool;
            v.boolValue = value;
            return v;
        }

        static FieldValue fromVector3(_In const blib::math::Vector<float, 3>& value)
        {
            FieldValue v;
            v.kind = Kind::Vector3;
            v.vector3Value = value;
            return v;
        }
    };

    /**
     * IComponentField — интерфейс одного поля компонента (рефлексия).
     *
     * Контракт:
     * - Поле принадлежит ДЕСКРИПТОРУ конкретного типа компонента
     *   (ComponentTypeDescriptor): getValue/setValue вызываются только
     *   с компонентами того же типа (дескриптор хранится в Scene per
     *   ComponentType — см. registerComponentType);
     * - getValue копирует значение ПОЛЯ в FieldValue (kind валиден);
     * - setValue применяет значение поля (kind обязан совпадать с
     *   getKind(); реализация поля может отклонить правку — тогда
     *   значение не меняется).
     *
     * Поля живут как статические объекты в .cpp компонента и живут
     * всё время процесса (как componentTypeName-литералы).
     */
    class __beng_api IComponentField
    {
    public:
        virtual ~IComponentField() = default;

        /**
         * Имя поля (отображается в Inspector; уникально в дескрипторе).
         */
        virtual const char* getName() const __blib_pure_virtual_function;

        /**
         * Тип значения поля (как его рисовать/парсить).
         */
        virtual FieldValue::Kind getKind() const __blib_pure_virtual_function;

        /**
         * Прочитать значение поля компонента.
         *
         * @param component Компонент типа, которому принадлежит поле
         * @param outValue Приёмник (kind выставляется полем)
         */
        virtual void getValue(_In const IComponent& component, _Out FieldValue& outValue) const __blib_pure_virtual_function;

        /**
         * Записать значение поля компонента.
         *
         * @param component Компонент типа, которому принадлежит поле
         * @param value Значение (kind обязан совпадать с getKind())
         */
        virtual void setValue(_In IComponent& component, _In const FieldValue& value) const __blib_pure_virtual_function;
    };

    /**
     * Типы функций доступа к полю: геттер/сеттер. Обычно — лямбды
     * без захвата из .cpp компонента (преобразуются в fn-указатели),
     * что позволяет полям жить как статические объекты.
     */
    typedef void (*FieldGetter)(_In const IComponent& component, _Out FieldValue& outValue);
    typedef void (*FieldSetter)(_In IComponent& component, _In const FieldValue& value);

    /**
     * FunctionField — поле, реализованное парой геттер/сеттер.
     *
     * Декларативный доступ через публичные API компонента — рефлексия
     * не лезет в приватные члены (нет friend-инвазий). Геттер может
     * вычислять значение (например, азимут из direction).
     *
     * Пример (в .cpp компонента):
     *   static const beng::FunctionField s_intensityField(
     *       "intensity", beng::FieldValue::Kind::Float,
     *       [](const beng::IComponent& c, beng::FieldValue& out) {
     *           out = beng::FieldValue::fromFloat(
     *               static_cast<const MyComponent&>(c).getIntensity());
     *       },
     *       [](beng::IComponent& c, const beng::FieldValue& v) {
     *           static_cast<MyComponent&>(c).setIntensity(v.floatValue);
     *       });
     */
    class __beng_api FunctionField : public IComponentField
    {
    private:
        const char* name;
        FieldValue::Kind kind;
        FieldGetter getter;
        FieldSetter setter;

    public:
        FunctionField(_In const char* name, FieldValue::Kind kind, _In FieldGetter getter, _In FieldSetter setter)
            : name(name)
            , kind(kind)
            , getter(getter)
            , setter(setter)
        {
        }

        const char* getName() const __blib_override { return this->name; }

        FieldValue::Kind getKind() const __blib_override { return this->kind; }

        void getValue(_In const IComponent& component, _Out FieldValue& outValue) const __blib_override
        {
            this->getter(component, outValue);
        }

        void setValue(_In IComponent& component, _In const FieldValue& value) const __blib_override
        {
            this->setter(component, value);
        }
    };

    /**
     * ComponentTypeDescriptor — статический дескриптор типа компонента:
     * стабильное имя + список полей (рефлексия).
     *
     * Из дескрипторов растут Inspector (правка полей без знания
     * конкретного типа), инструменты диагностики и будущая сетевая
     * репликация. Дескриптор живёт как статический объект в .cpp
     * компонента и предоставляется через `T::componentReflection()`
     * (контракт проверяется трейтом HasComponentReflection<T>).
     *
     * Поля перечисляются в порядке отображения (порядок = порядок
     * в Inspector); имена — для отображения (не обязательно совпадают
     * с именами членов).
     */
    class __beng_api ComponentTypeDescriptor
    {
    private:
        const char* typeName;
        const IComponentField* const* fields;
        buint32 fieldCount;

    public:
        ComponentTypeDescriptor(_In const char* typeName, _In const IComponentField* const* fields, buint32 fieldCount)
            : typeName(typeName)
            , fields(fields)
            , fieldCount(fieldCount)
        {
        }

        /**
         * Стабильное имя типа (== T::componentTypeName).
         */
        const char* getTypeName() const { return this->typeName; }

        /**
         * Количество полей.
         */
        buint32 getFieldCount() const { return this->fieldCount; }

        /**
         * Поле по индексу (nullptr при выходе за границы).
         */
        const IComponentField* getField(buint32 index) const
        {
            return (index < this->fieldCount) ? this->fields[index] : nullptr;
        }
    };

    /**
     * HasComponentReflection<T> — трейт контракта рефлексии компонента.
     *
     * Значение: std::true_type если T объявляет статический метод
     * `componentReflection()`, возвращающий const ComponentTypeDescriptor&;
     * иначе std::false_type (компонент без рефлексии — Inspector
     * показывает только имя типа).
     *
     * SFINAE (void_t) гарантирует отсутствие hard error при проверке
     * типа без метода — false_type вместо ошибки компиляции.
     */
    template<typename T, typename = void>
    struct HasComponentReflection : std::false_type
    {
    };

    template<typename T>
    struct HasComponentReflection<T, std::void_t<decltype(T::componentReflection())>>
        : std::is_convertible<decltype(T::componentReflection()), const ComponentTypeDescriptor&>
    {
    };

} // namespace beng
