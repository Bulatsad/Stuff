#include <blib/graphics/blobShadow.h>

#include <blib/graphics/face.h>

#include <Windows.h>
#include <gl/GL.h>

namespace blib
{
    namespace graphics
    {
        namespace
        {
            // Вершин на квад и треугольников на квад
            constexpr buint32 verticesPerShadow = 4;
            constexpr buint32 facesPerShadow = 2;
        }

        BlobShadow::BlobShadow()
        {
        }

        void BlobShadow::create(float radius, _In const blib::graphics::Image& image)
        {
            blib::graphics::Mesh& shadowMesh = this->mesh;

            shadowMesh.vertices.resize(verticesPerShadow);
            shadowMesh.normals.resize(verticesPerShadow);
            shadowMesh.textureCoords.resize(verticesPerShadow);
            shadowMesh.faces.reserve(facesPerShadow);

            // Квад в плоскости XZ, локальный центр в нуле:
            // 0 — left-top, 1 — left-bottom, 2 — right-bottom,
            // 3 — right-top (вид сверху). Обход CCW при взгляде
            // сверху — лицевые грани вверх
            shadowMesh.vertices[0] = blib::graphics::Vector3f(-radius, 0.0f, -radius);
            shadowMesh.vertices[1] = blib::graphics::Vector3f(-radius, 0.0f, radius);
            shadowMesh.vertices[2] = blib::graphics::Vector3f(radius, 0.0f, radius);
            shadowMesh.vertices[3] = blib::graphics::Vector3f(radius, 0.0f, -radius);

            // Нормаль — строго вверх (Unlit не использует, для корректности)
            const blib::graphics::Vector3f upNormal(0.0f, 1.0f, 0.0f);
            for (buint32 i = 0; i < verticesPerShadow; ++i)
            {
                shadowMesh.normals[i] = upNormal;
            }

            shadowMesh.textureCoords[0] = blib::graphics::Vector3f(0.0f, 0.0f, 0.0f);
            shadowMesh.textureCoords[1] = blib::graphics::Vector3f(0.0f, 1.0f, 0.0f);
            shadowMesh.textureCoords[2] = blib::graphics::Vector3f(1.0f, 1.0f, 0.0f);
            shadowMesh.textureCoords[3] = blib::graphics::Vector3f(1.0f, 0.0f, 0.0f);

            blib::graphics::Face faceFirst;
            faceFirst.indices = { 0, 1, 2 };
            shadowMesh.faces.push_back(faceFirst);

            blib::graphics::Face faceSecond;
            faceSecond.indices = { 0, 2, 3 };
            shadowMesh.faces.push_back(faceSecond);

            shadowMesh.primitiveType = blib::graphics::PrimitiveType::Triangle;

            // Unlit без alpha-test: мягкость края решает блендинг
            // (альфа канала текстуры), discard отключён
            shadowMesh.material.diffuseImage = image;
            shadowMesh.material.shadingMode = blib::graphics::ShadingMode::Unlit;
            shadowMesh.material.m_alphaTest = 0.0f;
        }

        const blib::graphics::Mesh& BlobShadow::getMesh() const
        {
            return this->mesh;
        }

        blib::graphics::Mesh BlobShadow::takeMesh()
        {
            // Move-конструктор Mesh передаёт владение ctx (источник
            // обнуляется) — после вызова тень пуста
            return std::move(this->mesh);
        }

        void BlobShadow::draw(RenderContext& ctx) const
        {
            // Трансформ тени синхронизируется в меш (позиция задаётся
            // на самой тени)
            this->mesh.setTransform(this->getTransform());

            // Альфа-блендинг + запрет записи глубины: тень не должна
            // спорить с землёй (z-fighting) и с другими тенями.
            // Состояние восстанавливается после отрисовки — вызывающий
            // рисует тени отдельным блоком до непрозрачных объектов
            ctx.api.ogl.__blib_glEnable(GL_BLEND);
            ctx.api.ogl.__blib_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            ctx.api.ogl.__blib_glDepthMask(GL_FALSE);

            this->mesh.draw(ctx);

            ctx.api.ogl.__blib_glDepthMask(GL_TRUE);
            ctx.api.ogl.__blib_glDisable(GL_BLEND);
        }

    }
}
