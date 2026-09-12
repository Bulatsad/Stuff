#include <gravelands/client/core/isometricTileset.h>

#include <blib/core/console/console.h>
#include <blib/graphics/color.h>
#include <blib/graphics/face.h>
#include <blib/graphics/image.h>

namespace gravelands
{
    namespace
    {
        // Количество тайлов в ряду/столбце сетки
        constexpr buint32 tileGridSide = 10;

        // Сторона квадратного тайла в мировых единицах: тайлы лежат
        // на плоскости XZ (земля, нормаль +Y), а «ромб» на экране —
        // результат наклонной изометрической проекции камеры
        constexpr float tileSize = 30.0f;

        // Цвета шахматной разметки сетки (два оттенка серого:
        // шахматка видна без освещения и текстур)
        constexpr buint8 checkerLightComponent = 200;
        constexpr buint8 checkerDarkComponent = 120;
    }

    IsometricTileset::IsometricTileset()
    {
        // Вершин на тайл (квадрат: 4 угла) и треугольников на тайл
        const buint32 verticesPerTile = 4;
        const buint32 facesPerTile = 2;

        const buint32 tileCount = tileGridSide * tileGridSide;
        const buint32 vertexCount = tileCount * verticesPerTile;

        mesh.vertices.resize(vertexCount);
        mesh.textureCoords.resize(vertexCount);
        mesh.normals.resize(vertexCount);
        mesh.faces.reserve(tileCount * facesPerTile);

        // Сдвиг, центрирующий сетку вокруг начала координат
        const float halfGrid = static_cast<float>(tileGridSide) * 0.5f;
        const float halfTile = tileSize * 0.5f;

        // Нормаль всех тайлов — строго вверх (земля)
        const blib::graphics::Vector3f upNormal(0.0f, 1.0f, 0.0f);

        for (buint32 gz = 0; gz < tileGridSide; ++gz)
        {
            for (buint32 gx = 0; gx < tileGridSide; ++gx)
            {
                // Центр квадрата на плоскости XZ (y = 0)
                const float cx = (static_cast<float>(gx) - halfGrid) * tileSize;
                const float cz = (static_cast<float>(gz) - halfGrid) * tileSize;

                const buint32 base = (gz * tileGridSide + gx) * verticesPerTile;

                // Углы квадрата: left-top, right-top, right-bottom,
                // left-bottom (вид сверху; порядок даёт лицевые грани
                // вверх, CCW при взгляде сверху)
                mesh.vertices[base + 0] = blib::graphics::Vector3f(cx - halfTile, 0.0f, cz - halfTile);
                mesh.vertices[base + 1] = blib::graphics::Vector3f(cx + halfTile, 0.0f, cz - halfTile);
                mesh.vertices[base + 2] = blib::graphics::Vector3f(cx + halfTile, 0.0f, cz + halfTile);
                mesh.vertices[base + 3] = blib::graphics::Vector3f(cx - halfTile, 0.0f, cz + halfTile);

                // Нормали копируются по всем вершинам (одна и та же)
                mesh.normals[base + 0] = upNormal;
                mesh.normals[base + 1] = upNormal;
                mesh.normals[base + 2] = upNormal;
                mesh.normals[base + 3] = upNormal;

                // UV-диапазон тайла в шахматной текстуре: каждая ячейка
                // текстуры — один тайл (см. сборку изображения ниже)
                const float u0 = static_cast<float>(gx) / static_cast<float>(tileGridSide);
                const float u1 = static_cast<float>(gx + 1) / static_cast<float>(tileGridSide);
                const float v0 = static_cast<float>(gz) / static_cast<float>(tileGridSide);
                const float v1 = static_cast<float>(gz + 1) / static_cast<float>(tileGridSide);

                mesh.textureCoords[base + 0] = blib::graphics::Vector3f(u0, v0, 0.0f);
                mesh.textureCoords[base + 1] = blib::graphics::Vector3f(u1, v0, 0.0f);
                mesh.textureCoords[base + 2] = blib::graphics::Vector3f(u1, v1, 0.0f);
                mesh.textureCoords[base + 3] = blib::graphics::Vector3f(u0, v1, 0.0f);

                // Два треугольника на квадрат: (0, 3, 2) и (0, 2, 1) —
                // обход CCW при взгляде сверху, лицевые грани вверх
                blib::graphics::Face faceFirst;
                faceFirst.indices = { base + 0, base + 3, base + 2 };
                mesh.faces.push_back(faceFirst);

                blib::graphics::Face faceSecond;
                faceSecond.indices = { base + 0, base + 2, base + 1 };
                mesh.faces.push_back(faceSecond);
            }
        }

        mesh.primitiveType = blib::graphics::PrimitiveType::Triangle;

        __blib_log_info("tileset: grid %ux%u on XZ plane, vertices: %u, faces: %u",
            tileGridSide, tileGridSide,
            static_cast<unsigned int>(mesh.vertices.size()),
            static_cast<unsigned int>(mesh.faces.size()));

        // Шахматная текстура сетки: клетка (gx, gz) — один пиксель.
        // Тайлы сэмплируют «свой» пиксель через UV-диапазоны выше.
        // Загрузчик не участвует (изображение собирается вручную),
        // поэтому flip_vertically не влияет
        blib::graphics::Image checkerImage;
        checkerImage.create(
            static_cast<buint16>(tileGridSide),
            static_cast<buint16>(tileGridSide),
            blib::graphics::Color(checkerLightComponent, checkerLightComponent, checkerLightComponent, 255));

        for (buint32 gz = 0; gz < tileGridSide; ++gz)
        {
            for (buint32 gx = 0; gx < tileGridSide; ++gx)
            {
                // Шахматный порядок: сумма индексов чётная — светлая.
                // ВАЖНО: Image::operator[] индексируется [колонка][строка]
                // (image[x][y] = bitmap[y * width + x], см. image.h)
                if ((gx + gz) % 2 != 0)
                {
                    checkerImage[static_cast<buint16>(gx)][static_cast<buint16>(gz)] =
                        blib::graphics::Color(checkerDarkComponent, checkerDarkComponent, checkerDarkComponent, 255);
                }
            }
        }

        mesh.material.diffuseImage = checkerImage;
    }

    void IsometricTileset::draw(_In blib::graphics::IRenderTarget& target)
    {
        target.draw(mesh);
    }

} // namespace gravelands
