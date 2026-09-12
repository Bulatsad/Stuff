#version 330 core

layout (location = 0) in vec3 aPos;
//layout (location = 1) in vec4 aColor;
layout (location = 1) in vec3 aTexCoord;
layout (location = 5) in vec3 aNormal;

out vec3 TexCoord;
out vec3 Normal;
out vec3 WorldPos;
//out vec4 Color;

uniform mat4 gModelMatrix;
uniform mat4 gViewMatrix;
uniform mat4 gProjectionMatrix;

void main()
{
    vec4 worldPosition = gModelMatrix * vec4(aPos, 1.0);
    gl_Position = gProjectionMatrix * gViewMatrix * worldPosition;
    TexCoord = aTexCoord;
    // Нормаль в мировых координатах (mat3 — линейная часть модели;
    // для неоднородного масштаба понадобится inverse-transpose)
    Normal = mat3(gModelMatrix) * aNormal;
    // Мировая позиция — для rim-light и будущего тумана
    WorldPos = worldPosition.xyz;
    //gl_Position = gProjectionMatrix * gViewMatrix * gModelMatrix * vec4(aPos, 1.0);
    //Color = aColor;
}
