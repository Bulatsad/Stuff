#pragma once

#include <vector>
#include <string>

#include <assimp/scene.h>

#include <blib/config.h>
#include <blib/core/isaveloadable.h>
#include <blib/core/json/json.h>
#include <blib/graphics/transformMatrix.h>

#include <blib/graphics/hierarchal.h>

namespace blib
{
    namespace graphics
    {
        // Элемент цепочки трансформа кости: узел сцены и его bind-матрица.
        // FBX раскладывает трансформ кости по нескольким узлам
        // ("<имя>_$AssimpFbx$_Translation" и т.п.), а Assimp кладёт
        // анимационные каналы на эти узлы — для воспроизведения анимации
        // каждый элемент цепочки подменяется сэмплом канала с именем узла
        struct BoneChainElement
        {
            std::string nodeName;
            blib::graphics::TransformMatrix bindTransform;
        };

        class __blib_graphics_api Bone : public blib::graphics::IHierarchal, public blib::core::ISaveLoadable
        {
        public:

            bool loadFromAssimp(const aiBone* pbone);

            std::string name;
            // Узел сцены, соответствующий кости (aiBone::mNode из
            // Assimp, заполняется проходом PopulateArmatureData).
            // Используется для точного вычисления bind-позы по цепочке
            // предков (FBX раскладывает трансформ кости по нескольким
            // узлам-декомпозициям). nullptr — если узел не известен
            const aiNode* node = nullptr;
            // Цепочка узлов, составляющих локальный трансформ кости
            // (порядок: от внешнего к внутреннему; последний элемент —
            // собственный узел кости). Пуста, если узел не известен
            std::vector<BoneChainElement> chain;
            std::vector<std::pair<size_t/*VertexId*/, float /*Weight*/> >weights;
            blib::graphics::TransformMatrix offsetMatrix;
            blib::graphics::TransformMatrix localTransform;
            blib::graphics::TransformMatrix globalTransform;

            // ========== ISaveLoadable: сериализация и сравнение ==========
            //
            // Сериализуется: name, chain (nodeName + bindTransform),
            // weights, offsetMatrix, localTransform, globalTransform.
            // НЕ сериализуется: node (aiNode* — контекст Assimp-загрузки,
            // в JSON-восстановленной кости = nullptr; рантайм на нём не
            // зависит — applyClip/computeBindPose работают на chain и
            // матрицах) и parent/childs (иерархия восстанавливается на
            // уровне Skelet по parentIndex).
            // Не прятать 1-аргументную точку входа строгого сравнения.

            using blib::core::IStrongComparable::strongCompare;

            blib::core::json::JsonValue toJson() const;
            blib::core::LoadStatus fromJson(_In const blib::core::json::JsonValue& json);
            blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;
            blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;
            bool strongCompare(_In const blib::core::IStrongComparable& other,
                _In blib::core::CompareSession& session) const __blib_override;
            bool verify() const __blib_override;
        };
    }
}

