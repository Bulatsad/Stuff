#include <beng/client/systems/blobShadowSystem.h>

#include <beng/client/components/blobShadowComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/components/transform.h>
#include <beng/core/componentPool.h>
#include <beng/core/scene.h>

#include <blib/core/console/console.h>
#include <blib/core/math/quaternion.h>
#include <blib/graphics/bone.h>
#include <blib/graphics/skelet.h>
#include <blib/graphics/skinmodel.h>

#include <cctype>

namespace beng
{
    namespace
    {
        // Регистронезависимая проверка вхождения подстроки
        bool containsIgnoreCase(const std::string& haystack, const std::string& needle)
        {
            if (needle.empty() || haystack.size() < needle.size())
            {
                return false;
            }

            for (size_t i = 0; i + needle.size() <= haystack.size(); ++i)
            {
                bool matches = true;
                for (size_t j = 0; j < needle.size(); ++j)
                {
                    const char h = static_cast<char>(std::tolower(static_cast<unsigned char>(haystack[i + j])));
                    const char n = static_cast<char>(std::tolower(static_cast<unsigned char>(needle[j])));
                    if (h != n)
                    {
                        matches = false;
                        break;
                    }
                }
                if (matches)
                {
                    return true;
                }
            }
            return false;
        }

        // Разрешение позиции «тазовой» кости: точные кандидаты, затем
        // любая кость с "hips"/"pelvis" в имени (регистронезависимо) —
        // закрывает любые риги без хардкода конкретных имён
        bool resolveHipsBonePosition(
            _In const blib::graphics::Skelet& skelet,
            _In const std::string& preferredName,
            _Out blib::graphics::Vector3f& outPosition)
        {
            if (skelet.getBonePosition(preferredName, outPosition))
            {
                return true;
            }
            if (preferredName != "Hips" && skelet.getBonePosition("Hips", outPosition))
            {
                return true;
            }
            if (preferredName != "mixamorig:Hips" && skelet.getBonePosition("mixamorig:Hips", outPosition))
            {
                return true;
            }

            // Fallback по подстроке: первая кость с "hips"/"pelvis"
            const std::vector<blib::graphics::Bone>& bones = skelet.getBoneStorage();
            for (const blib::graphics::Bone& bone : bones)
            {
                if (containsIgnoreCase(bone.name, "hips") || containsIgnoreCase(bone.name, "pelvis"))
                {
                    return skelet.getBonePosition(bone.name, outPosition);
                }
            }
            return false;
        }
    }

    BlobShadowSystem::BlobShadowSystem()
    {
    }

    void BlobShadowSystem::update(_In beng::Scene& scene, float deltaTime)
    {
        beng::ComponentPool<BlobShadowComponent>* shadowPool = scene.tryGetComponentPool<BlobShadowComponent>();
        if (shadowPool == nullptr)
        {
            return;
        }

        for (auto it = shadowPool->begin(); it != shadowPool->end(); ++it)
        {
            BlobShadowComponent& shadowComp = *it;

            // Цель — скелетная модель (root-motion живёт в позе костей)
            SkinnedMeshComponent* targetMesh =
                scene.tryGetComponent<SkinnedMeshComponent>(shadowComp.getTargetEntity());
            if (targetMesh == nullptr || targetMesh->getModel() == nullptr)
            {
                continue;
            }

            // Позиция кости в model-space (анимация уже продвинута
            // AnimationSystem'ом — приоритет выше). Кандидаты имён:
            // заданное в компоненте, «Hips», «mixamorig:Hips», затем
            // любая кость с "hips"/"pelvis" в имени
            blib::graphics::Vector3f bonePosition;
            if (!resolveHipsBonePosition(targetMesh->getModel()->getSkelet(), shadowComp.getBoneName(), bonePosition))
            {
                if (!shadowComp.isBoneMissingLogged())
                {
                    __blib_log_warning("blob shadow: hips/pelvis bone not found (tried '%s', Hips, mixamorig:Hips, substring fallback) on target entity %llu",
                        shadowComp.getBoneName().c_str(),
                        static_cast<unsigned long long>(shadowComp.getTargetEntity()));
                    shadowComp.setBoneMissingLogged();
                }
                continue;
            }

            // Мировая позиция кости. ВАЖНО: Matrix::operator* в blib-core
            // вычисляет ТРАНСПОНИРОВАННОЕ произведение (lhs^T * rhs) —
            // трансформировать точки через него нельзя (теряется
            // трансляция). Используем проверенные API TransformComponent:
            // world = position + rotate(scale * bone, rotation)
            beng::TransformComponent* targetTransform =
                scene.tryGetComponent<beng::TransformComponent>(shadowComp.getTargetEntity());
            if (targetTransform != nullptr)
            {
                const blib::math::Vector<float, 3> worldPosition = targetTransform->getWorldPosition();
                const blib::math::Quaternion<float> worldRotation = targetTransform->getWorldRotation();
                const blib::math::Vector<float, 3> worldScale = targetTransform->getWorldScale();

                const blib::math::Vector<float, 3> scaledBone(
                    bonePosition.x * worldScale.x,
                    bonePosition.y * worldScale.y,
                    bonePosition.z * worldScale.z);

                bonePosition = worldPosition + blib::math::rotate(scaledBone, worldRotation);
            }

            // Blob-тень лежит СТРОГО под целью (с подъёмом над землёй —
            // защита от z-fighting): это «клякса» привязки к земле,
            // а не проекция от источника света
            const blib::math::Vector<float, 3> shadowPosition(
                bonePosition.x, shadowComp.getGroundOffset(), bonePosition.z);

            // Переместить сущность-тень (dirty-флаг → RenderSystem
            // пересчитает world-матрицу при отрисовке)
            beng::TransformComponent* shadowTransform =
                scene.tryGetComponent<beng::TransformComponent>(it.getEntityId());
            if (shadowTransform != nullptr)
            {
                shadowTransform->setLocalPosition(shadowPosition);
            }
        }
    }

} // namespace beng
