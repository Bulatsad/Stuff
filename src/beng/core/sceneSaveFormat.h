#pragma once

#include <blib/blibint.h>

namespace beng
{
    // ========== Формат файла сохранения сцены ==========
    //
    // Файл = магическая сигнатура (5 байт "JSON\0") + JSON-документ:
    //
    //   {
    //     "format": "beng.scene",    // sceneSaveFormatName (в поле sceneSaveFormatFieldName)
    //     "version": 1,             // sceneSaveVersion (в поле sceneSaveVersionField)
    //     "nextEntityId": <buint64>,
    //     "entities": [
    //       {
    //         "id": <buint64>,
    //         "components": [
    //           { "type": "beng.Transform", "data": { ...JSON компонента... } },
    //           ...
    //         ]
    //       },
    //       ...
    //     ]
    //   }
    //
    // - Типы компонентов ссылаются по стабильным именам T::componentTypeName
    //   (резолв на загрузке через typeIdByName сцены; незарегистрированное
    //   имя -> LoadStatus::ComponentTypeNotRegistered)
    // - ИНВАРИАНТ СЦЕНЫ: TransformComponent обязателен у КАЖДОЙ сущности —
    //   ровно одна запись "type": "beng.Transform" в components каждой
    //   сущности (иначе LoadStatus::InvalidData). Локальный typeId 0 всегда
    //   зарезервирован за TransformComponent (регистрируется сценой первым).
    //   Файлы, сохранённые до введения инварианта (сущность без Transform),
    //   загрузкой отвергаются; версия формата при этом не менялась.
    // - Системы не сериализуются (код игры добавляет их после load)
    // - Кеш ресурсов (RM) не сериализуется: путь модели пишется в данные
    //   SkinnedMeshComponent, после load компонент перезагружается через
    //   scene.getResources() в onLoaded (см. BENG.md, «Сохранение/загрузка»)

    // Размер магической сигнатуры в начале файла: "JSON" + NUL
    constexpr buint32 sceneSaveMagicSize = 5;

    // Магическая сигнатура файла (несовпадение -> LoadStatus::UnknownFormat)
    constexpr buint8 sceneSaveMagic[sceneSaveMagicSize] = { 'J', 'S', 'O', 'N', 0 };

    // Имя формата документа (значение поля sceneSaveFormatFieldName)
    constexpr const char* sceneSaveFormatName = "beng.scene";

    // Версия формата (несовпадение -> LoadStatus::VersionMismatch)
    constexpr buint32 sceneSaveVersion = 1;

    // Ключи полей JSON-документа (именованные константы — правило про
    // вшитые строки; тесты ссылаются на эти же имена)
    constexpr const char* sceneSaveFormatFieldName = "format";
    constexpr const char* sceneSaveVersionField = "version";
    constexpr const char* sceneSaveNextEntityIdField = "nextEntityId";
    constexpr const char* sceneSaveEntitiesField = "entities";
    constexpr const char* sceneSaveEntityIdField = "id";
    constexpr const char* sceneSaveEntityComponentsField = "components";
    constexpr const char* sceneSaveComponentTypeField = "type";
    constexpr const char* sceneSaveComponentDataField = "data";

} // namespace beng
