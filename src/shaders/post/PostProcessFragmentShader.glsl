#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D gSceneColor;
uniform sampler2D gSceneDepth;

// Дымка (fog): тёмная тонировка по линейной глубине — Hades-фишка
// «даль тонет в темноте». Фон за пределами мира получает цвет дымки
uniform vec3 gFogColor;
uniform float gFogStart;
uniform float gFogEnd;

// Параметры перспективной проекции для линеаризации глубины
uniform float gNearPlane;
uniform float gFarPlane;

// Color grading: lift (тени) / gamma (середина) / gain (света)
uniform vec3 gLift;
uniform vec3 gGamma;
uniform vec3 gGain;

// Насыщенность (1.0 — как есть)
uniform float gSaturation;

// Сила виньетки (0 — выключена, 1 — максимальная)
uniform float gVignetteStrength;

// Выходная гамма: рендер линейный, 2.2 — перевод в sRGB
// (см. GRAPHICS.md, «Свет»)
uniform float gGammaOutput;

// Линеаризация ndc-глубины в дистанцию вида
float linearizeDepth(float depth)
{
    float ndc = depth * 2.0 - 1.0;
    return (2.0 * gNearPlane * gFarPlane) / (gFarPlane + gNearPlane - ndc * (gFarPlane - gNearPlane));
}

void main()
{
    vec3 color = texture(gSceneColor, TexCoord).rgb;

    // Дымка по глубине
    float depth = texture(gSceneDepth, TexCoord).r;
    float linearDepth = linearizeDepth(depth);
    float fogFactor = smoothstep(gFogStart, gFogEnd, linearDepth);
    color = mix(color, gFogColor, fogFactor);

    // Color grading: lift/gamma/gain (DaVinci-модель)
    color = gGain * pow(max(color + gLift, vec3(0.0)), gGamma);

    // Насыщенность (веса — ITU-R BT.709)
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, gSaturation);

    // Виньетка: затемнение к углам
    float dist = length(TexCoord - 0.5);
    color *= 1.0 - gVignetteStrength * smoothstep(0.25, 0.75, dist);

    // Линейный → sRGB
    color = pow(max(color, vec3(0.0)), vec3(1.0 / gGammaOutput));

    FragColor = vec4(color, 1.0);
}
