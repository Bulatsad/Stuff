#include <blib/test/src/test.h>

#include <blib/graphics/skelet.h>
#include <blib/graphics/skinmesh.h>
#include <blib/graphics/skinmodel.h>
#include <blib/graphics/animationclip.h>
#include <blib/core/math/quaternion.h>
#include <blib/core/memoryStream.h>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <tuple>

namespace
{
    // Имена узлов/костей фикстурных сцен
    constexpr const char* sceneRootNodeName = "SceneRoot";
    constexpr const char* armatureNodeName = "Armature";
    constexpr const char* rootBoneName = "Root";
    constexpr const char* headBoneName = "Head";
    constexpr const char* neckBoneName = "Neck";
    constexpr const char* ribbonBoneName = "Ribbon";
    constexpr const char* ribbon2BoneName = "Ribbon2";
    constexpr const char* orphanBoneName = "Orphan";

    // Слоты весов на вершину (шейдер) — см. skinmesh.cpp
    constexpr size_t maxWeightsPerVertex = 4;

    // aiMatrix4x4 с трансляцией: row-major хранилище, трансляция —
    // в последней колонке (a4, b4, c4); transformFromAssimp кладёт её
    // в data[3][0..2] — колонку трансляции TransformMatrix движка
    // (column-major, см. CORE.md, «Конвенция матриц»)
    aiMatrix4x4 makeTranslationMatrix(float x, float y, float z)
    {
        aiMatrix4x4 m;
        m.a4 = x;
        m.b4 = y;
        m.c4 = z;
        return m;
    }

    // Совпадает ли offsetMatrix кости с трансляцией (x, y, z).
    // aiMatrix4x4 — row-major (трансляция в a4/b4/c4), хранилище
    // Matrix — data[колонка][строка] (column-major): трансляция
    // попадает в data[3][0..2]
    bool hasTranslationOffset(_In const blib::graphics::Bone& bone, float x, float y, float z)
    {
        constexpr float epsilon = 1e-5f;
        return std::abs(bone.offsetMatrix.data[3][0] - x) < epsilon &&
            std::abs(bone.offsetMatrix.data[3][1] - y) < epsilon &&
            std::abs(bone.offsetMatrix.data[3][2] - z) < epsilon;
    }

    // Матрица — единичная (с допуском)? Используется для проверки
    // инварианта bind-позы: финальная костная матрица (global *
    // inverse-bind) обязана быть identity, когда скелет в bind-позе
    bool isIdentityMatrix(_In const blib::graphics::TransformMatrix& m, float epsilon)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            for (size_t j = 0; j < 4; ++j)
            {
                const float expected = (i == j) ? 1.0f : 0.0f;
                if (std::abs(m.data[i][j] - expected) > epsilon)
                {
                    return false;
                }
            }
        }
        return true;
    }

    // aiMatrix4x4 поворота на angle градусов вокруг Z.
    // ВАЖНО: 16-аргументный конструктор aiMatrix4x4 принимает
    // аргументы ПОСТРОЧНО (row0 = a1..a4, row1 = b1..b4, ...) —
    // память row-major.
    aiMatrix4x4 makeRotationZMatrix(float angleDegrees)
    {
        const float rad = angleDegrees * 3.14159265359f / 180.0f;
        const float c = std::cos(rad);
        const float s = std::sin(rad);
        // Rz: r0=(c,-s,0,0), r1=(s,c,0,0), r2=(0,0,1,0), r3=(0,0,0,1)
        return aiMatrix4x4(
            c, -s, 0.0f, 0.0f,
            s, c, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    // aiMatrix4x4 чистой трансляции (x, y, z), аргументы — построчно
    aiMatrix4x4 makeTranslationMatrixFull(float x, float y, float z)
    {
        // T: r0=(1,0,0,x), r1=(0,1,0,y), r2=(0,0,1,z), r3=(0,0,0,1)
        return aiMatrix4x4(
            1.0f, 0.0f, 0.0f, x,
            0.0f, 1.0f, 0.0f, y,
            0.0f, 0.0f, 1.0f, z,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    /**
     * Хранилище объекта Assimp в сырой памяти.
     *
     * ГРАБЛИ: Assimp-структуры владеют своими массивами (delete[] в
     * ~aiBone/~aiMesh, delete рекурсивно в ~aiNode/~aiScene), а данные
     * фикстур живут в std::vector — вызов деструктора освободил бы
     * чужую память. Поэтому объект конструируется placement new'ом
     * (проектное правило: placement new разрешён, выделяющий new[]
     * запрещён), а его деструктор НЕ вызывается никогда — сырая
     * память не требует очистки.
     */
    template <typename T>
    struct AssimpFixtureSlot
    {
        alignas(T) buint8 storage[sizeof(T)];

        T* get()
        {
            return reinterpret_cast<T*>(this->storage);
        }

        const T* get() const
        {
            return reinterpret_cast<const T*>(this->storage);
        }

        // Конструирование вызывается после стабилизации размера
        // вектора слотов (реаллокация вектора перемещает сырую память)
        template <typename... Args>
        void construct(Args&&... args)
        {
            new (this->storage) T(std::forward<Args>(args)...);
        }
    };

    /**
     * Фикстурная Assimp-сцена со скелетом: сцен-рут → armature →
     * кости. Скелет строится настоящим загрузчиком
     * Skelet::loadFromAssimp — armature-поля aiBone (mArmature/mNode)
     * заполнены, как после aiProcess_PopulateArmatureData.
     */
    struct SkeletonSceneFixture
    {
        // Слоты узлов: [0] = сцен-рут, [1] = armature, [2+i] = i-я кость
        std::vector<AssimpFixtureSlot<aiNode>> nodes;
        std::vector<std::vector<aiNode*>> childLists;
        std::vector<AssimpFixtureSlot<aiBone>> bones;
        std::vector<aiBone*> bonePtrs;
        AssimpFixtureSlot<aiMesh> mesh;
        aiMesh* meshPtrs[1];
        AssimpFixtureSlot<aiScene> scene;

        // boneSpecs: пары (имя кости, имя кости-родителя; пустая
        // строка — родитель armature). Родитель должен идти раньше.
        // offsetMatrices: опциональные inverse-bind матрицы костей по
        // имени (по умолчанию identity).
        // intermediates: (имя кости, имя узла, трансформ) — узел
        // вставляется МЕЖДУ костью и её родителем, как узел
        // FBX-декомпозиции ("<имя>_$AssimpFbx$_Translation" и т.п.).
        // Несколько записей для одной кости образуют цепочку в порядке
        // перечисления: первая запись — ближе к родителю (внешний узел).
        bool build(
            _In const std::vector<std::pair<std::string, std::string>>& boneSpecs,
            _In const std::vector<std::pair<std::string, aiMatrix4x4>>& offsetMatrices = {},
            _In const std::vector<std::tuple<std::string, std::string, aiMatrix4x4>>& intermediates = {})
        {
            const size_t boneCount = boneSpecs.size();
            const size_t intermediateCount = intermediates.size();

            // Узлы: [0] = сцен-рут, [1] = armature,
            // [2..2+I-1] = промежуточные, далее — кости
            const size_t boneNodeBase = 2 + intermediateCount;

            this->nodes.resize(2 + intermediateCount + boneCount);
            this->childLists.resize(this->nodes.size());
            this->bones.resize(boneCount);
            this->bonePtrs.resize(boneCount);

            for (AssimpFixtureSlot<aiNode>& node : this->nodes)
            {
                node.construct();
            }
            for (AssimpFixtureSlot<aiBone>& bone : this->bones)
            {
                bone.construct();
            }
            this->mesh.construct();
            this->scene.construct();

            this->nodes[0].get()->mName = sceneRootNodeName;
            this->nodes[1].get()->mName = armatureNodeName;

            for (size_t i = 0; i < intermediateCount; ++i)
            {
                this->nodes[2 + i].get()->mName = std::get<1>(intermediates[i]);
            }

            for (size_t i = 0; i < boneCount; ++i)
            {
                this->nodes[boneNodeBase + i].get()->mName = boneSpecs[i].first;
            }

            // Поиск индекса узла по имени (все узлы уже поименованы)
            const auto findNodeIndex = [this](const std::string& name) -> size_t
            {
                for (size_t i = 0; i < this->nodes.size(); ++i)
                {
                    if (std::string(this->nodes[i].get()->mName.C_Str()) == name)
                    {
                        return i;
                    }
                }
                return this->nodes.size();
            };

            for (size_t i = 0; i < boneCount; ++i)
            {
                const size_t parentIndex = boneSpecs[i].second.empty() ? 1 : findNodeIndex(boneSpecs[i].second);
                if (parentIndex >= this->nodes.size())
                {
                    __blib_log_error("fixture: unknown bone parent '%s'", boneSpecs[i].second.c_str());
                    return false;
                }

                // Промежуточные узлы этой кости (в порядке перечисления:
                // внешний → внутренний)
                std::vector<size_t> chainIndices;
                for (size_t k = 0; k < intermediateCount; ++k)
                {
                    if (std::get<0>(intermediates[k]) == boneSpecs[i].first)
                    {
                        chainIndices.push_back(2 + k);
                    }
                }

                // Родитель указывает на внешний узел цепочки (или на кость)
                const aiNode* firstChild = chainIndices.empty()
                    ? this->nodes[boneNodeBase + i].get()
                    : this->nodes[chainIndices[0]].get();
                this->childLists[parentIndex].push_back(const_cast<aiNode*>(firstChild));

                // Цепочка промежуточных узлов → кость
                for (size_t k = 0; k < chainIndices.size(); ++k)
                {
                    const aiNode* nextChild = (k + 1 < chainIndices.size())
                        ? this->nodes[chainIndices[k + 1]].get()
                        : this->nodes[boneNodeBase + i].get();
                    this->childLists[chainIndices[k]].push_back(const_cast<aiNode*>(nextChild));
                }
            }

            // Сцен-рут → armature
            this->childLists[0].push_back(this->nodes[1].get());

            // Трансформы промежуточных узлов (bind-декомпозиция)
            for (size_t k = 0; k < intermediateCount; ++k)
            {
                this->nodes[2 + k].get()->mTransformation = std::get<2>(intermediates[k]);
            }

            for (size_t i = 0; i < this->nodes.size(); ++i)
            {
                aiNode* node = this->nodes[i].get();
                node->mChildren = this->childLists[i].empty() ? nullptr : this->childLists[i].data();
                node->mNumChildren = static_cast<unsigned int>(this->childLists[i].size());

                // Родительские ссылки: загрузчик скелета ходит по дереву
                // вверх (boneNode->mParent) при сборке цепочек трансформа
                for (aiNode* child : this->childLists[i])
                {
                    child->mParent = node;
                }
            }

            for (size_t i = 0; i < boneCount; ++i)
            {
                aiBone* bone = this->bones[i].get();
                bone->mName = boneSpecs[i].first;
                bone->mArmature = this->nodes[1].get();
                bone->mNode = this->nodes[boneNodeBase + i].get();
                this->bonePtrs[i] = bone;
            }

            // Опциональные inverse-bind матрицы костей
            for (const std::pair<std::string, aiMatrix4x4>& offset : offsetMatrices)
            {
                aiBone* bone = nullptr;
                for (size_t i = 0; i < boneCount; ++i)
                {
                    if (std::string(this->bones[i].get()->mName.C_Str()) == offset.first)
                    {
                        bone = this->bones[i].get();
                        break;
                    }
                }
                if (!bone)
                {
                    __blib_log_error("fixture: unknown bone '%s' for offset matrix", offset.first.c_str());
                    return false;
                }
                bone->mOffsetMatrix = offset.second;
            }

            aiMesh* skeletonMesh = this->mesh.get();
            skeletonMesh->mBones = this->bonePtrs.data();
            skeletonMesh->mNumBones = static_cast<unsigned int>(boneCount);

            this->meshPtrs[0] = skeletonMesh;
            this->scene.get()->mRootNode = this->nodes[0].get();
            this->scene.get()->mNumMeshes = 1;
            this->scene.get()->mMeshes = this->meshPtrs;

            return true;
        }
    };

    /**
     * Фикстурный скин-меш: вершины + кости с весами. Веса задаются
     * списком (имя кости, список пар вершина/вес). Вершины — нулевые
     * позиции (раскладке весов геометрия не важна).
     */
    struct SkinMeshFixture
    {
        std::vector<aiVector3D> vertexStorage;
        std::vector<AssimpFixtureSlot<aiBone>> bones;
        std::vector<aiBone*> bonePtrs;
        std::vector<std::vector<aiVertexWeight>> weightStorage;
        AssimpFixtureSlot<aiMesh> mesh;

        void build(
            _In size_t vertexCount,
            _In const std::vector<std::pair<std::string, std::vector<std::pair<size_t, float>>>>& bonesWithWeights)
        {
            this->vertexStorage.resize(vertexCount);
            this->bones.resize(bonesWithWeights.size());
            this->bonePtrs.resize(bonesWithWeights.size());
            this->weightStorage.resize(bonesWithWeights.size());

            for (AssimpFixtureSlot<aiBone>& bone : this->bones)
            {
                bone.construct();
            }
            this->mesh.construct();

            for (size_t b = 0; b < bonesWithWeights.size(); ++b)
            {
                aiBone* bone = this->bones[b].get();
                bone->mName = bonesWithWeights[b].first;

                std::vector<aiVertexWeight>& weights = this->weightStorage[b];
                weights.resize(bonesWithWeights[b].second.size());
                for (size_t w = 0; w < weights.size(); ++w)
                {
                    weights[w].mVertexId = static_cast<unsigned int>(bonesWithWeights[b].second[w].first);
                    weights[w].mWeight = bonesWithWeights[b].second[w].second;
                }
                bone->mWeights = weights.data();
                bone->mNumWeights = static_cast<unsigned int>(weights.size());

                this->bonePtrs[b] = bone;
            }

            aiMesh* skinMesh = this->mesh.get();
            skinMesh->mVertices = this->vertexStorage.data();
            skinMesh->mNumVertices = static_cast<unsigned int>(vertexCount);
            skinMesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
            skinMesh->mBones = this->bonePtrs.data();
            skinMesh->mNumBones = static_cast<unsigned int>(this->bonePtrs.size());
        }
    };
}

BLIB_TEST_CASE("skinmesh: force remaps unknown bone weights to existing ancestor")
{
    // Текущий скелет: Root → Head (кости-ленты отсутствуют).
    // Скелет-кандидат из файла скина: Root → Head → Ribbon
    SkeletonSceneFixture currentScene;
    BLIB_TEST_REQUIRE(currentScene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName } }));

    SkeletonSceneFixture candidateScene;
    BLIB_TEST_REQUIRE(candidateScene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName },
        { ribbonBoneName, headBoneName } }));

    blib::graphics::Skelet currentSkelet;
    BLIB_TEST_REQUIRE(currentSkelet.loadFromAssimp(currentScene.scene.get()));
    blib::graphics::Skelet candidateSkelet;
    BLIB_TEST_REQUIRE(candidateSkelet.loadFromAssimp(candidateScene.scene.get()));

    // Вершина 0 — только на отсутствующую ленту; вершина 1 — контрольная
    SkinMeshFixture skinMesh;
    skinMesh.build(2, {
        { ribbonBoneName, { {0, 1.0f} } },
        { headBoneName,   { {1, 1.0f} } } });

    blib::graphics::SkinMesh skin;
    BLIB_TEST_REQUIRE(skin.loadFromAssimpMesh(skinMesh.mesh.get(), currentSkelet, true, &candidateSkelet));

    const size_t headIndex = currentSkelet.findBoneIndex(headBoneName);
    BLIB_TEST_REQUIRE(headIndex < currentSkelet.getBoneStorage().size());

    // Вес ленты переехал на предка Head, сумма не изменилась
    BLIB_TEST_CHECK(skin.mesh.boneIds[0].data[0] == static_cast<int>(headIndex));
    BLIB_TEST_CHECK_CLOSE(skin.mesh.boneWeights[0].data[0], 1.0f, 0.0001f);

    // Контрольная вершина с известной костью не тронута
    BLIB_TEST_CHECK(skin.mesh.boneIds[1].data[0] == static_cast<int>(headIndex));
    BLIB_TEST_CHECK_CLOSE(skin.mesh.boneWeights[1].data[0], 1.0f, 0.0001f);
}

BLIB_TEST_CASE("skinmesh: force drops weights without ancestor and renormalizes")
{
    // Кость-сирота: в кандидате висит прямо под armature (не под Root),
    // поэтому в текущем скелете у неё нет ни её самой, ни предков
    SkeletonSceneFixture currentScene;
    BLIB_TEST_REQUIRE(currentScene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName } }));

    SkeletonSceneFixture candidateScene;
    BLIB_TEST_REQUIRE(candidateScene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName },
        { orphanBoneName, "" } }));

    blib::graphics::Skelet currentSkelet;
    BLIB_TEST_REQUIRE(currentSkelet.loadFromAssimp(currentScene.scene.get()));
    blib::graphics::Skelet candidateSkelet;
    BLIB_TEST_REQUIRE(candidateSkelet.loadFromAssimp(candidateScene.scene.get()));

    // Вершина 0 — только на сироту (все веса потеряны);
    // вершина 1 — сирота 0.5 + Head 0.5 (ренормализация к 1)
    SkinMeshFixture skinMesh;
    skinMesh.build(2, {
        { orphanBoneName, { {0, 1.0f}, {1, 0.5f} } },
        { headBoneName,   { {1, 0.5f} } } });

    blib::graphics::SkinMesh skin;
    BLIB_TEST_REQUIRE(skin.loadFromAssimpMesh(skinMesh.mesh.get(), currentSkelet, true, &candidateSkelet));

    const size_t headIndex = currentSkelet.findBoneIndex(headBoneName);
    BLIB_TEST_REQUIRE(headIndex < currentSkelet.getBoneStorage().size());

    // Вершина 0 осталась без весов (все кости неизвестны)
    for (size_t s = 0; s < maxWeightsPerVertex; ++s)
    {
        BLIB_TEST_CHECK(skin.mesh.boneWeights[0].data[s] == 0.0f);
    }

    // Вершина 1: вес Head ренормализован 0.5 → 1.0
    BLIB_TEST_CHECK(skin.mesh.boneIds[1].data[0] == static_cast<int>(headIndex));
    BLIB_TEST_CHECK_CLOSE(skin.mesh.boneWeights[1].data[0], 1.0f, 0.0001f);
}

BLIB_TEST_CASE("skinmesh: strict mode still rejects unknown bones")
{
    SkeletonSceneFixture currentScene;
    BLIB_TEST_REQUIRE(currentScene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName } }));

    blib::graphics::Skelet currentSkelet;
    BLIB_TEST_REQUIRE(currentSkelet.loadFromAssimp(currentScene.scene.get()));

    SkinMeshFixture skinMesh;
    skinMesh.build(1, {
        { ribbonBoneName, { {0, 1.0f} } } });

    blib::graphics::SkinMesh skin;
    BLIB_TEST_CHECK(!skin.loadFromAssimpMesh(skinMesh.mesh.get(), currentSkelet, false));
}

BLIB_TEST_CASE("skinmesh: remapped weights respect 4-slots-per-vertex limit")
{
    // Текущий: Root → Head → Neck. Кандидат добавляет две ленты
    // (Ribbon, Ribbon2) под Head — обе переносятся на Head
    SkeletonSceneFixture currentScene;
    BLIB_TEST_REQUIRE(currentScene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName },
        { neckBoneName, headBoneName } }));

    SkeletonSceneFixture candidateScene;
    BLIB_TEST_REQUIRE(candidateScene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName },
        { neckBoneName, headBoneName },
        { ribbonBoneName, headBoneName },
        { ribbon2BoneName, headBoneName } }));

    blib::graphics::Skelet currentSkelet;
    BLIB_TEST_REQUIRE(currentSkelet.loadFromAssimp(currentScene.scene.get()));
    blib::graphics::Skelet candidateSkelet;
    BLIB_TEST_REQUIRE(candidateSkelet.loadFromAssimp(candidateScene.scene.get()));

    // 5 костей на одну вершину: первые 4 занимают слоты, 5-я (Ribbon2)
    // отбрасывается по лимиту слотов (с warning) — сумма весов = 0.8
    SkinMeshFixture skinMesh;
    skinMesh.build(1, {
        { rootBoneName,   { {0, 0.2f} } },
        { headBoneName,   { {0, 0.2f} } },
        { neckBoneName,   { {0, 0.2f} } },
        { ribbonBoneName, { {0, 0.2f} } },
        { ribbon2BoneName,{ {0, 0.2f} } } });

    blib::graphics::SkinMesh skin;
    BLIB_TEST_REQUIRE(skin.loadFromAssimpMesh(skinMesh.mesh.get(), currentSkelet, true, &candidateSkelet));

    const size_t headIndex = currentSkelet.findBoneIndex(headBoneName);
    BLIB_TEST_REQUIRE(headIndex < currentSkelet.getBoneStorage().size());

    // Слот 3 занимает Ribbon → remap на Head
    BLIB_TEST_CHECK(skin.mesh.boneIds[0].data[3] == static_cast<int>(headIndex));

    float weightSum = 0.0f;
    for (size_t s = 0; s < maxWeightsPerVertex; ++s)
    {
        weightSum += skin.mesh.boneWeights[0].data[s];
    }
    BLIB_TEST_CHECK_CLOSE(weightSum, 0.8f, 0.0001f);
}

BLIB_TEST_CASE("skelet: adoptOffsetMatricesFrom copies offsets of common bones")
{
    // Текущий скелет: Root → Head с identity-offset-ами
    SkeletonSceneFixture currentScene;
    BLIB_TEST_REQUIRE(currentScene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName } }));

    // Кандидат: те же кости с другими inverse-bind + лишняя Ribbon
    SkeletonSceneFixture candidateScene;
    BLIB_TEST_REQUIRE(candidateScene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName },
        { ribbonBoneName, headBoneName } },
        {
            { rootBoneName, makeTranslationMatrix(1.0f, 0.0f, 0.0f) },
            { headBoneName, makeTranslationMatrix(0.0f, 5.0f, 0.0f) },
            { ribbonBoneName, makeTranslationMatrix(9.0f, 9.0f, 9.0f) } }));

    blib::graphics::Skelet currentSkelet;
    BLIB_TEST_REQUIRE(currentSkelet.loadFromAssimp(currentScene.scene.get()));
    blib::graphics::Skelet candidateSkelet;
    BLIB_TEST_REQUIRE(candidateSkelet.loadFromAssimp(candidateScene.scene.get()));

    // До переноса — identity
    BLIB_TEST_REQUIRE(hasTranslationOffset(currentSkelet.find(rootBoneName)[0], 0.0f, 0.0f, 0.0f));
    BLIB_TEST_REQUIRE(hasTranslationOffset(currentSkelet.find(headBoneName)[0], 0.0f, 0.0f, 0.0f));

    currentSkelet.adoptOffsetMatricesFrom(candidateSkelet);

    // Общие кости получили offset-ы кандидата
    BLIB_TEST_CHECK(hasTranslationOffset(currentSkelet.find(rootBoneName)[0], 1.0f, 0.0f, 0.0f));
    BLIB_TEST_CHECK(hasTranslationOffset(currentSkelet.find(headBoneName)[0], 0.0f, 5.0f, 0.0f));

    // Лишняя кость кандидата в текущий скелет не попала
    BLIB_TEST_CHECK(currentSkelet.getBoneStorage().size() == 2);
}

BLIB_TEST_CASE("skelet: adoptOffsetMatricesFrom is idempotent and does not touch source")
{
    SkeletonSceneFixture currentScene;
    BLIB_TEST_REQUIRE(currentScene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName } }));

    SkeletonSceneFixture candidateScene;
    BLIB_TEST_REQUIRE(candidateScene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName } },
        {
            { headBoneName, makeTranslationMatrix(3.0f, 0.0f, 0.0f) } }));

    blib::graphics::Skelet currentSkelet;
    BLIB_TEST_REQUIRE(currentSkelet.loadFromAssimp(currentScene.scene.get()));
    blib::graphics::Skelet candidateSkelet;
    BLIB_TEST_REQUIRE(candidateSkelet.loadFromAssimp(candidateScene.scene.get()));

    currentSkelet.adoptOffsetMatricesFrom(candidateSkelet);
    currentSkelet.adoptOffsetMatricesFrom(candidateSkelet);

    // Повторный вызов не меняет результат
    BLIB_TEST_CHECK(hasTranslationOffset(currentSkelet.find(headBoneName)[0], 3.0f, 0.0f, 0.0f));

    // Источник не тронут
    BLIB_TEST_CHECK(hasTranslationOffset(candidateSkelet.find(headBoneName)[0], 3.0f, 0.0f, 0.0f));
    BLIB_TEST_CHECK(hasTranslationOffset(candidateSkelet.find(rootBoneName)[0], 0.0f, 0.0f, 0.0f));
}

// ============================================================
// Сериализация (ISaveLoadable): round-trip и verify
// ============================================================

BLIB_TEST_CASE("serialization: image roundtrip and verify")
{
    // 3x2 RGBA с отличимыми пикселями
    blib::graphics::Image image(3, 2, nullptr);
    std::vector<blib::graphics::Color>& pixels = image.data();
    for (size_t i = 0; i < pixels.size(); ++i)
    {
        const buint8 v = static_cast<buint8>(i * 11);
        pixels[i] = blib::graphics::Color(v, static_cast<buint8>(v + 1),
            static_cast<buint8>(v + 2), static_cast<buint8>(v + 3));
    }

    blib::core::MemoryStream mem;
    BLIB_TEST_REQUIRE(image.save(mem) == blib::core::SaveStatus::None);

    blib::graphics::Image loaded;
    mem.seek(0, blib::core::SeekOrigin::Begin);
    BLIB_TEST_REQUIRE(loaded.load(mem) == blib::core::LoadStatus::None);

    BLIB_TEST_CHECK(image.strongCompare(loaded));
    BLIB_TEST_CHECK(image.verify());
}

BLIB_TEST_CASE("serialization: image rejects malformed json without state change")
{
    blib::graphics::Image image(2, 2, nullptr);

    // Длина pixels не совпадает с width * height * 4
    blib::core::MemoryStream mem;
    const char bad[] = "{\"width\":2,\"height\":2,\"pixels\":[1,2,3]}";
    mem.write(bad, sizeof(bad) - 1);
    mem.seek(0, blib::core::SeekOrigin::Begin);

    BLIB_TEST_CHECK(image.load(mem) == blib::core::LoadStatus::InvalidData);

    // Состояние не изменилось
    BLIB_TEST_CHECK(image.width == 2 && image.height == 2);
    BLIB_TEST_CHECK(image.data().size() == 4);
}

BLIB_TEST_CASE("serialization: mesh roundtrip and verify")
{
    // Меш с геометрией, гранями, цветами, весами и материалом
    // (включая битмап диффуза)
    blib::graphics::Mesh mesh;
    mesh.primitiveType = blib::graphics::PrimitiveType::Triangle;
    mesh.ngonencoding = true;
    mesh.vertices = {
        blib::graphics::Vector3f(0.0f, 0.0f, 0.0f),
        blib::graphics::Vector3f(1.0f, 0.0f, 0.0f),
        blib::graphics::Vector3f(0.0f, 1.0f, 0.0f) };
    mesh.normals = {
        blib::graphics::Vector3f(0.0f, 0.0f, 1.0f),
        blib::graphics::Vector3f(0.0f, 0.0f, 1.0f),
        blib::graphics::Vector3f(0.0f, 0.0f, 1.0f) };
    mesh.textureCoords = {
        blib::graphics::Vector3f(0.0f, 0.0f, 0.0f),
        blib::graphics::Vector3f(1.0f, 0.0f, 0.0f),
        blib::graphics::Vector3f(0.0f, 1.0f, 0.0f) };
    mesh.colors = {
        blib::graphics::Color(255, 0, 0, 255),
        blib::graphics::Color(0, 255, 0, 255),
        blib::graphics::Color(0, 0, 255, 255) };
    mesh.boneIds = {
        blib::graphics::Vector4i(0, 1, 0, 0),
        blib::graphics::Vector4i(0, 1, 0, 0),
        blib::graphics::Vector4i(1, 0, 0, 0) };
    mesh.boneWeights = {
        blib::graphics::Vector4f(0.7f, 0.3f, 0.0f, 0.0f),
        blib::graphics::Vector4f(0.7f, 0.3f, 0.0f, 0.0f),
        blib::graphics::Vector4f(1.0f, 0.0f, 0.0f, 0.0f) };
    blib::graphics::Face face;
    face.indices = { 0, 1, 2 };
    mesh.faces.push_back(face);

    mesh.material.m_name = "testMaterial";
    mesh.material.shadingMode = blib::graphics::ShadingMode::Toon;
    mesh.material.hasDiffuseColor = true;
    mesh.material.DiffuseColor = blib::graphics::Vector4f(1.0f, 0.5f, 0.25f, 1.0f);
    mesh.material.rimColor = blib::graphics::Vector3f(0.4f, 0.35f, 0.3f);
    mesh.material.outlineEnabled = true;
    mesh.material.diffuseImage.create(2, 2, blib::graphics::Color(1, 2, 3, 4));

    blib::core::MemoryStream mem;
    BLIB_TEST_REQUIRE(mesh.save(mem) == blib::core::SaveStatus::None);

    blib::graphics::Mesh loaded;
    mem.seek(0, blib::core::SeekOrigin::Begin);
    BLIB_TEST_REQUIRE(loaded.load(mem) == blib::core::LoadStatus::None);

    BLIB_TEST_CHECK(mesh.strongCompare(loaded));
    BLIB_TEST_CHECK(mesh.verify());
}

BLIB_TEST_CASE("serialization: skelet roundtrip restores hierarchy and bind pose")
{
    SkeletonSceneFixture scene;
    BLIB_TEST_REQUIRE(scene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName },
        { neckBoneName, headBoneName } },
        {
            { rootBoneName, makeTranslationMatrix(1.0f, 0.0f, 0.0f) },
            { headBoneName, makeTranslationMatrix(0.0f, 5.0f, 0.0f) },
            { neckBoneName, makeTranslationMatrix(9.0f, 9.0f, 9.0f) } }));

    blib::graphics::Skelet skelet;
    BLIB_TEST_REQUIRE(skelet.loadFromAssimp(scene.scene.get()));

    blib::core::MemoryStream mem;
    BLIB_TEST_REQUIRE(skelet.save(mem) == blib::core::SaveStatus::None);

    blib::graphics::Skelet loaded;
    mem.seek(0, blib::core::SeekOrigin::Begin);
    BLIB_TEST_REQUIRE(loaded.load(mem) == blib::core::LoadStatus::None);

    // Строгое сравнение и round-trip
    BLIB_TEST_CHECK(skelet.strongCompare(loaded));
    BLIB_TEST_CHECK(skelet.verify());

    // Иерархия восстановлена: Head висит на Root
    const blib::graphics::Bone* head = loaded.find(headBoneName);
    BLIB_TEST_REQUIRE(head != nullptr);
    const blib::graphics::IHierarchal* headParent = head->getParent();
    BLIB_TEST_REQUIRE(headParent != nullptr);
    BLIB_TEST_CHECK(static_cast<const blib::graphics::Bone*>(headParent)->name == rootBoneName);

    // finalMatrices пересчитаны и совпадают (global * offset)
    const std::vector<blib::graphics::TransformMatrix>& originalFinal = skelet.getFinalMatrices();
    const std::vector<blib::graphics::TransformMatrix>& loadedFinal = loaded.getFinalMatrices();
    BLIB_TEST_REQUIRE(originalFinal.size() == loadedFinal.size());
    for (size_t i = 0; i < originalFinal.size(); ++i)
    {
        for (size_t j = 0; j < 4; ++j)
        {
            for (size_t k = 0; k < 4; ++k)
            {
                BLIB_TEST_CHECK(originalFinal[i].data[j][k] == loadedFinal[i].data[j][k]);
            }
        }
    }
}

BLIB_TEST_CASE("serialization: skinmodel roundtrip and verify")
{
    // Модель: скелет (Root → Head → Neck с offset-ами) + скин-меш
    // с весами + материал с битмапом диффуза. Аниматор — без клипов
    // (фикстурная сцена без mAnimations)
    SkeletonSceneFixture scene;
    BLIB_TEST_REQUIRE(scene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName },
        { neckBoneName, headBoneName } },
        {
            { headBoneName, makeTranslationMatrix(0.0f, 5.0f, 0.0f) } }));

    blib::graphics::SkinModel model;
    BLIB_TEST_REQUIRE(model.getSkelet().loadFromAssimp(scene.scene.get()));

    SkinMeshFixture skinMeshFixture;
    skinMeshFixture.build(2, {
        { headBoneName, { {0, 1.0f}, {1, 0.5f} } },
        { neckBoneName, { {1, 0.5f} } } });

    blib::graphics::SkinMesh skin;
    BLIB_TEST_REQUIRE(skin.loadFromAssimpMesh(skinMeshFixture.mesh.get(), model.getSkelet()));
    skin.mesh.material.m_name = "skinMaterial";
    skin.mesh.material.diffuseImage.create(2, 1, blib::graphics::Color(7, 8, 9, 10));
    model.getMeshes().push_back(std::move(skin));

    blib::core::MemoryStream mem;
    BLIB_TEST_REQUIRE(model.save(mem) == blib::core::SaveStatus::None);

    blib::graphics::SkinModel loaded;
    mem.seek(0, blib::core::SeekOrigin::Begin);
    BLIB_TEST_REQUIRE(loaded.load(mem) == blib::core::LoadStatus::None);

    BLIB_TEST_CHECK(model.strongCompare(loaded));
    BLIB_TEST_CHECK(model.verify());
}

BLIB_TEST_CASE("skelet: FBX decomposition chain composes in tree order (outer to inner)")
{
    constexpr float epsilon = 1e-4f;

    // FBX-подобная декомпозиция трансформа Child: родитель (Root) →
    // Head_Translation (T(0,1,0)) → Head_Rotation (Rz90) → узел кости
    // Child (T(1,0,0)). Локальный трансформ Child обязан быть
    // произведением в ПОРЯДКЕ ДЕРЕВА (внешний → внутренний):
    // T(0,1,0) * Rz90 * T(1,0,0) = (x,y,z) → (-y, x+2, z)
    SkeletonSceneFixture scene;
    BLIB_TEST_REQUIRE(scene.build(
        { { rootBoneName, "" }, { headBoneName, rootBoneName } },
        {},
        {
            { headBoneName, "Head_Translation", makeTranslationMatrixFull(0.0f, 1.0f, 0.0f) },
            { headBoneName, "Head_Rotation", makeRotationZMatrix(90.0f) }
        }));

    // Собственный трансформ узла кости Child — T(1, 0, 0).
    // Индексы узлов: [0]=сцен-рут, [1]=armature, [2]=Head_Translation,
    // [3]=Head_Rotation, [4]=Root, [5]=Child
    scene.nodes[5].get()->mTransformation = makeTranslationMatrixFull(1.0f, 0.0f, 0.0f);

    blib::graphics::Skelet skelet;
    BLIB_TEST_REQUIRE(skelet.loadFromAssimp(scene.scene.get()));

    const blib::graphics::Bone* child = skelet.find(headBoneName);
    BLIB_TEST_REQUIRE(child != nullptr);
    const blib::graphics::TransformMatrix& local = child->localTransform;

    // Ожидаемая матрица: r0=(0,-1,0,0), r1=(1,0,0,2), r2=(0,0,1,0),
    // r3=(0,0,0,1); column-major: data[col][row]
    BLIB_TEST_CHECK_CLOSE(local.data[0][0], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(local.data[0][1], 1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(local.data[1][0], -1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(local.data[1][1], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(local.data[2][2], 1.0f, epsilon);
    // Трансляция (-y, x+2): колонка 3
    BLIB_TEST_CHECK_CLOSE(local.data[3][0], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(local.data[3][1], 2.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(local.data[3][2], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(local.data[3][3], 1.0f, epsilon);

    // Глобальный трансформ == локальному (Root в identity)
    const blib::graphics::TransformMatrix& global = child->globalTransform;
    BLIB_TEST_CHECK_CLOSE(global.data[3][0], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(global.data[3][1], 2.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(global.data[3][2], 0.0f, epsilon);
}

// ============================================================
// Репродукция «взорвавшегося» скиннинга: финальные костные матрицы.
//
// Математический инвариант скиннинга: в bind-позе финальная матрица
// кости (globalTransform * offsetMatrix, offsetMatrix — inverse bind)
// обязана быть identity. Если порядок умножения или раскладка
// нарушены (например, транспонированное содержимое), для ригов с
// ПОВЁРНУТЫМ родителем это ломается, и вершины разлетаются.
//
// Риг: Root — bind-local R(90°Z); Child — bind-local T(0,2,0).
//   globalRoot  = R90
//   globalChild = R90 * T(0,2,0)   (child bind global)
//   offsetRoot  = R(-90)
//   offsetChild = T(0,-2,0) * R(-90)
// ============================================================

BLIB_TEST_CASE("skelet: bind pose finalMatrices are identity (rotated parent)")
{
    constexpr float epsilon = 1e-4f;

    SkeletonSceneFixture scene;
    BLIB_TEST_REQUIRE(scene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName } },
        {
            { rootBoneName, makeRotationZMatrix(-90.0f) },
            { headBoneName, aiMatrix4x4(
                // T(0,-2,0) * Rz(-90), аргументы — построчно:
                // r0=(0,1,0,0), r1=(-1,0,0,-2), r2=(0,0,1,0), r3=(0,0,0,1)
                0.0f, 1.0f, 0.0f, 0.0f,
                -1.0f, 0.0f, 0.0f, -2.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f) } }));

    // Bind-трансформы узлов: Root — поворот на 90° вокруг Z,
    // Child — трансляция (0, 2, 0) относительно Root
    scene.nodes[2].get()->mTransformation = makeRotationZMatrix(90.0f);
    scene.nodes[3].get()->mTransformation = makeTranslationMatrixFull(0.0f, 2.0f, 0.0f);

    blib::graphics::Skelet skelet;
    BLIB_TEST_REQUIRE(skelet.loadFromAssimp(scene.scene.get()));

    // loadFromAssimp заканчивается computeBindPose — финальные
    // матрицы уже собраны
    const std::vector<blib::graphics::TransformMatrix>& final = skelet.getFinalMatrices();
    BLIB_TEST_REQUIRE(final.size() == 2);

    for (size_t i = 0; i < final.size(); ++i)
    {
        BLIB_TEST_CHECK(isIdentityMatrix(final[i], epsilon));
    }
}

BLIB_TEST_CASE("skelet: applyClip at t=0 matches bind pose, moved child shifts correctly")
{
    constexpr float epsilon = 1e-4f;

    SkeletonSceneFixture scene;
    BLIB_TEST_REQUIRE(scene.build({
        { rootBoneName, "" },
        { headBoneName, rootBoneName } },
        {
            { rootBoneName, makeRotationZMatrix(-90.0f) },
            { headBoneName, aiMatrix4x4(
                // T(0,-2,0) * Rz(-90), аргументы — построчно
                0.0f, 1.0f, 0.0f, 0.0f,
                -1.0f, 0.0f, 0.0f, -2.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f) } }));

    scene.nodes[2].get()->mTransformation = makeRotationZMatrix(90.0f);
    scene.nodes[3].get()->mTransformation = makeTranslationMatrixFull(0.0f, 2.0f, 0.0f);

    blib::graphics::Skelet skelet;
    BLIB_TEST_REQUIRE(skelet.loadFromAssimp(scene.scene.get()));

    // Синтетический клип: сэмпл t=0 == bind-поза.
    // Root: кватернион 90° вокруг Z; Child: позиция (0, 2, 0)
    const double cos45 = 0.7071067811865476;
    blib::graphics::AnimationClip clip;
    clip.channels.resize(2);
    clip.channels[0].boneName = rootBoneName;
    clip.channels[0].rotaionKeys.push_back(std::make_pair(
        0.0, blib::math::Quaternion<double>(cos45, 0.0, 0.0, cos45)));
    clip.channels[1].boneName = headBoneName;
    clip.channels[1].positionKeys.push_back(std::make_pair(
        0.0, blib::graphics::Vector3f(0.0f, 2.0f, 0.0f)));

    clip.boneChains.resize(2);
    clip.boneChains[0].boneIndex = 0;
    clip.boneChains[0].elements.push_back({ 0, blib::graphics::Identity });
    clip.boneChains[1].boneIndex = 1;
    clip.boneChains[1].elements.push_back({ 1, blib::graphics::Identity });

    skelet.applyClip(clip, 0.0);

    // Поза t=0 == bind-поза → финальные матрицы = identity
    const std::vector<blib::graphics::TransformMatrix>& final = skelet.getFinalMatrices();
    BLIB_TEST_REQUIRE(final.size() == 2);
    BLIB_TEST_CHECK(isIdentityMatrix(final[0], epsilon));
    BLIB_TEST_CHECK(isIdentityMatrix(final[1], epsilon));

    // Двигаем Child в (0, 3, 0): локальная +Y ребёнка в мировом
    // базисе = Rz90 * (0,1,0) = (-1,0,0) — сдвиг кости на +1 по
    // локальной Y смещает вершины на -1 по мировой X. Финальная
    // матрица Child обязана стать чистой трансляцией T(-1, 0, 0)
    clip.channels[1].positionKeys[0].second = blib::graphics::Vector3f(0.0f, 3.0f, 0.0f);
    skelet.applyClip(clip, 0.0);

    BLIB_TEST_CHECK(isIdentityMatrix(final[0], epsilon));
    // Трансляция (-1, 0, 0) в последней колонке, вращение — identity
    BLIB_TEST_CHECK_CLOSE(final[1].data[3][0], -1.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(final[1].data[3][1], 0.0f, epsilon);
    BLIB_TEST_CHECK_CLOSE(final[1].data[3][2], 0.0f, epsilon);
    for (size_t i = 0; i < 3; ++i)
    {
        for (size_t j = 0; j < 3; ++j)
        {
            BLIB_TEST_CHECK_CLOSE(final[1].data[i][j], (i == j) ? 1.0f : 0.0f, epsilon);
        }
    }
}

// ============================================================
// Реальный файл: Hip Hop Dancing.fbx (танцор gravelands-клиента).
//
// «Взорвавшийся» танцор: полигоны разлетаются на первом кадре.
// Путь клиента: loadFromFile → SkinModel::loadFromAssimp →
// AnimationSystem → update(0) → applyClip при t=0 (poseDirty).
// Инвариант: в bind-позе (и при t=0, если каналы стартуют из
// bind-позы) все finalMatrices обязаны быть identity.
// ============================================================

BLIB_TEST_CASE("real file: dancer bind pose finalMatrices are identity")
{
    constexpr float epsilon = 1e-3f;

    // Флаги постпроцессинга — как в SkinnedMeshComponent (beng-client)
    constexpr unsigned int assimpPostProcessFlags =
        aiPostProcessSteps::aiProcess_CalcTangentSpace |
        aiPostProcessSteps::aiProcess_Triangulate |
        aiPostProcessSteps::aiProcess_JoinIdenticalVertices |
        aiPostProcessSteps::aiProcess_SortByPType |
        aiPostProcessSteps::aiProcess_PopulateArmatureData;

    Assimp::Importer importer;
    const aiScene* pscene = importer.ReadFile(
        TEST_SOURCE_DIR "/../resources/Hip Hop Dancing.fbx",
        assimpPostProcessFlags);
    if (pscene == nullptr)
    {
        // Ассет лежит вне src/ (M:\Stuff\resources) — на машинах без
        // него тест мягко пропускается
        __blib_log_warning("skinmesh: test asset '%s' not found, skipping", importer.GetErrorString());
        return;
    }

    blib::graphics::SkinModel model;
    BLIB_TEST_REQUIRE(model.loadFromAssimp(
        pscene, TEST_SOURCE_DIR "/../resources/Hip Hop Dancing.fbx"));

    // Сразу после загрузки: computeBindPose собрал финальные матрицы
    // из трансформов узлов — bind-поза обязана давать identity
    const std::vector<blib::graphics::TransformMatrix>& finals =
        model.getSkelet().getFinalMatrices();
    BLIB_TEST_REQUIRE(finals.size() > 0);

    size_t failingBind = 0;
    for (size_t i = 0; i < finals.size(); ++i)
    {
        if (!isIdentityMatrix(finals[i], epsilon))
        {
            ++failingBind;
            if (failingBind <= 3)
            {
                const blib::graphics::Bone& bone = model.getSkelet().getBoneStorage()[i];
                __blib_log_info("dancer: bind final[%zu] '%s' is not identity", i, bone.name.c_str());
            }
        }
    }
    BLIB_TEST_CHECK(failingBind == 0);

    // Путь клиента: poseDirty → SkinModel::update(0) → applyClip(0).
    // ВАЖНО: у Mixamo-файлов кадр t=0 анимации НЕ совпадает с
    // bind-позой (каналы стартуют из другой позы), поэтому identity
    // здесь не ожидается. Проверяем ОБУСЛОВЛЕННОСТЬ: ротационная
    // часть ортонормальна (нормы колонок ≈ 1), трансляция конечна —
    // «взрыв» (гигантские/NaN-значения, как у бага с транспонированной
    // конверсией aiMatrix4x4) здесь же падает
    model.update(0.0f);

    size_t badT0 = 0;
    for (size_t i = 0; i < finals.size(); ++i)
    {
        const blib::graphics::TransformMatrix& f = finals[i];
        bool wellConditioned = true;
        for (size_t col = 0; col < 3; ++col)
        {
            const float len = std::sqrt(
                f.data[col][0] * f.data[col][0] +
                f.data[col][1] * f.data[col][1] +
                f.data[col][2] * f.data[col][2]);
            if (len < 0.9f || len > 1.1f)
            {
                wellConditioned = false;
            }
        }
        for (size_t k = 0; k < 3; ++k)
        {
            if (std::abs(f.data[3][k]) > 1e4f)
            {
                wellConditioned = false;
            }
        }
        if (!wellConditioned)
        {
            ++badT0;
            if (badT0 <= 3)
            {
                const blib::graphics::Bone& bone = model.getSkelet().getBoneStorage()[i];
                __blib_log_info("dancer: t0 final[%zu] '%s': t=(%.3f %.3f %.3f), r0=(%.3f %.3f %.3f), r1=(%.3f %.3f %.3f)",
                    i, bone.name.c_str(),
                    f.data[3][0], f.data[3][1], f.data[3][2],
                    f.data[0][0], f.data[1][0], f.data[2][0],
                    f.data[0][1], f.data[1][1], f.data[2][1]);
            }
        }
    }
    BLIB_TEST_CHECK(badT0 == 0);
}
