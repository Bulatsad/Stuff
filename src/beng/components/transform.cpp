#include <beng/components/transform.h>
#include <beng/core/scene.h>
#include <blib/core/console/console.h>
#include <blib/core/json/json.h>
#include <blib/core/verifyHelper.h>

namespace beng
{
    namespace
    {
        // Ключи JSON-объекта компонента (формат save/load) — именованные
        // константы по правилу про вшитые строки
        constexpr const char* keyPosition = "position";
        constexpr const char* keyRotation = "rotation";
        constexpr const char* keyScale = "scale";
        constexpr const char* keyParent = "parent";
        constexpr const char* keyChildren = "children";
        constexpr const char* keyWorldMatrix = "worldMatrix";
        constexpr const char* keyWorldMatrixDirty = "worldMatrixDirty";
        constexpr const char* keyIsActive = "isActive";

        // Размерности сериализуемых структур (квадратная матрица 4x4)
        constexpr buint32 matrixDim = 4;
        constexpr buint32 vector3Size = 3;
        constexpr buint32 quaternionSize = 4;

        // Чтение массива из 3 чисел с проверкой размера (Vector3).
        // false — ключ отсутствует, не массив или неверной длины
        bool readVector3(_In const blib::core::json::JsonValue& doc, _In const char* key,
            _Out blib::math::Vector<float, 3>& out)
        {
            if (!doc.has(key))
            {
                return false;
            }
            const blib::core::json::JsonValue& arr = doc.get(key);
            if (!arr.isArray() || arr.size() != vector3Size)
            {
                return false;
            }
            out = blib::math::Vector<float, 3>(
                arr[0].asBfloat(), arr[1].asBfloat(), arr[2].asBfloat());
            return true;
        }

        // Чтение массива из 4 чисел с проверкой размера (Quaternion).
        bool readQuaternion(_In const blib::core::json::JsonValue& doc, _In const char* key,
            _Out blib::math::Quaternion<float>& out)
        {
            if (!doc.has(key))
            {
                return false;
            }
            const blib::core::json::JsonValue& arr = doc.get(key);
            if (!arr.isArray() || arr.size() != quaternionSize)
            {
                return false;
            }
            out = blib::math::Quaternion<float>(
                arr[3].asBfloat(), arr[0].asBfloat(), arr[1].asBfloat(), arr[2].asBfloat());
            return true;
        }

        // Чтение булева поля с проверкой типа.
        bool readBool(_In const blib::core::json::JsonValue& doc, _In const char* key, _Out bool& out)
        {
            if (!doc.has(key) || !doc.get(key).isBool())
            {
                return false;
            }
            out = doc.get(key).asBool();
            return true;
        }

        // Чтение EntityID (buint64) с проверкой типа.
        bool readEntity(_In const blib::core::json::JsonValue& doc, _In const char* key, _Out EntityID& out)
        {
            if (!doc.has(key) || !doc.get(key).isNumber())
            {
                return false;
            }
            out = static_cast<EntityID>(doc.get(key).asBuint64());
            return true;
        }

        // Сборка локальной TRS-матрицы из позиции/вращения/масштаба.
        // Раскладка согласована с blib::graphics::composeMatrix и
        // ITransformable: column-major хранилище data[столбец][строка],
        // трансляция — в последней КОЛОНКЕ (data[3][0..2]) — именно
        // такую матрицу ожидает рендер (glUniformMatrix4fv с
        // transpose=GL_FALSE; см. CORE.md, «Конвенция матриц»).
        //
        // Формула дублирует blib::graphics::composeMatrix намеренно:
        // beng-core зависит только от blib-core (математика), а
        // composeMatrix живёт в blib-graphics — тащить сюда весь
        // графический модуль нельзя (см. ARCHITECTURE.md, слои).
        blib::math::Matrix<float, 4, 4> composeTrsMatrix(
            _In const blib::math::Vector<float, 3>& position,
            _In const blib::math::Quaternion<float>& rotation,
            _In const blib::math::Vector<float, 3>& scale)
        {
            const blib::math::Quaternion<float> q = rotation.normalize();
            const float qw = q.w, qx = q.x, qy = q.y, qz = q.z;

            const float r00 = 1.0f - 2.0f * (qy * qy + qz * qz);
            const float r01 = 2.0f * (qx * qy - qw * qz);
            const float r02 = 2.0f * (qx * qz + qw * qy);
            const float r10 = 2.0f * (qx * qy + qw * qz);
            const float r11 = 1.0f - 2.0f * (qx * qx + qz * qz);
            const float r12 = 2.0f * (qy * qz - qw * qx);
            const float r20 = 2.0f * (qx * qz - qw * qy);
            const float r21 = 2.0f * (qy * qz + qw * qx);
            const float r22 = 1.0f - 2.0f * (qx * qx + qy * qy);

            // Стандартная column-major TRS: столбцы 0..2 — колонки R*S,
            // колонка 3 — трансляция
            blib::math::Matrix<float, 4, 4> result;
            result.loadIdentity();
            result.data[0][0] = r00 * scale.x;
            result.data[0][1] = r10 * scale.x;
            result.data[0][2] = r20 * scale.x;
            result.data[1][0] = r01 * scale.y;
            result.data[1][1] = r11 * scale.y;
            result.data[1][2] = r21 * scale.y;
            result.data[2][0] = r02 * scale.z;
            result.data[2][1] = r12 * scale.z;
            result.data[2][2] = r22 * scale.z;
            result.data[3][0] = position.x;
            result.data[3][1] = position.y;
            result.data[3][2] = position.z;
            return result;
        }
    }

    TransformComponent::TransformComponent(_In Scene* scene)
        : localPosition(0.0f, 0.0f, 0.0f)
        , localRotation(1.0f, 0.0f, 0.0f, 0.0f) // identity quaternion (w=1)
        , localScale(1.0f, 1.0f, 1.0f)
        , parent(invalidEntity)
        , worldMatrixDirty(true)
        , ownerScene(scene)
    {
        // Инициализировать мировую матрицу как identity
        worldMatrix.loadIdentity();
    }

    TransformComponent::TransformComponent()
        : TransformComponent(nullptr)
    {
    }

    void TransformComponent::setLocalPosition(const blib::math::Vector<float, 3>& pos)
    {
        localPosition = pos;
        markDirty();
    }

    void TransformComponent::setLocalRotation(const blib::math::Quaternion<float>& rot)
    {
        localRotation = rot;
        markDirty();
    }

    void TransformComponent::setLocalScale(const blib::math::Vector<float, 3>& scale)
    {
        localScale = scale;
        markDirty();
    }

    blib::math::Vector<float, 3> TransformComponent::getWorldPosition() const
    {
        if (worldMatrixDirty)
        {
            updateWorldMatrix();
        }

        // Извлечь позицию из мировой матрицы: column-major, трансляция —
        // в последней колонке (data[3][0..2])
        return blib::math::Vector<float, 3>(
            worldMatrix.data[3][0],
            worldMatrix.data[3][1],
            worldMatrix.data[3][2]
        );
    }

    blib::math::Quaternion<float> TransformComponent::getWorldRotation() const
    {
        if (parent == invalidEntity)
        {
            return localRotation;
        }

        // Получить Transform родителя.
        // Циклы невозможны — setParent отклоняет их установку.
        TransformComponent* parentTransform = ownerScene->tryGetComponent<TransformComponent>(parent);
        if (parentTransform == nullptr)
        {
            __blib_log_warning("TransformComponent: parent Entity %llu has no Transform",
                static_cast<unsigned long long>(parent));
            return localRotation;
        }

        // Умножить quaternions: worldRot = parentWorldRot * localRot
        return parentTransform->getWorldRotation() * localRotation;
    }

    blib::math::Vector<float, 3> TransformComponent::getWorldScale() const
    {
        if (parent == invalidEntity)
        {
            return localScale;
        }

        // Получить Transform родителя
        TransformComponent* parentTransform = ownerScene->tryGetComponent<TransformComponent>(parent);
        if (parentTransform == nullptr)
        {
            return localScale;
        }

        // Покомпонентное умножение масштабов
        blib::math::Vector<float, 3> parentScale = parentTransform->getWorldScale();
        return blib::math::Vector<float, 3>(
            localScale.x * parentScale.x,
            localScale.y * parentScale.y,
            localScale.z * parentScale.z
        );
    }

    blib::math::Matrix<float, 4, 4> TransformComponent::getWorldMatrix() const
    {
        if (worldMatrixDirty)
        {
            updateWorldMatrix();
        }
        return worldMatrix;
    }

    void TransformComponent::setParent(EntityID parentId)
    {
        // Проверка на установку самого себя родителем
        if (parentId == getOwnerId())
        {
            __blib_log_warning("TransformComponent: Entity %llu cannot be its own parent",
                static_cast<unsigned long long>(getOwnerId()));
            return;
        }

        // Проверка на циклическую зависимость: проходим вверх по цепочке
        // родителей кандидата; если встречаем собственный EntityID — цикл.
        {
            EntityID cursor = parentId;
            while (cursor != invalidEntity)
            {
                if (cursor == getOwnerId())
                {
                    __blib_log_warning("TransformComponent: setting parent %llu would create a cycle for Entity %llu",
                        static_cast<unsigned long long>(parentId),
                        static_cast<unsigned long long>(getOwnerId()));
                    return;
                }

                TransformComponent* ancestor = ownerScene->tryGetComponent<TransformComponent>(cursor);
                if (ancestor == nullptr)
                {
                    break; // дальше идти некуда — цикла нет
                }
                cursor = ancestor->parent;
            }
        }

        // Отвязаться от старого родителя (убрать себя из его children)
        if (parent != invalidEntity)
        {
            TransformComponent* oldParentTransform = ownerScene->tryGetComponent<TransformComponent>(parent);
            if (oldParentTransform != nullptr)
            {
                // Прямой доступ к приватному полю другого экземпляра
                // того же класса — легально в C++
                auto& siblings = oldParentTransform->children;
                for (auto it = siblings.begin(); it != siblings.end(); ++it)
                {
                    if (*it == getOwnerId())
                    {
                        siblings.erase(it);
                        break;
                    }
                }
            }
        }

        // Привязаться к новому родителю
        if (parentId != invalidEntity)
        {
            TransformComponent* newParentTransform = ownerScene->tryGetComponent<TransformComponent>(parentId);
            if (newParentTransform == nullptr)
            {
                __blib_log_warning("TransformComponent: parent Entity %llu not found or has no Transform",
                    static_cast<unsigned long long>(parentId));
                parent = invalidEntity;
                markDirty();
                return;
            }

            // Добавить себя в children нового родителя (без дубликатов)
            bool alreadyListed = false;
            for (EntityID existingChild : newParentTransform->children)
            {
                if (existingChild == getOwnerId())
                {
                    alreadyListed = true;
                    break;
                }
            }
            if (!alreadyListed)
            {
                newParentTransform->children.push_back(getOwnerId());
            }
        }

        parent = parentId;
        markDirty();
    }

    void TransformComponent::markDirty()
    {
        worldMatrixDirty = true;

        // Рекурсивно пометить всех детей как dirty.
        // children-список поддерживается setParent в актуальном состоянии.
        for (EntityID childId : children)
        {
            TransformComponent* childTransform = ownerScene->tryGetComponent<TransformComponent>(childId);
            if (childTransform != nullptr)
            {
                childTransform->markDirty();
            }
        }
    }

    void TransformComponent::updateWorldMatrix() const
    {
        // Полная TRS-матрица: позиция + вращение + масштаб.
        // Раньше писалась только позиция (TODO в этом месте) — 
        // rotation/scale из local-полей игнорировались, что делало
        // мировую матрицу непригодной для рендера
        worldMatrix = composeTrsMatrix(localPosition, localRotation, localScale);

        // Если есть родитель — умножить на его мировую матрицу.
        // Стандартное column-vector произведение: world = parent * local
        // (Matrix::operator* — обычное произведение, не транспонированное;
        // см. CORE.md, «Конвенция матриц»). getWorldMatrix родителя
        // рекурсивно пересчитает его самого, если он dirty — порядок
        // итерации по dense не влияет на результат.
        if (parent != invalidEntity)
        {
            TransformComponent* parentTransform = ownerScene->tryGetComponent<TransformComponent>(parent);
            if (parentTransform != nullptr)
            {
                blib::math::Matrix<float, 4, 4> parentMatrix = parentTransform->getWorldMatrix();
                worldMatrix = parentMatrix * worldMatrix;
            }
        }

        worldMatrixDirty = false;
    }

    blib::core::SaveStatus TransformComponent::save(_In blib::core::IOutputStream& os) const
    {
        blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

        // Локальный TRS: массивы [x, y, z] / [x, y, z, w]
        blib::core::json::JsonValue& pos = doc.set(keyPosition, blib::core::json::JsonValue::makeArray());
        pos.pushBack(blib::core::json::JsonValue(localPosition.x));
        pos.pushBack(blib::core::json::JsonValue(localPosition.y));
        pos.pushBack(blib::core::json::JsonValue(localPosition.z));

        blib::core::json::JsonValue& rot = doc.set(keyRotation, blib::core::json::JsonValue::makeArray());
        rot.pushBack(blib::core::json::JsonValue(localRotation.x));
        rot.pushBack(blib::core::json::JsonValue(localRotation.y));
        rot.pushBack(blib::core::json::JsonValue(localRotation.z));
        rot.pushBack(blib::core::json::JsonValue(localRotation.w));

        blib::core::json::JsonValue& scale = doc.set(keyScale, blib::core::json::JsonValue::makeArray());
        scale.pushBack(blib::core::json::JsonValue(localScale.x));
        scale.pushBack(blib::core::json::JsonValue(localScale.y));
        scale.pushBack(blib::core::json::JsonValue(localScale.z));

        // Иерархия: parent + children (EntityID, ownerId не пишется —
        // владение восстанавливает Scene::load при создании компонента)
        doc.set(keyParent, blib::core::json::JsonValue(parent));
        blib::core::json::JsonValue& childrenArr =
            doc.set(keyChildren, blib::core::json::JsonValue::makeArray());
        for (EntityID childId : children)
        {
            childrenArr.pushBack(blib::core::json::JsonValue(childId));
        }

        // Кеш мировой матрицы: сериализуется целиком (контракт «всё
        // состояние бит-в-бит», включая кэши — см. ISaveable)
        blib::core::json::JsonValue& matArr =
            doc.set(keyWorldMatrix, blib::core::json::JsonValue::makeArray());
        for (buint32 i = 0; i < matrixDim; ++i)
        {
            for (buint32 j = 0; j < matrixDim; ++j)
            {
                matArr.pushBack(blib::core::json::JsonValue(worldMatrix.data[i][j]));
            }
        }
        doc.set(keyWorldMatrixDirty, blib::core::json::JsonValue(worldMatrixDirty));
        doc.set(keyIsActive, blib::core::json::JsonValue(isActive));

        if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::SaveStatus::WriteFailed,
                "TransformComponent: failed to write JSON to stream");
        }
        return blib::core::SaveStatus::None;
    }

    blib::core::LoadStatus TransformComponent::load(_In blib::core::IInputStream& is)
    {
        blib::core::json::JsonParser parser;
        blib::core::json::JsonValue doc;
        if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "TransformComponent: failed to parse JSON from stream");
        }
        if (__blib_unlikely(!doc.isObject()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "TransformComponent: root is not a JSON object");
        }

        // Сначала читаем всё в локальные переменные: при ошибке данных
        // состояние компонента остаётся нетронутым
        blib::math::Vector<float, 3> pos;
        blib::math::Quaternion<float> rot;
        blib::math::Vector<float, 3> scale;
        EntityID parentId;
        bool dirty;
        bool active;
        if (__blib_unlikely(!readVector3(doc, keyPosition, pos)) ||
            __blib_unlikely(!readQuaternion(doc, keyRotation, rot)) ||
            __blib_unlikely(!readVector3(doc, keyScale, scale)) ||
            __blib_unlikely(!readEntity(doc, keyParent, parentId)) ||
            __blib_unlikely(!readBool(doc, keyWorldMatrixDirty, dirty)) ||
            __blib_unlikely(!readBool(doc, keyIsActive, active)) ||
            __blib_unlikely(!doc.has(keyChildren)) ||
            __blib_unlikely(!doc.get(keyChildren).isArray()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "TransformComponent: missing or malformed scalar field");
        }

        // Список детей
        const blib::core::json::JsonValue& childrenArr = doc.get(keyChildren);
        std::vector<EntityID> loadedChildren;
        loadedChildren.reserve(childrenArr.size());
        for (buint32 i = 0; i < childrenArr.size(); ++i)
        {
            loadedChildren.push_back(static_cast<EntityID>(childrenArr[i].asBuint64()));
        }

        // Кеш мировой матрицы: ровно 16 чисел в порядке data[i][j]
        if (__blib_unlikely(!doc.has(keyWorldMatrix)))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "TransformComponent: missing world matrix field");
        }
        const blib::core::json::JsonValue& matArr = doc.get(keyWorldMatrix);
        if (__blib_unlikely(!matArr.isArray() || matArr.size() != matrixDim * matrixDim))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "TransformComponent: world matrix must be an array of %u numbers",
                static_cast<unsigned int>(matrixDim * matrixDim));
        }
        blib::math::Matrix<float, 4, 4> loadedMatrix;
        loadedMatrix.loadIdentity();
        buint32 matIndex = 0;
        for (buint32 i = 0; i < matrixDim; ++i)
        {
            for (buint32 j = 0; j < matrixDim; ++j)
            {
                loadedMatrix.data[i][j] = matArr[matIndex].asBfloat();
                ++matIndex;
            }
        }

        // Все поля валидны — применить состояние.
        // ownerScene/ownerId не восстанавливаются (контекст)
        localPosition = pos;
        localRotation = rot;
        localScale = scale;
        parent = parentId;
        children = loadedChildren;
        worldMatrix = loadedMatrix;
        worldMatrixDirty = dirty;
        isActive = active;

        return blib::core::LoadStatus::None;
    }

    bool TransformComponent::strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const
    {
        // Защита от циклов: пара уже сравнивается — считаем равной
        if (!session.enter(this, &other))
        {
            return true;
        }

        const TransformComponent& o = static_cast<const TransformComponent&>(other);

        // Базовые поля IComponent. ownerId сравнивается: строгая модель —
        // standalone-копия (invalidEntity) не равна компоненту в сцене
        if (getOwnerId() != o.getOwnerId() || isActive != o.isActive)
        {
            return false;
        }

        // Локальный TRS (бит-в-бит; NaN не сериализуем и не сравним)
        if (localPosition != o.localPosition || localScale != o.localScale)
        {
            return false;
        }
        if (localRotation.w != o.localRotation.w ||
            localRotation.x != o.localRotation.x ||
            localRotation.y != o.localRotation.y ||
            localRotation.z != o.localRotation.z)
        {
            return false;
        }

        // Иерархия
        if (parent != o.parent || children != o.children)
        {
            return false;
        }

        // Кеш мировой матрицы (сериализуется — сравнивается)
        if (worldMatrixDirty != o.worldMatrixDirty)
        {
            return false;
        }
        for (buint32 i = 0; i < matrixDim; ++i)
        {
            for (buint32 j = 0; j < matrixDim; ++j)
            {
                if (worldMatrix.data[i][j] != o.worldMatrix.data[i][j])
                {
                    return false;
                }
            }
        }

        // Контекст: ownerScene не сериализуется — сравнение по
        // null-состоянию (компонент в сцене ≠ standalone-копии)
        if ((ownerScene == nullptr) != (o.ownerScene == nullptr))
        {
            return false;
        }

        return true;
    }

    bool TransformComponent::verify() const
    {
        // Round-trip без RTTI: save -> свежий standalone-объект ->
        // load -> strongCompare (см. blib::core::verifyRoundTrip)
        return blib::core::verifyRoundTrip(*this);
    }

    void TransformComponent::onLoaded(_In Scene& scene)
    {
        // Вторая фаза Scene::load: компонент создавался default-ctor'ом
        // (ownerScene == nullptr) — привязать к сцене, в которую он
        // загружен (нужно для иерархии: мировая матрица тянет родителя
        // через scene.tryGetComponent)
        ownerScene = &scene;
    }

    // ========== Рефлексия (Inspector/эдитор) ==========
    //
    // Статические поля-дескрипторы: геттеры/сеттеры — лямбды без
    // захвата через публичные API компонента (рефлексия не лезет в
    // приватные члены). Дескриптор и поля живут всё время процесса
    // (как componentTypeName-литерал); сцена хранит указатель на
    // дескриптор (см. Scene::registerComponentType).

    namespace
    {
        constexpr const char* reflectionFieldPosition = "position";
        constexpr const char* reflectionFieldScale = "scale";

        const FunctionField s_positionField(
            reflectionFieldPosition, FieldValue::Kind::Vector3,
            [](_In const IComponent& component, _Out FieldValue& outValue)
            {
                outValue = FieldValue::fromVector3(
                    static_cast<const TransformComponent&>(component).getLocalPosition());
            },
            [](_In IComponent& component, _In const FieldValue& value)
            {
                static_cast<TransformComponent&>(component).setLocalPosition(value.vector3Value);
            });

        const FunctionField s_scaleField(
            reflectionFieldScale, FieldValue::Kind::Vector3,
            [](_In const IComponent& component, _Out FieldValue& outValue)
            {
                outValue = FieldValue::fromVector3(
                    static_cast<const TransformComponent&>(component).getLocalScale());
            },
            [](_In IComponent& component, _In const FieldValue& value)
            {
                static_cast<TransformComponent&>(component).setLocalScale(value.vector3Value);
            });

        const IComponentField* const s_transformFields[] = {
            &s_positionField,
            &s_scaleField
        };

        constexpr buint32 s_transformFieldCount =
            static_cast<buint32>(sizeof(s_transformFields) / sizeof(s_transformFields[0]));

        const ComponentTypeDescriptor s_transformReflection(
            TransformComponent::componentTypeName, s_transformFields, s_transformFieldCount);
    }

    const ComponentTypeDescriptor& TransformComponent::componentReflection()
    {
        return s_transformReflection;
    }

} // namespace beng
