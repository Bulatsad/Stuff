#pragma once

#include <vector>

#include <blib/graphics/drawable.h>
#include <blib/graphics/transformable.h>

#include <beng/config.h>
#include <beng/graphics/skinmesh.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api SkinMesh : public blib::graphics::IDrawable, public blib::graphics::ITransformable
        {
            mutable std::vector<beng::graphics::SkinMesh> meshes;

        };
    }
}
