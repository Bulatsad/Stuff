#pragma once

#include <blib/config.h>

//#ifdef __blib_compile_platform_windows

#include <Windows.h>
#include <opengl/api/glcorearb.h>
#include <opengl/api/wglext.h>
#define __blib_gl_calling_convension APIENTRY

//#else
//#error undefined render api platform
//#endif // __blib_compile_platform_windows

//typedef size_t GLsizeiptr;
//typedef char GLchar;

//#define GL_ARRAY_BUFFER 0x8892
//
//#define GL_STATIC_DRAW_ARB 0x88E4
//
//#define GL_STATIC_DRAW GL_STATIC_DRAW_ARB
//
//#define GL_FRAGMENT_SHADER 0x8B30
//#define GL_VERTEX_SHADER 0x8B31
//
#define GL_NULL_VERTEX_BUFFER 0x0000
#define GL_NULL_VERTEX_ARRAY 0x0000
#define GL_NONE_SHADER 0x0000
//
//#define GL_COMPILE_STATUS 0x8B81
//#define GL_LINK_STATUS 0x8B82
//#define GL_VALIDATE_STATUS 0x8B83
//
//#define GL_TEXTURE0 0x84C0
//#define GL_TEXTURE1 0x84C1
//#define GL_TEXTURE2 0x84C2


typedef void (__blib_gl_calling_convension* __blib_gl_signature_glBegin)(GLenum mode);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glEnd)();
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glEnable)(GLenum cap);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glDisable)(GLenum cap);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glRotatef)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glLoadIdentity)(void);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glPushMatrix)(void);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glPopMatrix)(void);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glClear)(GLbitfield mask);



typedef void (__blib_gl_calling_convension* __blib_gl_signature_glNormal3f)(GLfloat nx, GLfloat ny, GLfloat nz);

typedef void (__blib_gl_calling_convension* __blib_gl_signature_glTexCoord3f)(GLfloat s, GLfloat t, GLfloat r);

typedef void (__blib_gl_calling_convension* __blib_gl_signature_glVertex3f)(GLfloat x, GLfloat y, GLfloat z);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glTranslatef)(GLfloat x,GLfloat y,GLfloat z);

//EXT
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glGenBuffers)(GLsizei n, GLuint* buffers);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glGenVertexArrays)(GLsizei n, GLuint* arrays);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glBindVertexArray)(GLuint array);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glBindBuffer)(GLenum target, GLuint buffer);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glBufferData)(GLenum target,GLsizeiptr size,const void* data,GLenum usage);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glEnableVertexAttribArray)(GLuint index);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);

typedef GLuint (__blib_gl_calling_convension* __blib_gl_signature_glCreateShader)(GLenum shaderType);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glShaderSource)(GLuint shader, GLsizei count, const GLchar** string, const GLint* length);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glCompileShader)(GLuint shader);

typedef GLuint (__blib_gl_calling_convension* __blib_gl_signature_glCreateProgram)(void);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glAttachShader)(GLuint program, GLuint shader);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glDetachShader)(GLuint program, GLuint shader);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glLinkProgram)(GLuint program);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glUseProgram)(GLuint program);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glGetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glActiveTexture)(GLenum texture);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glBindTexture)(GLenum target, GLuint texture);
typedef GLint(__blib_gl_calling_convension* __blib_gl_signature_glGetUniformLocation)(GLuint program, const GLchar* name);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glUniform1i)(GLint location, GLint v0);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glGetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glDeleteTextures)(GLsizei n, const GLuint* textures);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glGenTextures)(GLsizei n, GLuint* textures);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glGenFramebuffers)(GLsizei n, GLuint* ids);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glTexParameteri)(GLenum target, GLenum pname, GLint param);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glFlush)(void);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glBindFramebuffer)(GLenum target, GLuint framebuffer);

typedef void (__blib_gl_calling_convension* __blib_gl_signature_glGenRenderbuffers)(GLsizei n, GLuint* renderbuffers);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (__blib_gl_calling_convension* __blib_gl_signature_glBlitFramebuffer)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);








namespace blib
{
    namespace graphics
    {


        struct __blib_api oglExt_s
        {
            //EXT
            __blib_gl_signature_glGenBuffers __blib_glGenBuffers = nullptr;
            __blib_gl_signature_glGenVertexArrays __blib_glGenVertexArrays = nullptr;
            __blib_gl_signature_glBindVertexArray __blib_glBindVertexArray = nullptr;
            __blib_gl_signature_glBindBuffer __blib_glBindBuffer = nullptr;
            __blib_gl_signature_glBufferData __blib_glBufferData = nullptr;
            __blib_gl_signature_glEnableVertexAttribArray __blib_glEnableVertexAttribArray = nullptr;
            __blib_gl_signature_glVertexAttribPointer __blib_glVertexAttribPointer  = nullptr;
            __blib_gl_signature_glCreateProgram __blib_gl_glCreateProgram = nullptr;
            __blib_gl_signature_glAttachShader __blib_gl_glAttachShader = nullptr;
            __blib_gl_signature_glDetachShader __blib_gl_glDetachShader = nullptr;
            __blib_gl_signature_glLinkProgram __blib_gl_glLinkProgram = nullptr;
            __blib_gl_signature_glUseProgram __blib_gl_glUseProgram = nullptr;
            __blib_gl_signature_glShaderSource __blib_gl_glShaderSource = nullptr;
            __blib_gl_signature_glCreateShader __blib_gl_glCreateShader = nullptr;
            __blib_gl_signature_glCompileShader __blib_gl_glCompileShader = nullptr;
            __blib_gl_signature_glGetShaderiv __blib_gl_glGetShaderiv = nullptr;
            __blib_gl_signature_glGetShaderInfoLog __blib_gl_glGetShaderInfoLog = nullptr;
            __blib_gl_signature_glActiveTexture __blib_gl_glActiveTexture = nullptr;
            __blib_gl_signature_glBindTexture __blib_gl_glBindTexture = nullptr;
            __blib_gl_signature_glGetUniformLocation __blib_gl_glGetUniformLocation = nullptr;
            __blib_gl_signature_glUniform1i __blib_gl_glUniform1i = nullptr;
            __blib_gl_signature_glGetProgramiv __blib_gl_glGetProgramiv = nullptr;
            __blib_gl_signature_glGetProgramInfoLog __blib_gl_glGetProgramInfoLog = nullptr;
            __blib_gl_signature_glDrawArrays __blib_gl_glDrawArrays = nullptr;
            __blib_gl_signature_glUniformMatrix4fv __blib_gl_glUniformMatrix4fv = nullptr;
            __blib_gl_signature_glDrawElements __blib_gl_glDrawElements = nullptr;
            __blib_gl_signature_glDeleteTextures __blib_gl_glDeleteTextures = nullptr;
            __blib_gl_signature_glGenTextures __blib_gl_glGenTextures = nullptr;
            __blib_gl_signature_glTexParameteri __blib_gl_glTexParameteri = nullptr;
            __blib_gl_signature_glTexImage2D __blib_gl_glTexImage2D = nullptr;
            __blib_gl_signature_glFlush __blib_gl_glFlush = nullptr;
            __blib_gl_signature_glGenFramebuffers __blib_gl_glGenFramebuffers = nullptr;
            __blib_gl_signature_glBindFramebuffer __blib_gl_glBindFramebuffer = nullptr;

            __blib_gl_signature_glGenRenderbuffers __blib_gl_glGenRenderbuffers = nullptr;
            __blib_gl_signature_glBindRenderbuffer __blib_gl_glBindRenderbuffer = nullptr;
            __blib_gl_signature_glRenderbufferStorage __blib_gl_glRenderbufferStorage = nullptr;
            __blib_gl_signature_glFramebufferRenderbuffer __blib_gl_glFramebufferRenderbuffer = nullptr;
            __blib_gl_signature_glFramebufferTexture2D __blib_gl_glFramebufferTexture2D = nullptr;
            __blib_gl_signature_glBlitFramebuffer __blib_gl_glBlitFramebuffer = nullptr;
            
        };

        struct __blib_api ogl_s
        {
            __blib_gl_signature_glBegin        __blib_glBegin = nullptr;
            __blib_gl_signature_glEnd          __blib_glEnd = nullptr;
            __blib_gl_signature_glEnable       __blib_glEnable = nullptr;
            __blib_gl_signature_glDisable      __blib_glDisable = nullptr;
            __blib_gl_signature_glNormal3f     __blib_glNormal3f = nullptr;
            __blib_gl_signature_glTexCoord3f   __blib_glTexCoord3f = nullptr;
            __blib_gl_signature_glVertex3f     __blib_glVertex3f = nullptr;
            __blib_gl_signature_glRotatef      __blib_glRotatef = nullptr;
            __blib_gl_signature_glTranslatef   __blib_glTranslatef = nullptr;
            __blib_gl_signature_glLoadIdentity __blib_glLoadIdentity = nullptr;
            __blib_gl_signature_glViewport     __blib_glViewport = nullptr;
            __blib_gl_signature_glPushMatrix   __blib_glPushMatrix = nullptr;
            __blib_gl_signature_glPopMatrix    __blib_glPopMatrix = nullptr;
            __blib_gl_signature_glClear        __blib_gl_glClear = nullptr;
            oglExt_s ext;
        };

        struct __blib_api RenderApi
        {
            void* getprocaddr(const char* fname);
            void InitGraphicsApi();


            ogl_s ogl;

        };
    }
}
