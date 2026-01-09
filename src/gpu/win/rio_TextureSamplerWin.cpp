#include <misc/rio_Types.h>

#if RIO_IS_WIN

#include <gpu/rio_TextureSampler.h>

namespace rio {

TextureSampler2D::TextureSampler2D()
{
    init_();

    RIO_GL_CALL(glGenSamplers(1, &mSamplerInner));
    RIO_ASSERT(mSamplerInner != GL_NONE);
}

TextureSampler2D::~TextureSampler2D()
{
    if (mSamplerInner != GL_NONE)
    {
        RIO_GL_CALL(glDeleteSamplers(1, &mSamplerInner));
        mSamplerInner = GL_NONE;
    }
}

void TextureSampler2D::updateFilter_() const
{
    switch (mMinFilter)
    {
    case TEX_XY_FILTER_MODE_POINT:
        switch (mMipFilter)
        {
        case TEX_MIP_FILTER_MODE_NONE:
            RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
            break;
        case TEX_MIP_FILTER_MODE_POINT:
            RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST));
            break;
        case TEX_MIP_FILTER_MODE_LINEAR:
            RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR));
            break;
        }
        break;
    case TEX_XY_FILTER_MODE_LINEAR:
        switch (mMipFilter)
        {
        case TEX_MIP_FILTER_MODE_NONE:
            RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
            break;
        case TEX_MIP_FILTER_MODE_POINT:
            RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST));
            break;
        case TEX_MIP_FILTER_MODE_LINEAR:
            RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
            break;
        }
        break;
    }

    switch (mMagFilter)
    {
    case TEX_XY_FILTER_MODE_POINT:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        break;
    case TEX_XY_FILTER_MODE_LINEAR:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        break;
    }

    bool isAnisoSupported = false; // not supported on some systems
#ifndef RIO_NO_GL_LOADER
    #ifdef RIO_USE_GLEW
        isAnisoSupported = GLEW_EXT_texture_filter_anisotropic != 0;
    #elif defined(GLAD_GL_EXT_texture_filter_anisotropic)
        isAnisoSupported = GLAD_GL_EXT_texture_filter_anisotropic;
    #elif defined(GLAD_GL_ARB_texture_filter_anisotropic)
        isAnisoSupported = GLAD_GL_ARB_texture_filter_anisotropic;
    #endif
#endif // RIO_NO_GL_LOADER

    if (isAnisoSupported)
    {
        switch (mMaxAniso)
        {
        case TEX_ANISO_1_TO_1:
            RIO_GL_CALL(glSamplerParameterf(mSamplerInner, GL_TEXTURE_MAX_ANISOTROPY, 1.0f));
            break;
        case TEX_ANISO_2_TO_1:
            RIO_GL_CALL(glSamplerParameterf(mSamplerInner, GL_TEXTURE_MAX_ANISOTROPY, 2.0f));
            break;
        case TEX_ANISO_4_TO_1:
            RIO_GL_CALL(glSamplerParameterf(mSamplerInner, GL_TEXTURE_MAX_ANISOTROPY, 4.0f));
            break;
        case TEX_ANISO_8_TO_1:
            RIO_GL_CALL(glSamplerParameterf(mSamplerInner, GL_TEXTURE_MAX_ANISOTROPY, 8.0f));
            break;
        case TEX_ANISO_16_TO_1:
            RIO_GL_CALL(glSamplerParameterf(mSamplerInner, GL_TEXTURE_MAX_ANISOTROPY, 16.0f));
            break;
        }
    }
}

// some of these are not available on opengl es 3.0
#ifdef RIO_GLES
    #define GL_CLAMP_TO_BORDER GL_CLAMP_TO_EDGE
    #define GL_MIRROR_CLAMP_EXT GL_MIRRORED_REPEAT//GL_MIRROR_CLAMP_TO_EDGE_EXT
    #define GL_CLAMP_TO_EDGE_TO_BORDER GL_CLAMP_TO_EDGE
    #define GL_MIRROR_CLAMP_TO_BORDER_EXT GL_MIRRORED_REPEAT//GL_MIRROR_CLAMP_TO_EDGE_EXT
#endif

void TextureSampler2D::updateWrap_() const
{
    switch (mWrapX)
    {
    case TEX_WRAP_MODE_REPEAT:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_S, GL_REPEAT));
        break;
    case TEX_WRAP_MODE_MIRROR:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT));
        break;
    case TEX_WRAP_MODE_CLAMP:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        break;
    case TEX_WRAP_MODE_MIRROR_ONCE:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_S, GL_MIRROR_CLAMP_TO_EDGE_EXT));
        break;
    case TEX_WRAP_MODE_CLAMP_HALF_BORDER:
        // NOTE: GL_CLAMP was REMOVED in OpenGL 3.2 Core profile.
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));// GL_CLAMP));
        break;
    case TEX_WRAP_MODE_MIRROR_ONCE_HALF_BORDER:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_S, GL_MIRROR_CLAMP_EXT));
        break;
    case TEX_WRAP_MODE_CLAMP_BORDER:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
        break;
    case TEX_WRAP_MODE_MIRROR_ONCE_BORDER:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_S, GL_MIRROR_CLAMP_TO_BORDER_EXT));
        break;
    }

    switch (mWrapY)
    {
    case TEX_WRAP_MODE_REPEAT:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_T, GL_REPEAT));
        break;
    case TEX_WRAP_MODE_MIRROR:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT));
        break;
    case TEX_WRAP_MODE_CLAMP:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        break;
    case TEX_WRAP_MODE_MIRROR_ONCE:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_T, GL_MIRROR_CLAMP_TO_EDGE_EXT));
        break;
    case TEX_WRAP_MODE_CLAMP_HALF_BORDER:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));//GL_CLAMP));
        break;
    case TEX_WRAP_MODE_MIRROR_ONCE_HALF_BORDER:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_T, GL_MIRROR_CLAMP_EXT));
        break;
    case TEX_WRAP_MODE_CLAMP_BORDER:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
        break;
    case TEX_WRAP_MODE_MIRROR_ONCE_BORDER:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_T, GL_MIRROR_CLAMP_TO_BORDER_EXT));
        break;
    }

    switch (mWrapZ)
    {
    case TEX_WRAP_MODE_REPEAT:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_R, GL_REPEAT));
        break;
    case TEX_WRAP_MODE_MIRROR:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT));
        break;
    case TEX_WRAP_MODE_CLAMP:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));
        break;
    case TEX_WRAP_MODE_MIRROR_ONCE:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_R, GL_MIRROR_CLAMP_TO_EDGE_EXT));
        break;
    case TEX_WRAP_MODE_CLAMP_HALF_BORDER:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));//GL_CLAMP));
        break;
    case TEX_WRAP_MODE_MIRROR_ONCE_HALF_BORDER:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_R, GL_MIRROR_CLAMP_EXT));
        break;
    case TEX_WRAP_MODE_CLAMP_BORDER:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER));
        break;
    case TEX_WRAP_MODE_MIRROR_ONCE_BORDER:
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_WRAP_R, GL_MIRROR_CLAMP_TO_BORDER_EXT));
        break;
    }
}

void TextureSampler2D::updateBorderColor_() const
{
#ifndef RIO_GLES
    RIO_GL_CALL(glSamplerParameterfv(mSamplerInner, GL_TEXTURE_BORDER_COLOR, mBorderColor));
#endif
}

void TextureSampler2D::updateLOD_() const
{
    RIO_GL_CALL(glSamplerParameterf(mSamplerInner, GL_TEXTURE_MIN_LOD, mMinLOD));
    RIO_GL_CALL(glSamplerParameterf(mSamplerInner, GL_TEXTURE_MAX_LOD, mMaxLOD));
#ifndef RIO_GLES
    RIO_GL_CALL(glSamplerParameterf(mSamplerInner, GL_TEXTURE_LOD_BIAS, mLODBias));
#endif
}

void TextureSampler2D::updateDepthComp_() const
{
    if (mDepthCompareEnable)
    {
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE));
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_COMPARE_FUNC, mDepthCompareFunc));
    }
    else
    {
        RIO_GL_CALL(glSamplerParameteri(mSamplerInner, GL_TEXTURE_COMPARE_MODE, GL_NONE));
    }
}

void TextureSampler2D::bind(u32 vs_location, u32 fs_location, u32 slot) const
{
    RIO_ASSERT(vs_location == fs_location);
    u32 location = vs_location;

    RIO_ASSERT(location != 0xFFFFFFFF);
    RIO_ASSERT(isBindable());
    RIO_ASSERT(slot < 16);

    update();

    RIO_GL_CALL(glActiveTexture(GL_TEXTURE0 + slot));
    RIO_GL_CALL(glBindTexture(GL_TEXTURE_2D, mTexture2DHandle));

    RIO_GL_CALL(glBindSampler(slot, mSamplerInner));
    RIO_GL_CALL(glUniform1i(location, slot));

    RIO_GL_CALL(glActiveTexture(GL_TEXTURE0 + 16)); // Make sure subsequent calls to glBindTexture
                                                    // don't affect the current unit slot
}

}

#endif // RIO_IS_WIN
