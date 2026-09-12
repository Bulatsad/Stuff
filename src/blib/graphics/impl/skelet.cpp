#include <blib/graphics/skelet.h>

#include <stack>

#include <blib/core/console/console.h>

namespace
{
    // Допуск сравнения элементов inverse bind матриц: у одинаковых
    // ригов (например, Mixamo) значения совпадают с точностью до
    // float-погрешности
    constexpr float skeletonCompatibilityEpsilon = 1e-4f;

    // Поэлементное сравнение матриц с допуском
    bool matricesNearEqual(_In const blib::graphics::TransformMatrix& lhs, _In const blib::graphics::TransformMatrix& rhs)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            for (size_t j = 0; j < 4; ++j)
            {
                const float diff = lhs.data[i][j] - rhs.data[i][j];
                if (diff > skeletonCompatibilityEpsilon || diff < -skeletonCompatibilityEpsilon)
                {
                    return false;
                }
            }
        }
        return true;
    }

    // Конвертация aiMatrix4x4 в TransformMatrix с тем же порядком
    // элементов, что используется в остальных загрузчиках Assimp
    blib::graphics::TransformMatrix transformFromAssimp(const aiMatrix4x4& m)
    {
        return blib::graphics::TransformMatrix(
            {
                m.a1, m.b1, m.c1, m.d1,
                m.a2, m.b2, m.c2, m.d2,
                m.a3, m.b3, m.c3, m.d3,
                m.a4, m.b4, m.c4, m.d4
            });
    }

    // Поиск канала анимации по имени узла: FBX-импортёр Assimp кладёт
    // каналы не на кости, а на узлы цепочки трансформа
    // ("<имя>_$AssimpFbx$_Rotation" и т.п.), а также на сам узел кости
    // для простых трансформов
    const blib::graphics::AnimationChannel* findChannel(
        const blib::graphics::AnimationClip& clip, const std::string& nodeName)
    {
        for (const blib::graphics::AnimationChannel& channel : clip.channels)
        {
            if (channel.boneName == nodeName)
            {
                return &channel;
            }
        }
        return nullptr;
    }

    // Суффикс узлов FBX-декомпозиции трансформа кости
    constexpr const char* decomposedNodePrefix = "_$AssimpFbx$_";

    // Поиск узла сцены по имени (обход в глубину)
    const aiNode* findNodeByName(const aiNode* node, const std::string& name)
    {
        if (!node)
        {
            return nullptr;
        }

        if (name == node->mName.C_Str())
        {
            return node;
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i)
        {
            const aiNode* found = findNodeByName(node->mChildren[i], name);
            if (found)
            {
                return found;
            }
        }

        return nullptr;
    }

    // Элемент привязки: канал по имени узла (если есть) + bind-трансформ
    blib::graphics::AnimationClip::BoneChainElement makeBoneChainElement(
        _In const blib::graphics::AnimationClip& clip,
        _In const aiNode* node)
    {
        blib::graphics::AnimationClip::BoneChainElement element;
        element.channelIndex = clip.channels.size();

        for (size_t i = 0; i < clip.channels.size(); ++i)
        {
            if (clip.channels[i].boneName == node->mName.C_Str())
            {
                element.channelIndex = i;
                break;
            }
        }

        element.bindTransform = transformFromAssimp(node->mTransformation);
        return element;
    }
}

blib::graphics::Bone* blib::graphics::Skelet::find(const std::string& name)
{
    for (size_t i = 0; i < this->boneStorage.size(); ++i)
    {
        if (this->boneStorage[i].name == name)
            return &(this->boneStorage[i]);
    }
    return nullptr;
}

const blib::graphics::Bone* blib::graphics::Skelet::find(const std::string& name) const
{
    for (size_t i = 0; i < this->boneStorage.size(); ++i)
    {
        if (this->boneStorage[i].name == name)
            return &(this->boneStorage[i]);
    }
    return nullptr;
}

size_t blib::graphics::Skelet::findBoneIndex(const std::string& name) const
{
    for (size_t i = 0; i < this->boneStorage.size(); ++i)
    {
        if (this->boneStorage[i].name == name)
            return i;
    }
    return this->boneStorage.size();
}

bool blib::graphics::Skelet::isCompatibleWith(_In const blib::graphics::Skelet& other) const
{
    if (this->boneStorage.size() != other.boneStorage.size())
    {
        __blib_log_error("skeleton mismatch: bone count %zu vs %zu",
            this->boneStorage.size(), other.boneStorage.size());
        return false;
    }

    for (const blib::graphics::Bone& bone : this->boneStorage)
    {
        const blib::graphics::Bone* otherBone = other.find(bone.name);
        if (__blib_unlikely(!otherBone))
        {
            __blib_log_error("skeleton mismatch: bone '%s' not found in other skeleton", bone.name.c_str());
            return false;
        }

        const blib::graphics::IHierarchal* parent = bone.getParent();
        const blib::graphics::IHierarchal* otherParent = otherBone->getParent();
        const std::string parentName = parent ? static_cast<const blib::graphics::Bone*>(parent)->name : std::string();
        const std::string otherParentName = otherParent ? static_cast<const blib::graphics::Bone*>(otherParent)->name : std::string();
        if (parentName != otherParentName)
        {
            __blib_log_error("skeleton mismatch: bone '%s' has parent '%s' vs '%s'",
                bone.name.c_str(), parentName.c_str(), otherParentName.c_str());
            return false;
        }

        if (__blib_unlikely(!matricesNearEqual(bone.offsetMatrix, otherBone->offsetMatrix)))
        {
            __blib_log_error("skeleton mismatch: bone '%s' inverse bind matrix differs", bone.name.c_str());
            return false;
        }
    }

    return true;
}

bool blib::graphics::Skelet::hasNodeName(_In const std::string& nodeName) const
{
    for (const blib::graphics::Bone& bone : this->boneStorage)
    {
        if (bone.name == nodeName)
        {
            return true;
        }

        for (const blib::graphics::BoneChainElement& element : bone.chain)
        {
            if (element.nodeName == nodeName)
            {
                return true;
            }
        }
    }

    return false;
}

bool blib::graphics::Skelet::makeBoneTree(const aiNode* pbone)
{
    struct boneTreeMakerCtx
    {
        const aiNode* pnode = nullptr;
        blib::graphics::Bone* bone = nullptr;
        blib::graphics::Bone* prev = nullptr;
    };

    std::stack<boneTreeMakerCtx> st;
    {
        boneTreeMakerCtx newCtx;
        newCtx.bone = this->root;
        newCtx.pnode = pbone;
        newCtx.prev = nullptr;
        st.push(newCtx);
    }

    while (!(st.empty()))
    {
        boneTreeMakerCtx ctx = st.top();
        st.pop();

        for (unsigned int i = 0; i < ctx.pnode->mNumChildren; ++i)
        {
            const aiNode* childNode = ctx.pnode->mChildren[i];
            blib::graphics::Bone* storedBone = this->find(childNode->mName.C_Str());
            if (__blib_unlikely(!storedBone))
            {
                // Узел без кости — не ошибка: FBX-импортёр Assimp
                // раскладывает трансформ кости по узлам-декомпозициям
                // ("<имя>_$AssimpFbx$_Translation" и т.п.). Пропускаем
                // его, но поддерево обходим с тем же родителем-костью:
                // глубже в цепочке лежит настоящий узел кости
                boneTreeMakerCtx newCtx;
                newCtx.bone = ctx.bone;
                newCtx.pnode = childNode;
                newCtx.prev = ctx.prev;
                st.push(newCtx);
                continue;
            }

            ctx.bone->addChild(storedBone);

            boneTreeMakerCtx newCtx;
            newCtx.bone = storedBone;
            newCtx.pnode = childNode;
            newCtx.prev = ctx.bone;
            st.push(newCtx);
        }

        if (__blib_unlikely(ctx.bone->getParent() != ctx.prev))
        {
            __blib_log_error("inconsistent bone parent while building bone tree (bone '%s')", ctx.bone->name.c_str());
            return false;
        }
    }

    return true;
}

bool blib::graphics::Skelet::loadDefaultPoseFromArmature(const aiNode* pbone)
{
    struct poseLoaderCtx
    {
        const aiNode* pnode = nullptr;
    };

    std::stack<poseLoaderCtx> st;
    // Сам узел armature не обрабатываем: это не-кость (либо созданный
    // нами синтетический корень), а его трансформ для настоящих костей
    // уже учтён в loadDefaultPoseFromNodes через цепочку предков
    for (unsigned int i = 0; i < pbone->mNumChildren; ++i)
    {
        poseLoaderCtx newCtx;
        newCtx.pnode = pbone->mChildren[i];
        st.push(newCtx);
    }

    while (!(st.empty()))
    {
        poseLoaderCtx ctx = st.top();
        st.pop();

        blib::graphics::Bone* bone = this->find(ctx.pnode->mName.C_Str());
        // Запасной путь только для костей без узла сцены (mNode не
        // заполнен): точную позу для остальных посчитала
        // loadDefaultPoseFromNodes
        if (bone && bone->node == nullptr)
        {
            const aiMatrix4x4& m = ctx.pnode->mTransformation;
            bone->localTransform = transformFromAssimp(m);
            bone->globalTransform = bone->localTransform;
        }

        for (unsigned int i = 0; i < ctx.pnode->mNumChildren; ++i)
        {
            poseLoaderCtx newCtx;
            newCtx.pnode = ctx.pnode->mChildren[i];
            st.push(newCtx);
        }
    }

    return true;
}

void blib::graphics::Skelet::loadDefaultPoseFromNodes(const aiNode* sceneRoot)
{
    if (!sceneRoot)
    {
        return;
    }

    // Точная bind-поза по цепочке узлов сцены: у FBX трансформ кости
    // разложен по нескольким узлам ("<имя>_$AssimpFbx$_Translation"
    // и т.п.) между родительской костью и самой костью. Локальный
    // трансформ кости — произведение трансформов узлов от узла кости
    // вверх по дереву:
    //  - остановка на первой кости-предке (её трансформ не входит в
    //    локальный — он относительно неё);
    //  - трансформ сцен-рута не включается никогда (у MD5 там лежит
    //    конверсия осей Z-up→Y-up, которую применяет вызывающий код
    //    самостоятельно).
    // Попутно запоминаем цепочку узлов в bone.chain — она нужна при
    // воспроизведении анимации: Assimp кладёт каналы на узлы цепочки

    // Предки от ближайшего к дальнему (в конце развернём в обратную
    // сторону); переиспользуемый буфер, чтобы не выделять на кость
    std::vector<blib::graphics::BoneChainElement> ancestors;

    for (blib::graphics::Bone& bone : this->boneStorage)
    {
        const aiNode* boneNode = bone.node;
        if (!boneNode)
        {
            continue;
        }

        ancestors.clear();

        const aiNode* parent = boneNode->mParent;
        while (parent && parent != sceneRoot)
        {
            // Остановка на настоящей кости-предке: найденная кость
            // должна быть привязана именно к этому узлу (bone->node).
            // Синтетический корень (node == nullptr) костью-предком
            // не считается — его имя может совпадать с именем узла
            // armature внутри цепочки FBX-декомпозиции
            const blib::graphics::Bone* ancestorBone = this->find(parent->mName.C_Str());
            if (ancestorBone && ancestorBone->node == parent)
            {
                break;
            }

            blib::graphics::BoneChainElement element;
            element.nodeName = parent->mName.C_Str();
            element.bindTransform = transformFromAssimp(parent->mTransformation);
            ancestors.push_back(element);
            parent = parent->mParent;
        }

        // Цепочка: от внешнего элемента к внутреннему; последний —
        // собственный узел кости (его имя совпадает с именем кости —
        // так подхватываются и «простые» каналы без суффиксов)
        bone.chain.clear();
        for (size_t i = ancestors.size(); i > 0; --i)
        {
            bone.chain.push_back(ancestors[i - 1]);
        }
        blib::graphics::BoneChainElement ownElement;
        ownElement.nodeName = boneNode->mName.C_Str();
        ownElement.bindTransform = transformFromAssimp(boneNode->mTransformation);
        bone.chain.push_back(ownElement);

        // Bind-поза — произведение цепочки в том же порядке
        blib::graphics::TransformMatrix acc = blib::graphics::Identity;
        for (const blib::graphics::BoneChainElement& element : bone.chain)
        {
            acc = blib::graphics::mul(acc, element.bindTransform);
        }
        bone.localTransform = acc;
    }
}

bool blib::graphics::Skelet::finishFromArmature(const aiNode* armature, const aiNode* sceneRoot)
{
    // Корень скелета:
    //  - если сам узел armature — кость, корень — она (как раньше);
    //  - иначе ищем ПЕРВУЮ настоящую кость в поддереве armature.
    //    Если поддерево этой кости содержит все кости armature —
    //    она становится корнем. Синтетическую корневую кость в этом
    //    случае НЕ создаём: у неё identity-трансформ в центре сцены,
    //    и отладочный скелет рисует фантомный сегмент от центра сцены
    //    до первой кости (у FBX это визуально «кость, которая
    //    удлиняется/уменьшается» с неподвижной точкой у центра);
    //  - если единой корневой кости нет (несколько независимых
    //    скелетов) или костей нет вовсе — патология: оставляем
    //    синтетическую кость (старое поведение)
    const aiNode* treeRootNode = armature;
    blib::graphics::Bone* rootBone = this->find(armature->mName.C_Str());
    if (!rootBone)
    {
        blib::graphics::Bone* firstBone = this->findFirstBoneInSubtree(armature);
        if (firstBone &&
            this->countBonesInSubtree(firstBone->node) == this->countBonesInSubtree(armature))
        {
            rootBone = firstBone;
            treeRootNode = firstBone->node;
        }
    }
    if (!rootBone)
    {
        // Синтетическая корневая кость: armature — не-кость, и общего
        // корня в поддереве нет. Она должна нести identity-трансформ:
        // её узел не настоящий, а реальная цепочка трансформов учтена
        // в позах настоящих костей
        this->boneStorage.emplace_back();
        rootBone = &(this->boneStorage.back());
        rootBone->name = armature->mName.C_Str();
        rootBone->offsetMatrix.loadIdentity();
        rootBone->localTransform.loadIdentity();
        rootBone->globalTransform.loadIdentity();
    }
    this->root = rootBone;

    this->finalMatrices.resize(this->boneStorage.size());

    if (!(this->makeBoneTree(treeRootNode)))
    {
        return false;
    }
    // Сначала точная поза по цепочке узлов сцены, затем запасной проход
    // по дереву armature для костей, у которых нет узла сцены
    this->loadDefaultPoseFromNodes(sceneRoot);
    if (!(this->loadDefaultPoseFromArmature(armature)))
    {
        return false;
    }

    // Сразу приводим скелет в консистентную bind-позу: иерархические
    // globalTransform + финальные матрицы для скиннинга. Без этого
    // до первого applyClip скелет, рисуемый линиями, «разобран»
    this->computeBindPose();

    return true;
}

blib::graphics::Bone* blib::graphics::Skelet::findFirstBoneInSubtree(const aiNode* node)
{
    std::stack<const aiNode*> st;
    st.push(node);

    while (!(st.empty()))
    {
        const aiNode* current = st.top();
        st.pop();

        blib::graphics::Bone* bone = this->find(current->mName.C_Str());
        if (bone && bone->node == current)
        {
            return bone;
        }

        for (unsigned int i = 0; i < current->mNumChildren; ++i)
        {
            st.push(current->mChildren[i]);
        }
    }

    return nullptr;
}

size_t blib::graphics::Skelet::countBonesInSubtree(const aiNode* node) const
{
    size_t count = 0;
    std::stack<const aiNode*> st;
    st.push(node);

    while (!(st.empty()))
    {
        const aiNode* current = st.top();
        st.pop();

        const blib::graphics::Bone* bone = this->find(current->mName.C_Str());
        if (bone && bone->node == current)
        {
            ++count;
        }

        for (unsigned int i = 0; i < current->mNumChildren; ++i)
        {
            st.push(current->mChildren[i]);
        }
    }

    return count;
}

bool blib::graphics::Skelet::loadFromAssimp(const aiMesh* paimesh)
{
    if (__blib_unlikely(paimesh->mNumBones == 0))
    {
        __blib_log_error("no bones to load from mesh");
        return false;
    }

    if (__blib_unlikely(!(paimesh->mBones[0]->mArmature)))
    {
        __blib_log_error("armature field not populated in mesh bones");
        return false;
    }

    for (unsigned int i = 0; i < paimesh->mNumBones; ++i)
    {
        if (__blib_unlikely(paimesh->mBones[0]->mArmature != paimesh->mBones[i]->mArmature))
        {
            __blib_log_error("mesh bones belong to different armatures");
            return false;
        }
    }

    this->boneStorage.resize(paimesh->mNumBones);

    for (unsigned int i = 0; i < paimesh->mNumBones; ++i)
    {
        if (__blib_unlikely(!(this->boneStorage[i].loadFromAssimp(paimesh->mBones[i]))))
        {
            __blib_log_error("error on loading bone #%u", i);
            return false;
        }
    }

    // Сцен-рут для расчёта позы выводим подъёмом по родителям узла
    // первой кости (если узел известен)
    const aiNode* sceneRoot = paimesh->mBones[0]->mNode;
    if (sceneRoot)
    {
        while (sceneRoot->mParent)
        {
            sceneRoot = sceneRoot->mParent;
        }
    }

    return this->finishFromArmature(paimesh->mBones[0]->mArmature, sceneRoot);
}

bool blib::graphics::Skelet::loadFromAssimp(const aiScene* paiscene)
{
    const aiNode* armature = nullptr;

    for (unsigned int m = 0; m < paiscene->mNumMeshes; ++m)
    {
        const aiMesh* paimesh = paiscene->mMeshes[m];
        for (unsigned int b = 0; b < paimesh->mNumBones; ++b)
        {
            if (!armature)
            {
                armature = paimesh->mBones[b]->mArmature;
            }

            if (!(this->find(paimesh->mBones[b]->mName.C_Str())))
            {
                this->boneStorage.emplace_back();
                if (__blib_unlikely(!(this->boneStorage.back().loadFromAssimp(paimesh->mBones[b]))))
                {
                    __blib_log_error("error on loading bone '%s'", paimesh->mBones[b]->mName.C_Str());
                    return false;
                }
            }
        }
    }

    if (__blib_unlikely(this->boneStorage.empty()))
    {
        __blib_log_error("no bones to load from scene");
        return false;
    }

    if (__blib_unlikely(!armature))
    {
        __blib_log_error("armature field not populated in scene bones");
        return false;
    }

    return this->finishFromArmature(armature, paiscene->mRootNode);
}

void blib::graphics::Skelet::updateTransforms(blib::graphics::Bone* pbone)
{
    const blib::graphics::IHierarchal* pParent = pbone->getParent();
    if (pParent)
    {
        pbone->globalTransform = blib::graphics::mul(static_cast<const blib::graphics::Bone*>(pParent)->globalTransform, pbone->localTransform);
    }
    else
    {
        pbone->globalTransform = pbone->localTransform;
    }

    for (blib::graphics::IHierarchal* pchild : pbone->getChilds())
    {
        this->updateTransforms(static_cast<blib::graphics::Bone*>(pchild));
    }
}

void blib::graphics::Skelet::computeBindPose()
{
    if (__blib_unlikely(!(this->root)))
    {
        __blib_log_warning("computeBindPose: skeleton has no root");
        return;
    }

    // globalTransform каждой кости = parent.global * bone.local
    // (рекурсивно сверху вниз — та же логика, что в applyClip)
    this->updateTransforms(this->root);

    // Финальные матрицы для скиннинга: global * inverse-bind (offset)
    for (size_t i = 0; i < this->boneStorage.size(); ++i)
    {
        this->finalMatrices[i] = blib::graphics::mul(this->boneStorage[i].globalTransform, this->boneStorage[i].offsetMatrix);
    }
}

void blib::graphics::Skelet::bindClipToSkeleton(_In blib::graphics::AnimationClip& clip, _In const aiScene* animationScene) const
{
    clip.boneChains.clear();

    if (__blib_unlikely(!animationScene || !animationScene->mRootNode))
    {
        return;
    }

    for (size_t boneIndex = 0; boneIndex < this->boneStorage.size(); ++boneIndex)
    {
        const blib::graphics::Bone& bone = this->boneStorage[boneIndex];

        // Кость анимируется, если канал лежит на ней самой или на
        // узлах её FBX-декомпозиции ("<имя>_$AssimpFbx$_...")
        bool hasChannel = false;
        const std::string decomposedPrefix = bone.name + decomposedNodePrefix;
        for (const blib::graphics::AnimationChannel& channel : clip.channels)
        {
            if (channel.boneName == bone.name ||
                channel.boneName.compare(0, decomposedPrefix.size(), decomposedPrefix) == 0)
            {
                hasChannel = true;
                break;
            }
        }
        if (!hasChannel)
        {
            continue;
        }

        const aiNode* boneNode = findNodeByName(animationScene->mRootNode, bone.name);
        if (__blib_unlikely(!boneNode))
        {
            __blib_log_warning("animation: node '%s' not found in animation scene, bone skipped", bone.name.c_str());
            continue;
        }

        // Цепочка узлов файла анимации: от внешнего узла к собственной
        // кости (как в loadDefaultPoseFromNodes — до кости-предка или
        // корня сцены, трансформ которого не входит в локальный)
        std::vector<const aiNode*> chainNodes;
        const aiNode* parent = boneNode->mParent;
        while (parent && parent != animationScene->mRootNode)
        {
            if (this->find(parent->mName.C_Str()))
            {
                break;
            }
            chainNodes.push_back(parent);
            parent = parent->mParent;
        }

        blib::graphics::AnimationClip::BoneChain boneChain;
        boneChain.boneIndex = boneIndex;
        for (size_t i = chainNodes.size(); i > 0; --i)
        {
            boneChain.elements.push_back(makeBoneChainElement(clip, chainNodes[i - 1]));
        }
        boneChain.elements.push_back(makeBoneChainElement(clip, boneNode));

        clip.boneChains.push_back(boneChain);
    }
}

void blib::graphics::Skelet::applyClip(const blib::graphics::AnimationClip& clip, double timeTicks)
{
    if (!(clip.boneChains.empty()))
    {
        // Клип привязан к костям по дереву файла анимации: его
        // FBX-декомпозиция может отличаться от модели, поэтому
        // локальный трансформ собирается по цепочке узлов самого
        // файла анимации (сэмпл канала или bind-трансформ узла)
        for (const blib::graphics::AnimationClip::BoneChain& boneChain : clip.boneChains)
        {
            if (__blib_unlikely(boneChain.boneIndex >= this->boneStorage.size()))
            {
                continue;
            }

            blib::graphics::TransformMatrix local = blib::graphics::Identity;
            for (const blib::graphics::AnimationClip::BoneChainElement& element : boneChain.elements)
            {
                if (element.channelIndex < clip.channels.size())
                {
                    const blib::graphics::AnimationChannel& channel = clip.channels[element.channelIndex];

                    blib::graphics::Vector3f position;
                    blib::math::Quaternion<double> rotation;
                    blib::graphics::Vector3f scale;

                    if (channel.sample(timeTicks, position, rotation, scale))
                    {
                        local = blib::graphics::mul(local, blib::graphics::composeMatrix(position, rotation, scale));
                        continue;
                    }
                }

                local = blib::graphics::mul(local, element.bindTransform);
            }

            this->boneStorage[boneChain.boneIndex].localTransform = local;
        }
    }
    else
    {
        // Каналы анимации хранят трансформы относительно родительской
        // кости. У FBX трансформ кости разложен по цепочке узлов, а каналы
        // лежат на узлах цепочки — локальный трансформ собирается из неё:
        // каждый элемент подменяется сэмплом канала с именем его узла
        // (канала нет — остаётся bind-матрица элемента)
        for (blib::graphics::Bone& bone : this->boneStorage)
        {
            if (!(bone.chain.empty()))
            {
                blib::graphics::TransformMatrix local = blib::graphics::Identity;
                for (const blib::graphics::BoneChainElement& element : bone.chain)
                {
                    const blib::graphics::AnimationChannel* channel = findChannel(clip, element.nodeName);
                    if (channel)
                    {
                        blib::graphics::Vector3f position;
                        blib::math::Quaternion<double> rotation;
                        blib::graphics::Vector3f scale;

                        if (channel->sample(timeTicks, position, rotation, scale))
                        {
                            local = blib::graphics::mul(local, blib::graphics::composeMatrix(position, rotation, scale));
                            continue;
                        }
                    }

                    local = blib::graphics::mul(local, element.bindTransform);
                }
                bone.localTransform = local;
                continue;
            }

            // Кость без известной цепочки (или без узла сцены): канал
            // ищется по имени самой кости, как раньше
            const blib::graphics::AnimationChannel* channel = findChannel(clip, bone.name);
            if (!channel)
            {
                continue;
            }

            blib::graphics::Vector3f position;
            blib::math::Quaternion<double> rotation;
            blib::graphics::Vector3f scale;

            if (!(channel->sample(timeTicks, position, rotation, scale)))
            {
                continue;
            }

            bone.localTransform = blib::graphics::composeMatrix(position, rotation, scale);
        }
    }

    // bones without animated channels keep their bind local transforms,
    // so the whole hierarchy always has valid locals
    if (this->root)
    {
        this->updateTransforms(this->root);
    }

    for (size_t i = 0; i < this->boneStorage.size(); ++i)
    {
        this->finalMatrices[i] = blib::graphics::mul(this->boneStorage[i].globalTransform, this->boneStorage[i].offsetMatrix);
    }
}

std::vector<blib::graphics::Bone>& blib::graphics::Skelet::getBoneStorage()
{
    return this->boneStorage;
}

const std::vector<blib::graphics::Bone>& blib::graphics::Skelet::getBoneStorage() const
{
    return this->boneStorage;
}

const std::vector<blib::graphics::TransformMatrix>& blib::graphics::Skelet::getFinalMatrices() const
{
    return this->finalMatrices;
}
