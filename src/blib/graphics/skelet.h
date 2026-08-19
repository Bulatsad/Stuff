#pragma once

#include <vector>
#include <string>

#include <assimp/scene.h>

#include <blib/config.h>
#include <blib/graphics/bone.h>
#include <blib/graphics/animationclip.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Skelet 
        {
        public:
            blib::graphics::Bone* root = nullptr;

            bool loadFromAssimp(const aiMesh* paimesh);
            bool loadFromAssimp(const aiScene* paiscene);

            blib::graphics::Bone* find(const std::string& name);
            const blib::graphics::Bone* find(const std::string& name) const;
            size_t findBoneIndex(const std::string& name) const;

            bool makeBoneTree(const aiNode* pbone);
            bool loadDefaultPoseFromArmature(const aiNode* pbone);

            void applyClip(const blib::graphics::AnimationClip& clip, double timeTicks);

            std::vector<blib::graphics::Bone>& getBoneStorage();
            const std::vector<blib::graphics::Bone>& getBoneStorage() const;
            const std::vector<blib::graphics::TransformMatrix>& getFinalMatrices() const;

        private:
            std::vector<blib::graphics::Bone> boneStorage;
            std::vector<blib::graphics::TransformMatrix> finalMatrices;

            bool finishFromArmature(const aiNode* armature);
            void updateTransforms(blib::graphics::Bone* pbone);
        };
    }
}
