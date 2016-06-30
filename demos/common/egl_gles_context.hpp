#pragma once

#ifdef FASTUIDRAW_GL_USE_GLES
#include <EGL/egl.h>
#endif

#include <SDL.h>
#include <fastuidraw/util/reference_counted.hpp>

class egl_gles_context:
  public fastuidraw::reference_counted<egl_gles_context>::non_concurrent
{
public:

  class params
  {
  public:
    params(void):
      m_msaa(0)
    {}
    
    int m_red_bits, m_green_bits, m_blue_bits, m_alpha_bits;
    int m_depth_bits, m_stencil_bits;
    
    // 0 means no MSAA, all other values are enabled and number samples
    int m_msaa;

    int m_gles_version;
  };

  egl_gles_context(const params &P, SDL_Window *w);
  ~egl_gles_context();

  void
  make_current(void);

  void
  swap_buffers(void);

  static
  void*
  egl_get_proc(const char *name);
  
private:

#ifdef FASTUIDRAW_GL_USE_GLES
  EGLContext m_ctx;
  EGLSurface m_surface;
  EGLDisplay m_dpy;
#endif
};
