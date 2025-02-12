#pragma once

#include <blib/config.h>

#include <blib/graphics/vector.h>

#include <blib/graphics/renderWindow.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Mouse
        {
        public:
            enum class Button
            {
                Left,       // The left mouse button
                Right,      // The right mouse button
                Middle,     // The middle (wheel) mouse button
                XButton1,   // The first extra mouse button
                XButton2,   // The second extra mouse button

                END_OF_ENUM
            };
            
            static bool isButtonPressed(Button button);
            static Vector2i getPosition(RenderWindow& wnd);
            static void setPosition(RenderWindow& wnd, const Vector2i& position);

        };
    }
}
