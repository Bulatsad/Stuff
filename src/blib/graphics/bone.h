#pragma once

#include <vector>
#include <string>

#include <assimp/scene.h>

#include <blib/config.h>
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

        class __blib_graphics_api Bone : public blib::graphics::IHierarchal
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
        };
    }
}
