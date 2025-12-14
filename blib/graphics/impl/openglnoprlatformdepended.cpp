#include <blib/graphics/opengl.h>

#include <iostream>

#include <Windows.h>
#include <gl/GL.h>

void __blib_glPushMatrix(void)
{
    glPushMatrix();
}

void __blib_glPopMatrix(void)
{
    glPopMatrix();
}

void __blib_glBegin(GLenum mode)
{
    glBegin(mode);
}

void __blib_glEnd()
{
    glEnd();
}

void __blib_glLoadIdentity(void)
{
    glLoadIdentity();
}

void __blib_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    glRotatef(angle, x, y, z);
}

void blib::graphics::RenderApi::InitGraphicsApi()
{
    //this->__blib_glBegin         = static_cast<__blib_gl_signature_glBegin>     (getprocaddr("glBegin"));
    //this->__blib_glEnd           = static_cast<__blib_gl_signature_glEnd>       (getprocaddr("glEnd"));
    //this->__blib_glEnable        = static_cast<__blib_gl_signature_glEnable>    (getprocaddr("glEnable"));
    //this->__blib_glDisable       = static_cast<__blib_gl_signature_glDisable>   (getprocaddr("glDisable"));
    //
    //this->__blib_glNormal3f = static_cast<__blib_gl_signature_glNormal3f>       (getprocaddr("glNormal3f"));
    //
    //this->__blib_glTexCoord3f = static_cast<__blib_gl_signature_glTexCoord3f>   (getprocaddr("glTexCoord3f"));
    //
    //this->__blib_glVertex3f = static_cast<__blib_gl_signature_glVertex3f>       (getprocaddr("glVertex3f"));

    this->ogl.__blib_glBegin        = &__blib_glBegin;        //static_cast<__blib_gl_signature_glBegin>(getprocaddr("glBegin"));
    this->ogl.__blib_glEnd          = &__blib_glEnd;          //static_cast<__blib_gl_signature_glEnd>(getprocaddr("glEnd"));
    this->ogl.__blib_glEnable       = &glEnable;              //static_cast<__blib_gl_signature_glEnable>(getprocaddr("glEnable"));
    this->ogl.__blib_glDisable      = &glDisable;             //static_cast<__blib_gl_signature_glDisable>(getprocaddr("glDisable"));
    this->ogl.__blib_glNormal3f     = &glNormal3f;            //static_cast<__blib_gl_signature_glNormal3f>(getprocaddr("glNormal3f"));
    this->ogl.__blib_glTexCoord3f   = &glTexCoord3f;          //static_cast<__blib_gl_signature_glTexCoord3f>(getprocaddr("glTexCoord3f"));
    this->ogl.__blib_glVertex3f     = &glVertex3f;            //static_cast<__blib_gl_signature_glVertex3f>(getprocaddr("glVertex3f"));
    this->ogl.__blib_glRotatef      = &__blib_glRotatef;      //static_cast<__blib_gl_signature_glRotatef>(getprocaddr("glRotatef"));
    this->ogl.__blib_glTranslatef   = &glTranslatef;          //static_cast<__blib_gl_signature_glTranslatef>(getprocaddr("glTranslatef"));
    this->ogl.__blib_glViewport     = &glViewport; 
    this->ogl.__blib_glLoadIdentity = &__blib_glLoadIdentity;
    this->ogl.__blib_glPushMatrix   = &__blib_glPushMatrix;
    this->ogl.__blib_glPopMatrix    = &__blib_glPopMatrix;
    this->ogl.__blib_gl_glClear     = &glClear;               //static_cast<__blib_gl_signature_glClear>(getprocaddr("glClear"));


    this->ogl.ext.__blib_glGenBuffers              = static_cast<__blib_gl_signature_glGenBuffers>(getprocaddr("glGenBuffers"));
    this->ogl.ext.__blib_glGenVertexArrays         = static_cast<__blib_gl_signature_glGenVertexArrays>(getprocaddr("glGenVertexArrays"));
    this->ogl.ext.__blib_glBindVertexArray         = static_cast<__blib_gl_signature_glBindVertexArray>(getprocaddr("glBindVertexArray"));
    this->ogl.ext.__blib_glBindBuffer              = static_cast<__blib_gl_signature_glBindBuffer>(getprocaddr("glBindBuffer"));
    this->ogl.ext.__blib_glBufferData              = static_cast<__blib_gl_signature_glBufferData>(getprocaddr("glBufferData"));
    this->ogl.ext.__blib_glEnableVertexAttribArray = static_cast<__blib_gl_signature_glEnableVertexAttribArray>(getprocaddr("glEnableVertexAttribArray"));
    this->ogl.ext.__blib_glVertexAttribPointer     = static_cast<__blib_gl_signature_glVertexAttribPointer>(getprocaddr("glVertexAttribPointer"));

    this->ogl.ext.__blib_gl_glCreateProgram = static_cast<__blib_gl_signature_glCreateProgram>(getprocaddr("glCreateProgram"));
    this->ogl.ext.__blib_gl_glAttachShader = static_cast<__blib_gl_signature_glAttachShader>(getprocaddr("glAttachShader"));
    this->ogl.ext.__blib_gl_glDetachShader = static_cast<__blib_gl_signature_glDetachShader>(getprocaddr("glDetachShader"));
    this->ogl.ext.__blib_gl_glLinkProgram = static_cast<__blib_gl_signature_glLinkProgram>(getprocaddr("glLinkProgram"));
    this->ogl.ext.__blib_gl_glUseProgram = static_cast<__blib_gl_signature_glUseProgram>(getprocaddr("glUseProgram"));
    this->ogl.ext.__blib_gl_glShaderSource = static_cast<__blib_gl_signature_glShaderSource>(getprocaddr("glShaderSource"));
    this->ogl.ext.__blib_gl_glCreateShader = static_cast<__blib_gl_signature_glCreateShader>(getprocaddr("glCreateShader")); 
    this->ogl.ext.__blib_gl_glCompileShader = static_cast<__blib_gl_signature_glCompileShader>(getprocaddr("glCompileShader")); 
    this->ogl.ext.__blib_gl_glGetShaderiv = static_cast<__blib_gl_signature_glGetShaderiv>(getprocaddr("glGetShaderiv"));
    this->ogl.ext.__blib_gl_glGetShaderInfoLog = static_cast<__blib_gl_signature_glGetShaderInfoLog>(getprocaddr("glGetShaderInfoLog"));
    this->ogl.ext.__blib_gl_glActiveTexture = static_cast<__blib_gl_signature_glActiveTexture>(getprocaddr("glActiveTexture"));
    this->ogl.ext.__blib_gl_glBindTexture = static_cast<__blib_gl_signature_glBindTexture>(getprocaddr("glBindTexture"));
    this->ogl.ext.__blib_gl_glGetUniformLocation = static_cast<__blib_gl_signature_glGetUniformLocation>(getprocaddr("glGetUniformLocation")); 
    this->ogl.ext.__blib_gl_glUniform1i = static_cast<__blib_gl_signature_glUniform1i>(getprocaddr("glUniform1i")); 
    this->ogl.ext.__blib_gl_glGetProgramiv = static_cast<__blib_gl_signature_glGetProgramiv>(getprocaddr("glGetProgramiv")); 
    this->ogl.ext.__blib_gl_glGetProgramInfoLog = static_cast<__blib_gl_signature_glGetProgramInfoLog>(getprocaddr("glGetProgramInfoLog"));
    this->ogl.ext.__blib_gl_glDrawArrays = static_cast<__blib_gl_signature_glDrawArrays>(getprocaddr("glDrawArrays"));
    this->ogl.ext.__blib_gl_glUniformMatrix4fv = static_cast<__blib_gl_signature_glUniformMatrix4fv>(getprocaddr("glUniformMatrix4fv"));
    this->ogl.ext.__blib_gl_glDrawElements = static_cast<__blib_gl_signature_glDrawElements>(getprocaddr("glDrawElements"));
    this->ogl.ext.__blib_gl_glDeleteTextures = static_cast<__blib_gl_signature_glDeleteTextures>(getprocaddr("glDeleteTextures"));
    this->ogl.ext.__blib_gl_glGenTextures = static_cast<__blib_gl_signature_glGenTextures>(getprocaddr("glGenTextures"));
    this->ogl.ext.__blib_gl_glTexParameteri = &glTexParameteri;//static_cast<__blib_gl_signature_glTexParameteri>(getprocaddr("glTexParameteri"));
    this->ogl.ext.__blib_gl_glTexImage2D = &glTexImage2D;//static_cast<__blib_gl_signature_glTexImage2D>(getprocaddr("glTexImage2D"));
    this->ogl.ext.__blib_gl_glFlush = &glFlush;//static_cast<__blib_gl_signature_glFlush>(getprocaddr("glFlush"));
}
