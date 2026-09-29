#pragma once

#include <beng/config.h>
#include <beng/core/icomponent.h>
#include <beng/core/componentReflection.h>

namespace gravelands
{
    /**
     * UnitComponent — игровой юнит Gravelands (первый игровой
     * компонент, gravelands.Unit).
     *
     * Назначение:
     * - Живёт на АВТОРИТЕТНОЙ сцене сервера: moveSpeed — скорость,
     *   isPlayer — управляется клиентом, moveX/moveZ — входной вектор
     *   (заполняется из CommandPacket, читается MovementSystem);
     * - На клиенте не создаётся: клиент рендерит зеркала юнитов из
     *   снапшотов (см. GRAVELANDS.md, «Сетевой цикл»);
     * - Сериализуем со сценой (свет-паттерн: JSON, verifyRoundTrip).
     *
     * Поля:
     * - moveSpeed — скорость перемещения (мир. ед./с);
     * - isPlayer — юнит управляется игроком (одним на сессию, MVP);
     * - moveX/moveZ — текущий входной вектор (-1/0/+1), НЕ сериализуем
     *   как состояние мира? Сериализуем для простоты roundtrip —
     *   это входной кеш, в save/load сохраняется как есть.
     */
    class UnitComponent : public beng::IComponent
    {
    private:
        bfloat moveSpeed;
        bool isPlayer;
        bint8 moveX;
        bint8 moveZ;

    public:
        // Стабильное имя типа — идентичность типа в таблице типов Scene
        // (регистрация, резолв в шаблонных методах, save/load)
        static constexpr const char* componentTypeName = "gravelands.Unit";

        // Не прятать 1-аргументную точку входа строгого сравнения
        using blib::core::IStrongComparable::strongCompare;

        /**
         * Конструктор: дефолтная скорость из common/config.h
         * (playerMoveSpeed), не-игрок, нулевой ввод.
         */
        UnitComponent();

        ~UnitComponent() override = default;

        UnitComponent(const UnitComponent&) = delete;
        UnitComponent& operator=(const UnitComponent&) = delete;

        bfloat getMoveSpeed() const { return moveSpeed; }
        bool getIsPlayer() const { return isPlayer; }
        bint8 getMoveX() const { return moveX; }
        bint8 getMoveZ() const { return moveZ; }

        void setMoveSpeed(bfloat value) { moveSpeed = value; }
        void setIsPlayer(bool value) { isPlayer = value; }
        void setMoveX(bint8 value) { moveX = value; }
        void setMoveZ(bint8 value) { moveZ = value; }

        /**
         * Рефлексия компонента (контракт HasComponentReflection) —
         * см. componentReflection.h. Определение — в .cpp.
         *
         * Поля: moveSpeed (Float), isPlayer (Bool).
         */
        static const beng::ComponentTypeDescriptor& componentReflection();

        // ========== ISaveLoadable: сериализация и сравнение ==========

        /**
         * Сохранить состояние компонента в поток (JSON-объект).
         */
        blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;

        /**
         * Загрузить состояние компонента из потока (JSON-объект).
         */
        blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;

        /**
         * Строгое (бит-в-бит) сравнение: базовые поля + все данные.
         */
        bool strongCompare(_In const blib::core::IStrongComparable& other,
            _In blib::core::CompareSession& session) const __blib_override;

        /**
         * Round-trip валидация (verifyRoundTrip).
         */
        bool verify() const __blib_override;
    };

} // namespace gravelands
