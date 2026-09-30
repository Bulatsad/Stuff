#pragma once

#include <beng/config.h>
#include <beng/core/replicationCodec.h>
#include <beng/core/replicationFramer.h>
#include <beng/core/replicationSchema.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

namespace beng
{
    class Scene;

    namespace server
    {
        /**
         * ReplicationManager — серверная сторона рефлексивной репликации
         * (beng-server): строит снапшоты авторитетной сцены для каждого
         * клиента из per-client ЗЕРКАЛ (последнее отправленное состояние).
         *
         * Модель «зеркало на сервере»: для каждого клиента хранится его
         * текущее известное состояние мира (сущности + значения полей).
         * После тика симуляции buildSnapshot сравнивает сцену с зеркалом:
         * - неизвестная сущность → полный патч (spawn);
         * - изменённые поля → дельта-патч (только грязные значения);
         * - сущность исчезла из сцены → destroy-патч;
         * - неизменённое — не отправляется.
         *
         * TCP упорядочен и надёжен, поэтому зеркало обновляется тем,
         * что РЕАЛЬНО отправлено; дроп очереди NetworkServer (переполнение
         * при WouldBlock) помечает клиента forceFull — следующий снапшот
         * целиком полный (зеркало сбрасывается).
         *
         * Ограничения (MVP, см. SERVER.md):
         * - удаление КОМПОНЕНТА у живой сущности не реплицируется
         *   (зеркало не узнает); сущности — только целиком;
         * - лимиты структур фиксированы (maxSnapshotEntities и т.д.).
         */
        class __beng_api ReplicationManager
        {
        public:
            // Максимум клиентов (совпадает с NetworkServer)
            static constexpr buint32 maxClients = 4;

            // Максимум зеркальных сущностей на клиента (на сервере).
            // Зеркала хранятся В КУЧЕ (GlobalAllocator): суммарный
            // размер per-client структур не умещается на стеке
            static constexpr buint32 maxMirrorEntities = maxSnapshotEntities * 2;

            ReplicationManager();
            ~ReplicationManager();

            ReplicationManager(const ReplicationManager&) = delete;
            ReplicationManager& operator=(const ReplicationManager&) = delete;

            /**
             * Привязать схему репликации (построена по сцене сервера).
             * Вызывать до buildSnapshot.
             */
            void initialize(_In const ReplicationSchema& schema);

            /**
             * Клиент подключился: сбросить его зеркало (следующий
             * снапшот — полностью полный).
             */
            void onClientConnected(buint32 clientId);

            /**
             * Клиент отключился: освободить его зеркало.
             */
            void onClientDisconnected(buint32 clientId);

            /**
             * Собрать снапшот сцены для клиента в буферы вызывающего.
             *
             * @param clientId Клиент (0..maxClients-1)
             * @param forceFull Полный снапшот (после дропа очереди сети)
             * @param outEntities Приёмник патчей (массив maxSnapshotEntities)
             * @param outEntityCount Число заполненных патчей
             * @return false — лимиты превышены (снапшот не собран)
             */
            bool buildSnapshot(buint32 clientId, _In const Scene& scene, buint32 tickNumber,
                bool forceFull, _Out ReplicationEntityPatch* outEntities, _Out buint32& outEntityCount);

        private:
            // Зеркало КОМПОНЕНТА: последние отправленные значения полей
            struct MirrorComponent
            {
                ComponentType localTypeId;
                buint8 fieldCount;                      // == схеме
                FieldValue values[maxReplicatedFields]; // значения полей схемы
            };

            // Зеркало СУЩНОСТИ
            struct MirrorEntity
            {
                EntityID entityId;
                MirrorComponent components[maxSnapshotComponentsPerEntity];
                buint32 componentCount;
            };

            // Зеркало КЛИЕНТА
            struct ClientMirror
            {
                bool active;
                MirrorEntity entities[maxMirrorEntities];
                buint32 entityCount;

                // Сущности, удалённые со сцены, но ещё не сообщённые клиенту
                EntityID pendingDestroys[maxMirrorEntities];
                buint32 pendingDestroyCount;
            };

            const ReplicationSchema* schema;

            // Зеркала клиентов — в куче (GlobalAllocator): массивы
            // суммарно ~1 МБ — стек не резиновый
            ClientMirror* mirrors;

            // Индекс зеркальной сущности или maxMirrorEntities (нет)
            buint32 findMirrorEntity(_In const ClientMirror& mirror, EntityID id) const;

            // Собрать полный патч сущности (все реплицируемые компоненты)
            bool fillFullPatch(_In const Scene& scene, EntityID entityId,
                _Out ReplicationEntityPatch& patch, _In_Out MirrorEntity& mirrorEntity);

            // Собрать дельта-патч: сравнение с зеркалом по рефлексии
            bool fillDeltaPatch(_In const Scene& scene, EntityID entityId,
                _In_Out MirrorEntity& mirrorEntity, _Out ReplicationEntityPatch& patch);
        };

    } // namespace server
} // namespace beng
