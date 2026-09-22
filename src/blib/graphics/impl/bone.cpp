#include <blib/graphics/bone.h>

#include <blib/core/console/console.h>
#include <blib/core/verifyHelper.h>

namespace
{
    // Ключи JSON-объекта кости (формат сериализации)
    constexpr const char* boneKeyName = "name";
    constexpr const char* boneKeyChain = "chain";
    constexpr const char* boneKeyNodeName = "nodeName";
    constexpr const char* boneKeyBindTransform = "bindTransform";
    constexpr const char* boneKeyWeights = "weights";
    constexpr const char* boneKeyOffsetMatrix = "offsetMatrix";
    constexpr const char* boneKeyLocalTransform = "localTransform";
    constexpr const char* boneKeyGlobalTransform = "globalTransform";

    // Число элементов матрицы 4x4
    constexpr buint32 boneMatrixElementCount = 16;

    // Матрица → массив 16 чисел в порядке, согласованном с
    // Matrix(std::initializer_list): data[0][0], data[1][0], ..., data[3][3]
    blib::core::json::JsonValue boneMatrixToArray(_In const blib::graphics::TransformMatrix& m)
    {
        blib::core::json::JsonValue arr = blib::core::json::JsonValue::makeArray();
        for (buint32 j = 0; j < 4; ++j)
        {
            for (buint32 i = 0; i < 4; ++i)
            {
                arr.pushBack(blib::core::json::JsonValue(m.data[i][j]));
            }
        }
        return arr;
    }

    // Массив 16 чисел → матрица (false — неверная форма)
    bool boneReadMatrix(_In const blib::core::json::JsonValue& arr, _Out blib::graphics::TransformMatrix& out)
    {
        if (!arr.isArray() || arr.size() != boneMatrixElementCount)
        {
            return false;
        }
        blib::graphics::TransformMatrix m;
        m.loadIdentity();
        buint32 index = 0;
        for (buint32 j = 0; j < 4; ++j)
        {
            for (buint32 i = 0; i < 4; ++i)
            {
                m.data[i][j] = arr[index].asBfloat();
                ++index;
            }
        }
        out = m;
        return true;
    }

    // Поэлементное сравнение матриц (бит-в-бит)
    bool boneMatricesEqual(_In const blib::graphics::TransformMatrix& a, _In const blib::graphics::TransformMatrix& b)
    {
        for (buint32 j = 0; j < 4; ++j)
        {
            for (buint32 i = 0; i < 4; ++i)
            {
                if (a.data[i][j] != b.data[i][j])
                {
                    return false;
                }
            }
        }
        return true;
    }
}

bool blib::graphics::Bone::loadFromAssimp(const aiBone* pbone)
{
    this->name = std::string(pbone->mName.C_Str());

    if (__blib_unlikely(this->name.empty()))
    {
        __blib_log_error("empty bone name while loading from assimp");
        return false;
    }

    this->node = pbone->mNode;

    this->offsetMatrix = blib::graphics::TransformMatrix(
        {
            pbone->mOffsetMatrix.a1, pbone->mOffsetMatrix.b1, pbone->mOffsetMatrix.c1, pbone->mOffsetMatrix.d1,
            pbone->mOffsetMatrix.a2, pbone->mOffsetMatrix.b2, pbone->mOffsetMatrix.c2, pbone->mOffsetMatrix.d2,
            pbone->mOffsetMatrix.a3, pbone->mOffsetMatrix.b3, pbone->mOffsetMatrix.c3, pbone->mOffsetMatrix.d3,
            pbone->mOffsetMatrix.a4, pbone->mOffsetMatrix.b4, pbone->mOffsetMatrix.c4, pbone->mOffsetMatrix.d4
        });

    this->localTransform = blib::graphics::Identity;
    this->globalTransform = blib::graphics::Identity;

    this->weights.resize(pbone->mNumWeights);

    for (size_t i = 0; i < this->weights.size(); ++i)
    {
        this->weights[i] = std::make_pair(pbone->mWeights[i].mVertexId, pbone->mWeights[i].mWeight);
    }

    return true;
}

blib::core::json::JsonValue blib::graphics::Bone::toJson() const
{
    blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

    doc.set(boneKeyName, blib::core::json::JsonValue(this->name.c_str()));

    // Цепочка узлов трансформа кости (nodeName + bindTransform)
    blib::core::json::JsonValue& chain = doc.set(boneKeyChain, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::BoneChainElement& element : this->chain)
    {
        blib::core::json::JsonValue chainElement = blib::core::json::JsonValue::makeObject();
        chainElement.set(boneKeyNodeName, blib::core::json::JsonValue(element.nodeName.c_str()));
        chainElement.set(boneKeyBindTransform, boneMatrixToArray(element.bindTransform));
        chain.pushBack(chainElement);
    }

    // Веса: пары [vertexId, weight]
    blib::core::json::JsonValue& weights = doc.set(boneKeyWeights, blib::core::json::JsonValue::makeArray());
    for (const std::pair<size_t, float>& weight : this->weights)
    {
        blib::core::json::JsonValue weightPair = blib::core::json::JsonValue::makeArray();
        weightPair.pushBack(blib::core::json::JsonValue(static_cast<buint64>(weight.first)));
        weightPair.pushBack(blib::core::json::JsonValue(weight.second));
        weights.pushBack(weightPair);
    }

    doc.set(boneKeyOffsetMatrix, boneMatrixToArray(this->offsetMatrix));
    doc.set(boneKeyLocalTransform, boneMatrixToArray(this->localTransform));
    doc.set(boneKeyGlobalTransform, boneMatrixToArray(this->globalTransform));

    return doc;
}

blib::core::LoadStatus blib::graphics::Bone::fromJson(_In const blib::core::json::JsonValue& json)
{
    // Валидация формы ДО применения: при ошибке состояние не меняется
    if (!json.isObject() ||
        !json.has(boneKeyName) || !json.get(boneKeyName).isString() ||
        !json.has(boneKeyChain) || !json.has(boneKeyWeights) ||
        !json.has(boneKeyOffsetMatrix) || !json.has(boneKeyLocalTransform) ||
        !json.has(boneKeyGlobalTransform))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    // Цепочка
    std::vector<blib::graphics::BoneChainElement> chain;
    const blib::core::json::JsonValue& chainArr = json.get(boneKeyChain);
    if (!chainArr.isArray())
    {
        return blib::core::LoadStatus::InvalidData;
    }
    for (buint32 i = 0; i < chainArr.size(); ++i)
    {
        const blib::core::json::JsonValue& elementObj = chainArr[i];
        if (!elementObj.isObject() ||
            !elementObj.has(boneKeyNodeName) || !elementObj.get(boneKeyNodeName).isString() ||
            !elementObj.has(boneKeyBindTransform))
        {
            return blib::core::LoadStatus::InvalidData;
        }
        blib::graphics::BoneChainElement element;
        element.nodeName = elementObj.get(boneKeyNodeName).asString().c_str();
        if (!boneReadMatrix(elementObj.get(boneKeyBindTransform), element.bindTransform))
        {
            return blib::core::LoadStatus::InvalidData;
        }
        chain.push_back(element);
    }

    // Веса
    std::vector<std::pair<size_t, float>> weights;
    const blib::core::json::JsonValue& weightsArr = json.get(boneKeyWeights);
    if (!weightsArr.isArray())
    {
        return blib::core::LoadStatus::InvalidData;
    }
    for (buint32 i = 0; i < weightsArr.size(); ++i)
    {
        const blib::core::json::JsonValue& weightPair = weightsArr[i];
        if (!weightPair.isArray() || weightPair.size() != 2)
        {
            return blib::core::LoadStatus::InvalidData;
        }
        weights.push_back(std::make_pair(
            static_cast<size_t>(weightPair[0].asBuint64()),
            weightPair[1].asBfloat()));
    }

    // Матрицы
    blib::graphics::TransformMatrix offset;
    blib::graphics::TransformMatrix local;
    blib::graphics::TransformMatrix global;
    if (!boneReadMatrix(json.get(boneKeyOffsetMatrix), offset) ||
        !boneReadMatrix(json.get(boneKeyLocalTransform), local) ||
        !boneReadMatrix(json.get(boneKeyGlobalTransform), global))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    // Все данные валидны — применить. node (контекст Assimp) и
    // parent/childs (иерархия — забота Skelet) не трогаем
    this->name = json.get(boneKeyName).asString().c_str();
    this->chain = std::move(chain);
    this->weights = std::move(weights);
    this->offsetMatrix = offset;
    this->localTransform = local;
    this->globalTransform = global;

    return blib::core::LoadStatus::None;
}

blib::core::SaveStatus blib::graphics::Bone::save(_In blib::core::IOutputStream& os) const
{
    const blib::core::json::JsonValue doc = this->toJson();
    if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
    {
        return blib::core::SaveStatus::WriteFailed;
    }
    return blib::core::SaveStatus::None;
}

blib::core::LoadStatus blib::graphics::Bone::load(_In blib::core::IInputStream& is)
{
    blib::core::json::JsonParser parser;
    blib::core::json::JsonValue doc;
    if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
    {
        return blib::core::LoadStatus::InvalidData;
    }
    return this->fromJson(doc);
}

bool blib::graphics::Bone::strongCompare(_In const blib::core::IStrongComparable& other,
    _In blib::core::CompareSession& session) const
{
    if (!session.enter(this, &other))
    {
        return true;
    }

    const blib::graphics::Bone& o = static_cast<const blib::graphics::Bone&>(other);

    // node (контекст Assimp) и parent/childs (иерархия — сравнивает
    // Skelet по parentIndex) НЕ сравниваются
    if (this->name != o.name || this->chain.size() != o.chain.size() ||
        this->weights.size() != o.weights.size())
    {
        return false;
    }

    for (size_t i = 0; i < this->chain.size(); ++i)
    {
        if (this->chain[i].nodeName != o.chain[i].nodeName ||
            !boneMatricesEqual(this->chain[i].bindTransform, o.chain[i].bindTransform))
        {
            return false;
        }
    }

    for (size_t i = 0; i < this->weights.size(); ++i)
    {
        if (this->weights[i].first != o.weights[i].first ||
            this->weights[i].second != o.weights[i].second)
        {
            return false;
        }
    }

    return boneMatricesEqual(this->offsetMatrix, o.offsetMatrix) &&
        boneMatricesEqual(this->localTransform, o.localTransform) &&
        boneMatricesEqual(this->globalTransform, o.globalTransform);
}

bool blib::graphics::Bone::verify() const
{
    // Round-trip без RTTI (см. blib::core::verifyRoundTrip)
    return blib::core::verifyRoundTrip(*this);
}
