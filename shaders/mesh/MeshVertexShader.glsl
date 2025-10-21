#version 330 core

layout (location = 0) in vec3 aPos;
//layout (location = 1) in vec4 aColor;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
//out vec4 Color;

uniform mat4 gModelMatrix;
uniform mat4 gViewMatrix;
uniform mat4 gProjectionMatrix;

void main()
{
    gl_Position = gProjectionMatrix * gViewMatrix * gModelMatrix * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    //gl_Position = gProjectionMatrix * gViewMatrix * gModelMatrix * vec4(aPos, 1.0);
    //Color = aColor;
}
