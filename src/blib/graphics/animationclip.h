#pragma once

#include <blib/config.h>

#include <blib/graphics/vector.h>
#include <blib/math/quaternion.h>

#include <assimp/anim.h>

#include <vector>

namespace blib
{
    namespace graphics
    {
        class __blib_api AnimationChannel
        {
        public:
            template<class KeyT, class ValT>
            std::pair<size_t, size_t> findBorders(const std::vector<std::pair<KeyT, ValT> >& vec, const KeyT& currentKey)
            {
                if (vec.size() == 0)
                {
                    // TODO : Logging
                    // get from t-pose. not implemented
                    exit(1);
                }
                if (vec.size() == 1)
                {
                    return std::make_pair(0, 0);
                }
                
                if (currentKey <= vec[0].first) {
                    return std::make_pair(0, 0);
                }
                if (currentKey >= vec.back().first) {
                    size_t last = vec.size() - 1;
                    return std::make_pair(last, last);
                }

                for (size_t i = 0; i < vec.size() - 1; ++i)
                {
                    if (currentKey >= vec[i].first && currentKey <= vec[i + 1].first)
                    {
                        return std::make_pair(i, i + 1);
                    }
                }

                return std::make_pair(vec.size() - 2, vec.size() - 1);
            }

            std::string boneName;

            std::vector<std::pair<double/*time*/, blib::graphics::Vector3f      /*position*/> >positionKeys;
            std::vector<std::pair<double/*time*/, blib::math::Quaternion<double>/*rotation*/> >rotaionKeys;
            std::vector<std::pair<double/*time*/, blib::graphics::Vector3f      /*scale*/   > >scaleKeys;

            

            bool loadFromAssimp(const aiNodeAnim* panimationChannel)
            {
                this->boneName = panimationChannel->mNodeName.C_Str();

                positionKeys.resize(panimationChannel->mNumPositionKeys);
                rotaionKeys.resize(panimationChannel->mNumRotationKeys);
                scaleKeys.resize(panimationChannel->mNumScalingKeys);

                for (size_t i = 0; i < this->positionKeys.size(); ++i)
                {
                    if (!(this->positionKeys[i].second.loadFromAssimp(&(panimationChannel->mPositionKeys[i].mValue))))
                    {
                        // TODO : Logging
                        return false;
                    }
                    this->positionKeys[i].first = panimationChannel->mPositionKeys[i].mTime;
                }
                for (size_t i = 0; i < this->scaleKeys.size(); ++i)
                {
                    if (!(this->scaleKeys[i].second.loadFromAssimp(&(panimationChannel->mScalingKeys[i].mValue))))
                    {
                        // TODO : Logging
                        return false;
                    }
                    this->scaleKeys[i].first = panimationChannel->mScalingKeys[i].mTime;
                }
                for (size_t i = 0; i < this->rotaionKeys.size(); ++i)
                {
                    if (!(this->rotaionKeys[i].second.loadFromAssimp(&(panimationChannel->mRotationKeys[i].mValue))))
                    {
                        // TODO : Logging
                        return false;
                    }
                    this->rotaionKeys[i].first = panimationChannel->mRotationKeys[i].mTime;
                }

                return true;
            }
        };

        class __blib_api AnimationClip
        {
        public:
            std::string name;
            double tickPerSecond;
            double durationTicks;
            double durationMs;
            bool cycled = false;
            
            std::vector<blib::graphics::AnimationChannel> channels;

            bool loadFromAssimp(const aiAnimation* panim)
            {
                this->channels.resize(panim->mNumChannels);
                this->name = panim->mName.C_Str();
                this->durationTicks = panim->mDuration;
                this->tickPerSecond = panim->mTicksPerSecond;
                this->durationMs = this->durationTicks / this->tickPerSecond;

                for (size_t i = 0 ; i < this->channels.size(); ++i)
                {
                    if (!(this->channels[i].loadFromAssimp(panim->mChannels[i])))
                    {
                        // TODO : Logging
                        return false;
                    }
                }

                return true;
            }
        };
    }
}
