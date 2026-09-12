#include <beng/client/components/skinnedMeshComponent.h>

#include <blib/core/console/console.h>
#include <blib/system/memory/globalAllocator.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace beng
{
    namespace
    {
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

} // namespace beng
