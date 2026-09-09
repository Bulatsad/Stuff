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

        // Полуразмеры ромба-тайла: при таких шагах соседние тайлы
        // соприкасаются рёбрами и сетка не имеет щелей
        constexpr float tileHalfWidth = 15.0f;
        constexpr float tileHalfHeight = 8.0f;

        // Плоскость тайлов по Z (перед камерой по умолчанию, смотрит в -Z)
        constexpr float tilePlaneZ = -150.0f;

        // Сторона диффузной текстуры материала (1x1 пиксель)
        constexpr buint16 tileTextureSize = 1;
    }

    IsometricTileset::IsometricTileset()
    {
        // Вершин на тайл (ромб: top, right, bottom, left) и треугольников на тайл
        const buint32 verticesPerTile = 4;
        const buint32 facesPerTile = 2;

        const buint32 tileCount = tileGridSide * tileGridSide;
        const buint32 vertexCount = tileCount * verticesPerTile;

        mesh.vertices.resize(vertexCount);
        mesh.textureCoords.resize(vertexCount);
        mesh.faces.reserve(tileCount * facesPerTile);

        // Сдвиг, центрирующий сетку вокруг начала координат
        const float halfGrid = static_cast<float>(tileGridSide) * 0.5f;

        for (buint32 gy = 0; gy < tileGridSide; ++gy)
        {
            for (buint32 gx = 0; gx < tileGridSide; ++gx)
            {
                // Центр ромба в мировой плоскости XY
                const float cx = (static_cast<float>(gx) - halfGrid) * tileHalfWidth;
                const float cy = (static_cast<float>(gy) - halfGrid) * tileHalfHeight;

                const buint32 base = (gy * tileGridSide + gx) * verticesPerTile;

                // Вершины ромба: top, right, bottom, left
                mesh.vertices[base + 0] = blib::graphics::Vector3f(cx, cy + tileHalfHeight, tilePlaneZ);
                mesh.vertices[base + 1] = blib::graphics::Vector3f(cx + tileHalfWidth, cy, tilePlaneZ);
                mesh.vertices[base + 2] = blib::graphics::Vector3f(cx, cy - tileHalfHeight, tilePlaneZ);
                mesh.vertices[base + 3] = blib::graphics::Vector3f(cx - tileHalfWidth, cy, tilePlaneZ);

                // Текстурные координаты (текстура однородная — значения произвольны)
                mesh.textureCoords[base + 0] = blib::graphics::Vector3f(0.5f, 1.0f, 0.0f);
                mesh.textureCoords[base + 1] = blib::graphics::Vector3f(1.0f, 0.5f, 0.0f);
                mesh.textureCoords[base + 2] = blib::graphics::Vector3f(0.5f, 0.0f, 0.0f);
                mesh.textureCoords[base + 3] = blib::graphics::Vector3f(0.0f, 0.5f, 0.0f);

                // Два треугольника на ромб: (top, right, bottom) и (bottom, left, top)
                blib::graphics::Face faceFirst;
                faceFirst.indices = { base + 0, base + 1, base + 2 };
                mesh.faces.push_back(faceFirst);

                blib::graphics::Face faceSecond;
                faceSecond.indices = { base + 2, base + 3, base + 0 };
                mesh.faces.push_back(faceSecond);
            }
        }

        mesh.primitiveType = blib::graphics::PrimitiveType::Triangle;

        __blib_log_info("tileset: grid %ux%u, vertices: %u, faces: %u",
            tileGridSide, tileGridSide,
            static_cast<unsigned int>(mesh.vertices.size()),
            static_cast<unsigned int>(mesh.faces.size()));

        // Белая диффузная текстура 1x1: материал «запечёт» её при первом draw.
        // Color::White передаём НАПРЯМУЮ, а не через static-копию: копирование
        // статических Color на этапе статической инициализации попадает в
        // static initialization order fiasco (читает ещё не созданный
        // Color::White → чёрный цвет).
        blib::graphics::Image whiteImage;
        whiteImage.create(tileTextureSize, tileTextureSize, blib::graphics::Color::White);
        mesh.material.diffuseImage = whiteImage;
    }

    void IsometricTileset::draw(_In blib::graphics::IRenderTarget& target)
    {
        target.draw(mesh);
    }

} // namespace gravelands
