#ifndef RIO_GL_H
#define RIO_GL_H

#include <misc/rio_Types.h>

#ifndef RIO_NO_GL_LOADER
    #if RIO_USE_GLEW
            #include <GL/glew.h>
    #else
        // use GLAD by default
        #ifdef RIO_GLES
            #include <glad/gles2.h>
        #else
            #include <glad/gl.h>
        #endif
    #endif // RIO_USE_GLEW
#endif // RIO_NO_GL_LOADER

#if !defined(RIO_NO_GLFW_CALLS) && !defined(RIO_NO_GLFW_IMPORT)
    #define GLFW_INCLUDE_NONE
    #include <GLFW/glfw3.h>
#else
    typedef void GLFWwindow;
#endif

// define functions that do not exist on OpenGL ES
#ifdef RIO_GLES
    #define glDepthRange glDepthRangef
    #define glClearDepth glClearDepthf
    #ifndef RIO_NO_CLIP_CONTROL
        #define RIO_NO_CLIP_CONTROL // do not call glClipControl
    #endif
#elif defined(__APPLE__)
    #ifndef RIO_NO_CLIP_CONTROL
        #define RIO_NO_CLIP_CONTROL // not supported on macOS
    #endif
#endif // RIO_GLES

#ifdef RIO_DEBUG

inline void GLClearError(const char* file, s32 line)
{
    bool printed = false;

    while (GLenum error = glGetError())
    {
        if (!printed)
        {
            RIO_LOG("File \"%s\", line %d\n", file, line);
            printed = true;
        }
        RIO_LOG("[OpenGL Error] (%u): Clear\n", error);
    }
}

inline void GLCheckError(const char* file, s32 line, const char* arg = nullptr)
{
    bool printed = false;

    while (GLenum error = glGetError())
    {
        if (!printed)
        {
            if (arg != nullptr)
            {
                RIO_LOG("File \"%s\", line %d\n%s\n", file, line, arg);
            }
            else
            {
                RIO_LOG("File \"%s\", line %d\n", file, line);
            }
            printed = true;
        }
        RIO_LOG("[OpenGL Error] (%u)\n", error);
    }

    if (printed)
    {
        RIO_ASSERT(false);
    }
}

#define RIO_GL_CHECK_ERROR() GLCheckError(__FILE__, __LINE__)


#ifdef RIO_GL_TRACE
#define RIO_GL_TRACE_LOG(ARG)                                                  \
        do                                                                     \
        {                                                                      \
            RIO_LOG("[RIO_GL_TRACE] Function: %s | Call: %s | File: %s:%d\n",  \
            __func__, #ARG, __FILE__, __LINE__); \
        } while (0)
#else
#define RIO_GL_TRACE_LOG(ARG)
#endif

#define RIO_GL_CALL(ARG)                        \
    do                                          \
    {                                           \
        GLClearError(__FILE__, __LINE__);       \
        RIO_GL_TRACE_LOG(ARG);                  \
        ARG;                                    \
        GLCheckError(__FILE__, __LINE__, #ARG); \
    } while (0)

#else

#define RIO_GL_CHECK_ERROR()
#define RIO_GL_CALL(ARG) ARG

#endif // RIO_DEBUG

#endif // RIO_GL_H
