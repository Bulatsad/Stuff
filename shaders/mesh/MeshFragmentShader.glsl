#version 330 core

in vec3 TexCoord;
//in vec4 Color;
out vec4 FragColor;

uniform sampler2D textureSampler;

void main()
{
    FragColor = texture(textureSampler, vec2(TexCoord));
    //FragColor = vec4(1,1,1,0);
}
