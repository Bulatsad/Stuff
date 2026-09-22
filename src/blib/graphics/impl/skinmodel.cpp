#include <blib/graphics/skinmodel.h>

#include <blib/core/console/console.h>
#include <blib/core/folder.h>
#include <blib/core/verifyHelper.h>

namespace
{
    // Ключи JSON-объекта модели (формат сериализации)
    constexpr const char* modelKeySkelet = "skelet";
    constexpr const char* modelKeyMeshes = "meshes";
    constexpr const char* modelKeyAnimator = "animator";

    // Загрузка мешей и материалов сцены против заданного скелета.
    // Меши заполняются через resize (Mesh владеет сырым ctx-указателем,
    // копирование недопустимо), при ошибке outMeshes может содержать
    // частично загруженные меши — вызывающий отбрасывает вектор.
    // candidateSkelet — скелет файла-источника: в force-режиме по его
    // иерархии ищутся предки для весов неизвестных костей
    bool loadMeshesFromAssimp(
        _In const aiScene* paiscene,
        _In const std::string& filename,
        _In const blib::graphics::Skelet& skelet,
        _In bool force,
        _In_opt const blib::graphics::Skelet* candidateSkelet,
        _Out std::vector<blib::graphics::SkinMesh>& outMeshes)
    {
        outMeshes.resize(paiscene->mNumMeshes);
        for (size_t i = 0; i < outMeshes.size(); ++i)
        {
            if (!(outMeshes[i].loadFromAssimpMesh(paiscene->mMeshes[i], skelet, force, candidateSkelet)))
            {
                return false;
            }

            if (!(filename.empty()) && (paiscene->mNumMaterials > 0) && (paiscene->mMeshes[i]->mMaterialIndex < paiscene->mNumMaterials))
            {
                blib::core::Folder folder(filename);
                outMeshes[i].mesh.material.loadFromAssimpMaterial(paiscene->mMaterials[paiscene->mMeshes[i]->mMaterialIndex], folder, paiscene);
            }
        }

        return true;
    }
}

bool blib::graphics::SkinModel::loadFromAssimp(const aiScene* paiscene, const std::string& filename, const aiScene* panimationScene)
{
    if (!(this->skelet.loadFromAssimp(paiscene)))
    {
        return false;
    }

    if (!(this->animator.loadFromAssimp(panimationScene ? panimationScene : paiscene)))
    {
        return false;
    }

    // Скелеты и аниматор текущей модели не трогаются
    return loadMeshesFromAssimp(paiscene, filename, this->skelet, false, nullptr, this->meshes);
}

bool blib::graphics::SkinModel::replaceMeshesFromAssimp(_In const aiScene* paiscene, _In const std::string& filename, _In bool force)
{
    // Файл без мешей (например, Mixamo animation FBX без скина) не
    // может заменить скин — иначе модель осталась бы без геометрии
    // (force эту проверку не отменяет)
    if (__blib_unlikely(paiscene->mNumMeshes == 0))
    {
        __blib_log_error("skin '%s' contains no meshes", filename.c_str());
        return false;
    }

    // Скелет из нового файла — только для проверки совместимости:
    // скиннинг выполняется offset-матрицами текущего скелета, поэтому
    // bind-поза обязана совпадать
    blib::graphics::Skelet candidateSkelet;
    if (!(candidateSkelet.loadFromAssimp(paiscene)))
    {
        __blib_log_error("skin '%s': failed to load skeleton for compatibility check", filename.c_str());
        return false;
    }

    if (!(candidateSkelet.isCompatibleWith(this->skelet)))
    {
        if (!force)
        {
            __blib_log_error("skin '%s' is not compatible with current skeleton", filename.c_str());
            return false;
        }

        // Force-режим: retarget кожи на текущий риг — переносим
        // inverse-bind матрицы кандидата на одноимённые кости текущего
        // скелета. Меш после этого рендерится как нативно скиннутый к
        // текущему ригу: пропорции следуют текущему скелету, швы на
        // суставах не расходятся при анимации (веса костей без пары
        // обработает remap ниже). computeBindPose пересчитывает
        // finalMatrices с новыми offset-ами; при играющей анимации
        // applyClip перезапишет их в следующем кадре
        __blib_log_warning("skin '%s': skeleton mismatch ignored (forced apply)", filename.c_str());
        this->skelet.adoptOffsetMatricesFrom(candidateSkelet);
        this->skelet.computeBindPose();
    }

    // Загрузка во временный вектор: при ошибке текущие меши не тронуты.
    // Веса костей, отсутствующих в текущем скелете, переносятся на
    // ближайших существующих предков из иерархии скелета-кандидата
    // (или отбрасываются и ренормализуются, если предка нет)
    std::vector<blib::graphics::SkinMesh> newMeshes;
    if (!(loadMeshesFromAssimp(paiscene, filename, this->skelet, force, force ? &candidateSkelet : nullptr, newMeshes)))
    {
        __blib_log_error("skin '%s': failed to load meshes", filename.c_str());
        return false;
    }

    // Старые меши уничтожаются вместе с временным вектором
    this->meshes.swap(newMeshes);
    return true;
}

void blib::graphics::SkinModel::update(float deltaTimeMs)
{
    this->animator.update(deltaTimeMs);

    const blib::graphics::AnimationClip* pclip = this->animator.getCurrentAnimation();
    if (!pclip || pclip->tickPerSecond <= 0.0)
    {
        return;
    }

    double timeTicks = (this->animator.getCurrentTimeMs() / 1000.0) * pclip->tickPerSecond;
    this->skelet.applyClip(*pclip, timeTicks);
}

bool blib::graphics::SkinModel::selectAnimation(const std::string& animationName)
{
    return this->animator.selectAnimation(animationName);
}

bool blib::graphics::SkinModel::playAnimation()
{
    return this->animator.play();
}

blib::graphics::Skelet& blib::graphics::SkinModel::getSkelet()
{
    return this->skelet;
}

const blib::graphics::Skelet& blib::graphics::SkinModel::getSkelet() const
{
    return this->skelet;
}

blib::graphics::Animator& blib::graphics::SkinModel::getAnimator()
{
    return this->animator;
}

const blib::graphics::Animator& blib::graphics::SkinModel::getAnimator() const
{
    return this->animator;
}

std::vector<blib::graphics::SkinMesh>& blib::graphics::SkinModel::getMeshes()
{
    return this->meshes;
}

const std::vector<blib::graphics::SkinMesh>& blib::graphics::SkinModel::getMeshes() const
{
    return this->meshes;
}

void blib::graphics::SkinModel::draw(blib::graphics::RenderContext& ctx) const
{
    const std::vector<blib::graphics::TransformMatrix>& finalMatrices = this->skelet.getFinalMatrices();
    const blib::graphics::TransformMatrix transform = this->getTransform();

    for (const blib::graphics::SkinMesh& skinMesh : this->meshes)
    {
        const blib::graphics::TransformMatrix meshTransformCopy = skinMesh.mesh.getTransform();
        const blib::graphics::TransformMatrix meshTransform = transform * meshTransformCopy;
        skinMesh.mesh.setTransform(meshTransform);
        skinMesh.mesh.draw(ctx, &finalMatrices);
        skinMesh.mesh.setTransform(meshTransformCopy);
    }
}

blib::core::json::JsonValue blib::graphics::SkinModel::toJson() const
{
    blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

    doc.set(modelKeySkelet, this->skelet.toJson());

    blib::core::json::JsonValue& meshesArr =
        doc.set(modelKeyMeshes, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::SkinMesh& skinMesh : this->meshes)
    {
        meshesArr.pushBack(skinMesh.toJson());
    }

    doc.set(modelKeyAnimator, this->animator.toJson());

    return doc;
}

blib::core::LoadStatus blib::graphics::SkinModel::fromJson(_In const blib::core::json::JsonValue& json)
{
    // Валидация формы ДО применения: при ошибке состояние не меняется
    if (!json.isObject() ||
        !json.has(modelKeySkelet) || !json.has(modelKeyMeshes) ||
        !json.has(modelKeyAnimator))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    // Скелет и аниматор — во временные объекты (полная валидация)
    blib::graphics::Skelet skelet;
    if (__blib_unlikely(skelet.fromJson(json.get(modelKeySkelet)) != blib::core::LoadStatus::None))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    blib::graphics::Animator animator;
    if (__blib_unlikely(animator.fromJson(json.get(modelKeyAnimator)) != blib::core::LoadStatus::None))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    // Меши — во временный вектор. SkinMesh внутри держит Mesh с
    // сырым ctx-указателем: копирование запрещено, поэтому вектор
    // строится resize + fromJson (Mesh::fromJson сам пересоздаёт меш)
    std::vector<blib::graphics::SkinMesh> meshes;
    const blib::core::json::JsonValue& meshesArr = json.get(modelKeyMeshes);
    if (!meshesArr.isArray())
    {
        return blib::core::LoadStatus::InvalidData;
    }
    meshes.resize(meshesArr.size());
    for (buint32 i = 0; i < meshesArr.size(); ++i)
    {
        if (__blib_unlikely(meshes[i].fromJson(meshesArr[i]) != blib::core::LoadStatus::None))
        {
            return blib::core::LoadStatus::InvalidData;
        }
    }

    // Все данные валидны — применить (move; старые меши уничтожаются
    // вместе со старым вектором — их деструкторы требуют живого
    // RenderContext, если модель была запечена)
    this->skelet = std::move(skelet);
    this->animator = std::move(animator);
    this->meshes = std::move(meshes);

    return blib::core::LoadStatus::None;
}

blib::core::SaveStatus blib::graphics::SkinModel::save(_In blib::core::IOutputStream& os) const
{
    const blib::core::json::JsonValue doc = this->toJson();
    if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
    {
        return blib::core::SaveStatus::WriteFailed;
    }
    return blib::core::SaveStatus::None;
}

blib::core::LoadStatus blib::graphics::SkinModel::load(_In blib::core::IInputStream& is)
{
    blib::core::json::JsonParser parser;
    blib::core::json::JsonValue doc;
    if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
    {
        return blib::core::LoadStatus::InvalidData;
    }
    return this->fromJson(doc);
}

bool blib::graphics::SkinModel::strongCompare(_In const blib::core::IStrongComparable& other,
    _In blib::core::CompareSession& session) const
{
    if (!session.enter(this, &other))
    {
        return true;
    }

    const blib::graphics::SkinModel& o = static_cast<const blib::graphics::SkinModel&>(other);

    if (this->meshes.size() != o.meshes.size())
    {
        return false;
    }

    if (!this->skelet.strongCompare(o.skelet, session) ||
        !this->animator.strongCompare(o.animator, session))
    {
        return false;
    }

    for (size_t i = 0; i < this->meshes.size(); ++i)
    {
        if (!this->meshes[i].strongCompare(o.meshes[i], session))
        {
            return false;
        }
    }

    return true;
}

bool blib::graphics::SkinModel::verify() const
{
    // Round-trip без RTTI (см. blib::core::verifyRoundTrip).
    // Перезапись НЕзапечённой модели безопасна без RenderContext
    return blib::core::verifyRoundTrip(*this);
}
