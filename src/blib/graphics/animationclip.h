#pragma once

#include <blib/config.h>

#include <blib/graphics/vector.h>
#include <blib/core/math/quaternion.h>
#include <blib/core/math/vector.h>

#include <assimp/anim.h>

#include <stdexcept>
#include <vector>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api AnimationChannel
        {
        public:
            template<class KeyT, class ValT>
            std::pair<size_t, size_t> findBorders(const std::vector<std::pair<KeyT, ValT> >& vec, const KeyT& currentKey) const
            {
                if (vec.size() == 0)
                {
                    throw std::runtime_error("Cannot find borders in empty key vector");
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

            static blib::graphics::Vector3f interpolateKeys(const blib::graphics::Vector3f& a, const blib::graphics::Vector3f& b, double t)
            {
                return blib::math::lerp(a, b, static_cast<float>(t));
            }

            static blib::math::Quaternion<double> interpolateKeys(const blib::math::Quaternion<double>& a, const blib::math::Quaternion<double>& b, double t)
            {
                return blib::math::nlerp(a, b, t);
            }

            template<class ValT>
            ValT sampleKey(const std::vector<std::pair<double, ValT> >& keys, double time) const
            {
                std::pair<size_t, size_t> borders = this->findBorders(keys, time);
                if (borders.first == borders.second)
                {
                    return keys[borders.first].second;
                }

                double t0 = keys[borders.first].first;
                double t1 = keys[borders.second].first;
                double t = (t1 > t0) ? ((time - t0) / (t1 - t0)) : 0.0;

                return AnimationChannel::interpolateKeys(keys[borders.first].second, keys[borders.second].second, t);
            }

            bool sample(double time, blib::graphics::Vector3f& position, blib::math::Quaternion<double>& rotation, blib::graphics::Vector3f& scale) const
            {
                position = blib::graphics::Vector3f(0, 0, 0);
                rotation = blib::math::Quaternion<double>(1, 0, 0, 0);
                scale = blib::graphics::Vector3f(1, 1, 1);

                if (!(this->positionKeys.empty()))
                    position = this->sampleKey(this->positionKeys, time);
                if (!(this->rotaionKeys.empty()))
                    rotation = this->sampleKey(this->rotaionKeys, time);
                if (!(this->scaleKeys.empty()))
                    scale = this->sampleKey(this->scaleKeys, time);

                return true;
            }

            std::string boneName;

            std::vector<std::pair<double/*time*/, blib::graphics::Vector3f      /*position*/ > >positionKeys;
            std::vector<std::pair<double/*time*/, blib::math::Quaternion<double>/*rotation*/ > >rotaionKeys;
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

        class __blib_graphics_api AnimationClip
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
                this->durationMs = (this->tickPerSecond > 0.0) ? ((this->durationTicks / this->tickPerSecond) * 1000.0) : 0.0;
                this->cycled = true;

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
