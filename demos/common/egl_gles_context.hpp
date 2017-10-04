#pragma once

#include <EGL/egl.h>
#include <iostream>
#include <SDL.h>
#include <fastuidraw/util/util.hpp>
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

    int m_gles_major_version;
    int m_gles_minor_version;
  };

  egl_gles_context(const params &P, SDL_Window *w);
  ~egl_gles_context();

  void
  make_current(void);

  void
  swap_buffers(void);

  static
  void*
  egl_get_proc(fastuidraw::c_string name);

  void
  print_info(std::ostream &dst);

private:

  EGLContext m_ctx;
  EGLSurface m_surface;
  EGLDisplay m_dpy;
};
