#include "..\sphere.h"

void blib::graphics::Sphere::createSpere(float radius, buint32 pointPerCircle, blib::graphics::Color color)
{
    float step = radius / static_cast<float>(pointPerCircle);

    blib::graphics::Mesh& mesh = this->sphereMesh;

    // x^2 + y^2 + z^2 = r^2
    for (buint64 i = 1; i <= pointPerCircle; ++i)
    {

    }
}
