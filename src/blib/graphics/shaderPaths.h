#pragma once

#include <blib/config.h>

namespace blib
{
    namespace graphics
    {
        // ---------------------------------------------------------------
        // Пути к GLSL-шейдерам мешей. Вынесены из Mesh::bake(), чтобы
        // вшитые литералы не жили в коде (правило проекта: все строки
        // через именованные константы).
        //
        // TODO: заменить абсолютный путь на разрешение относительно
        // каталога исполняемого файла/рабочей директории — сейчас
        // вьювер работает только при запуске из корня репозитория.
        // ---------------------------------------------------------------
        constexpr const char* meshShaderBasePath = "M:\\Stuff\\src\\shaders\\mesh\\";
        constexpr const char* meshVertexShaderName = "MeshVertexShader.glsl";
        constexpr const char* skinMeshVertexShaderName = "SkinMeshVertexShader.glsl";
        constexpr const char* meshFragmentShaderName = "MeshFragmentShader.glsl";

        // Шейдеры отладочных линий (см. lineRenderer.h)
        constexpr const char* lineShaderBasePath = "M:\\Stuff\\src\\shaders\\line\\";
        constexpr const char* lineVertexShaderName = "LineVertexShader.glsl";
        constexpr const char* lineFragmentShaderName = "LineFragmentShader.glsl";
    }
}
