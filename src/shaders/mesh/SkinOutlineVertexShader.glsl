#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in ivec4 aBoneIds;
layout (location = 3) in vec4 aBoneWeights;
layout (location = 5) in vec3 aNormal;

uniform mat4 gModelMatrix;
uniform mat4 gViewMatrix;
uniform mat4 gProjectionMatrix;
uniform float gOutlineWidth;

const int maxBones = 100;
uniform mat4 gBones[maxBones];

void main()
{
    mat4 skinMatrix = aBoneWeights.x * gBones[aBoneIds.x]
                    + aBoneWeights.y * gBones[aBoneIds.y]
                    + aBoneWeights.z * gBones[aBoneIds.z]
                    + aBoneWeights.w * gBones[aBoneIds.w];

    // Inverted hull со скиннингом: позиция и нормаль трансформируются
    // костями, затем позиция раздувается вдоль нормали
    vec4 skinnedPosition = skinMatrix * vec4(aPos, 1.0);
    vec3 skinnedNormal = normalize(mat3(skinMatrix) * aNormal);

    vec4 worldPosition = gModelMatrix * vec4(skinnedPosition.xyz + skinnedNormal * gOutlineWidth, 1.0);
    gl_Position = gProjectionMatrix * gViewMatrix * worldPosition;
}
