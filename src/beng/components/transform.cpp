#include <beng/components/transform.h>
#include <beng/core/scene.h>
#include <blib/core/console/console.h>

namespace beng
{
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

        // Извлечь позицию из мировой матрицы (4-я колонка)
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
        // Создать локальную матрицу TRS (Translation-Rotation-Scale)

        // TODO: Полноценное вычисление TRS матрицы
        // Пока упрощённо — только позиция
        worldMatrix.loadIdentity();
        worldMatrix.data[3][0] = localPosition.x;
        worldMatrix.data[3][1] = localPosition.y;
        worldMatrix.data[3][2] = localPosition.z;

        // Если есть родитель — умножить на его мировую матрицу.
        // getWorldMatrix родителя рекурсивно пересчитает его самого,
        // если он dirty — порядок итерации по dense не влияет на результат.
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

} // namespace beng
