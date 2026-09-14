#include <blib/graphics/sphere.h>

#include <blib/core/math/consts.h>
#include <blib/core/math/trigonometry.h>
#include <blib/core/math/vector.h>
#include <blib/graphics/face.h>
#include <blib/graphics/image.h>

namespace blib
{
    namespace graphics
    {
        namespace
        {
            // Минимальное число сегментов по экватору: меньше — плоская
            // фигура вместо сферы
            constexpr buint32 minSegments = 3;

            // Минимальное число колец по широте (верхний полюс —
            // экватор — нижний полюс)
            constexpr buint32 minRings = 2;

            // Сторона синтезированной 1x1 диффузной текстуры
            constexpr buint16 colorTextureSize = 1;

            // Треугольников на один квад сетки
            constexpr buint32 facesPerQuad = 2;
        }

        void Sphere::createSpere(float radius, buint32 pointPerCircle, blib::graphics::Color color)
        {
            // Защита от вырожденных параметров (дефолт 4 — призма,
            // для тестов вызывающий передаёт больше)
            const buint32 segments = pointPerCircle >= minSegments ? pointPerCircle : minSegments;
            const buint32 rings = pointPerCircle / 2 >= minRings ? pointPerCircle / 2 : minRings;

            // Сетка UV-сферы: (rings + 1) рядов по широте (полюсные ряды
            // вырождаются в точку), (segments + 1) колонок по долготе —
            // последняя дублирует первую для замыкания текстурного шва
            const buint32 rowCount = rings + 1;
            const buint32 colCount = segments + 1;
            const buint32 vertexCount = rowCount * colCount;

            blib::graphics::Mesh& mesh = this->sphereMesh;

            mesh.vertices.resize(vertexCount);
            mesh.normals.resize(vertexCount);
            mesh.textureCoords.resize(vertexCount);
            mesh.faces.reserve(rings * segments * facesPerQuad);

            const float pi = static_cast<float>(blib::math::pi);
            const float twoPi = 2.0f * pi;

            for (buint32 i = 0; i < rowCount; ++i)
            {
                // Полярный угол: 0 на верхнем полюсе, pi на нижнем;
                // y = cos(phi) — верх полюса в +Y (мировая «вверх»)
                const float phi = pi * static_cast<float>(i) / static_cast<float>(rings);
                const float sinPhi = blib::math::sin(phi);
                const float cosPhi = blib::math::cos(phi);

                for (buint32 j = 0; j < colCount; ++j)
                {
                    const float theta = twoPi * static_cast<float>(j) / static_cast<float>(segments);
                    const float sinTheta = blib::math::sin(theta);
                    const float cosTheta = blib::math::cos(theta);

                    const buint32 index = i * colCount + j;

                    blib::graphics::Vector3f position(
                        radius * sinPhi * cosTheta,
                        radius * cosPhi,
                        radius * sinPhi * sinTheta);

                    mesh.vertices[index] = position;

                    // Для идеальной сферы нормаль = нормализованная позиция
                    mesh.normals[index] = blib::math::normalize(position);

                    // UV: u — долгота (0..1), v — широта (0 — верхний полюс)
                    mesh.textureCoords[index] = blib::graphics::Vector3f(
                        static_cast<float>(j) / static_cast<float>(segments),
                        static_cast<float>(i) / static_cast<float>(rings),
                        0.0f);
                }
            }

            // Индексы: каждый квад (i, j) → два треугольника, обход CCW
            // при взгляде снаружи (лицевые грани наружу, нормаль наружу)
            for (buint32 i = 0; i < rings; ++i)
            {
                for (buint32 j = 0; j < segments; ++j)
                {
                    const buint32 northWest = i * colCount + j;           // (i, j)
                    const buint32 northEast = i * colCount + j + 1;       // (i, j+1)
                    const buint32 southWest = (i + 1) * colCount + j;     // (i+1, j)
                    const buint32 southEast = (i + 1) * colCount + j + 1; // (i+1, j+1)

                    blib::graphics::Face faceFirst;
                    faceFirst.indices = { northEast, southEast, southWest };
                    mesh.faces.push_back(faceFirst);

                    blib::graphics::Face faceSecond;
                    faceSecond.indices = { northEast, southWest, northWest };
                    mesh.faces.push_back(faceSecond);
                }
            }

            mesh.primitiveType = blib::graphics::PrimitiveType::Triangle;

            // Однотонная диффузная текстура 1x1 из цвета: материал
            // «запечёт» её при первом draw (см. Material::bake)
            blib::graphics::Image colorImage;
            colorImage.create(colorTextureSize, colorTextureSize, color);
            mesh.material.diffuseImage = colorImage;
        }

        const blib::graphics::Mesh& Sphere::getMesh() const
        {
            return this->sphereMesh;
        }

        blib::graphics::Mesh Sphere::takeMesh()
        {
            // Move-конструктор Mesh передаёт владение ctx (источник
            // обнуляется) — после вызова сфера пуста
            return std::move(this->sphereMesh);
        }

        void Sphere::draw(RenderContext& ctx) const
        {
            // Трансформ сферы (ITransformable) синхронизируется в меш:
            // позиция задаётся через setPosition/setScale на самой сфере
            this->sphereMesh.setTransform(this->getTransform());
            this->sphereMesh.draw(ctx);
        }

    }
}
