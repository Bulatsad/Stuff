#pragma once

#include <vector>

#include <assimp/scene.h>

#include <blib/config.h>
#include <blib/core/isaveloadable.h>
#include <blib/core/json/json.h>
#include <blib/graphics/drawable.h>
#include <blib/graphics/transformable.h>
#include <blib/graphics/skinmesh.h>
#include <blib/graphics/skelet.h>
#include <blib/graphics/animator.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api SkinModel : public blib::graphics::IDrawable, public blib::graphics::ITransformable,
            public blib::core::ISaveLoadable
        {
        public:
            bool loadFromAssimp(const aiScene* paiscene, const std::string& filename = std::string(), const aiScene* panimationScene = nullptr);
            void update(float deltaTimeMs);

            bool selectAnimation(const std::string& animationName);
            bool playAnimation();

            /**
             * Подмена мешей (skin) у существующего скелета. Новый файл
             * обязан содержать полностью совпадающий скелет
             * (Skelet::isCompatibleWith), иначе отказ. Скелет, аниматор
             * и состояние плейбека не трогаются; при ошибке модель
             * остаётся без изменений.
             *
             * @param force true — при несовместимом скелете не
             *        отказывать, а загружать как есть: веса костей,
             *        отсутствующих в текущем скелете, отбрасываются,
             *        остальные ренормализуются (SkinMesh::loadFromAssimpMesh)
             */
            bool replaceMeshesFromAssimp(_In const aiScene* paiscene, _In const std::string& filename, _In bool force = false);

            blib::graphics::Skelet& getSkelet();
            const blib::graphics::Skelet& getSkelet() const;
            blib::graphics::Animator& getAnimator();
            const blib::graphics::Animator& getAnimator() const;

            // Прямой доступ к мешам модели (настройка материалов —
            // NPR-параметры, контуры; инспекция). Структуру вектора
            // (число элементов/порядок) менять нельзя — подмена
            // скина идёт только через replaceMeshesFromAssimp
            std::vector<blib::graphics::SkinMesh>& getMeshes();
            const std::vector<blib::graphics::SkinMesh>& getMeshes() const;

            // IDrawable
            virtual void draw(blib::graphics::RenderContext& ctx) const override;

            // ========== ISaveLoadable: сериализация и сравнение ==========
            //
            // Формат toJson: {skelet, meshes[], animator} — ПОЛНОЕ
            // содержимое модели (кости, веса, геометрия, материалы с
            // битмапами диффуза, клипы анимации). GL-состояние мешей
            // (ctx/шейдеры/baked) не сериализуемо: fromJson пересоздаёт
            // меши (см. Mesh::fromJson), перезапись ЗАПЕЧЁННОЙ модели
            // требует живого RenderContext. Bone::node (aiNode*) —
            // контекст Assimp, после восстановления nullptr (рантайм
            // на нём не зависит). fromJson валидирует всё до применения.
            // Не прятать 1-аргументную точку входа строгого сравнения.

            using blib::core::IStrongComparable::strongCompare;

            blib::core::json::JsonValue toJson() const;
            blib::core::LoadStatus fromJson(_In const blib::core::json::JsonValue& json);
            blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;
            blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;
            bool strongCompare(_In const blib::core::IStrongComparable& other,
                _In blib::core::CompareSession& session) const __blib_override;
            bool verify() const __blib_override;

        private:
            mutable std::vector<blib::graphics::SkinMesh> meshes;
            blib::graphics::Skelet skelet;
            blib::graphics::Animator animator;
        };
    }
}
