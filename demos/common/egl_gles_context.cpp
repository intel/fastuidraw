#include <iostream>
#include <iomanip>

#include <SDL_syswm.h>
#include "egl_gles_context.hpp"

static
void
print_egl_errors(void)
{
  EGLint error;
  while((error = eglGetError()) != EGL_SUCCESS)
    {
#define lazy(X) case X: std::cout << #X; break
      switch(error)
        {
          lazy(EGL_NOT_INITIALIZED);
          lazy(EGL_BAD_ACCESS);
          lazy(EGL_BAD_ALLOC);
          lazy(EGL_BAD_ATTRIBUTE);
          lazy(EGL_BAD_CONTEXT);
          lazy(EGL_BAD_CONFIG);
          lazy(EGL_BAD_CURRENT_SURFACE);
          lazy(EGL_BAD_DISPLAY);
          lazy(EGL_BAD_SURFACE);
          lazy(EGL_BAD_MATCH);
          lazy(EGL_BAD_PARAMETER);
          lazy(EGL_BAD_NATIVE_PIXMAP);
          lazy(EGL_BAD_NATIVE_WINDOW);
          lazy(EGL_CONTEXT_LOST);
        default:
          std::cout << "Unknown error code 0x" << std::hex
                    << error << std::dec << "\n";
        }
    }
}

#define FASTUIDRAWassert_and_check_errors(X) do {         \
    print_egl_errors();                                   \
    FASTUIDRAWassert(X); } while(0)


egl_gles_context::
egl_gles_context(const params &P, SDL_Window *sdl)
{
  FASTUIDRAWunused(P);
  FASTUIDRAWunused(sdl);

  SDL_SysWMinfo wm;
  int egl_major(0), egl_minor(0);

  SDL_VERSION(&wm.version);
  SDL_GetWindowWMInfo(sdl, &wm);
  //FASTUIDRAWassert(wm.subsystem == SDL_SYSWM_X11);

  m_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(m_dpy, &egl_major, &egl_minor);
  FASTUIDRAWassert_and_check_errors(true);

  /* find a config.
   */
  EGLConfig config;
  {
    EGLint config_attribs[32];
    EGLint n(0), renderable_type(0), num_configs(0);
    EGLBoolean ret;

    #ifdef FASTUIDRAW_GL_USE_GLES
      {
        renderable_type |= EGL_OPENGL_ES3_BIT;
      }
    #else
      {
        renderable_type |= EGL_OPENGL_BIT;
      }
    #endif

    config_attribs[n++] = EGL_RED_SIZE;
    config_attribs[n++] = P.m_red_bits;
    config_attribs[n++] = EGL_GREEN_SIZE;
    config_attribs[n++] = P.m_green_bits;
    config_attribs[n++] = EGL_BLUE_SIZE;
    config_attribs[n++] = P.m_blue_bits;
    config_attribs[n++] = EGL_ALPHA_SIZE;
    config_attribs[n++] = P.m_alpha_bits;
    config_attribs[n++] = EGL_DEPTH_SIZE;
    config_attribs[n++] = P.m_depth_bits;
    config_attribs[n++] = EGL_STENCIL_SIZE;
    config_attribs[n++] = P.m_stencil_bits;
    config_attribs[n++] = EGL_SURFACE_TYPE;
    config_attribs[n++] = EGL_WINDOW_BIT;
    config_attribs[n++] = EGL_RENDERABLE_TYPE;
    config_attribs[n++] = renderable_type;

    config_attribs[n] = EGL_NONE;
    ret = eglChooseConfig(m_dpy, config_attribs, &config, 1, &num_configs);
    FASTUIDRAWassert_and_check_errors(ret);
    FASTUIDRAWassert(num_configs != 0);
    FASTUIDRAWunused(ret);
  }
  m_surface = eglCreateWindowSurface(m_dpy, config, wm.info.x11.window, nullptr);
  FASTUIDRAWassert_and_check_errors(m_surface != EGL_NO_SURFACE);

  {
    EGLint context_attribs[32];
    int n(0);

    context_attribs[n++] = EGL_CONTEXT_MAJOR_VERSION;
    context_attribs[n++] = P.m_gles_major_version;
    if(P.m_gles_minor_version != 0)
      {
	context_attribs[n++] = EGL_CONTEXT_MINOR_VERSION;
	context_attribs[n++] = P.m_gles_minor_version;
      }
    context_attribs[n++] = EGL_NONE;

#ifdef FASTUIDRAW_GL_USE_GLES
    eglBindAPI(EGL_OPENGL_ES_API);
#else
    eglBindAPI(EGL_OPENGL_API);
#endif

    m_ctx = eglCreateContext(m_dpy, config, EGL_NO_CONTEXT, context_attribs);
    FASTUIDRAWassert_and_check_errors(m_ctx != EGL_NO_CONTEXT);
    eglMakeCurrent(m_dpy, m_surface, m_surface, m_ctx);
  }
}

egl_gles_context::
~egl_gles_context()
{
  eglMakeCurrent(m_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(m_dpy, m_ctx);
  eglDestroySurface(m_dpy, m_surface);
  eglTerminate(m_dpy);
}


void
egl_gles_context::
make_current(void)
{
  eglMakeCurrent(m_dpy, m_surface, m_surface, m_ctx);
}

void
egl_gles_context::
swap_buffers(void)
{
  eglSwapBuffers(m_dpy, m_surface);
}

void*
egl_gles_context::
egl_get_proc(fastuidraw::c_string name)
{
  return (void*)eglGetProcAddress(name);
}

void
egl_gles_context::
print_info(std::ostream &dst)
{
  dst << "\nEGL extensions: " << eglQueryString(m_dpy, EGL_EXTENSIONS);
}
