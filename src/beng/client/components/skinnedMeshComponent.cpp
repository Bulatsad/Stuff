#include <beng/client/components/skinnedMeshComponent.h>

#include <blib/core/console/console.h>
#include <blib/core/json/json.h>
#include <blib/core/verifyHelper.h>
#include <blib/system/memory/globalAllocator.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace beng
{
    namespace
    {
        // Ключи JSON-объекта компонента (формат save/load); содержимое
        // модели сериализует blib::graphics::SkinModel (см. SkinModel::toJson)
        constexpr const char* keyModel = "model";
        constexpr const char* keyIsActive = "isActive";

        // Флаги постпроцессинга Assimp, общие для всех загрузок:
        // триангуляция обязательна (рендер строит EBO по треугольникам),
        // PopulateArmatureData — обязателен для скелетной анимации
        constexpr unsigned int assimpPostProcessFlags =
            aiPostProcessSteps::aiProcess_CalcTangentSpace |
            aiPostProcessSteps::aiProcess_Triangulate |
            aiPostProcessSteps::aiProcess_JoinIdenticalVertices |
            aiPostProcessSteps::aiProcess_SortByPType |
            aiPostProcessSteps::aiProcess_PopulateArmatureData;

        // Имя файла без пути и расширения используется как имя клипа:
        // у Mixamo клипы часто называются "mixamo.com"
        constexpr const char* pathSeparatorChars = "\\/";
        constexpr char extensionSeparatorChar = '.';

        std::string extractFileStem(_In const std::string& path)
        {
            size_t start = path.find_last_of(pathSeparatorChars);
            start = (start == std::string::npos) ? 0 : start + 1;

            size_t end = path.find_last_of(extensionSeparatorChar);
            if (end == std::string::npos || end < start)
            {
                end = path.size();
            }

            return path.substr(start, end - start);
        }
    }

    SkinnedMeshComponent::SkinnedMeshComponent()
        : model(nullptr)
    {
    }

    SkinnedMeshComponent::~SkinnedMeshComponent()
    {
        this->unload();
    }

    bool SkinnedMeshComponent::loadFromFile(_In const std::string& path)
    {
        // Повторная загрузка: сначала выгружаем старую модель
        this->unload();

        // Assimp: парсим файл. aiScene принадлежит импортеру и живёт
        // только внутри этой функции — SkinModel::loadFromAssimp
        // копирует всё нужное в собственные структуры
        Assimp::Importer importer;
        const aiScene* pscene = importer.ReadFile(path, assimpPostProcessFlags);
        if (__blib_unlikely(!pscene))
        {
            __blib_return_error(false, "assimp: failed to load '%s': %s", path.c_str(), importer.GetErrorString());
        }

        // Аллокация модели через GlobalAllocator + placement new
        // (проектное правило: выделяющие new/delete запрещены)
        this->model = static_cast<blib::graphics::SkinModel*>(
            blib::memory::GlobalAllocator::instance().allocate(sizeof(blib::graphics::SkinModel)));
        new (this->model) blib::graphics::SkinModel();

        if (__blib_unlikely(!this->model->loadFromAssimp(pscene, path)))
        {
            __blib_log_error("failed to build model from assimp scene: %s", path.c_str());
            this->unload();
            return false;
        }

        __blib_log_info("model loaded: %s", path.c_str());
        return true;
    }

    bool SkinnedMeshComponent::loadSkinFromFile(_In const std::string& path, _In bool force)
    {
        if (__blib_unlikely(!this->model))
        {
            __blib_return_error(false, "skin '%s': no model loaded", path.c_str());
        }

        // Сцена нужна только для проверки скелета и загрузки мешей:
        // скелет и аниматор текущей модели не трогаются
        Assimp::Importer importer;
        const aiScene* pscene = importer.ReadFile(path, assimpPostProcessFlags);
        if (__blib_unlikely(!pscene))
        {
            __blib_return_error(false, "assimp: failed to load skin '%s': %s", path.c_str(), importer.GetErrorString());
        }

        if (__blib_unlikely(!this->model->replaceMeshesFromAssimp(pscene, path, force)))
        {
            __blib_return_error(false, "failed to replace skin from '%s'", path.c_str());
        }

        __blib_log_info("skin replaced: %s%s", path.c_str(), force ? " (forced)" : "");
        return true;
    }

    bool SkinnedMeshComponent::loadAnimationsFromFile(_In const std::string& path)
    {
        if (__blib_unlikely(!this->model))
        {
            __blib_return_error(false, "animations '%s': no model loaded", path.c_str());
        }

        Assimp::Importer importer;
        const aiScene* pscene = importer.ReadFile(path, assimpPostProcessFlags);
        if (__blib_unlikely(!pscene))
        {
            __blib_return_error(false, "assimp: failed to load animations '%s': %s", path.c_str(), importer.GetErrorString());
        }

        blib::graphics::Animator& animator = this->model->getAnimator();
        const size_t clipsBefore = animator.getAnimations().size();

        // Имя файла — имя единственного клипа (см. Animator::appendFromAssimp);
        // скелет задан, чтобы каналы привязались к костям по дереву
        // файла анимации (декомпозиция FBX у него может отличаться)
        if (__blib_unlikely(!animator.appendFromAssimp(pscene, &this->model->getSkelet(), extractFileStem(path))))
        {
            __blib_return_error(false, "failed to add animations from '%s'", path.c_str());
        }

        // Валидация: каждый канал новой анимации должен быть привязан
        // к какой-либо кости скелета. Непривязанные каналы не
        // применяются — это предупреждение, не ошибка
        const std::vector<blib::graphics::AnimationClip>& clips = animator.getAnimations();
        size_t unmatchedChannels = 0;
        for (size_t i = clipsBefore; i < clips.size(); ++i)
        {
            const blib::graphics::AnimationClip& clip = clips[i];
            for (size_t c = 0; c < clip.channels.size(); ++c)
            {
                bool bound = false;
                for (const blib::graphics::AnimationClip::BoneChain& boneChain : clip.boneChains)
                {
                    for (const blib::graphics::AnimationClip::BoneChainElement& element : boneChain.elements)
                    {
                        if (element.channelIndex == c)
                        {
                            bound = true;
                            break;
                        }
                    }
                    if (bound)
                    {
                        break;
                    }
                }

                if (!bound)
                {
                    ++unmatchedChannels;
                }
            }
        }

        if (unmatchedChannels > 0)
        {
            __blib_log_warning("animations '%s': %zu channels reference unknown bones (skeleton mismatch?)",
                path.c_str(), unmatchedChannels);
        }

        __blib_log_info("animations added: %s", path.c_str());
        return true;
    }

    void SkinnedMeshComponent::unload()
    {
        if (this->model)
        {
            // Явный деструктор + возврат памяти глобальному аллокатору
            this->model->~SkinModel();
            blib::memory::GlobalAllocator::instance().deallocate(this->model, sizeof(blib::graphics::SkinModel));
            this->model = nullptr;
        }
    }

    blib::graphics::SkinModel* SkinnedMeshComponent::getModel()
    {
        return this->model;
    }

    const blib::graphics::SkinModel* SkinnedMeshComponent::getModel() const
    {
        return this->model;
    }

    blib::core::SaveStatus SkinnedMeshComponent::save(_In blib::core::IOutputStream& os) const
    {
        blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

        // Полное содержимое модели или null («модель не загружена»)
        if (this->model)
        {
            doc.set(keyModel, this->model->toJson());
        }
        else
        {
            doc.set(keyModel, blib::core::json::JsonValue(nullptr));
        }
        doc.set(keyIsActive, blib::core::json::JsonValue(isActive));

        if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::SaveStatus::WriteFailed,
                "SkinnedMeshComponent: failed to write JSON to stream");
        }
        return blib::core::SaveStatus::None;
    }

    blib::core::LoadStatus SkinnedMeshComponent::load(_In blib::core::IInputStream& is)
    {
        blib::core::json::JsonParser parser;
        blib::core::json::JsonValue doc;
        if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "SkinnedMeshComponent: failed to parse JSON from stream");
        }

        // Валидация полей компонента ДО применения (модель валидирует
        // SkinModel::fromJson — состояние при ошибке не меняется)
        if (__blib_unlikely(!doc.isObject()) ||
            __blib_unlikely(!doc.has(keyModel)) ||
            __blib_unlikely(!doc.has(keyIsActive) || !doc.get(keyIsActive).isBool()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "SkinnedMeshComponent: missing or malformed field");
        }

        const blib::core::json::JsonValue& modelNode = doc.get(keyModel);
        if (modelNode.isNull())
        {
            // «Модель не загружена» — выгрузить текущую
            this->unload();
            isActive = doc.get(keyIsActive).asBool();
            return blib::core::LoadStatus::None;
        }

        // Модель в файле есть — аллоцировать при отсутствии
        // (GlobalAllocator + placement new, как в loadFromFile)
        if (this->model == nullptr)
        {
            this->model = static_cast<blib::graphics::SkinModel*>(
                blib::memory::GlobalAllocator::instance().allocate(sizeof(blib::graphics::SkinModel)));
            if (__blib_unlikely(this->model == nullptr))
            {
                __blib_return_error(blib::core::LoadStatus::ReadFailed,
                    "SkinnedMeshComponent: failed to allocate SkinModel");
            }
            new (this->model) blib::graphics::SkinModel();
        }

        // Восстановить содержимое модели целиком (скелет, веса,
        // геометрия, материалы, клипы). Перезапись ЗАПЕЧЁННОЙ модели
        // требует живого RenderContext — см. SkinModel::fromJson
        if (__blib_unlikely(this->model->fromJson(modelNode) != blib::core::LoadStatus::None))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "SkinnedMeshComponent: malformed model data");
        }

        isActive = doc.get(keyIsActive).asBool();

        return blib::core::LoadStatus::None;
    }

    bool SkinnedMeshComponent::strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const
    {
        // Защита от циклов
        if (!session.enter(this, &other))
        {
            return true;
        }

        const SkinnedMeshComponent& o = static_cast<const SkinnedMeshComponent&>(other);

        // Базовые поля + модель: null-состояние должно совпадать, а
        // при наличии моделей — полное сравнение их содержимого
        if (getOwnerId() != o.getOwnerId() || isActive != o.isActive)
        {
            return false;
        }
        if ((model == nullptr) != (o.model == nullptr))
        {
            return false;
        }
        if (model && !model->strongCompare(*o.model, session))
        {
            return false;
        }

        return true;
    }

    bool SkinnedMeshComponent::verify() const
    {
        // Round-trip без RTTI (см. blib::core::verifyRoundTrip).
        // Валидируется сериализуемое состояние — CPU-содержимое
        // модели (GL-кэш в него не входит)
        return blib::core::verifyRoundTrip(*this);
    }

} // namespace beng
