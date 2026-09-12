#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 5) in vec3 aNormal;

uniform mat4 gModelMatrix;
uniform mat4 gViewMatrix;
uniform mat4 gProjectionMatrix;
uniform float gOutlineWidth;

void main()
{
    // Inverted hull: позиция раздувается вдоль нормали. Рисуются
    // ТОЛЬКО задние грани (cull front) — вокруг объекта остаётся
    // «кайма» контура, не перекрывающая переднюю поверхность
    vec4 worldPosition = gModelMatrix * vec4(aPos + normalize(aNormal) * gOutlineWidth, 1.0);
    gl_Position = gProjectionMatrix * gViewMatrix * worldPosition;
}
