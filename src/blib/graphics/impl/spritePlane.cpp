#include <blib/graphics/spritePlane.h>

#include <blib/graphics/face.h>

namespace blib
{
    namespace graphics
    {
        namespace
        {
            // Вершин на квад и треугольников на квад
            constexpr buint32 verticesPerPlane = 4;
            constexpr buint32 facesPerPlane = 2;

            // Порог alpha-test по умолчанию: полупрозрачные пиксели
            // текстуры с альфой ниже порога отбрасываются в шейдере
            constexpr float defaultAlphaTest = 0.5f;
        }

        SpritePlane::SpritePlane()
        {
        }

        void SpritePlane::create(float width, float height, _In const blib::graphics::Image& image)
        {
            blib::graphics::Mesh& planeMesh = this->mesh;

            planeMesh.vertices.resize(verticesPerPlane);
            planeMesh.normals.resize(verticesPerPlane);
            planeMesh.textureCoords.resize(verticesPerPlane);
            planeMesh.faces.reserve(facesPerPlane);

            const float halfWidth = width * 0.5f;
            const float halfHeight = height * 0.5f;

            // Квад в плоскости XY, локальный центр в нуле:
            // 0 — left-bottom, 1 — right-bottom, 2 — right-top,
            // 3 — left-top (вид со стороны +Z, лицевая грань)
            planeMesh.vertices[0] = blib::graphics::Vector3f(-halfWidth, -halfHeight, 0.0f);
            planeMesh.vertices[1] = blib::graphics::Vector3f(halfWidth, -halfHeight, 0.0f);
            planeMesh.vertices[2] = blib::graphics::Vector3f(halfWidth, halfHeight, 0.0f);
            planeMesh.vertices[3] = blib::graphics::Vector3f(-halfWidth, halfHeight, 0.0f);

            // Нормаль — +Z (для корректности; Unlit её не использует)
            const blib::graphics::Vector3f planeNormal(0.0f, 0.0f, 1.0f);
            for (buint32 i = 0; i < verticesPerPlane; ++i)
            {
                planeMesh.normals[i] = planeNormal;
            }

            // UV: v=0 внизу (низ — «нога» плоскости)
            planeMesh.textureCoords[0] = blib::graphics::Vector3f(0.0f, 0.0f, 0.0f);
            planeMesh.textureCoords[1] = blib::graphics::Vector3f(1.0f, 0.0f, 0.0f);
            planeMesh.textureCoords[2] = blib::graphics::Vector3f(1.0f, 1.0f, 0.0f);
            planeMesh.textureCoords[3] = blib::graphics::Vector3f(0.0f, 1.0f, 0.0f);

            // Треугольники CCW при взгляде с +Z (лицевые грани к камере)
            blib::graphics::Face faceFirst;
            faceFirst.indices = { 0, 1, 2 };
            planeMesh.faces.push_back(faceFirst);

            blib::graphics::Face faceSecond;
            faceSecond.indices = { 0, 2, 3 };
            planeMesh.faces.push_back(faceSecond);

            planeMesh.primitiveType = blib::graphics::PrimitiveType::Triangle;

            planeMesh.material.diffuseImage = image;
            planeMesh.material.shadingMode = blib::graphics::ShadingMode::Unlit;
            planeMesh.material.m_alphaTest = defaultAlphaTest;
        }

        void SpritePlane::setAlphaTest(float alphaTest)
        {
            this->mesh.material.m_alphaTest = alphaTest;
        }

        const blib::graphics::Mesh& SpritePlane::getMesh() const
        {
            return this->mesh;
        }

        blib::graphics::Mesh SpritePlane::takeMesh()
        {
            // Move-конструктор Mesh передаёт владение ctx (источник
            // обнуляется) — после вызова плоскость пуста
            return std::move(this->mesh);
        }

        void SpritePlane::draw(RenderContext& ctx) const
        {
            // Трансформ плоскости синхронизируется в меш: позиция
            // и поворот задаются на самой плоскости (ITransformable)
            this->mesh.setTransform(this->getTransform());
            this->mesh.draw(ctx);
        }

    }
}
