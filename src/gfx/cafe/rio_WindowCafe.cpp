#include <misc/rio_Types.h>

#if RIO_IS_CAFE

#include <audio/rio_AudioMgr.h>
#include <gfx/rio_Window.h>
#include <gpu/rio_RenderState.h>

#include <coreinit/foreground.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/memfrmheap.h>
#include <gx2/clear.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/state.h>
#include <gx2/swap.h>
#include <proc_ui/procui.h>

#include <whb/crash.h>
#include <whb/log_cafe.h>
#include <whb/log_udp.h>

extern "C" void OSBlockThreadsOnExit(void);
extern "C" void GX2ConvertDepthBufferToTextureSurface(const GX2DepthBuffer* src_buffer, GX2Surface* dst_surface, u32 dst_mip, u32 dst_slice);

namespace rio {

bool Window::foregroundAcquire_()
{
#ifdef RIO_DEBUG
    if (mNativeWindow.mIsRunning)
    {
        WHBInitCrashHandler();
        WHBLogCafeInit();
        WHBLogUdpInit();
    }
#endif // RIO_DEBUG

    // Get the MEM1 heap and Foreground Bucket heap handles
    mNativeWindow.mHeapHandle_MEM1 = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
    mNativeWindow.mHeapHandle_Fg = MEMGetBaseHeapHandle(MEM_BASE_HEAP_FG);

    if (!(mNativeWindow.mHeapHandle_MEM1 && mNativeWindow.mHeapHandle_Fg))
        return false;

    // Allocate TV scan buffer
    {
        GX2TVRenderMode tv_render_mode;

        // Get current TV scan mode
        GX2TVScanMode tv_scan_mode = GX2GetSystemTVScanMode();

        // Determine TV framebuffer dimensions (scan buffer, specifically)
        if (tv_scan_mode != GX2_TV_SCAN_MODE_576I && tv_scan_mode != GX2_TV_SCAN_MODE_480I
            && mWidth >= 1920 && mHeight >= 1080)
        {
            mWidth = 1920;
            mHeight = 1080;
            tv_render_mode = GX2_TV_RENDER_MODE_WIDE_1080P;
        }
        else if (mWidth >= 1280 && mHeight >= 720)
        {
            mWidth = 1280;
            mHeight = 720;
            tv_render_mode = GX2_TV_RENDER_MODE_WIDE_720P;
        }
        else if (mWidth >= 850 && mHeight >= 480)
        {
            mWidth = 854;
            mHeight = 480;
            tv_render_mode = GX2_TV_RENDER_MODE_WIDE_480P;
        }
        else // if (mWidth >= 640 && mHeight >= 480)
        {
            mWidth = 640;
            mHeight = 480;
            tv_render_mode = GX2_TV_RENDER_MODE_STANDARD_480P;
        }

        // Calculate TV scan buffer byte size
        u32 tv_scan_buffer_size, unk;
        GX2CalcTVSize(
            tv_render_mode,                       // Render Mode
            GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, // Scan Buffer Surface Format
            GX2_BUFFERING_MODE_DOUBLE,            // Two buffers for double-buffering
            &tv_scan_buffer_size,                 // Output byte size
            &unk                                  // Unknown; seems like we have no use for it
        );

        // Allocate TV scan buffer
        mNativeWindow.mpTvScanBuffer = MEMAllocFromFrmHeapEx(
            mNativeWindow.mHeapHandle_Fg,
            tv_scan_buffer_size,
            GX2_SCAN_BUFFER_ALIGNMENT // Required alignment
        );

        if (!mNativeWindow.mpTvScanBuffer)
            return false;

        // Flush allocated buffer from CPU cache
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, mNativeWindow.mpTvScanBuffer, tv_scan_buffer_size);

        // Set the current TV scan buffer
        GX2SetTVBuffer(
            mNativeWindow.mpTvScanBuffer,           // Scan Buffer
            tv_scan_buffer_size,                    // Scan Buffer Size
            tv_render_mode,                         // Render Mode
            GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,   // Scan Buffer Surface Format
            GX2_BUFFERING_MODE_DOUBLE               // Enable double-buffering
        );

        // Set the current TV scan buffer dimensions
        GX2SetTVScale(mWidth, mHeight);
    }

    // Allocate DRC (Gamepad) scan buffer
    {
        u32 drc_width = 854;
        u32 drc_height = 480;

        // Calculate DRC scan buffer byte size
        u32 drc_scan_buffer_size, unk;
        GX2CalcDRCSize(
            GX2_DRC_RENDER_MODE_SINGLE,           // Render Mode
            GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, // Scan Buffer Surface Format
            GX2_BUFFERING_MODE_DOUBLE,            // Two buffers for double-buffering
            &drc_scan_buffer_size,                // Output byte size
            &unk                                  // Unknown; seems like we have no use for it
        );

        // Allocate DRC scan buffer
        mNativeWindow.mpDrcScanBuffer = MEMAllocFromFrmHeapEx(
            mNativeWindow.mHeapHandle_Fg,
            drc_scan_buffer_size,
            GX2_SCAN_BUFFER_ALIGNMENT // Required alignment
        );

        if (!mNativeWindow.mpDrcScanBuffer)
            return false;

        // Flush allocated buffer from CPU cache
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, mNativeWindow.mpDrcScanBuffer, drc_scan_buffer_size);

        // Set the current DRC scan buffer
        GX2SetDRCBuffer(
            mNativeWindow.mpDrcScanBuffer,          // Scan Buffer
            drc_scan_buffer_size,                   // Scan Buffer Size
            GX2_DRC_RENDER_MODE_SINGLE,             // Render Mode
            GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,   // Scan Buffer Surface Format
            GX2_BUFFERING_MODE_DOUBLE               // Enable double-buffering
        );

        // Set the current DRC scan buffer dimensions
        GX2SetDRCScale(drc_width, drc_height);
    }

    // Initialize TV color buffer
    {
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.width = mWidth;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.height = mHeight;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.depth = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.mipLevels = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.aa = GX2_AA_MODE1X;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.mipmaps = nullptr;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.swizzle = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.viewMip = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.viewFirstSlice = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.viewNumSlices = 1;
        GX2CalcSurfaceSizeAndAlignment(&mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface);
        GX2InitColorBufferRegs(&mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer);

        // Allocate color buffer data
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpColorBufferImageData = MEMAllocFromFrmHeapEx(
            mNativeWindow.mHeapHandle_MEM1,
            mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.imageSize, // Data byte size
            mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.alignment  // Required alignment
        );

        if (!mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpColorBufferImageData)
            return false;

        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.image = mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpColorBufferImageData;

        // Flush allocated buffer from CPU cache
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpColorBufferImageData, mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface.imageSize);

        // Copy color buffer to a texture
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBufferTexture.surface = mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer.surface;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBufferTexture.surface.use = GX2_SURFACE_USE_TEXTURE;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBufferTexture.viewFirstMip = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBufferTexture.viewNumMips = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBufferTexture.viewFirstSlice = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBufferTexture.viewNumSlices = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBufferTexture.compMap = 0x00010203;
        GX2InitTextureRegs(&mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBufferTexture);
    }
    
    // Initialize TV depth buffer
    {
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.width = mWidth;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.height = mHeight;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.depth = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.mipLevels = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.format = GX2_SURFACE_FORMAT_FLOAT_D24_S8;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.aa = GX2_AA_MODE1X;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.use = GX2_SURFACE_USE_DEPTH_BUFFER;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.mipmaps = nullptr;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.swizzle = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.viewMip = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.viewFirstSlice = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.viewNumSlices = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.hiZPtr = nullptr;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.hiZSize = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.depthClear = 1.0f;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.stencilClear = 0;
        GX2CalcSurfaceSizeAndAlignment(&mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface);
        GX2InitDepthBufferRegs(&mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer);

        // Allocate depth buffer data
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpDepthBufferImageData = MEMAllocFromFrmHeapEx(
            mNativeWindow.mHeapHandle_MEM1,
            mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.imageSize, // Data byte size
            mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.alignment  // Required alignment
        );

        if (!mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpDepthBufferImageData)
            return false;

        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.image = mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpDepthBufferImageData;

        // Flush allocated buffer from CPU cache
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpDepthBufferImageData, mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBuffer.surface.imageSize);

        // Initialize depth buffer texture
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.width = mWidth;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.height = mHeight;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.depth = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.mipLevels = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.format = GX2_SURFACE_FORMAT_FLOAT_R32;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.aa = GX2_AA_MODE1X;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.use = GX2_SURFACE_USE_TEXTURE;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.mipmaps = nullptr;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.swizzle = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.viewFirstMip = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.viewNumMips = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.viewFirstSlice = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.viewNumSlices = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.compMap = 0x00040405;
        GX2CalcSurfaceSizeAndAlignment(&mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface);
        GX2InitTextureRegs(&mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture);

        // Allocate depth buffer texture data
        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpDepthBufferTextureImageData = MEMAllocFromFrmHeapEx(
            mNativeWindow.mHeapHandle_MEM1,
            mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.imageSize, // Data byte size
            mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.alignment  // Required alignment
        );

        if (!mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpDepthBufferTextureImageData)
            return false;

        mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.image = mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpDepthBufferTextureImageData;

        // Flush allocated buffer from CPU cache
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpDepthBufferTextureImageData, mNativeWindow.mBufferData[IMAGE_TYPE_TV].mDepthBufferTexture.surface.imageSize);
    }

    // Initialize DRC color buffer
    {
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.width = mWidth;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.height = mHeight;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.depth = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.mipLevels = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.aa = GX2_AA_MODE1X;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.mipmaps = nullptr;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.swizzle = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.viewMip = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.viewFirstSlice = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.viewNumSlices = 1;
        GX2CalcSurfaceSizeAndAlignment(&mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface);
        GX2InitColorBufferRegs(&mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer);

        // Allocate color buffer data
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpColorBufferImageData = MEMAllocFromFrmHeapEx(
            mNativeWindow.mHeapHandle_MEM1,
            mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.imageSize, // Data byte size
            mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.alignment  // Required alignment
        );

        if (!mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpColorBufferImageData)
            return false;

        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.image = mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpColorBufferImageData;

        // Flush allocated buffer from CPU cache
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpColorBufferImageData, mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface.imageSize);

        // Copy color buffer to a texture
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBufferTexture.surface = mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer.surface;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBufferTexture.surface.use = GX2_SURFACE_USE_TEXTURE;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBufferTexture.viewFirstMip = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBufferTexture.viewNumMips = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBufferTexture.viewFirstSlice = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBufferTexture.viewNumSlices = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBufferTexture.compMap = 0x00010203;
        GX2InitTextureRegs(&mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBufferTexture);
    }

    // Initialize DRC depth buffer
    {
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.width = mWidth;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.height = mHeight;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.depth = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.mipLevels = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.format = GX2_SURFACE_FORMAT_FLOAT_D24_S8;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.aa = GX2_AA_MODE1X;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.use = GX2_SURFACE_USE_DEPTH_BUFFER;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.mipmaps = nullptr;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.swizzle = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.viewMip = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.viewFirstSlice = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.viewNumSlices = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.hiZPtr = nullptr;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.hiZSize = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.depthClear = 1.0f;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.stencilClear = 0;
        GX2CalcSurfaceSizeAndAlignment(&mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface);
        GX2InitDepthBufferRegs(&mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer);

        // Allocate depth buffer data
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpDepthBufferImageData = MEMAllocFromFrmHeapEx(
            mNativeWindow.mHeapHandle_MEM1,
            mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.imageSize, // Data byte size
            mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.alignment  // Required alignment
        );

        if (!mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpDepthBufferImageData)
            return false;

        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.image = mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpDepthBufferImageData;

        // Flush allocated buffer from CPU cache
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpDepthBufferImageData, mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBuffer.surface.imageSize);

        // Initialize depth buffer texture
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.width = mWidth;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.height = mHeight;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.depth = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.mipLevels = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.format = GX2_SURFACE_FORMAT_FLOAT_R32;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.aa = GX2_AA_MODE1X;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.use = GX2_SURFACE_USE_TEXTURE;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.mipmaps = nullptr;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.swizzle = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.viewFirstMip = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.viewNumMips = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.viewFirstSlice = 0;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.viewNumSlices = 1;
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.compMap = 0x00040405;
        GX2CalcSurfaceSizeAndAlignment(&mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface);
        GX2InitTextureRegs(&mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture);

        // Allocate depth buffer texture data
        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpDepthBufferTextureImageData = MEMAllocFromFrmHeapEx(
            mNativeWindow.mHeapHandle_MEM1,
            mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.imageSize, // Data byte size
            mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.alignment  // Required alignment
        );

        if (!mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpDepthBufferTextureImageData)
            return false;

        mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.image = mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpDepthBufferTextureImageData;

        // Flush allocated buffer from CPU cache
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpDepthBufferTextureImageData, mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mDepthBufferTexture.surface.imageSize);
    }
    

    // Enable TV and DRC
    GX2SetTVEnable(true);
    GX2SetDRCEnable(true);

    // If not first time in foreground, restore the GX2 context state
    if (mNativeWindow.mIsRunning)
    {
        GX2SetContextState(mNativeWindow.mpContextState);
        GX2SetColorBuffer(&mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mColorBuffer, GX2_RENDER_TARGET_0);
        GX2SetDepthBuffer(&mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer);
    }

    // Initialize GQR2 to GQR5
    asm volatile ("mtspr %0, %1" : : "i" (898), "r" (0x00040004));
    asm volatile ("mtspr %0, %1" : : "i" (899), "r" (0x00050005));
    asm volatile ("mtspr %0, %1" : : "i" (900), "r" (0x00060006));
    asm volatile ("mtspr %0, %1" : : "i" (901), "r" (0x00070007));

    return true;
}

void Window::foregroundRelease_()
{
    if (mNativeWindow.mIsRunning)
        ProcUIDrawDoneRelease();

    if (mNativeWindow.mHeapHandle_Fg)
    {
        MEMFreeToFrmHeap(mNativeWindow.mHeapHandle_Fg, MEM_FRM_HEAP_FREE_ALL);
        mNativeWindow.mHeapHandle_Fg = nullptr;
    }
    mNativeWindow.mpTvScanBuffer = nullptr;
    mNativeWindow.mpDrcScanBuffer = nullptr;

    if (mNativeWindow.mHeapHandle_MEM1)
    {
        MEMFreeToFrmHeap(mNativeWindow.mHeapHandle_MEM1, MEM_FRM_HEAP_FREE_ALL);
        mNativeWindow.mHeapHandle_MEM1 = nullptr;
    }

    mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpColorBufferImageData = nullptr;
    mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpDepthBufferImageData = nullptr;
    mNativeWindow.mBufferData[IMAGE_TYPE_TV].mpDepthBufferTextureImageData = nullptr;

    mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpColorBufferImageData = nullptr;
    mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpDepthBufferImageData = nullptr;
    mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mpDepthBufferTextureImageData = nullptr;

#ifdef RIO_DEBUG
    WHBLogCafeDeinit();
    WHBLogUdpDeinit();
#endif // RIO_DEBUG
}

bool Window::initialize_()
{
    // Allocate GX2 command buffer
    mNativeWindow.mpCmdList = MEMAllocFromDefaultHeapEx(
        0x400000,                    // A very commonly used size in Nintendo games
        GX2_COMMAND_BUFFER_ALIGNMENT // Required alignment
    );

    if (!mNativeWindow.mpCmdList)
    {
        terminate_();
        return false;
    }

    // Several parameters to initialize GX2 with
    u32 initAttribs[] = {
        GX2_INIT_CMD_BUF_BASE, (uintptr_t)mNativeWindow.mpCmdList,  // Command Buffer Base Address
        GX2_INIT_CMD_BUF_POOL_SIZE, 0x400000,                       // Command Buffer Size
        GX2_INIT_ARGC, 0,                                           // main() arguments count
        GX2_INIT_ARGV, (uintptr_t)nullptr,                          // main() arguments vector
        GX2_INIT_END                                                // Ending delimiter
    };

    // Initialize GX2
    GX2Init(initAttribs);

    // Create TV and DRC scan, color and depth buffers
    if (!foregroundAcquire_())
    {
        terminate_();
        return false;
    }

    // Allocate context state instance
    mNativeWindow.mpContextState = (GX2ContextState*)MEMAllocFromDefaultHeapEx(
        sizeof(GX2ContextState),    // Size of context
        GX2_CONTEXT_STATE_ALIGNMENT // Required alignment
    );

    if (!mNativeWindow.mpContextState)
    {
        terminate_();
        return false;
    }

    // Initialize it to default state
    GX2SetupContextStateEx(mNativeWindow.mpContextState, false);

    // Make context of window current
    makeContextCurrent(IMAGE_TYPE_TV);

    // Set swap interval to 1 by default
    setSwapInterval(1);

    // Scissor test is always enabled in GX2

    // Initialize ProcUI
    ProcUIInit(&OSSavesDone_ReadyToRelease);

    mNativeWindow.mIsRunning = true;
    return true;
}

bool Window::isRunning() const
{
    return mNativeWindow.mIsRunning;
}

void Window::terminate_()
{
    mNativeWindow.mIsRunning = false;

    if (mNativeWindow.mpCmdList)
    {
        MEMFreeToDefaultHeap(mNativeWindow.mpCmdList);
        mNativeWindow.mpCmdList = nullptr;
    }

    if (mNativeWindow.mpContextState)
    {
        MEMFreeToDefaultHeap(mNativeWindow.mpContextState);
        mNativeWindow.mpContextState = nullptr;
    }

    AudioMgr::destroySingleton();

    foregroundRelease_();

    OSBlockThreadsOnExit();
    _Exit(-1);
}

void Window::makeContextCurrent(ImageType type) const
{
    if (type == IMAGE_TYPE_DEFAULT)
        type = mNativeWindow.mCurrentImageType;

    GX2SetContextState(mNativeWindow.mpContextState);
    GX2SetColorBuffer(&mNativeWindow.mBufferData[type].mColorBuffer, GX2_RENDER_TARGET_0);
    GX2SetDepthBuffer(&mNativeWindow.mBufferData[type].mDepthBuffer);

    mNativeWindow.mCurrentImageType = type;
}

void Window::setSwapInterval(u32 swap_interval)
{
    GX2SetSwapInterval(swap_interval);
}

void Window::swapBuffers() const
{
    // Disable Depth test to achieve same behavior as the MS Windows version
    RenderState render_state;
    render_state.setDepthTestEnable(false);
    render_state.apply();

    // Make sure to flush all commands to GPU before copying the color buffer to the scan buffers
    // (Calling GX2DrawDone instead here causes slow downs)
    GX2Flush();

    // Copy the color buffer to the TV and DRC scan buffers
    GX2CopyColorBufferToScanBuffer(&mNativeWindow.mBufferData[IMAGE_TYPE_TV].mColorBuffer, GX2_SCAN_TARGET_TV);
    GX2CopyColorBufferToScanBuffer(&mNativeWindow.mBufferData[IMAGE_TYPE_DRC].mColorBuffer, GX2_SCAN_TARGET_DRC);
    // Flip
    GX2SwapScanBuffers();

    // Reset context state for next frame
    GX2SetContextState(mNativeWindow.mpContextState);

    // Flush all commands to GPU before GX2WaitForFlip since it will block the CPU
    GX2Flush();

    // Make sure TV and DRC are enabled
    GX2SetTVEnable(true);
    GX2SetDRCEnable(true);

    // Wait until swapping is done
    GX2WaitForFlip();

    // ProcUI
    ProcUIStatus status = ProcUIProcessMessages(true);
    ProcUIStatus previous_status = status;

    if (status == PROCUI_STATUS_RELEASE_FOREGROUND)
    {
        const_cast<Window*>(this)->foregroundRelease_();
        status = ProcUIProcessMessages(true);
    }

    if (status == PROCUI_STATUS_EXITING ||
        (previous_status == PROCUI_STATUS_RELEASE_FOREGROUND &&
         status == PROCUI_STATUS_IN_FOREGROUND &&
         !const_cast<Window*>(this)->foregroundAcquire_()))
    {
        ProcUIShutdown();
        const_cast<Window*>(this)->terminate_();
    }
}

void Window::clearColor(f32 r, f32 g, f32 b, f32 a)
{
    // Clear using the given color
    // Does not need a current context to be set
    GX2ClearColor(&mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mColorBuffer, r, g, b, a);

    // GX2ClearColor invalidates the current context and the window context
    // must be made current again
    makeContextCurrent();
}

void Window::clearDepth()
{
    // Clear
    // Does not need a current context to be set
    GX2ClearDepthStencilEx(&mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer,
        mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer.depthClear,
        0,
        GX2_CLEAR_FLAGS_DEPTH);

    // GX2ClearDepthStencilEx invalidates the current context and the window context
    // must be made current again
    makeContextCurrent();
}

void Window::clearDepth(f32 depth)
{
    // Clear using the given depth
    // Does not need a current context to be set
    GX2ClearDepthStencilEx(&mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer,
        depth,
        0,
        GX2_CLEAR_FLAGS_DEPTH);

    // GX2ClearDepthStencilEx invalidates the current context and the window context
    // must be made current again
    makeContextCurrent();
}

void Window::clearStencil()
{
    // Clear
    // Does not need a current context to be set
    GX2ClearDepthStencilEx(&mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer,
        0,
        mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer.stencilClear,
        GX2_CLEAR_FLAGS_STENCIL);

    // GX2ClearDepthStencilEx invalidates the current context and the window context
    // must be made current again
    makeContextCurrent();
}

void Window::clearStencil(u8 stencil)
{
    // Clear using the given stencil
    // Does not need a current context to be set
    GX2ClearDepthStencilEx(&mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer,
                           0,
                           stencil,
                           GX2_CLEAR_FLAGS_STENCIL);

    // GX2ClearDepthStencilEx invalidates the current context and the window context
    // must be made current again
    makeContextCurrent();
}

void Window::clearDepthStencil()
{
    // Clear
    // Does not need a current context to be set
    GX2ClearDepthStencilEx(&mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer,
                           mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer.depthClear,
                           mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer.stencilClear,
                           (GX2ClearFlags)(GX2_CLEAR_FLAGS_DEPTH |
                                           GX2_CLEAR_FLAGS_STENCIL));

    // GX2ClearDepthStencilEx invalidates the current context and the window context
    // must be made current again
    makeContextCurrent();
}

void Window::clearDepthStencil(f32 depth, u8 stencil)
{
    // Clear using the given depth-stencil
    // Does not need a current context to be set
    GX2ClearDepthStencilEx(&mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer,
                           depth,
                           stencil,
                           (GX2ClearFlags)(GX2_CLEAR_FLAGS_DEPTH |
                                           GX2_CLEAR_FLAGS_STENCIL));

    // GX2ClearDepthStencilEx invalidates the current context and the window context
    // must be made current again
    makeContextCurrent();
}

void Window::updateDepthBufferTexture_()
{
    GX2Invalidate(GX2_INVALIDATE_MODE_DEPTH_BUFFER, mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer.surface.image, mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer.surface.imageSize);

    GX2ConvertDepthBufferToTextureSurface(
        &mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBuffer,
        &mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBufferTexture.surface,
        0,
        0
    );

    GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBufferTexture.surface.image, mNativeWindow.mBufferData[mNativeWindow.mCurrentImageType].mDepthBufferTexture.surface.imageSize);
}

}

#endif // RIO_IS_CAFE
