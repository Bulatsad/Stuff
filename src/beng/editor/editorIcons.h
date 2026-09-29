#pragma once

#include <beng/config.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

#include <imgui/imgui.h>

#include <string.h>

namespace beng
{
    namespace editor
    {
        /**
         * editorIcons — иконки UI эдитора: константы кодов и хелперы
         * отрисовки.
         *
         * Источник глифов — вендорный шрифт Material Design Icons
         * (MDPI v7.4.47, Apache 2.0; thirdparty/fonts). Каркас грузит
         * его ОТДЕЛЬНЫМ ImFont (не merge: у MDPI своя baseline, merge
         * смещает глифы — см. EditorApplication::reloadEditorFonts) —
         * указатель доступен через EditorApplication::getIconFont().
         *
         * КОСТЫЛЬ (см. BENG.md «Шрифты и иконки»): коды MDPI лежат за
         * пределами BMP (0xF0000+), поэтому ImGui собирается с
         * IMGUI_USE_WCHAR32 (задаётся из CMake — правка вендорного
         * imconfig.h запрещена правилами проекта). Строки глифов —
         * u8-литералы: UTF-8-байты гарантированы стандартом и не
         * зависят от кодировки исходников.
         */
        namespace icons
        {
            // --- PIE ---
            constexpr const char* play  = u8"\U000F040A";  // mdi-play
            constexpr const char* stop  = u8"\U000F04DB";  // mdi-stop

            // --- Инструменты gizmo (тулбар вьюпорта) ---
            constexpr const char* translate = u8"\U000F01BE";  // mdi-cursor-move
            constexpr const char* rotate    = u8"\U000F0464";  // mdi-rotate-3d-variant
            constexpr const char* scale     = u8"\U000F004C";  // mdi-arrow-expand-all

            // --- Сущности/компоненты (маппинг по componentTypeName) ---
            constexpr const char* entity           = u8"\U000F01A7";  // mdi-cube-outline
            constexpr const char* skinnedMesh      = u8"\U000F02E6";  // mdi-human
            constexpr const char* mesh             = u8"\U000F0832";  // mdi-shape-outline
            constexpr const char* directionalLight = u8"\U000F05A8";  // mdi-white-balance-sunny
            constexpr const char* ambientLight     = u8"\U000F06E9";  // mdi-lightbulb-on-outline
            constexpr const char* blobShadow       = u8"\U000F1853";  // mdi-circle-opacity
            constexpr const char* animator         = u8"\U000F05D8";  // mdi-animation
            constexpr const char* transform        = u8"\U000F0D49";  // mdi-axis-arrow

            // --- Прочее ---
            constexpr const char* info = u8"\U000F02FD";  // mdi-information-outline

            // Акцент/состояния (единый источник с палитрой темы —
            // см. applyEditorTheme в editorApplication.cpp)
            constexpr ImVec4 accent(0.13f, 0.59f, 0.95f, 1.00f);
            constexpr ImVec4 accentHovered(0.20f, 0.65f, 0.98f, 1.00f);
            constexpr ImVec4 accentActive(0.10f, 0.47f, 0.78f, 1.00f);
            constexpr ImVec4 activeText(1.00f, 1.00f, 1.00f, 1.00f);
            constexpr ImVec4 defaultText(0.90f, 0.90f, 0.90f, 1.00f);

            // Цвета иконок сущностей (по типу компонента — как в
            // референсных эдиторах: свет тёплый, геометрия холодная)
            constexpr ImVec4 meshColor(0.55f, 0.75f, 0.95f, 1.00f);
            constexpr ImVec4 lightColor(1.00f, 0.85f, 0.35f, 1.00f);
            constexpr ImVec4 ambientColor(0.95f, 0.75f, 0.45f, 1.00f);
            constexpr ImVec4 shadowColor(0.60f, 0.60f, 0.65f, 1.00f);
            constexpr ImVec4 animationColor(0.75f, 0.60f, 0.95f, 1.00f);

            // Стабильные имена типов компонентов (componentTypeName
            // движковых типов; игровые типы хостов сюда не входят —
            // им достаётся иконка по умолчанию)
            constexpr const char* typeNameTransform        = "beng.Transform";
            constexpr const char* typeNameSkinnedMesh      = "beng.SkinnedMesh";
            constexpr const char* typeNameMeshRender       = "beng.MeshRender";
            constexpr const char* typeNameDirectionalLight = "beng.DirectionalLight";
            constexpr const char* typeNameAmbientLight     = "beng.AmbientLight";
            constexpr const char* typeNameBlobShadow       = "beng.BlobShadow";
            constexpr const char* typeNameAnimator         = "beng.Animator";
        }

        /**
         * Иконка типа компонента: глиф + цвет + приоритет для выбора
         * иконки СУЩНОСТИ (в иерархии побеждает компонент с большим
         * приоритетом; 0 — иконки нет / не участвует).
         */
        struct ComponentIcon
        {
            const char* code;
            ImVec4 color;
            buint8 priority;
        };

        /**
         * Иконка по стабильному имени типа компонента (type-erased:
         * сцена даёт имя — маппинг живёт здесь). Неизвестный тип
         * (игровые компоненты) — code == nullptr (без иконки).
         */
        inline ComponentIcon iconForComponentType(_In_ const char* typeName)
        {
            using namespace icons;

            if (typeName == nullptr)
            {
                return ComponentIcon{ nullptr, icons::defaultText, 0 };
            }

            if (strcmp(typeName, icons::typeNameSkinnedMesh) == 0)
            {
                return ComponentIcon{ icons::skinnedMesh, icons::meshColor, 60 };
            }
            if (strcmp(typeName, icons::typeNameMeshRender) == 0)
            {
                return ComponentIcon{ icons::mesh, icons::meshColor, 50 };
            }
            if (strcmp(typeName, icons::typeNameDirectionalLight) == 0)
            {
                return ComponentIcon{ icons::directionalLight, icons::lightColor, 40 };
            }
            if (strcmp(typeName, icons::typeNameAmbientLight) == 0)
            {
                return ComponentIcon{ icons::ambientLight, icons::ambientColor, 35 };
            }
            if (strcmp(typeName, icons::typeNameAnimator) == 0)
            {
                return ComponentIcon{ icons::animator, icons::animationColor, 30 };
            }
            if (strcmp(typeName, icons::typeNameBlobShadow) == 0)
            {
                return ComponentIcon{ icons::blobShadow, icons::shadowColor, 20 };
            }
            if (strcmp(typeName, icons::typeNameTransform) == 0)
            {
                // Приоритет 0: иконка для заголовка в инспекторе, но
                // сущность «только с Transform» получает куб-фолбэк
                return ComponentIcon{ icons::transform, icons::defaultText, 0 };
            }

            return ComponentIcon{ nullptr, icons::defaultText, 0 };
        }

        /**
         * Отрисовка иконки у текущего курсора. iconFont == nullptr —
         * иконочный шрифт не загружен: ничего не рисуется (вызывающий
         * может не проверять). sizePx <= 0 — размер шрифта по умолчанию.
         */
        inline void drawIcon(
            _In_opt ImFont* iconFont,
            _In_ const char* iconCode,
            float sizePx = 0.0f,
            _In_ const ImVec4& color = icons::defaultText)
        {
            if (iconFont == nullptr || iconCode == nullptr)
            {
                return;
            }

            if (sizePx > 0.0f)
            {
                ImGui::PushFont(iconFont, sizePx);
            }
            else
            {
                ImGui::PushFont(iconFont);
            }
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(iconCode);
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }

        /**
         * Иконка-кнопка (квадратная: глиф по центру). active — элемент
         * выбран: фон — акцент темы, глиф белый (как активный инструмент
         * в референсных эдиторах); tooltip — подсказка при наведении
         * (nullptr — без подсказки). false без загруженного шрифта.
         */
        inline bool iconButton(
            _In_opt ImFont* iconFont,
            _In_ const char* iconCode,
            bool active = false,
            float sizePx = 0.0f,
            _In_opt const char* tooltip = nullptr)
        {
            if (iconFont == nullptr || iconCode == nullptr)
            {
                return false;
            }

            const ImGuiStyle& style = ImGui::GetStyle();
            const float iconSize = (sizePx > 0.0f) ? sizePx : ImGui::GetFontSize();
            const float buttonSide = iconSize + style.FramePadding.y * 2.0f;

            ImGui::PushID(iconCode);
            if (sizePx > 0.0f)
            {
                ImGui::PushFont(iconFont, sizePx);
            }
            else
            {
                ImGui::PushFont(iconFont);
            }
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, icons::accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, icons::accentHovered);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, icons::accentActive);
                ImGui::PushStyleColor(ImGuiCol_Text, icons::activeText);
            }

            const bool pressed = ImGui::Button(iconCode, ImVec2(buttonSide, buttonSide));

            if (active)
            {
                ImGui::PopStyleColor(4);
            }
            ImGui::PopFont();
            if (tooltip != nullptr && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", tooltip);
            }
            ImGui::PopID();
            return pressed;
        }

    } // namespace editor
} // namespace beng
