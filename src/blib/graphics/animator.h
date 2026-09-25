#pragma once

#include <blib/config.h>

#include <assimp/scene.h>

#include <blib/core/isaveloadable.h>
#include <blib/core/json/json.h>
#include <blib/system/memory/iallocatorAware.h>
#include <blib/graphics/animationclip.h>
#include <blib/graphics/skelet.h>

#include <string>
#include <vector>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Animator : public blib::core::ISaveLoadable, public blib::memory::IAllocatorAware
        {
        private:
            std::vector<blib::graphics::AnimationClip> animationList;
            size_t currentAnimationIndex = 0;
            double currentTimeMs = 0.0;
            bool isPlaying = false;

        public:
            // Стабильное имя типа ресурса — тег кеша ресурсов
            // (ResourceManager; сравнение по содержимому, не по адресу)
            static constexpr const char* resourceTypeName = "blib.graphics.Animator";

            const std::vector<blib::graphics::AnimationClip>& getAnimations() const { return this->animationList; }
            const blib::graphics::AnimationClip* getCurrentAnimation() const;
            double getCurrentTimeMs() const { return this->currentTimeMs; }
            bool playing() const { return this->isPlaying; }

            // Скраб времени: принудительно выставить позицию в текущем
            // клипе (используется слайдером плейбека; позу нужно
            // переложить через update — см. SkinModel::update)
            void setCurrentTime(double timeMs) { this->currentTimeMs = timeMs; }

            // Включает/выключает зацикливание ТЕКУЩЕГО клипа
            // (при выключенном cycled время не оборачивается fmod'ом)
            void setCycled(bool cycled)
            {
                if (!(this->animationList.empty()))
                {
                    this->animationList[this->currentAnimationIndex].cycled = cycled;
                }
            }

            bool loadFromAssimp(const aiScene* paiscene);

            // Добавить клипы из внешней сцены к текущему списку, не
            // сбрасывая текущий клип/время/воспроизведение. fallbackName
            // используется для безымянных и дублирующихся клипов, а
            // также как имя единственного клипа файла (у Mixamo клипы
            // часто называются "mixamo.com" — имя файла информативнее).
            // Если задан skelet, каналы клипов привязываются к его
            // костям (см. Skelet::bindClipToSkeleton)
            bool appendFromAssimp(
                _In const aiScene* paiscene,
                _In const blib::graphics::Skelet* skelet,
                _In const std::string& fallbackName);

            bool selectAnimation(const std::string& animationName);
            bool play();
            bool pause();
            bool update(float deltaTimeMs);

            // ========== ISaveLoadable: сериализация и сравнение ==========
            //
            // Сериализуется весь список клипов + состояние плейбека
            // (индекс текущего клипа, время, isPlaying). Реализация —
            // impl/animator.cpp. Не прятать 1-аргументную точку входа
            // строгого сравнения.

            using blib::core::IStrongComparable::strongCompare;

            blib::core::json::JsonValue toJson() const;
            blib::core::LoadStatus fromJson(_In const blib::core::json::JsonValue& json);
            blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;
            blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;
            bool strongCompare(_In const blib::core::IStrongComparable& other,
                _In blib::core::CompareSession& session) const __blib_override;
            bool verify() const __blib_override;
        };
    }
}
