#include <blib/graphics/mouse.h>

#include <blib/utilmacro.h>
#include <blib/inline.h>

#include <blib/graphics/impl/win/winRenderWindowUtil.h>

__blib_private_func __blib_force_inline int blibToWinApi(const blib::graphics::Mouse::Button button)
{
    switch (button)
    {
    case blib::graphics::Mouse::Button::Left:     return VK_LBUTTON;
    case blib::graphics::Mouse::Button::Right:    return VK_RBUTTON;
    case blib::graphics::Mouse::Button::Middle:   return VK_MBUTTON;
    case blib::graphics::Mouse::Button::XButton1: return VK_XBUTTON1;
    case blib::graphics::Mouse::Button::XButton2: return VK_XBUTTON2;
    default:
        return 0;
    }
}

bool blib::graphics::Mouse::isButtonPressed(Button button)
{
    return (GetAsyncKeyState(blibToWinApi(button)) & 0x8000) != 0;
}

blib::graphics::vector2i blib::graphics::Mouse::getPosition(RenderWindow& wnd)
{
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(__blib_render_window_context(wnd.__getCtx())->hwnd, &p);

    vector2i res;
    res.x = p.x;
    res.y = p.y;
    return res;
}

void blib::graphics::Mouse::setPosition(RenderWindow& wnd, const vector2i& position)
{
    POINT point;
    point.x = position.x;
    point.y = position.y;
  
    ClientToScreen(__blib_render_window_context(wnd.__getCtx())->hwnd, &point);
    SetCursorPos(point.x, point.y);
}
