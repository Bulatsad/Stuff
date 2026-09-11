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
