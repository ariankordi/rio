#ifndef RIO_GPU_DRAWER_WIN_H
#define RIO_GPU_DRAWER_WIN_H

// This file is included by rio_Drawer.h
//#include <gpu/rio_Drawer.h>

#include <misc/gl/rio_GL.h>

namespace rio {

enum Drawer::PrimitiveMode : u32
{
    POINTS                      = GL_POINTS,
    LINES                       = GL_LINES,
    LINE_STRIP                  = GL_LINE_STRIP,
    TRIANGLES                   = GL_TRIANGLES,
    TRIANGLE_FAN                = GL_TRIANGLE_FAN,
    TRIANGLE_STRIP              = GL_TRIANGLE_STRIP,
#ifdef GL_LINES_ADJACENCY // not available on gles 3
    LINES_ADJACENCY             = GL_LINES_ADJACENCY,
    LINE_STRIP_ADJACENCY        = GL_LINE_STRIP_ADJACENCY,
    TRIANGLES_ADJACENCY         = GL_TRIANGLES_ADJACENCY,
    TRIANGLE_STRIP_ADJACENCY    = GL_TRIANGLE_STRIP_ADJACENCY,
#endif
    LINE_LOOP                   = GL_LINE_LOOP
};

inline void Drawer::DrawArraysInstanced(PrimitiveMode mode, u32 count, u32 instanceCount, u32 first)
{
    RIO_GL_CALL(glDrawArraysInstanced(mode, first, count, instanceCount));
}

inline void Drawer::DrawElementsInstanced(PrimitiveMode mode, u32 count, const u32* indices, u32 instanceCount)
{

    RIO_GL_CALL(glDrawElementsInstanced(mode, count, GL_UNSIGNED_INT, indices, instanceCount));
}

inline void Drawer::DrawElementsInstanced(PrimitiveMode mode, u32 count, const u16* indices, u32 instanceCount)
{

    RIO_GL_CALL(glDrawElementsInstanced(mode, count, GL_UNSIGNED_SHORT, indices, instanceCount));
}

inline void Drawer::DrawArrays(PrimitiveMode mode, u32 count, u32 first)
{
    RIO_GL_CALL(glDrawArrays(mode, first, count));
}

inline void Drawer::DrawElements(PrimitiveMode mode, u32 count, const u32* indices)
{
#ifdef __APPLE__
    // DrawElements function compatible with macOS
    GLuint ebo;
    RIO_GL_CALL(glGenBuffers(1, &ebo));
    RIO_GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
    RIO_GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(u32), indices, GL_STATIC_DRAW));
    RIO_GL_CALL(glDrawElements(mode, count, GL_UNSIGNED_INT, (void*)0));
    RIO_GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_NONE));
    RIO_GL_CALL(glDeleteBuffers(1, &ebo));
#else
    RIO_GL_CALL(glDrawElements(mode, count, GL_UNSIGNED_INT, indices));
#endif
}

inline void Drawer::DrawElements(PrimitiveMode mode, u32 count, const u16* indices)
{
#ifdef __APPLE__
    // DrawElements function compatible with macOS
    GLuint ebo;
    RIO_GL_CALL(glGenBuffers(1, &ebo));
    RIO_GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
    RIO_GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(u16), indices, GL_STATIC_DRAW));
    RIO_GL_CALL(glDrawElements(mode, count, GL_UNSIGNED_SHORT, (void*)0));
    RIO_GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_NONE));
    RIO_GL_CALL(glDeleteBuffers(1, &ebo));
#else
    RIO_GL_CALL(glDrawElements(mode, count, GL_UNSIGNED_SHORT, indices));
#endif
}

}

#endif // RIO_GPU_DRAWER_WIN_H
