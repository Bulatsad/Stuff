#include <blib/graphics/opengl.h>

#include <iostream>

#include <Windows.h>

void* blib::graphics::RenderApi::getprocaddr(const char* fname)
{
    PROC addr = wglGetProcAddress(fname);

    if (!addr)
    {
        auto currentctx = wglGetCurrentContext();
        auto err = GetLastError();
        LPSTR messageBuffer = nullptr;

        //Ask Win32 to give us the string version of that message ID.
        //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        //Copy the error message into a std::string.
        std::string message(messageBuffer, size);

        //Free the Win32's string's buffer.
        LocalFree(messageBuffer);


        std::cerr << "Error on loading opengl function. Function name: " << fname << std::endl;
        std::cerr << "Cause : " << message << std::endl;
        throw new std::runtime_error("Error on loading opengl function");


    }

    return addr;
}

