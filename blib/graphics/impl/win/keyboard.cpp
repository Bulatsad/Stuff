#include <blib/graphics/keyboard.h>

#include <blib/utilmacro.h>
#include <blib/inline.h>

#include<Windows.h>

__blib_private_func __blib_force_inline int blibToWinApi(const blib::graphics::Keyboard::Key key)
{
    switch (key)
    {
    case blib::graphics::Keyboard::Key::A:          return 'A';
    case blib::graphics::Keyboard::Key::B:          return 'B';
    case blib::graphics::Keyboard::Key::C:          return 'C';
    case blib::graphics::Keyboard::Key::D:          return 'D';
    case blib::graphics::Keyboard::Key::E:          return 'E';
    case blib::graphics::Keyboard::Key::F:          return 'F';
    case blib::graphics::Keyboard::Key::G:          return 'G';
    case blib::graphics::Keyboard::Key::H:          return 'H';
    case blib::graphics::Keyboard::Key::I:          return 'I';
    case blib::graphics::Keyboard::Key::J:          return 'J';
    case blib::graphics::Keyboard::Key::K:          return 'K';
    case blib::graphics::Keyboard::Key::L:          return 'L';
    case blib::graphics::Keyboard::Key::M:          return 'M';
    case blib::graphics::Keyboard::Key::N:          return 'N';
    case blib::graphics::Keyboard::Key::O:          return 'O';
    case blib::graphics::Keyboard::Key::P:          return 'P';
    case blib::graphics::Keyboard::Key::Q:          return 'Q';
    case blib::graphics::Keyboard::Key::R:          return 'R';
    case blib::graphics::Keyboard::Key::S:          return 'S';
    case blib::graphics::Keyboard::Key::T:          return 'T';
    case blib::graphics::Keyboard::Key::U:          return 'U';
    case blib::graphics::Keyboard::Key::V:          return 'V';
    case blib::graphics::Keyboard::Key::W:          return 'W';
    case blib::graphics::Keyboard::Key::X:          return 'X';
    case blib::graphics::Keyboard::Key::Y:          return 'Y';
    case blib::graphics::Keyboard::Key::Z:          return 'Z';
    case blib::graphics::Keyboard::Key::Num0:       return '0';
    case blib::graphics::Keyboard::Key::Num1:       return '1';
    case blib::graphics::Keyboard::Key::Num2:       return '2';
    case blib::graphics::Keyboard::Key::Num3:       return '3';
    case blib::graphics::Keyboard::Key::Num4:       return '4';
    case blib::graphics::Keyboard::Key::Num5:       return '5';
    case blib::graphics::Keyboard::Key::Num6:       return '6';
    case blib::graphics::Keyboard::Key::Num7:       return '7';
    case blib::graphics::Keyboard::Key::Num8:       return '8';
    case blib::graphics::Keyboard::Key::Num9:       return '9';
    case blib::graphics::Keyboard::Key::Escape:     return VK_ESCAPE;
    case blib::graphics::Keyboard::Key::LControl:   return VK_LCONTROL;
    case blib::graphics::Keyboard::Key::LShift:     return VK_LSHIFT;
    case blib::graphics::Keyboard::Key::LAlt:       return VK_LMENU;
    case blib::graphics::Keyboard::Key::LSystem:    return VK_LWIN;
    case blib::graphics::Keyboard::Key::RControl:   return VK_RCONTROL;
    case blib::graphics::Keyboard::Key::RShift:     return VK_RSHIFT;
    case blib::graphics::Keyboard::Key::RAlt:       return VK_RMENU;
    case blib::graphics::Keyboard::Key::RSystem:    return VK_RWIN;
    case blib::graphics::Keyboard::Key::Menu:       return VK_APPS;
    case blib::graphics::Keyboard::Key::LBracket:   return VK_OEM_4;
    case blib::graphics::Keyboard::Key::RBracket:   return VK_OEM_6;
    case blib::graphics::Keyboard::Key::Semicolon:  return VK_OEM_1;
    case blib::graphics::Keyboard::Key::Comma:      return VK_OEM_COMMA;
    case blib::graphics::Keyboard::Key::Period:     return VK_OEM_PERIOD;
    case blib::graphics::Keyboard::Key::Apostrophe: return VK_OEM_7;
    case blib::graphics::Keyboard::Key::Slash:      return VK_OEM_2;
    case blib::graphics::Keyboard::Key::Backslash:  return VK_OEM_5;
    case blib::graphics::Keyboard::Key::Grave:      return VK_OEM_3;
    case blib::graphics::Keyboard::Key::Equal:      return VK_OEM_PLUS;
    case blib::graphics::Keyboard::Key::Hyphen:     return VK_OEM_MINUS;
    case blib::graphics::Keyboard::Key::Space:      return VK_SPACE;
    case blib::graphics::Keyboard::Key::Enter:      return VK_RETURN;
    case blib::graphics::Keyboard::Key::Backspace:  return VK_BACK;
    case blib::graphics::Keyboard::Key::Tab:        return VK_TAB;
    case blib::graphics::Keyboard::Key::PageUp:     return VK_PRIOR;
    case blib::graphics::Keyboard::Key::PageDown:   return VK_NEXT;
    case blib::graphics::Keyboard::Key::End:        return VK_END;
    case blib::graphics::Keyboard::Key::Home:       return VK_HOME;
    case blib::graphics::Keyboard::Key::Insert:     return VK_INSERT;
    case blib::graphics::Keyboard::Key::Delete:     return VK_DELETE;
    case blib::graphics::Keyboard::Key::Add:        return VK_ADD;
    case blib::graphics::Keyboard::Key::Subtract:   return VK_SUBTRACT;
    case blib::graphics::Keyboard::Key::Multiply:   return VK_MULTIPLY;
    case blib::graphics::Keyboard::Key::Divide:     return VK_DIVIDE;
    case blib::graphics::Keyboard::Key::Left:       return VK_LEFT;
    case blib::graphics::Keyboard::Key::Right:      return VK_RIGHT;
    case blib::graphics::Keyboard::Key::Up:         return VK_UP;
    case blib::graphics::Keyboard::Key::Down:       return VK_DOWN;
    case blib::graphics::Keyboard::Key::Numpad0:    return VK_NUMPAD0;
    case blib::graphics::Keyboard::Key::Numpad1:    return VK_NUMPAD1;
    case blib::graphics::Keyboard::Key::Numpad2:    return VK_NUMPAD2;
    case blib::graphics::Keyboard::Key::Numpad3:    return VK_NUMPAD3;
    case blib::graphics::Keyboard::Key::Numpad4:    return VK_NUMPAD4;
    case blib::graphics::Keyboard::Key::Numpad5:    return VK_NUMPAD5;
    case blib::graphics::Keyboard::Key::Numpad6:    return VK_NUMPAD6;
    case blib::graphics::Keyboard::Key::Numpad7:    return VK_NUMPAD7;
    case blib::graphics::Keyboard::Key::Numpad8:    return VK_NUMPAD8;
    case blib::graphics::Keyboard::Key::Numpad9:    return VK_NUMPAD9;
    case blib::graphics::Keyboard::Key::F1:         return VK_F1;
    case blib::graphics::Keyboard::Key::F2:         return VK_F2;
    case blib::graphics::Keyboard::Key::F3:         return VK_F3;
    case blib::graphics::Keyboard::Key::F4:         return VK_F4;
    case blib::graphics::Keyboard::Key::F5:         return VK_F5;
    case blib::graphics::Keyboard::Key::F6:         return VK_F6;
    case blib::graphics::Keyboard::Key::F7:         return VK_F7;
    case blib::graphics::Keyboard::Key::F8:         return VK_F8;
    case blib::graphics::Keyboard::Key::F9:         return VK_F9;
    case blib::graphics::Keyboard::Key::F10:        return VK_F10;
    case blib::graphics::Keyboard::Key::F11:        return VK_F11;
    case blib::graphics::Keyboard::Key::F12:        return VK_F12;
    case blib::graphics::Keyboard::Key::F13:        return VK_F13;
    case blib::graphics::Keyboard::Key::F14:        return VK_F14;
    case blib::graphics::Keyboard::Key::F15:        return VK_F15;
    case blib::graphics::Keyboard::Key::Pause:      return VK_PAUSE;
    default:
        return 0;
    }
}

__blib_private_func __blib_force_inline blib::graphics::Keyboard::Key WinApiToBlib(int key)
{
    switch (key)
    {
    case 'A':           return blib::graphics::Keyboard::Key::A;
    case 'B':           return blib::graphics::Keyboard::Key::B;
    case 'C':           return blib::graphics::Keyboard::Key::C;
    case 'D':           return blib::graphics::Keyboard::Key::D;
    case 'E':           return blib::graphics::Keyboard::Key::E;
    case 'F':           return blib::graphics::Keyboard::Key::F;
    case 'G':           return blib::graphics::Keyboard::Key::G;
    case 'H':           return blib::graphics::Keyboard::Key::H;
    case 'I':           return blib::graphics::Keyboard::Key::I;
    case 'J':           return blib::graphics::Keyboard::Key::J;
    case 'K':           return blib::graphics::Keyboard::Key::K;
    case 'L':           return blib::graphics::Keyboard::Key::L;
    case 'M':           return blib::graphics::Keyboard::Key::M;
    case 'N':           return blib::graphics::Keyboard::Key::N;
    case 'O':           return blib::graphics::Keyboard::Key::O;
    case 'P':           return blib::graphics::Keyboard::Key::P;
    case 'Q':           return blib::graphics::Keyboard::Key::Q;
    case 'R':           return blib::graphics::Keyboard::Key::R;
    case 'S':           return blib::graphics::Keyboard::Key::S;
    case 'T':           return blib::graphics::Keyboard::Key::T;
    case 'U':           return blib::graphics::Keyboard::Key::U;
    case 'V':           return blib::graphics::Keyboard::Key::V;
    case 'W':           return blib::graphics::Keyboard::Key::W;
    case 'X':           return blib::graphics::Keyboard::Key::X;
    case 'Y':           return blib::graphics::Keyboard::Key::Y;
    case 'Z':           return blib::graphics::Keyboard::Key::Z;
    case '0':           return blib::graphics::Keyboard::Key::Num0;
    case '1':           return blib::graphics::Keyboard::Key::Num1;
    case '2':           return blib::graphics::Keyboard::Key::Num2;
    case '3':           return blib::graphics::Keyboard::Key::Num3;
    case '4':           return blib::graphics::Keyboard::Key::Num4;
    case '5':           return blib::graphics::Keyboard::Key::Num5;
    case '6':           return blib::graphics::Keyboard::Key::Num6;
    case '7':           return blib::graphics::Keyboard::Key::Num7;
    case '8':           return blib::graphics::Keyboard::Key::Num8;
    case '9':           return blib::graphics::Keyboard::Key::Num9;
    case VK_ESCAPE:     return blib::graphics::Keyboard::Key::Escape;
    case VK_LCONTROL:   return blib::graphics::Keyboard::Key::LControl;
    case VK_LSHIFT:     return blib::graphics::Keyboard::Key::LShift;
    case VK_LMENU:      return blib::graphics::Keyboard::Key::LAlt;
    case VK_LWIN:       return blib::graphics::Keyboard::Key::LSystem;
    case VK_RCONTROL:   return blib::graphics::Keyboard::Key::RControl;
    case VK_RSHIFT:     return blib::graphics::Keyboard::Key::RShift;
    case VK_RMENU:      return blib::graphics::Keyboard::Key::RAlt;
    case VK_RWIN:       return blib::graphics::Keyboard::Key::RSystem;
    case VK_APPS:       return blib::graphics::Keyboard::Key::Menu;
    case VK_OEM_4:      return blib::graphics::Keyboard::Key::LBracket;
    case VK_OEM_6:      return blib::graphics::Keyboard::Key::RBracket;
    case VK_OEM_1:      return blib::graphics::Keyboard::Key::Semicolon;
    case VK_OEM_COMMA:  return blib::graphics::Keyboard::Key::Comma;
    case VK_OEM_PERIOD: return blib::graphics::Keyboard::Key::Period;
    case VK_OEM_7:      return blib::graphics::Keyboard::Key::Apostrophe;
    case VK_OEM_2:      return blib::graphics::Keyboard::Key::Slash;
    case VK_OEM_5:      return blib::graphics::Keyboard::Key::Backslash;
    case VK_OEM_3:      return blib::graphics::Keyboard::Key::Grave;
    case VK_OEM_PLUS:   return blib::graphics::Keyboard::Key::Equal;
    case VK_OEM_MINUS:  return blib::graphics::Keyboard::Key::Hyphen;
    case VK_SPACE:      return blib::graphics::Keyboard::Key::Space;
    case VK_RETURN:     return blib::graphics::Keyboard::Key::Enter;
    case VK_BACK:       return blib::graphics::Keyboard::Key::Backspace;
    case VK_TAB:        return blib::graphics::Keyboard::Key::Tab;
    case VK_PRIOR:      return blib::graphics::Keyboard::Key::PageUp;
    case VK_NEXT:       return blib::graphics::Keyboard::Key::PageDown;
    case VK_END:        return blib::graphics::Keyboard::Key::End;
    case VK_HOME:       return blib::graphics::Keyboard::Key::Home;
    case VK_INSERT:     return blib::graphics::Keyboard::Key::Insert;
    case VK_DELETE:     return blib::graphics::Keyboard::Key::Delete;
    case VK_ADD:        return blib::graphics::Keyboard::Key::Add;
    case VK_SUBTRACT:   return blib::graphics::Keyboard::Key::Subtract;
    case VK_MULTIPLY:   return blib::graphics::Keyboard::Key::Multiply;
    case VK_DIVIDE:     return blib::graphics::Keyboard::Key::Divide;
    case VK_LEFT:       return blib::graphics::Keyboard::Key::Left;
    case VK_RIGHT:      return blib::graphics::Keyboard::Key::Right;
    case VK_UP:         return blib::graphics::Keyboard::Key::Up;
    case VK_DOWN:       return blib::graphics::Keyboard::Key::Down;
    case VK_NUMPAD0:    return blib::graphics::Keyboard::Key::Numpad0;
    case VK_NUMPAD1:    return blib::graphics::Keyboard::Key::Numpad1;
    case VK_NUMPAD2:    return blib::graphics::Keyboard::Key::Numpad2;
    case VK_NUMPAD3:    return blib::graphics::Keyboard::Key::Numpad3;
    case VK_NUMPAD4:    return blib::graphics::Keyboard::Key::Numpad4;
    case VK_NUMPAD5:    return blib::graphics::Keyboard::Key::Numpad5;
    case VK_NUMPAD6:    return blib::graphics::Keyboard::Key::Numpad6;
    case VK_NUMPAD7:    return blib::graphics::Keyboard::Key::Numpad7;
    case VK_NUMPAD8:    return blib::graphics::Keyboard::Key::Numpad8;
    case VK_NUMPAD9:    return blib::graphics::Keyboard::Key::Numpad9;
    case VK_F1:         return blib::graphics::Keyboard::Key::F1;
    case VK_F2:         return blib::graphics::Keyboard::Key::F2;
    case VK_F3:         return blib::graphics::Keyboard::Key::F3;
    case VK_F4:         return blib::graphics::Keyboard::Key::F4;
    case VK_F5:         return blib::graphics::Keyboard::Key::F5;
    case VK_F6:         return blib::graphics::Keyboard::Key::F6;
    case VK_F7:         return blib::graphics::Keyboard::Key::F7;
    case VK_F8:         return blib::graphics::Keyboard::Key::F8;
    case VK_F9:         return blib::graphics::Keyboard::Key::F9;
    case VK_F10:        return blib::graphics::Keyboard::Key::F10;
    case VK_F11:        return blib::graphics::Keyboard::Key::F11;
    case VK_F12:        return blib::graphics::Keyboard::Key::F12;
    case VK_F13:        return blib::graphics::Keyboard::Key::F13;
    case VK_F14:        return blib::graphics::Keyboard::Key::F14;
    case VK_F15:        return blib::graphics::Keyboard::Key::F15;
    case VK_PAUSE:      return blib::graphics::Keyboard::Key::Pause;
    default:            return blib::graphics::Keyboard::Key::END_OF_ENUM;
    }
}

bool blib::graphics::Keyboard::isKeyPressed(Keyboard::Key key)
{
    return (GetAsyncKeyState(blibToWinApi(key)) & 0x8000) != 0;
}
