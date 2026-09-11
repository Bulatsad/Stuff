#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

out vec4 vertexColor;

uniform mat4 gModelMatrix;
uniform mat4 gViewMatrix;
uniform mat4 gProjectionMatrix;

void main()
{
    gl_Position = gProjectionMatrix * gViewMatrix * gModelMatrix * vec4(aPos, 1.0);
    vertexColor = aColor;
}
