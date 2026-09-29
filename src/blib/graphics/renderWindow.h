#pragma once

#include <blib/config.h>

#include <stdint.h>
#include <string>

#include <blib/graphics/color.h>
#include <blib/graphics/drawable.h>
#include <blib/graphics/rendercontext.h>
#include <blib/graphics/rendertarget.h>

namespace blib
{
    namespace graphics
    {
        enum class WindowStile
        {
            None,
            Fullscreen,
            Close,
            Resize,

            END_OF_ENUM
        };

        class __blib_graphics_api RenderWindow
        {
        private:
            void* ctx;

            uint16_t width = 0;
            uint16_t height = 0;

        public:
            RenderWindow(uint16_t _width, uint16_t _height, const std::string& title, WindowStile style = WindowStile::None);
            
            virtual ~RenderWindow();

            uint16_t getHeight() const { return this->height; }
            uint16_t getWight() const { return this->width; }

            void enableIsometricTileGreed();

            /**
             * Сделать GL-контекст этого окна текущим для потока.
             * Кэшируется по wglGetCurrentContext: вызов драйвера
             * (wglMakeCurrent) происходит ТОЛЬКО при реальной смене
             * окна — в однооконных приложениях это no-op после первого
             * кадра. Нужен multi-window режиму (PIE: эдитор + клиентское
             * окно в одном процессе) — см. GRAPHICS.md, «Владение GL».
             */
            void makeCurrent();

            void update();
            bool isOpen();
            void display(IRenderTarget& rt, bint16 xStart = 0, bint16 yStart = 0);
            void close();

            // Блит рендер-таргета в back-буфер БЕЗ SwapBuffers: для
            // приложений, которые между блитом и презентацией рисуют
            // UI (ImGui) поверх сцены. Пара — swapBuffers() ниже
            void blitToBackbuffer(IRenderTarget& rt, bint16 xStart = 0, bint16 yStart = 0);

            // SwapBuffers без блита рендер-таргета: для приложений,
            // которые рисуют UI поверх сцены сами (ImGui-вьюпорты,
            // эдитор) и не используют display()/blit
            void swapBuffers();

            void* __getCtx();
        };
    }
}
