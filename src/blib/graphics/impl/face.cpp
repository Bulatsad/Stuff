#include <blib/graphics/face.h>

#include <blib/core/console/console.h>

void blib::graphics::Face::loadFromAssimpFace(const aiFace* paiface)
{
    this->indices.resize(paiface->mNumIndices);
    for (size_t i = 0; i < paiface->mNumIndices; ++i)
    {
        auto& bengindex = this->indices[i];
        auto& aiindex = paiface->mIndices[i];

        bengindex = aiindex;
    }
}

std::vector<buint32> blib::graphics::compileFaces(const blib::graphics::Faces& faces)
{
    std::vector<buint32> res;

    // Все ошибки возвращают пустой вектор (сигнатура отдаёт значение,
    // код ошибки передать нельзя) + лог через Console
    if (__blib_unlikely(faces.size() == 0))
    {
        __blib_log_error("compileFaces: empty faces array");
        return res;
    }

    size_t fsz = faces[0].indices.size();

    if (__blib_unlikely(fsz == 0))
    {
        __blib_log_error("compileFaces: first face has no indices");
        return res;
    }

    // Check that all faces have same sizes
    for (const blib::graphics::Face& f : faces)
    {
        if (__blib_unlikely(f.indices.size() != fsz))
        {
            __blib_log_error("compileFaces: faces have different index counts (%zu vs %zu)", f.indices.size(), fsz);
            return res;
        }
    }

    res.resize(faces.size() * fsz);
    for (size_t i = 0; i < res.size(); ++i)
    {
        res[i] = faces[i / fsz].indices[i % fsz];
    }

    return res;
}
