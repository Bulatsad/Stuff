#include <blib/test/src/test.h>

#include <blib/graphics/skelet.h>
#include <blib/graphics/skinmesh.h>

#include <assimp/scene.h>

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

    // aiMatrix4x4 с трансляцией: transformFromAssimp кладёт m.d1..d3
    // в data[*][3] — колонку трансляции TransformMatrix движка
    aiMatrix4x4 makeTranslationMatrix(float x, float y, float z)
    {
        aiMatrix4x4 m;
        m.d1 = x;
        m.d2 = y;
        m.d3 = z;
        return m;
    }

    // Совпадает ли offsetMatrix кости с трансляцией (x, y, z).
    // Хранилище Matrix — data[колонка][строка]: компоненты d1..d3
    // (трансляция aiMatrix4x4) попадают в data[3][0..2]
    bool hasTranslationOffset(_In const blib::graphics::Bone& bone, float x, float y, float z)
    {
        constexpr float epsilon = 1e-5f;
        return std::abs(bone.offsetMatrix.data[3][0] - x) < epsilon &&
            std::abs(bone.offsetMatrix.data[3][1] - y) < epsilon &&
            std::abs(bone.offsetMatrix.data[3][2] - z) < epsilon;
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
        // имени (по умолчанию identity)
        bool build(
            _In const std::vector<std::pair<std::string, std::string>>& boneSpecs,
            _In const std::vector<std::pair<std::string, aiMatrix4x4>>& offsetMatrices = {})
        {
            const size_t boneCount = boneSpecs.size();

            this->nodes.resize(2 + boneCount);
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

            for (size_t i = 0; i < boneCount; ++i)
            {
                this->nodes[2 + i].get()->mName = boneSpecs[i].first;
            }

            // Поиск индекса узла по имени (кости уже лежат в nodes)
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
                this->childLists[parentIndex].push_back(this->nodes[2 + i].get());
            }

            // Сцен-рут → armature
            this->childLists[0].push_back(this->nodes[1].get());

            for (size_t i = 0; i < this->nodes.size(); ++i)
            {
                aiNode* node = this->nodes[i].get();
                node->mChildren = this->childLists[i].empty() ? nullptr : this->childLists[i].data();
                node->mNumChildren = static_cast<unsigned int>(this->childLists[i].size());
            }

            for (size_t i = 0; i < boneCount; ++i)
            {
                aiBone* bone = this->bones[i].get();
                bone->mName = boneSpecs[i].first;
                bone->mArmature = this->nodes[1].get();
                bone->mNode = this->nodes[2 + i].get();
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
