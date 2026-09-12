#version 330 core

out vec2 TexCoord;

void main()
{
    // Полноэкранный треугольник из gl_VertexID (без VBO):
    // три вершины покрывают весь экран, атрибутов нет
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 pos = positions[gl_VertexID];
    gl_Position = vec4(pos, 0.0, 1.0);
    TexCoord = pos * 0.5 + 0.5;
}
