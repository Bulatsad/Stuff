#pragma once

#include <Windows.h>

struct WinCtx
{
    bool open;
    HWND hwnd;
    HDC hdc;
    HGLRC context;
};

typedef WinCtx RenderWindowContext;

#define __blib_render_window_this_context(__this) (reinterpret_cast<RenderWindowContext*>(__this->ctx))
#define __blib_render_window_context(_ctx) (reinterpret_cast<RenderWindowContext*>(_ctx))
