#include <blib/graphics/renderWindow.h>

#include <Windows.h>
#include <gl/GL.h>

#include <blib/inline.h>
#include <blib/graphics/impl/win/winRenderWindowUtil.h>

__blib_private_func LRESULT wndProc(HWND hwnd, UINT uMsg, WPARAM wparam, LPARAM lparam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
    case WM_CLOSE:
    {
        PostQuitMessage(EXIT_SUCCESS);
    }
    return 0;
    }
    return DefWindowProc(hwnd, uMsg, wparam, lparam);
}

blib::graphics::RenderWindow::RenderWindow(uint16_t _width, uint16_t _height, const std::string& title, WindowStile style)
{
    this->width = _width;
    this->height = _height;

    //if (!m_fullscreen)
    {
        RECT rectangle = { 0, 0, width, height };
        AdjustWindowRect(&rectangle, WS_VISIBLE, false);
        width = rectangle.right - rectangle.left;
        height = rectangle.bottom - rectangle.top;
    }

    this->ctx = new RenderWindowContext;
    HINSTANCE hInstance = GetModuleHandle(NULL);
    int nCmdShow = SW_SHOW;

    WNDCLASSEX wc;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_HAND);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    wc.hInstance = hInstance;
    wc.lpfnWndProc = &wndProc;
    wc.lpszClassName = "RenderWindow";
    wc.lpszMenuName = NULL;
    wc.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;

    if (!RegisterClassEx(&wc))
    {
        //TODO : logging
    }

    __blib_render_window_this_context(this)->hwnd = CreateWindow(wc.lpszClassName,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        this->width,
        this->height,
        NULL,
        NULL,
        wc.hInstance,
        NULL
    );
    if (__blib_render_window_this_context(this)->hwnd == INVALID_HANDLE_VALUE)
    {
        //TODO : logging
    }

    __blib_render_window_this_context(this)->hdc = GetDC(__blib_render_window_this_context(this)->hwnd);
    PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    // Flags
        PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
        32,                   // Colordepth of the framebuffer.
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,                   // Number of bits for the depthbuffer
        8,                    // Number of bits for the stencilbuffer
        0,                    // Number of Aux buffers in the framebuffer.
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };
    int PixelFormat = ChoosePixelFormat(__blib_render_window_this_context(this)->hdc, &pfd);
    SetPixelFormat(__blib_render_window_this_context(this)->hdc, PixelFormat, &pfd);
    HGLRC glContext = wglCreateContext(__blib_render_window_this_context(this)->hdc);
    wglMakeCurrent(__blib_render_window_this_context(this)->hdc, glContext);

    ShowWindow(__blib_render_window_this_context(this)->hwnd, nCmdShow);
    UpdateWindow(__blib_render_window_this_context(this)->hwnd);

    __blib_render_window_this_context(this)->open = true;

    RECT rectangle = { 0, 0, static_cast<long>(this->width), static_cast<long>(this->height) };
    AdjustWindowRect(&rectangle, static_cast<DWORD>(GetWindowLongPtr(__blib_render_window_this_context(this)->hwnd, GWL_STYLE)), false);
    int width = rectangle.right - rectangle.left;
    int height = rectangle.bottom - rectangle.top;

    SetWindowPos(__blib_render_window_this_context(this)->hwnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);

    glViewport(0, 0, this->width, this->width);
    glLoadIdentity();
}

void blib::graphics::RenderWindow::enableIsometricTileGreed()
{
    glPushMatrix();



    glPopMatrix();
}

void blib::graphics::RenderWindow::update()
{
    MSG msg;
    if (PeekMessage(&msg, __blib_render_window_this_context(this)->hwnd, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_CLOSE || msg.message == WM_QUIT || msg.message == WM_DESTROY)
        {
            __blib_render_window_this_context(this)->open = false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);

    }
}

bool blib::graphics::RenderWindow::isOpen()
{
    return __blib_render_window_this_context(this)->open;
}

void blib::graphics::RenderWindow::clear(const Color& color)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(
        (color.red) / static_cast<float>(255),
        color.green / static_cast<float>(255),
        color.blue / static_cast<float>(255),
        color.alpha / static_cast<float>(255)
    );
}

void blib::graphics::RenderWindow::display()
{
    SwapBuffers(__blib_render_window_this_context(this)->hdc);
}

void* blib::graphics::RenderWindow::__getCtx()
{
    return this->ctx;
}
