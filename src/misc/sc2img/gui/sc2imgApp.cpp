#include <sc2img/gui/sc2imgApp.h>

#include <sc2img/core/imageExport.h>

#include <beng/editor/panels/iPanel.h>

#include <blib/core/console/console.h>
#include <blib/graphics/color.h>
#include <blib/graphics/impl/win/winRenderWindowUtil.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/renderWindow.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/texture.h>
#include <blib/system/memory/globalAllocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>

#include <imgui/imgui.h>

#include <Windows.h>
#include <commdlg.h>

#include <string>
#include <vector>

namespace sc2img
{
    namespace
    {
        // ---------------------------------------------------------------
        // Константы утилиты (правило проекта: без вшитых литералов)
        // ---------------------------------------------------------------

        // Окно (передаётся каркасу в initialize)
        constexpr uint16_t windowWidth = 1280;
        constexpr uint16_t windowHeight = 720;
        constexpr const char* windowTitle = "sc2img - Stronghold Image Converter";

        // Верхняя панель: число строк контролов (высоту каркас считает
        // из метрик шрифта ImGui — см. setHostBarRows)
        constexpr buint32 toolBarRows = 3;

        // Размеры буферов ввода верхней панели
        constexpr size_t pathBufferSize = 512;
        constexpr size_t outDirBufferSize = 512;

        // Строки UI
        constexpr const char* toolBarTitle = "File";
        constexpr const char* tabName = "Images";
        constexpr const char* pathInputLabel = "Path";
        constexpr const char* openButtonLabel = "Open";
        constexpr const char* browseButtonLabel = "Browse...";
        constexpr const char* formatComboLabel = "Format";
        constexpr const char* alignCheckboxLabel = "Align by offsets";
        constexpr const char* playerColorCheckboxLabel = "Player color";
        constexpr const char* playerColorInputLabel = "##playerColor";
        constexpr const char* outDirInputLabel = "Out dir";
        constexpr const char* exportSelectedButtonLabel = "Export Selected";
        constexpr const char* exportAllButtonLabel = "Export All";
        constexpr const char* selectAllButtonLabel = "Select All";
        constexpr const char* selectNoneButtonLabel = "Select None";
        constexpr const char* noDocumentText = "No file loaded. Open a .tgx/.gm1 (Ctrl+O).";
        constexpr const char* zoomSliderLabel = "Zoom";
        constexpr const char* frameCounterText = "frame %u / %u";

        constexpr const char* fileDialogTitle = "Open Stronghold image";
        constexpr const char* fileFilter =
            "Stronghold Images (*.gm1;*.tgx)\0*.gm1;*.tgx\0"
            "GM1 File (*.gm1)\0*.gm1\0"
            "TGX File (*.tgx)\0*.tgx\0"
            "All Files (*.*)\0*.*\0";

        // Имена форматов комбо-бокса (индексы = OutputFormat)
        const char* formatNames[] = { "PNG", "BMP", "JPEG" };
        constexpr buint32 formatNameCount = 3;

        // Резерв ширины под группы кнопок верхней панели (путь +
        // Open/Browse; папка вывода + экспорт) — в строках шрифта ImGui
        constexpr float toolBarButtonsWidthRows = 10.0f;

        // Ширины контролов верхней панели (в строках шрифта ImGui)
        constexpr float formatComboWidthRows = 5.0f;
        constexpr float playerColorInputWidthRows = 3.5f;

        // ------------------------------------------------
        // Раскладка вкладки: размеры — доли области и строки шрифта
        // ImGui (без пиксельных констант; см. план полировки)
        // ------------------------------------------------

        // Ширина панели превью — доля ширины вкладки (и минимум в строках)
        constexpr float previewPaneWidthFraction = 0.38f;
        constexpr float previewPaneMinWidthRows = 12.0f;

        // Минимальная ширина сетки-контактника (в строках шрифта) —
        // превью ужимается не сильнее, чем до этого порога
        constexpr float gridMinWidthRows = 8.0f;

        // Ширина слайдера зума (в строках шрифта)
        constexpr float zoomSliderWidthRows = 6.0f;

        // Превью: лимиты зума
        constexpr float previewZoomMin = 0.25f;
        constexpr float previewZoomMax = 8.0f;

        // Шахматный фон превью (клетка, px; два серых)
        constexpr float checkerCellSize = 8.0f;
        constexpr buint8 checkerColorLightComponent = 150;
        constexpr buint8 checkerColorDarkComponent = 110;
        constexpr buint8 checkerAlphaComponent = 255;

        // Максимум цветовых наборов игроков в палитре GM1
        constexpr buint32 maxPlayerColorIndex = 31;
    }

    /**
     * Каталог файла (без имени). Нет слэша — текущий каталог ".".
     * Понадобится для дефолтной папки вывода экспорта.
     */
    namespace
    {
        std::string dirnameOf(_In const std::string& path)
        {
            const size_t lastSlash = path.find_last_of("\\/");
            if (lastSlash == std::string::npos)
            {
                return std::string(".");
            }
            if (lastSlash == 0)
            {
                return path.substr(0, 1);
            }
            return path.substr(0, lastSlash);
        }
    }

    // ---------------------------------------------------------------
    // Внутренности утилиты: документ, GL-миниатюры, маска выбора,
    // центральная вкладка. Полные определения скрыты в .cpp (pimpl) —
    // заголовок не тянет графические типы в потребителей.
    // ---------------------------------------------------------------

    struct Sc2imgApp::Sc2imgAppImpl
    {
        // Аллокатор служебных контейнеров. Объявлен ПЕРЕД векторами:
        // адаптеры хранят на него указатель (канон из scene.h)
        blib::memory::Allocator containerAllocator;

        // Документ (CPU-данные: изображения + метаданные)
        Sc2imgDocument document;

        // GL-миниатюры (по одной на изображение; 0 — не создана)
        std::vector<blib::graphics::Texture, blib::memory::StdAllocatorAdapter<blib::graphics::Texture>> thumbnails;

        // Маска выбора изображений на экспорт (1 — выбрано)
        std::vector<buint8, blib::memory::StdAllocatorAdapter<buint8>> selectedMask;

        // Индекс изображения в превью (правой части вкладки)
        buint32 previewIndex;
        float previewZoom;

        // Запрос прокрутки сетки к previewIndex (ставится стрелками;
        // потребляется при отрисовке ячейки предпросмотра)
        bool scrollToPreview;

        // Центральная вкладка «Images» (создаётся в initialize,
        // живёт до shutdown; каркас хранит только указатель)
        ImagesTab* imagesTab;

        // Буферы ввода верхней панели
        char pathBuffer[pathBufferSize];
        char outDirBuffer[outDirBufferSize];

        // Опции экспорта
        buint8 formatIndex;         // 0=PNG, 1=BMP, 2=JPEG (OutputFormat)
        bool alignByOffsets;        // выравнивание кадров по offsets
        bool playerColorOverride;   // применить выбранный цвет игрока
        buint32 playerColor;        // индекс цветового набора игрока

        Sc2imgAppImpl()
            : containerAllocator()
            , document()
            , thumbnails{ blib::memory::StdAllocatorAdapter<blib::graphics::Texture>(&containerAllocator) }
            , selectedMask{ blib::memory::StdAllocatorAdapter<buint8>(&containerAllocator) }
            , previewIndex(0)
            , previewZoom(1.0f)
            , scrollToPreview(false)
            , imagesTab(nullptr)
            , formatIndex(0)
            , alignByOffsets(false)
            , playerColorOverride(false)
            , playerColor(0)
        {
            // Пустые буферы (иначе — мусор в полях ввода)
            memset(this->pathBuffer, 0, pathBufferSize);
            memset(this->outDirBuffer, 0, outDirBufferSize);
        }
    };

    /**
     * Центральная вкладка «Images» (контракт ICenterTabView) —
     * заменяет содержимое вкладки «Scene» каркаса (setSceneTabView):
     * сетка-контактник миниатюр (выкладка в духе Stronghold Image
     * Toolbox) с выбором на экспорт и превью выбранного кадра с
     * шахматным фоном прозрачности. Стрелки листают кадры.
     */
    class Sc2imgApp::ImagesTab : public beng::editor::ICenterTabView
    {
    private:
        Sc2imgAppImpl* host;  // состояние утилиты (не владеет)

        /**
         * Шахматный фон под превью (ImDrawList): клетки фиксированного
         * размера, светлый/тёмный серый — визуализация прозрачности.
         */
        static void drawCheckerBackground(_In const ImVec2& topLeft, _In const ImVec2& size);

    public:
        explicit ImagesTab(_In Sc2imgAppImpl* state)
            : host(state)
        {
        }

        void drawContents() __blib_override;
        const char* getTabName() const __blib_override;
    };

    // ---------------------------------------------------------------
    // ImagesTab
    // ---------------------------------------------------------------

    const char* Sc2imgApp::ImagesTab::getTabName() const
    {
        return tabName;
    }

    void Sc2imgApp::ImagesTab::drawCheckerBackground(_In const ImVec2& topLeft, _In const ImVec2& size)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const ImU32 light = IM_COL32(
            checkerColorLightComponent, checkerColorLightComponent, checkerColorLightComponent, checkerAlphaComponent);
        const ImU32 dark = IM_COL32(
            checkerColorDarkComponent, checkerColorDarkComponent, checkerColorDarkComponent, checkerAlphaComponent);

        const float right = topLeft.x + size.x;
        const float bottom = topLeft.y + size.y;
        bool evenCell = true;

        for (float y = topLeft.y; y < bottom; y += checkerCellSize)
        {
            bool cellIsLight = evenCell;
            for (float x = topLeft.x; x < right; x += checkerCellSize)
            {
                const float cellRight = (x + checkerCellSize < right) ? x + checkerCellSize : right;
                const float cellBottom = (y + checkerCellSize < bottom) ? y + checkerCellSize : bottom;
                drawList->AddRectFilled(
                    ImVec2(x, y), ImVec2(cellRight, cellBottom), cellIsLight ? light : dark);
                cellIsLight = !cellIsLight;
            }
            evenCell = !evenCell;
        }
    }

    void Sc2imgApp::ImagesTab::drawContents()
    {
        Sc2imgAppImpl& state = *this->host;

        if (state.document.isEmpty())
        {
            ImGui::TextUnformatted(noDocumentText);
            return;
        }

        const buint32 count = state.document.getImageCount();
        const ImGuiStyle& style = ImGui::GetStyle();
        const float rowHeight = ImGui::GetFrameHeight();

        // ---------------------------------------------------------------
        // Стрелки: листание кадров (быстрый просмотр анимации).
        // Вниз/вправо — следующий, вверх/влево — предыдущий; удержание
        // листает с автоповтором. Пока фокус в поле ввода (путь и т.п.) —
        // клавиши не перехватываем
        // ---------------------------------------------------------------
        if (!ImGui::GetIO().WantTextInput)
        {
            const bool nextPressed =
                ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) ||
                ImGui::IsKeyPressed(ImGuiKey_RightArrow, true);
            const bool previousPressed =
                ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) ||
                ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true);

            if (nextPressed && state.previewIndex + 1 < count)
            {
                ++state.previewIndex;
                state.scrollToPreview = true;
            }
            else if (previousPressed && state.previewIndex > 0)
            {
                --state.previewIndex;
                state.scrollToPreview = true;
            }
        }

        // Сводка: имя файла, количество, текущий кадр + быстрый выбор
        ImGui::Text("%s: %u image(s)", state.document.getFilePath().c_str(), count);
        ImGui::SameLine();
        ImGui::Text(frameCounterText, state.previewIndex + 1, count);
        ImGui::SameLine();

        if (ImGui::Button(selectAllButtonLabel))
        {
            for (buint32 i = 0; i < count; ++i)
            {
                state.selectedMask[i] = 1;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(selectNoneButtonLabel))
        {
            for (buint32 i = 0; i < count; ++i)
            {
                state.selectedMask[i] = 0;
            }
        }
        ImGui::Spacing();

        // ---------------------------------------------------------------
        // Раскладка области: сетка-контактник слева + превью справа.
        // Размеры — доли ширины области и строки шрифта ImGui (без
        // пиксельных констант)
        // ---------------------------------------------------------------

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float previewMinWidth = rowHeight * previewPaneMinWidthRows;
        const float gridMinWidth = rowHeight * gridMinWidthRows;

        float previewWidth = availableWidth * previewPaneWidthFraction;
        if (previewWidth < previewMinWidth)
        {
            previewWidth = previewMinWidth;
        }

        // Не даём превью выдавить сетку уже минимальной ширины
        const float maxPreviewWidth = availableWidth - gridMinWidth - style.ItemSpacing.x;
        if (maxPreviewWidth > previewMinWidth && previewWidth > maxPreviewWidth)
        {
            previewWidth = maxPreviewWidth;
        }

        float gridWidth = availableWidth - previewWidth - style.ItemSpacing.x;
        if (gridWidth < rowHeight)
        {
            gridWidth = rowHeight;
        }

        // ---------------------------------------------------------------
        // Сетка-контактник (как PictureFlowBox Stronghold Image Toolbox):
        // ячейка = нативная картинка 1:1 (без масштабирования и
        // квадратного растяжения), сверху — компактный чекбокс выбора с
        // номером исходного кадра; предпросматриваемый кадр подсвечен
        // акцентом темы
        // ---------------------------------------------------------------

        ImGui::BeginChild("##imageGrid", ImVec2(gridWidth, 0.0f), true,
            ImGuiWindowFlags_HorizontalScrollbar);

        for (buint32 i = 0; i < count; ++i)
        {
            const blib::graphics::Texture& thumbnail = state.thumbnails[i];
            const ImageEntry& entry = state.document.getEntry(i);
            const bool isPreview = (i == state.previewIndex);

            // Размер ячейки — нативный размер изображения (как в Toolbox:
            // тайлы 30x16 не искажаются под квадрат)
            const float imageWidth = static_cast<float>(entry.image.width);
            const float imageHeight = static_cast<float>(entry.image.height);

            // Фон предпросмотра рисуется ПОД элементами ячейки: канал 0 —
            // фон, канал 1 — элементы (порядок draw list)
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->ChannelsSplit(2);
            drawList->ChannelsSetCurrent(1);

            ImGui::BeginGroup();

            // Выбор на экспорт + номер исходного кадра файла
            bool selected = state.selectedMask[i] != 0;
            ImGui::Checkbox(("##select" + std::to_string(i)).c_str(), &selected);
            state.selectedMask[i] = selected ? 1 : 0;
            ImGui::SameLine();
            ImGui::Text("#%u", entry.sourceIndex);

            // Кликабельная миниатюра — на превью (зум сбрасывается)
            if (thumbnail.getContext().textureID != 0)
            {
                if (ImGui::ImageButton(("##thumb" + std::to_string(i)).c_str(),
                    static_cast<ImTextureID>(thumbnail.getContext().textureID),
                    ImVec2(imageWidth, imageHeight)))
                {
                    state.previewIndex = i;
                    state.previewZoom = 1.0f;
                }
            }
            else
            {
                // Текстура не создалась (ошибка GL): пустое место
                ImGui::InvisibleButton(("##thumb" + std::to_string(i)).c_str(),
                    ImVec2(imageWidth, imageHeight));
            }

            ImGui::EndGroup();

            // Фон ячейки предпросмотра — в задний канал (под элементами)
            if (isPreview)
            {
                const ImVec2 cellMin = ImGui::GetItemRectMin();
                const ImVec2 cellMax = ImGui::GetItemRectMax();
                drawList->ChannelsSetCurrent(0);
                drawList->AddRectFilled(
                    ImVec2(cellMin.x - style.FramePadding.x, cellMin.y - style.FramePadding.y),
                    ImVec2(cellMax.x + style.FramePadding.x, cellMax.y + style.FramePadding.y),
                    ImGui::GetColorU32(ImGuiCol_Header), style.FrameRounding);
                drawList->ChannelsSetCurrent(1);
            }

            drawList->ChannelsMerge();

            // Прокрутка сетки к кадру, выбранному стрелками
            if (isPreview && state.scrollToPreview)
            {
                ImGui::SetScrollHereY(0.5f);
                state.scrollToPreview = false;
            }

            // Перенос строки: следующий кадр ставим на ту же линию,
            // только если он целиком влезает в видимую ширину сетки
            // (иначе обычный перенос курсора на новую строку)
            if (i + 1 < count)
            {
                const ImageEntry& nextEntry = state.document.getEntry(i + 1);
                const float nextWidth = static_cast<float>(nextEntry.image.width) +
                    style.FramePadding.x * 2.0f;
                const float rowRightEdge = ImGui::GetWindowPos().x +
                    ImGui::GetWindowContentRegionMax().x;

                if (ImGui::GetItemRectMax().x + style.ItemSpacing.x + nextWidth <= rowRightEdge)
                {
                    ImGui::SameLine();
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // ---------------------------------------------------------------
        // Правая колонка: превью выбранного + метаданные
        // ---------------------------------------------------------------

        ImGui::BeginChild("##previewPane", ImVec2(previewWidth, 0.0f), true);

        if (state.previewIndex < count && state.thumbnails[state.previewIndex].getContext().textureID != 0)
        {
            const ImageEntry& entry = state.document.getEntry(state.previewIndex);
            const float scaledWidth = entry.image.width * state.previewZoom;
            const float scaledHeight = entry.image.height * state.previewZoom;

            ImGui::Text(frameCounterText, state.previewIndex + 1, count);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(rowHeight * zoomSliderWidthRows);
            ImGui::SliderFloat(zoomSliderLabel, &state.previewZoom, previewZoomMin, previewZoomMax, "%.2f");

            const ImVec2 previewTopLeft = ImGui::GetCursorScreenPos();
            drawCheckerBackground(previewTopLeft, ImVec2(scaledWidth, scaledHeight));
            ImGui::Image(
                static_cast<ImTextureID>(state.thumbnails[state.previewIndex].getContext().textureID),
                ImVec2(scaledWidth, scaledHeight));

            // Метаданные кадра (заголовок GM1)
            ImGui::Spacing();
            ImGui::Text("size: %ux%u",
                static_cast<buint32>(entry.image.width), static_cast<buint32>(entry.image.height));
            ImGui::Text("offset: %u, %u",
                static_cast<buint32>(entry.horizontalOffset), static_cast<buint32>(entry.verticalOffset));
            ImGui::Text("part: %u / %u",
                static_cast<buint32>(entry.part), static_cast<buint32>(entry.subparts));
            ImGui::Text("base height: %u", static_cast<buint32>(entry.baseHeight));
            ImGui::Text("direction: %u", static_cast<buint32>(entry.direction));
            ImGui::Text("performance id: %u", static_cast<buint32>(entry.performanceId));
        }
        else
        {
            ImGui::TextUnformatted("No preview");
        }

        ImGui::EndChild();
    }

    // ---------------------------------------------------------------
    // Хуки EditorApplication
    // ---------------------------------------------------------------

    void Sc2imgApp::onInput()
    {
        // Ctrl+O — диалог выбора файла (паттерн model_viewer)
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::O) &&
            blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LControl))
        {
            this->browseImageFile();
        }
    }

    void Sc2imgApp::onUi()
    {
        // Верхняя полоса: позицию/размер выставил каркас перед onUi
        this->drawToolBar();
    }

    // ---------------------------------------------------------------
    // Жизненный цикл
    // ---------------------------------------------------------------

    Sc2imgApp::Sc2imgApp()
        : impl(nullptr)
    {
    }

    Sc2imgApp::~Sc2imgApp()
    {
        // Страховка: если владелец не вызвал shutdown явно
        this->shutdown();
    }

    bool Sc2imgApp::initialize(
        _In uint16_t windowWidth, _In uint16_t windowHeight, _In const char* windowTitle)
    {
        (void)windowWidth;
        (void)windowHeight;
        (void)windowTitle;

        auto& globalAllocator = blib::memory::GlobalAllocator::instance();

        // Настройки каркаса ДО его инициализации:
        // - сценные панели (Hierarchy/Inspector) не нужны;
        // - вкладка «Scene» (3D-вьюпорт) подменяется вкладкой «Images»;
        // - верхняя панель хоста — три строки контролов
        this->setScenePanelsEnabled(false);

        // Аллокация через GlobalAllocator + placement new (проектное
        // правило: выделяющие new/delete запрещены). impl и вкладка
        // создаются ДО каркаса: setSceneTabView() обязан получить
        // указатель до initialize()
        this->impl = static_cast<Sc2imgAppImpl*>(globalAllocator.allocate(sizeof(Sc2imgAppImpl)));
        new (this->impl) Sc2imgAppImpl();

        this->impl->imagesTab = static_cast<ImagesTab*>(globalAllocator.allocate(sizeof(ImagesTab)));
        new (this->impl->imagesTab) ImagesTab(this->impl);

        this->setSceneTabView(this->impl->imagesTab);
        this->setHostBarRows(toolBarRows);

        // Окно — собственные параметры утилиты (инструмент, не плагин;
        // квалифицированы по namespace — параметры их затеняют)
        return this->EditorApplication::initialize(
            sc2img::windowWidth, sc2img::windowHeight, sc2img::windowTitle);
    }

    void Sc2imgApp::shutdown()
    {
        if (this->impl == nullptr)
        {
            return;
        }

        // GL-текстуры миниатюр освобождаем ДО гашения каркаса:
        // GL-контекст обязан их пережить
        blib::graphics::RenderContext& rc = this->getRenderTarget().rc;
        for (blib::graphics::Texture& thumbnail : this->impl->thumbnails)
        {
            thumbnail.free(rc);
        }

        // Вкладка и impl разрушаются ДО каркаса (ImGui-контекст жив)
        this->impl->imagesTab->~ImagesTab();
        auto& globalAllocator = blib::memory::GlobalAllocator::instance();
        globalAllocator.deallocate(this->impl->imagesTab, sizeof(ImagesTab));

        this->impl->~Sc2imgAppImpl();
        globalAllocator.deallocate(this->impl, sizeof(Sc2imgAppImpl));
        this->impl = nullptr;

        // Каркас: ImGui, окно, сцена
        this->EditorApplication::shutdown();
    }

    // ---------------------------------------------------------------
    // Загрузка и экспорт
    // ---------------------------------------------------------------

    void Sc2imgApp::loadDocument(_In const std::string& path)
    {
        const Sc2imgError error = this->impl->document.load(
            path, this->impl->playerColorOverride,
            static_cast<buint8>(this->impl->playerColor));
        if (__blib_unlikely(error != Sc2imgError::None))
        {
            return;
        }

        // Дефолтная папка вывода — каталог входного файла
        // (если пользователь ещё не указал свою)
        if (this->impl->outDirBuffer[0] == '\0')
        {
            const std::string dir = dirnameOf(path);
            strncpy_s(this->impl->outDirBuffer, outDirBufferSize, dir.c_str(), _TRUNCATE);
        }

        this->rebuildThumbnails();
    }

    void Sc2imgApp::rebuildThumbnails()
    {
        blib::graphics::RenderContext& rc = this->getRenderTarget().rc;

        // Старые GL-текстуры — free, затем пересборка под новый документ
        for (blib::graphics::Texture& thumbnail : this->impl->thumbnails)
        {
            thumbnail.free(rc);
        }
        this->impl->thumbnails.clear();
        this->impl->selectedMask.clear();

        const buint32 count = this->impl->document.getImageCount();
        this->impl->thumbnails.resize(count);
        this->impl->selectedMask.resize(count, 1);

        for (buint32 i = 0; i < count; ++i)
        {
            const blib::graphics::TextureError textureError = this->impl->thumbnails[i].create(
                this->impl->document.getEntry(i).image, rc,
                blib::graphics::Texture::genFlags::clamp_to_edge);
            if (__blib_unlikely(textureError != blib::graphics::TextureError::None))
            {
                __blib_log_warning("sc2img: failed to create thumbnail #%u", i);
            }
        }

        this->impl->previewIndex = 0;
        this->impl->previewZoom = 1.0f;
    }

    void Sc2imgApp::exportImages(_In_opt const buint8* selectedMask)
    {
        ExportOptions options;
        options.format = static_cast<OutputFormat>(this->impl->formatIndex);
        options.alignByOffsets = this->impl->alignByOffsets;

        buint32 exportedCount = 0;
        const Sc2imgError error = exportAll(
            this->impl->document, std::string(this->impl->outDirBuffer),
            options, selectedMask, exportedCount);
        if (__blib_unlikely(error != Sc2imgError::None))
        {
            __blib_log_warning("sc2img: export failed (%u)", static_cast<buint32>(error));
        }
    }

    void Sc2imgApp::openFile(_In const std::string& path)
    {
        if (__blib_unlikely(this->impl == nullptr))
        {
            return;
        }

        strncpy_s(this->impl->pathBuffer, pathBufferSize, path.c_str(), _TRUNCATE);
        this->loadDocument(path);
    }

    // ---------------------------------------------------------------
    // Верхняя панель
    // ---------------------------------------------------------------

    void Sc2imgApp::drawToolBar()
    {
        Sc2imgAppImpl& state = *this->impl;

        // Без шапки и скроллбара — полоса целиком контент (паттерн
        // drawModelBar вьювера: шапка съедает высоту бара)
        if (ImGui::Begin(toolBarTitle, nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar))
        {
            const float rowHeight = ImGui::GetFrameHeight();

            // Строка 1: путь к файлу + Open/Browse
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - rowHeight * toolBarButtonsWidthRows);
            const bool enterPressed = ImGui::InputText(
                pathInputLabel, state.pathBuffer, pathBufferSize,
                ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();
            const bool openClicked = ImGui::Button(openButtonLabel);

            ImGui::SameLine();
            const bool browseClicked = ImGui::Button(browseButtonLabel);

            if ((enterPressed || openClicked) && state.pathBuffer[0] != '\0')
            {
                this->loadDocument(std::string(state.pathBuffer));
            }
            if (browseClicked)
            {
                this->browseImageFile();
            }

            // Строка 2: опции экспорта
            buint32 formatIndex = state.formatIndex;
            ImGui::SetNextItemWidth(rowHeight * formatComboWidthRows);
            ImGui::Combo(formatComboLabel, reinterpret_cast<int*>(&formatIndex),
                formatNames, static_cast<int>(formatNameCount));
            state.formatIndex = static_cast<buint8>(formatIndex);

            ImGui::SameLine();
            ImGui::Checkbox(alignCheckboxLabel, &state.alignByOffsets);

            // Цвет игрока: изменение чекбокса/значения — перезагрузка
            // документа (палитра GM1 сегментирована по игрокам)
            bool playerColorChanged = false;
            ImGui::SameLine();
            playerColorChanged |= ImGui::Checkbox(playerColorCheckboxLabel, &state.playerColorOverride);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(rowHeight * playerColorInputWidthRows);
            if (ImGui::InputScalar(playerColorInputLabel, ImGuiDataType_U32,
                &state.playerColor, nullptr, nullptr, "%u"))
            {
                playerColorChanged = true;
            }
            if (state.playerColor > maxPlayerColorIndex)
            {
                state.playerColor = maxPlayerColorIndex;
            }

            if (playerColorChanged && !state.document.isEmpty())
            {
                this->loadDocument(state.document.getFilePath());
            }

            // Строка 3: папка вывода + экспорт
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - rowHeight * toolBarButtonsWidthRows);
            ImGui::InputText(outDirInputLabel, state.outDirBuffer, outDirBufferSize);

            ImGui::SameLine();
            const bool exportSelectedClicked = ImGui::Button(exportSelectedButtonLabel);

            ImGui::SameLine();
            const bool exportAllClicked = ImGui::Button(exportAllButtonLabel);

            if (exportSelectedClicked || exportAllClicked)
            {
                if (state.document.isEmpty())
                {
                    __blib_log_warning("sc2img: nothing loaded to export");
                }
                else
                {
                    this->exportImages(exportSelectedClicked ? state.selectedMask.data() : nullptr);
                }
            }
        }
        ImGui::End();
    }

    // ---------------------------------------------------------------
    // Диалог выбора файла (Win32)
    // ---------------------------------------------------------------

    bool Sc2imgApp::browseFile(_In const char* title, _In const char* filter, _Out char* outPath, size_t outSize)
    {
        // Стандартный диалог выбора файла (Win32). Буфер MAX_PATH —
        // системная константа
        char pathBuffer[MAX_PATH] = "";
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = __blib_render_window_context(this->getWindow().__getCtx())->hwnd;
        ofn.lpstrFilter = filter;
        ofn.lpstrFile = pathBuffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = title;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

        if (!GetOpenFileNameA(&ofn))
        {
            return false;
        }

        strncpy_s(outPath, outSize, pathBuffer, _TRUNCATE);
        return true;
    }

    void Sc2imgApp::browseImageFile()
    {
        // Путь показываем в поле ввода и сразу загружаем
        if (this->browseFile(fileDialogTitle, fileFilter,
            this->impl->pathBuffer, pathBufferSize))
        {
            this->loadDocument(std::string(this->impl->pathBuffer));
        }
    }

} // namespace sc2img
