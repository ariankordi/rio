#if !defined(RIO_NO_GLAD_IMPLEMENTATION) && !defined(RIO_USE_GLEW)
    #ifdef RIO_GLES
        #define GLAD_EGL_IMPLEMENTATION
        #define GLAD_GLES2_IMPLEMENTATION
    #else
        #define GLAD_GL_IMPLEMENTATION
    #endif
#endif

#include <misc/rio_Types.h>

#if RIO_IS_WIN

#include <gfx/rio_Window.h>
#include <gfx/lyr/rio_Layer.h>
#include <gpu/rio_RenderState.h>
#include <gpu/rio_Shader.h>
#include <gpu/rio_VertexArray.h>

/*
#ifndef __EMSCRIPTEN__
    #define GLFW_EXPOSE_NATIVE_EGL 1
    #include <GLFW/glfw3native.h>
#endif
*/
namespace {

static rio::Shader gScreenShader;
static rio::VertexArray* gVertexArray = nullptr;
static rio::VertexBuffer* gVertexBuffer = nullptr;

struct Vertex
{
    f32 pos[2];
    f32 texCoord[2];
};

static rio::VertexStream gPosStream     (0, rio::VertexStream::FORMAT_32_32_FLOAT, offsetof(Vertex, pos));
static rio::VertexStream gTexCoordStream(1, rio::VertexStream::FORMAT_32_32_FLOAT, offsetof(Vertex, texCoord));

static const Vertex vertices[] = {
    { { -1.0f,  1.0f }, { 0.0f, 1.0f } },
    { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
    { {  1.0f, -1.0f }, { 1.0f, 0.0f } },
    { {  1.0f, -1.0f }, { 1.0f, 0.0f } },
    { {  1.0f,  1.0f }, { 1.0f, 1.0f } },
    { { -1.0f,  1.0f }, { 0.0f, 1.0f } }
};

}

namespace rio {

void Window::resizeCallback_(s32 width, s32 height)
{
    width  = std::max<s32>(1, width );
    height = std::max<s32>(1, height);

    mWidth = width;
    mHeight = height;

    // Set Color Buffer dimensions and format
    RIO_GL_CALL(glBindTexture(GL_TEXTURE_2D, mNativeWindow.mColorBufferTextureHandle));
    RIO_GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
    RIO_GL_CALL(glBindTexture(GL_TEXTURE_2D, GL_NONE));

#if !defined(RIO_GLES) || defined(GL_ES_VERSION_3_0) // GL ES not version 3.0
    // Set Depth-Stencil Buffer dimensions and format
    RIO_GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, mNativeWindow.mDepthBufferHandle));
    RIO_GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mWidth, mHeight));
    RIO_GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, GL_NONE));

    // Set Depth-Stencil Buffer dimensions and format
    RIO_GL_CALL(glBindTexture(GL_TEXTURE_2D, mNativeWindow.mDepthBufferTextureHandle));
    RIO_GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, mWidth, mHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr));
    RIO_GL_CALL(glBindTexture(GL_TEXTURE_2D, GL_NONE));
#endif // !defined(RIO_GLES) || defined(GL_ES_VERSION_3_0)

    lyr::Layer::onResize_(width, height);

    const auto& callback = mNativeWindow.mpOnResizeCallback;
    if (callback == nullptr)
        return;

    (*callback)(width, height);
}

void Window::resizeCallback_(GLFWwindow* glfw_window, s32 width, s32 height)
{
    Window* const window = Window::instance();
    if (window == nullptr || window->getNativeWindow().getGLFWwindow() != glfw_window)
        return;

    window->resizeCallback_(width, height);
}

void errorCallbackForGLFW(int error, const char* msg)
{
    RIO_LOG("GLFW error %d: %s\n", error, msg);
}

bool Window::initialize_(bool resizable, bool invisible, u32 gl_major, u32 gl_minor)
{
#ifdef RIO_NO_GLFW_CALLS
    RIO_LOG("WARNING: Window::initialize_ was called, but this was built with the definition RIO_NO_GLFW_CALLS set, which means that no GLFW calls will be made, and no windows will ever be created. This program will probably crash now.\n");
#else
    glfwSetErrorCallback(errorCallbackForGLFW);

#ifdef RIO_USE_OSMESA
    // only works on glfw 3.4 and higher
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
#endif

    // Initialize GLFW
    if (!glfwInit())
    {
        RIO_LOG("Failed to initialize GLFW.\n");
        return false;
    }
    /*if (resizable)
    {
        // Start maximized if resizable
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    }
    else*/
    if (!resizable)
    {
        // Disable resizing
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    }

    if (invisible)
    {
        // make window invisible for headless operations
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

#ifndef __EMSCRIPTEN__
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, 1);
#endif // __EMSCRIPTEN__
    // Request OpenGL Core Profile
    RIO_LOG("OpenGL Context Version: %u.%u\n", gl_major, gl_minor);
#ifdef RIO_GLES
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#endif
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl_minor);
  //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(RIO_GLES) && !defined(__EMSCRIPTEN__)
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
#elif defined(RIO_USE_OSMESA)
    // "OpenGL ES is not available on OSMesa"
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_OSMESA_CONTEXT_API);
#endif

    // Enforce double-buffering
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);


    // Create the window instance
    mNativeWindow.mpGLFWwindow = glfwCreateWindow(mWidth, mHeight, "Game", nullptr, nullptr);
    if (!mNativeWindow.mpGLFWwindow)
    {
        RIO_LOG("Failed to create GLFW window.\n");
        terminate_();
        return false;
    }

    // Query the Frame Buffer size
    glfwGetFramebufferSize(mNativeWindow.mpGLFWwindow,
                           reinterpret_cast<int*>(&mWidth),
                           reinterpret_cast<int*>(&mHeight));

    // Make context of window current
    glfwMakeContextCurrent(mNativeWindow.mpGLFWwindow);
/*
#ifndef __EMSCRIPTEN__
    EGLDisplay display = glfwGetEGLDisplay();
    int egl_version = gladLoaderLoadEGL(display);
    printf("EGL %d.%d\n", GLAD_VERSION_MAJOR(egl_version), GLAD_VERSION_MINOR(egl_version));
#endif
*/

#ifndef RIO_NO_GL_LOADER
    #if RIO_USE_GLEW
            GLenum err = glewInit();
        #ifndef __EMSCRIPTEN__
            if (err != GLEW_OK && err != GLEW_ERROR_NO_GLX_DISPLAY)
        #else
            if (err != GLEW_OK)
        #endif // __EMSCRIPTEN__
            {
                RIO_LOG("GLEW Initialization Error: %s (code: %d)\n", glewGetErrorString(err), err);
                terminate_();
                return false;
            }
    #else
        // use GLAD by default
        #ifdef RIO_GLES
            gladLoadGLES2(glfwGetProcAddress);
        #else
            gladLoadGL(glfwGetProcAddress);
        #endif // RIO_GLES
    #endif // RIO_USE_GLEW

#else
    RIO_LOG("WARNING: In Window::initialize_, but RIO_NO_GL_LOADER was defined. This will probably crash right now if no other GL loader is resident.\n");
#endif // RIO_NO_GL_LOADER

    // Retrieve and log the renderer string
    const char* renderer_str = (const char*)glGetString(GL_RENDERER);
    if (renderer_str)
    {
        RIO_LOG("OpenGL Renderer: %s\n", renderer_str);
    }
    else
    {
        RIO_LOG("Failed to retrieve the renderer string.\n");
    }
    // Retrieve and log the OpenGL version string
    const char* version_str = (const char*)glGetString(GL_VERSION);
    if (version_str)
    {
        RIO_LOG("OpenGL Version: %s\n", version_str);
    }
    else
    {
        RIO_LOG("Failed to retrieve the OpenGL version string.\n");
    }

    // Retrieve and log the OpenGL core profile version string
    const char* core_profile_version_str = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (core_profile_version_str)
    {
        RIO_LOG("OpenGL GLSL Version: %s\n", core_profile_version_str);
    }
    else
    {
        RIO_LOG("Failed to retrieve the OpenGL core profile version string.\n");
    }

    // Set swap interval to 1 by default
    setSwapInterval(1);

    // Check clip control extension
    #ifndef RIO_NO_CLIP_CONTROL
#if RIO_USE_GLEW
    if (!GLEW_VERSION_4_5 && !GLEW_ARB_clip_control)
#else
    if (!(GLAD_GL_VERSION_4_5 || GLAD_GL_ARB_clip_control))
#endif // RIO_USE_GLEW
    {
        printf("Required OpenGL extensions not supported: GL_VERSION_4_5, GL_ARB_clip_control. Continuing anyway.\n");

        /*terminate_();
        return false;*/
    }
    else
    {
        // Change coordinate-system to be compliant with GX2
        RIO_GL_CALL(glClipControl(GL_UPPER_LEFT, GL_NEGATIVE_ONE_TO_ONE));
    }
    #endif

    // The screen will now be rendered upside-down.
    // Therefore, we will render it to our own frame buffer, then render that
    // frame buffer upside-down to the window frame buffer.

    // Load screen shader
    gScreenShader.load("screen_shader_win");

    // Create and setup vertex array and buffer
    gVertexArray = new VertexArray();
    gVertexBuffer = new VertexBuffer(vertices, sizeof(vertices), sizeof(Vertex), 0);
    if (!gVertexArray || !gVertexBuffer)
    {
        RIO_LOG("Failed to create vertex array or buffer.\n");
        terminate_();
        return false;
    }

    // Process Vertex Array
    gVertexArray->addAttribute(gPosStream, *gVertexBuffer);
    gVertexArray->addAttribute(gTexCoordStream, *gVertexBuffer);
    gVertexArray->process();

    // Create and bind the Frame Buffer
    if (!createFb_())
    {
        RIO_LOG("Failed to create frame buffer.\n");
        terminate_();
        return false;
    }
    RIO_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mNativeWindow.mFramebufferHandle));
    RIO_GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mNativeWindow.mColorBufferTextureHandle, 0));
#if !defined(RIO_GLES) || defined(GL_ES_VERSION_3_0)
    RIO_GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mNativeWindow.mDepthBufferHandle));
#endif

    // Enable scissor test
    RIO_GL_CALL(glEnable(GL_SCISSOR_TEST));

    // Set callback if window is resizable
    if (resizable)
        glfwSetFramebufferSizeCallback(mNativeWindow.mpGLFWwindow, &Window::resizeCallback_);
#endif // RIO_NO_GLFW_CALLS
    return true;
}

bool Window::isRunning() const
{
#ifndef RIO_NO_GLFW_CALLS
    return !glfwWindowShouldClose(mNativeWindow.mpGLFWwindow);
#else
    return false;
#endif
}

void Window::terminate_()
{
    destroyFb_();

    if (gVertexBuffer)
    {
        delete gVertexBuffer;
        gVertexBuffer = nullptr;
    }

    if (gVertexArray)
    {
        delete gVertexArray;
        gVertexArray = nullptr;
    }

    gScreenShader.unload();
#ifndef RIO_NO_GLFW_CALLS
    glfwTerminate();
#endif
}

bool Window::createFb_()
{
    // Generate OpenGL Frame Buffer
    RIO_GL_CALL(glGenFramebuffers(1, &mNativeWindow.mFramebufferHandle));
    if (mNativeWindow.mFramebufferHandle == GL_NONE)
        return false;

    // Bind it
    RIO_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mNativeWindow.mFramebufferHandle));

    // Generate Color Buffer as OpenGL texture
    RIO_GL_CALL(glGenTextures(1, &mNativeWindow.mColorBufferTextureHandle));
    if (mNativeWindow.mColorBufferTextureHandle == GL_NONE)
        return false;

    // Set Color Buffer dimensions and format
    RIO_GL_CALL(glBindTexture(GL_TEXTURE_2D, mNativeWindow.mColorBufferTextureHandle));
#if !defined(RIO_GLES) || defined(GL_ES_VERSION_3_0)
    RIO_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0));
    RIO_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0));
#endif
    RIO_GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
    RIO_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    RIO_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    RIO_GL_CALL(glBindTexture(GL_TEXTURE_2D, GL_NONE));
    mNativeWindow.mColorBufferTextureFormat = TEXTURE_FORMAT_R8_G8_B8_A8_UNORM;

    // Attach it to the Frame Buffer
    RIO_GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mNativeWindow.mColorBufferTextureHandle, 0));

#if !defined(RIO_GLES) || defined(GL_ES_VERSION_3_0)
    // Generate Depth-Stencil Buffer as OpenGL render target
    RIO_GL_CALL(glGenRenderbuffers(1, &mNativeWindow.mDepthBufferHandle));
    if (mNativeWindow.mDepthBufferHandle == GL_NONE)
        return false;

    // Set Depth-Stencil Buffer dimensions and format
    RIO_GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, mNativeWindow.mDepthBufferHandle));
    RIO_GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mWidth, mHeight));
    RIO_GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, GL_NONE));

    // Attach it to the Frame Buffer
    RIO_GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mNativeWindow.mDepthBufferHandle));

    // Check Frame Buffer completeness
    GLenum framebuffer_status;
    RIO_GL_CALL(framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
    if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    // Generate Depth-Stencil Buffer copy texture
    RIO_GL_CALL(glGenTextures(1, &mNativeWindow.mDepthBufferTextureHandle));
    if (mNativeWindow.mDepthBufferTextureHandle == GL_NONE)
        return false;

    // Set Depth-Stencil Buffer dimensions and format
    RIO_GL_CALL(glBindTexture(GL_TEXTURE_2D, mNativeWindow.mDepthBufferTextureHandle));
    RIO_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0));
    RIO_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0));
    RIO_GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, mWidth, mHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr));
    RIO_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    RIO_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    RIO_GL_CALL(glBindTexture(GL_TEXTURE_2D, GL_NONE));
    mNativeWindow.mDepthBufferTextureFormat = TextureFormat(0x00000011);

    // Create the source framebuffer with a depth-stencil renderbuffer
    RIO_GL_CALL(glGenFramebuffers(1, &mNativeWindow.mDepthBufferCopyFramebufferSrc));
    if (mNativeWindow.mDepthBufferCopyFramebufferSrc == GL_NONE)
        return false;

    // Bind it
    RIO_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mNativeWindow.mDepthBufferCopyFramebufferSrc));

    // Attach the depth-stencil renderbuffer
    RIO_GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mNativeWindow.mDepthBufferHandle));

    // Check Frame Buffer completeness
    RIO_GL_CALL(framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
    if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    // Create the destination framebuffer with a depth-stencil texture
    RIO_GL_CALL(glGenFramebuffers(1, &mNativeWindow.mDepthBufferCopyFramebufferDst));
    if (mNativeWindow.mDepthBufferCopyFramebufferDst == GL_NONE)
        return false;

    // Bind it
    RIO_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mNativeWindow.mDepthBufferCopyFramebufferDst));

    // Attach the depth-stencil texture
    RIO_GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, mNativeWindow.mDepthBufferTextureHandle, 0));

    // Check Frame Buffer completeness
    RIO_GL_CALL(framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
    if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE)
        return false;

#endif // !defined(RIO_GLES) || defined(GL_ES_VERSION_3_0)

    return true;
}

void Window::destroyFb_()
{
    if (mNativeWindow.mDepthBufferCopyFramebufferDst != GL_NONE)
    {
        RIO_GL_CALL(glDeleteFramebuffers(1, &mNativeWindow.mDepthBufferCopyFramebufferDst));
        mNativeWindow.mDepthBufferCopyFramebufferDst = GL_NONE;
    }

    if (mNativeWindow.mDepthBufferCopyFramebufferSrc != GL_NONE)
    {
        RIO_GL_CALL(glDeleteFramebuffers(1, &mNativeWindow.mDepthBufferCopyFramebufferSrc));
        mNativeWindow.mDepthBufferCopyFramebufferSrc = GL_NONE;
    }

    if (mNativeWindow.mDepthBufferTextureHandle != GL_NONE)
    {
        RIO_GL_CALL(glDeleteTextures(1, &mNativeWindow.mDepthBufferTextureHandle));
        mNativeWindow.mDepthBufferTextureHandle = GL_NONE;
    }

    if (mNativeWindow.mDepthBufferHandle != GL_NONE)
    {
        RIO_GL_CALL(glDeleteRenderbuffers(1, &mNativeWindow.mDepthBufferHandle));
        mNativeWindow.mDepthBufferHandle = GL_NONE;
    }

    if (mNativeWindow.mColorBufferTextureHandle != GL_NONE)
    {
        RIO_GL_CALL(glDeleteTextures(1, &mNativeWindow.mColorBufferTextureHandle));
        mNativeWindow.mColorBufferTextureHandle = GL_NONE;
    }

    if (mNativeWindow.mFramebufferHandle != GL_NONE)
    {
        RIO_GL_CALL(glDeleteFramebuffers(1, &mNativeWindow.mFramebufferHandle));
        mNativeWindow.mFramebufferHandle = GL_NONE;
    }
}

void Window::makeContextCurrent() const
{
#ifndef RIO_NO_GLFW_CALLS
    glfwMakeContextCurrent(mNativeWindow.mpGLFWwindow);
#endif
    // Bind our Frame Buffer
    RIO_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mNativeWindow.mFramebufferHandle));
    RIO_GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mNativeWindow.mColorBufferTextureHandle, 0));
#if !defined(RIO_GLES) || defined(GL_ES_VERSION_3_0)
    RIO_GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mNativeWindow.mDepthBufferHandle));
#endif
}

void Window::setSwapInterval(u32 swap_interval)
{
#ifndef RIO_NO_GLFW_CALLS
    glfwSwapInterval(0);
#endif
    mNativeWindow.setSwapInterval_(swap_interval);
}

void Window::setVpToFb_() const
{
    RIO_GL_CALL(glViewport(0, 0, mWidth, mHeight));
    RIO_GL_CALL(glDepthRange(0.f, 1.f));
    RIO_GL_CALL(glScissor(0, 0, mWidth, mHeight));
}

void Window::restoreVp_() const
{
    RIO_GL_CALL(glViewport(Graphics::sViewportX, Graphics::sViewportY, Graphics::sViewportWidth, Graphics::sViewportHeight));
    RIO_GL_CALL(glDepthRange(Graphics::sViewportNear, Graphics::sViewportFar));
    RIO_GL_CALL(glScissor(Graphics::sScissorX, Graphics::sScissorY, Graphics::sScissorWidth, Graphics::sScissorHeight));
}

void Window::swapBuffers() const
{
    // Bind the default (window) Frame Buffer
    RIO_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE));

    // Set viewport and scissor
    setVpToFb_();

    // Disable Depth test
    RenderState render_state;
    render_state.setDepthTestEnable(false);
    render_state.apply();

    // Bind the screen shader and vertex array
    gScreenShader.bind();
    gVertexArray->bind();

    // Bind screen texture (Color Buffer texture)
    RIO_GL_CALL(glActiveTexture(GL_TEXTURE0));
    RIO_GL_CALL(glBindTexture(GL_TEXTURE_2D, mNativeWindow.mColorBufferTextureHandle));

    // Draw it
    RIO_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
#ifndef RIO_NO_GLFW_CALLS
    // Swap front and back buffers
    glfwSwapBuffers(mNativeWindow.mpGLFWwindow);
    mNativeWindow.onSwapBuffers_();
    // Poll for and process events
    glfwPollEvents();

    // Restore our Frame Buffer
    RIO_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mNativeWindow.mFramebufferHandle));

    // Restore viewport and scissor
    restoreVp_();
#ifdef RIO_DEBUG
    // FPS calculation and update
    static double lastTime = glfwGetTime();
    static int nbFrames = 0;

    // Measure speed
    double currentTime = glfwGetTime();
    nbFrames++;
    if (currentTime - lastTime >= 1.0)
    { // If last update was more than 1 second ago
        double fps = double(nbFrames) / (currentTime - lastTime);

        char title[32];
        snprintf(title, sizeof(title), "FPS: %.2f", fps);

        glfwSetWindowTitle(mNativeWindow.mpGLFWwindow, title);

        nbFrames = 0;
        lastTime = currentTime;
    }
#endif // RIO_DEBUG
#endif // RIO_NO_GLFW_CALLS
}

void Window::clearColor(f32 r, f32 g, f32 b, f32 a)
{
    // Make sure window context is current
    makeContextCurrent();

    // Set viewport and scissor
    setVpToFb_();

    // Clear using the given color
    RIO_GL_CALL(glClearColor(r, g, b, a));
    RIO_GL_CALL(glClear(GL_COLOR_BUFFER_BIT));

    // Restore viewport and scissor
    restoreVp_();
}

void Window::clearDepth()
{
    // Make sure window context is current
    makeContextCurrent();

    // Set viewport and scissor
    setVpToFb_();

    // Clear
    RIO_GL_CALL(glDepthMask(GL_TRUE));
    RIO_GL_CALL(glClearDepth(1.0f));
    RIO_GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));

    // Restore viewport and scissor
    restoreVp_();
}

void Window::clearDepth(f32 depth)
{
    // Make sure window context is current
    makeContextCurrent();

    // Set viewport and scissor
    setVpToFb_();

    // Clear
    RIO_GL_CALL(glDepthMask(GL_TRUE));
    RIO_GL_CALL(glClearDepth(depth));
    RIO_GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));

    // Restore viewport and scissor
    restoreVp_();
}

void Window::clearStencil()
{
    // Make sure window context is current
    makeContextCurrent();

    // Set viewport and scissor
    setVpToFb_();

    // Clear
    RIO_GL_CALL(glStencilMask(0xFF));
    RIO_GL_CALL(glClearStencil(0));
    RIO_GL_CALL(glClear(GL_STENCIL_BUFFER_BIT));

    // Restore viewport and scissor
    restoreVp_();
}

void Window::clearStencil(u8 stencil)
{
    // Make sure window context is current
    makeContextCurrent();

    // Set viewport and scissor
    setVpToFb_();

    // Clear
    RIO_GL_CALL(glStencilMask(0xFF));
    RIO_GL_CALL(glClearStencil(stencil));
    RIO_GL_CALL(glClear(GL_STENCIL_BUFFER_BIT));

    // Restore viewport and scissor
    restoreVp_();
}

void Window::clearDepthStencil()
{
    // Make sure window context is current
    makeContextCurrent();

    // Set viewport and scissor
    setVpToFb_();

    // Clear
    RIO_GL_CALL(glDepthMask(GL_TRUE));
    RIO_GL_CALL(glClearDepth(1.0f));
    RIO_GL_CALL(glStencilMask(0xFF));
    RIO_GL_CALL(glClearStencil(0));
    RIO_GL_CALL(glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

    // Restore viewport and scissor
    restoreVp_();
}

void Window::clearDepthStencil(f32 depth, u8 stencil)
{
    // Make sure window context is current
    makeContextCurrent();

    // Set viewport and scissor
    setVpToFb_();

    // Clear
    RIO_GL_CALL(glDepthMask(GL_TRUE));
    RIO_GL_CALL(glClearDepth(depth));
    RIO_GL_CALL(glStencilMask(0xFF));
    RIO_GL_CALL(glClearStencil(stencil));
    RIO_GL_CALL(glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

    // Restore viewport and scissor
    restoreVp_();
}

void Window::updateDepthBufferTexture_()
{
#if defined(RIO_GLES) && !defined(GL_ES_VERSION_3_0)
    RIO_ASSERT(false);
#else
    Graphics::setViewport(0, 0, mWidth, mHeight);
    Graphics::setScissor(0, 0, mWidth, mHeight);

    // Blit the depth-stencil renderbuffer to the depth-stencil texture
    RIO_GL_CALL(glBindFramebuffer(GL_READ_FRAMEBUFFER, mNativeWindow.mDepthBufferCopyFramebufferSrc));
    RIO_GL_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mNativeWindow.mDepthBufferCopyFramebufferDst));
    RIO_GL_CALL(glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST));
#endif
}

}

#endif // RIO_IS_WIN
