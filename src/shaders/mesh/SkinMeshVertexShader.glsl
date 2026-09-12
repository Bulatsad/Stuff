#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aTexCoord;
layout (location = 2) in ivec4 aBoneIds;
layout (location = 3) in vec4 aBoneWeights;
layout (location = 5) in vec3 aNormal;

out vec3 TexCoord;
out vec3 Normal;
out vec3 WorldPos;

uniform mat4 gModelMatrix;
uniform mat4 gViewMatrix;
uniform mat4 gProjectionMatrix;
const int maxBones = 100;
uniform mat4 gBones[maxBones];

void main()
{
    mat4 skinMatrix = aBoneWeights.x * gBones[aBoneIds.x]
                    + aBoneWeights.y * gBones[aBoneIds.y]
                    + aBoneWeights.z * gBones[aBoneIds.z]
                    + aBoneWeights.w * gBones[aBoneIds.w];

    vec4 skinnedPosition = skinMatrix * vec4(aPos, 1.0);
    vec4 worldPosition = gModelMatrix * skinnedPosition;
    gl_Position = gProjectionMatrix * gViewMatrix * worldPosition;
    TexCoord = aTexCoord;

    // Нормаль: скиннинг (линейная часть костной матрицы) + модель.
    // Ненормированная — нормализуется во фрагментном шейдере
    Normal = mat3(gModelMatrix) * mat3(skinMatrix) * aNormal;

    // Мировая позиция — для rim-light и будущего тумана
    WorldPos = worldPosition.xyz;
}
