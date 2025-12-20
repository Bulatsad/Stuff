#pragma once

#include <vector>
#include <string>

#include <assimp/scene.h>

#include <beng/graphics/bone.h>
#include <beng/config.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api Skelet 
        {
        public:
            beng::graphics::Bone* root;

            bool loadFromAssimp(const aiMesh* paimesh);

            beng::graphics::Bone* find(const std::string& name);
            bool makeBoneTree(const aiNode* pbone);
            std::vector<beng::graphics::Bone>& getBoneStorage();
        private:
            std::vector<beng::graphics::Bone> boneStorage;
        };
    }
}
