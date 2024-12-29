#ifndef RIO_GFX_NATIVE_WINDOW_CAFE_H
#define RIO_GFX_NATIVE_WINDOW_CAFE_H

#include <misc/rio_Types.h>

#include <coreinit/memheap.h>
#include <gx2/context.h>
#include <gx2/texture.h>

namespace rio {
class Window;
enum ImageType
{
    IMAGE_TYPE_TV = 0,
    IMAGE_TYPE_DRC,
    IMAGE_TYPE_DEFAULT,
    IMAGE_TYPE_MAX
};
class NativeWindow
{
public:
    struct ImageBufferData {
        GX2ColorBuffer mColorBuffer;
        void* mpColorBufferImageData;

        GX2Texture mColorBufferTexture;

        GX2DepthBuffer mDepthBuffer;
        void* mpDepthBufferImageData;

        GX2Texture mDepthBufferTexture;
        void* mpDepthBufferTextureImageData;
    };
public:
    NativeWindow()
        : mpCmdList(nullptr)
        , mpContextState(nullptr)
        , mpTvScanBuffer(nullptr)
        , mpDrcScanBuffer(nullptr)
    {
    }

    GX2ContextState* getContextState() const { return mpContextState; }

    const GX2ColorBuffer& getColorBuffer(ImageType type) const { return mBufferData[type].mColorBuffer; }
    const GX2Texture& getColorBufferTexture(ImageType type) const { return mBufferData[type].mColorBufferTexture; }

    const GX2DepthBuffer& getDepthBuffer(ImageType type) const { return mBufferData[type].mDepthBuffer; }
    const GX2Texture& getDepthBufferTexture(ImageType type) const { return mBufferData[type].mDepthBufferTexture; }

private:
    void* mpCmdList;

    GX2ContextState* mpContextState;

    void* mpTvScanBuffer;
    void* mpDrcScanBuffer;

    ImageBufferData mBufferData[IMAGE_TYPE_MAX];
    mutable ImageType mCurrentImageType;

    MEMHeapHandle mHeapHandle_MEM1;
    MEMHeapHandle mHeapHandle_Fg;

    bool mIsRunning;

    friend class Window;
};

}

#endif // RIO_GFX_NATIVE_WINDOW_CAFE_H
