#ifndef RIO_GPU_DRAWER_WIN_H
#define RIO_GPU_DRAWER_WIN_H

// This file is included by rio_Drawer.h
//#include <gpu/rio_Drawer.h>

#include <GL/glew.h>

namespace rio {

enum Drawer::PrimitiveMode : u32
{
    // TODO: More modes
    TRIANGLES       = GL_TRIANGLES,
    TRIANGLE_STRIP  = GL_TRIANGLE_STRIP,
    LINE_LOOP       = GL_LINE_LOOP
};

inline void Drawer::DrawArraysInstanced(PrimitiveMode mode, u32 count, u32 instanceCount, u32 first)
{
    glDrawArraysInstanced(mode, first, count, instanceCount);
}

inline void Drawer::DrawElementsInstanced(PrimitiveMode mode, u32 count, const u32* indices, u32 instanceCount)
{
    glDrawElementsInstanced(mode, count, GL_UNSIGNED_INT, indices, instanceCount);
}

inline void Drawer::DrawElementsInstanced(PrimitiveMode mode, u32 count, const u16* indices, u32 instanceCount)
{
    glDrawElementsInstanced(mode, count, GL_UNSIGNED_SHORT, indices, instanceCount);
}

inline void Drawer::DrawArrays(PrimitiveMode mode, u32 count, u32 first)
{
    glDrawArrays(mode, first, count);
}

inline void Drawer::DrawElements(PrimitiveMode mode, u32 count, const u32* indices)
{
    glDrawElements(mode, count, GL_UNSIGNED_INT, indices);
}

inline void Drawer::DrawElements(PrimitiveMode mode, u32 count, const u16* indices)
{
    glDrawElements(mode, count, GL_UNSIGNED_SHORT, indices);
}

}

#endif // RIO_GPU_DRAWER_WIN_H