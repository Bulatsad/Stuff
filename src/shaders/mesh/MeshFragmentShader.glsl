#version 330 core

in vec3 TexCoord;
in vec3 Normal;
in vec3 WorldPos;
out vec4 FragColor;

uniform sampler2D textureSampler;
uniform int gShowNormals;

// ---------------------------------------------------------------
// NPR-параметры материала (см. Material::apply):
// gShadingMode: 0 — unlit (чистый альбедо), 1 — мягкий toon
// ---------------------------------------------------------------
uniform int gShadingMode;
uniform float gRampSoftness;
uniform vec3 gRimColor;
uniform float gRimPower;
uniform vec3 gEmission;

// Свет сцены (см. RenderContext::sendLightsToShaderProgram).
// Все векторы — в мировых координатах
uniform vec3 gLightDir;
uniform vec3 gLightColor;
uniform vec3 gAmbientColor;
uniform vec3 gCameraPosition;

// Порог alpha-test (см. Material::m_alphaTest): пиксели с альфой
// ниже порога отбрасываются — жёсткие края рисованных плоскостей
// (SpritePlane), без полупрозрачного блендинга
uniform float gAlphaTest;

void main()
{
    vec4 texColor = texture(textureSampler, vec2(TexCoord));
    if (texColor.a < gAlphaTest)
    {
        discard;
    }

    vec3 albedo = texColor.rgb;

    vec3 color = albedo;

    if (gShadingMode == 1)
    {
        vec3 normal = normalize(Normal);
        vec3 lightDir = normalize(gLightDir);

        // Мягкий toon-ramp: smoothstep вокруг NdotL = 0.5.
        // Ширина ступени — gRampSoftness (малое значение — почти
        // жёсткий cel-переход, 0.5 — линейный градиент).
        // В отличие от Hades'овских пре-рендеренных спрайтов это
        // реальный свет: тени двигаются за источником
        float nDotL = dot(normal, lightDir);
        float ramp = smoothstep(0.5 - gRampSoftness, 0.5 + gRampSoftness, nDotL);

        vec3 diffuse = albedo * (gAmbientColor + gLightColor * ramp);

        // Контровый свет по силуэту: подчёркивает форму против света
        vec3 viewDir = normalize(gCameraPosition - WorldPos);
        float rim = pow(1.0 - max(dot(normal, viewDir), 0.0), gRimPower);

        color = diffuse + gRimColor * rim + gEmission;
    }
    else
    {
        // Unlit: чистое альбедо + свечение (тайлы, рисованные плоскости)
        color = albedo + gEmission;
    }

    // Отладочная раскраска нормалями: [-1, 1] → [0, 1]
    vec3 normalColor = normalize(Normal) * 0.5 + 0.5;
    color = mix(color, normalColor, float(gShowNormals));

    // Альфа из текстуры (нужна полупрозрачным объектам с блендингом —
    // blob-тени; для непрозрачных текстур alpha = 1)
    FragColor = vec4(color, texColor.a);
}
