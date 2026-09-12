#version 330 core

uniform vec3 gOutlineColor;

out vec4 FragColor;

void main()
{
    // Контур — плоский цвет из материала (по умолчанию тёмный,
    // Hades-стайл: тонкая тёмная кайма)
    FragColor = vec4(gOutlineColor, 1.0);
}
