#pragma once

#include <vector>
#include <string>

#include <assimp/scene.h>

#include <blib/config.h>
#include <blib/graphics/bone.h>
#include <blib/graphics/animationclip.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Skelet 
        {
        public:
            blib::graphics::Bone* root = nullptr;

            bool loadFromAssimp(const aiMesh* paimesh);
            bool loadFromAssimp(const aiScene* paiscene);

            blib::graphics::Bone* find(const std::string& name);
            const blib::graphics::Bone* find(const std::string& name) const;
            size_t findBoneIndex(const std::string& name) const;

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
