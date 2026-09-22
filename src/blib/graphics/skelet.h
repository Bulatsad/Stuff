#pragma once

#include <vector>
#include <string>

#include <assimp/scene.h>

#include <blib/config.h>
#include <blib/core/isaveloadable.h>
#include <blib/core/json/json.h>
#include <blib/graphics/bone.h>
#include <blib/graphics/animationclip.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Skelet : public blib::core::ISaveLoadable
        {
        public:
            blib::graphics::Bone* root = nullptr;

            bool loadFromAssimp(const aiMesh* paimesh);
            bool loadFromAssimp(const aiScene* paiscene);

            blib::graphics::Bone* find(const std::string& name);
            const blib::graphics::Bone* find(const std::string& name) const;
            size_t findBoneIndex(const std::string& name) const;

            // Позиция кости по имени в model-space (трансляция
            // globalTransform кости — обновляется при applyClip) —
            // привязка эффектов (blob-тени, точки крепления) к
            // анимированной позе. ВАЖНО: это НЕ трансляция
            // finalMatrices (там global * inverse-bind — «дельта»
            // от bind-позы). Требует computeBindPose/applyClip.
            // Возвращает false, если кость с таким именем не найдена
            bool getBonePosition(_In const std::string& name, _Out blib::graphics::Vector3f& outPosition) const;

            // Полное совпадение скелетов: число костей, имена, иерархия
            // (имя родителя) и inverse bind матрицы (offsetMatrix) с
            // допуском. Требуется для подмены мешей (skin) у текущего
            // скелета: скиннинг использует его offset-матрицы, поэтому
            // при другой bind-позе меш деформируется неверно
            bool isCompatibleWith(_In const blib::graphics::Skelet& other) const;

            // Перенос bind-позы скина: копирует offsetMatrix (inverse
            // bind) из другого скелета для всех одноимённых костей.
            // Используется force-подменой мешей, когда скелеты не
            // совпадают: меш чужого рига после этого рендерится как
            // нативно скиннутый к ТЕКУЩЕМУ скелету (пропорции следуют
            // текущему ригу, швы на суставах не расходятся при
            // анимации). Кости без пары остаются как есть
            void adoptOffsetMatricesFrom(_In const blib::graphics::Skelet& other);

            // Есть ли узел с таким именем среди костей или элементов их
            // FBX-цепочек (валидация каналов внешних анимаций)
            bool hasNodeName(_In const std::string& nodeName) const;

            bool makeBoneTree(const aiNode* pbone);
            bool loadDefaultPoseFromArmature(const aiNode* pbone);

            // Вычисляет bind-позу: прогоняет globalTransform по иерархии
            // от root и заполняет finalMatrices. Обязателен после загрузки
            // до первой отрисовки: без него finalMatrices нулевые, а
            // globalTransform костей не учитывают иерархию (скелет,
            // рисуемый линиями, будет «разобран»)
            void computeBindPose();

            void applyClip(const blib::graphics::AnimationClip& clip, double timeTicks);

            // Построить привязку каналов клипа к костям скелета по
            // сцене файла анимации: для каждой анимируемой кости —
            // цепочка узлов с bind-трансформами и индексами каналов.
            // Нужна, когда FBX-декомпозиция файла анимации отличается
            // от файла модели (каналы лежат на других узлах)
            void bindClipToSkeleton(_In blib::graphics::AnimationClip& clip, _In const aiScene* animationScene) const;

            std::vector<blib::graphics::Bone>& getBoneStorage();
            const std::vector<blib::graphics::Bone>& getBoneStorage() const;
            const std::vector<blib::graphics::TransformMatrix>& getFinalMatrices() const;

            // ========== ISaveLoadable: сериализация и сравнение ==========
            //
            // Формат toJson: {bones[{...bone.toJson(), parentIndex}],
            // rootIndex}. Иерархия (parent/childs IHierarchal) НЕ пишется
            // внутрь костей — восстанавливается fromJson по parentIndex
            // (setParent/addChild) + computeBindPose() пересчитывает
            // finalMatrices. Bone::node (aiNode*) — контекст Assimp,
            // после JSON-восстановления nullptr (рантайм на нём не
            // зависит). fromJson валидирует всё до применения.
            // Не прятать 1-аргументную точку входа строгого сравнения.

            using blib::core::IStrongComparable::strongCompare;

            blib::core::json::JsonValue toJson() const;
            blib::core::LoadStatus fromJson(_In const blib::core::json::JsonValue& json);
            blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;
            blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;
            bool strongCompare(_In const blib::core::IStrongComparable& other,
                _In blib::core::CompareSession& session) const __blib_override;
            bool verify() const __blib_override;

        private:
            std::vector<blib::graphics::Bone> boneStorage;
            std::vector<blib::graphics::TransformMatrix> finalMatrices;

            bool finishFromArmature(const aiNode* armature, const aiNode* sceneRoot);
            void loadDefaultPoseFromNodes(const aiNode* sceneRoot);
            void updateTransforms(blib::graphics::Bone* pbone);

            // Первая настоящая кость в поддереве узла (обход в глубину);
            // nullptr, если костей нет
            blib::graphics::Bone* findFirstBoneInSubtree(const aiNode* node);

            // Количество настоящих костей в поддереве узла
            size_t countBonesInSubtree(const aiNode* node) const;
        };
    }
}
