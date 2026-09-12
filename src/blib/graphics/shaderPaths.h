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
        // Пути относительные: Shader::compile резолвит их относительно
        // рабочей директории → каталога exe → подъёмом по родителям
        // (candidate и src\candidate на каждом уровне) — приложения
        // работают из любого cwd (студия и деплой).
        // ---------------------------------------------------------------
        constexpr const char* meshShaderBasePath = "shaders\\mesh\\";
        constexpr const char* meshVertexShaderName = "MeshVertexShader.glsl";
        constexpr const char* skinMeshVertexShaderName = "SkinMeshVertexShader.glsl";
        constexpr const char* meshFragmentShaderName = "MeshFragmentShader.glsl";

        // Шейдеры отладочных линий (см. lineRenderer.h)
        constexpr const char* lineShaderBasePath = "shaders\\line\\";
        constexpr const char* lineVertexShaderName = "LineVertexShader.glsl";
        constexpr const char* lineFragmentShaderName = "LineFragmentShader.glsl";

        // Шейдеры пост-процессинга (см. postprocess.h)
        constexpr const char* postShaderBasePath = "shaders\\post\\";
        constexpr const char* postVertexShaderName = "PostProcessVertexShader.glsl";
        constexpr const char* postFragmentShaderName = "PostProcessFragmentShader.glsl";

        // Шейдеры контуров (inverted hull, см. material.h outline-параметры)
        constexpr const char* outlineVertexShaderName = "OutlineVertexShader.glsl";
        constexpr const char* skinOutlineVertexShaderName = "SkinOutlineVertexShader.glsl";
        constexpr const char* outlineFragmentShaderName = "OutlineFragmentShader.glsl";
    }
}
