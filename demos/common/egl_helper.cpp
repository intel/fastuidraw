#include <iostream>
#include <iomanip>

#include <SDL_syswm.h>
#include "egl_helper.hpp"

#ifdef EGL_HELPER_DISABLED
egl_helper::
egl_helper(const fastuidraw::reference_counted_ptr<StreamHolder> &str,
           const params&, SDL_Window*)
{
  FASTUIDRAWassert(!"Platform does not support EGL");
  std::abort();
}

egl_helper::
~egl_helper()
{}

void
egl_helper::
make_current(void)
{}

void
egl_helper::
swap_buffers(void)
{}

void*
egl_helper::
egl_get_proc(fastuidraw::c_string)
{
  return nullptr;
}

void
egl_helper::
print_info(std::ostream&)
{}

#else

/* Shudders. FastUIDraws's ngl_egl.hpp is/was built with the system
 * defaults, which means the typedefs of EGL native types to X11
 * types. The Wayland types are different, but have the exact same
 * sizes. For C this is fine, but the header ngl_egl.hpp defines
 * the debug function C++ style in a namespace, which means the
 * symbols will be decorated with the argument types. We get around
 * this entire heartache by simply including the Wayland header
 * -AFTER- the EGL headers and we do C-style casts to do the deed.
 */
#include <fastuidraw/egl_binding.hpp>
#include <fastuidraw/ngl_egl.hpp>
#include <wayland-egl.h>

namespace
{
  void*
  get_proc(fastuidraw::c_string proc_name)
  {
    return (void*)eglGetProcAddress(proc_name);
  }

  class Logger:public fastuidraw::egl_binding::CallbackEGL
  {
  public:
    Logger(const fastuidraw::reference_counted_ptr<StreamHolder> &str):
      m_str(str)
    {}

    void
    pre_call(fastuidraw::c_string call_string_values,
             fastuidraw::c_string call_string_src,
             fastuidraw::c_string function_name,
             void *function_ptr,
             fastuidraw::c_string src_file, int src_line)
    {
      FASTUIDRAWunused(call_string_src);
      FASTUIDRAWunused(function_name);
      FASTUIDRAWunused(function_ptr);
      m_str->stream() << "Pre: [" << src_file << "," << src_line << "] "
                      << call_string_values << "\n";
    }

    void
    post_call(fastuidraw::c_string call_string_values,
              fastuidraw::c_string call_string_src,
              fastuidraw::c_string function_name,
              fastuidraw::c_string error_string,
              void *function_ptr,
              fastuidraw::c_string src_file, int src_line)
    {
      FASTUIDRAWunused(call_string_src);
      FASTUIDRAWunused(function_name);
      FASTUIDRAWunused(function_ptr);
      FASTUIDRAWunused(error_string);
      m_str->stream() << "Post: [" << src_file << "," << src_line << "] "
                      << call_string_values;

      if (error_string && *error_string)
        {
          m_str->stream() << "{" << error_string << "}";
        }
      m_str->stream() << "\n";
    }

  private:
    fastuidraw::reference_counted_ptr<StreamHolder> m_str;
  };

  EGLConfig
  choose_config(void *dpy, const egl_helper::params &P)
  {
    EGLint config_attribs[32];
    EGLint n(0), renderable_type(0), num_configs(0);
    EGLConfig ret;

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
    if (P.m_msaa > 0)
      {
        config_attribs[n++] = EGL_SAMPLE_BUFFERS;
        config_attribs[n++] = 1;
        config_attribs[n++] = EGL_SAMPLES;
        config_attribs[n++] = P.m_msaa;
      }

    config_attribs[n] = EGL_NONE;
    /* TODO: rather than just getting one config, examine
     * all the configs and choose one that matches tightest;
     * eglChooseConfig's ordering of choices places those
     * with deepest color-depths first, which means we actually
     * should walk the list to get closest match.
     */
    fastuidraw_eglChooseConfig(dpy, config_attribs, &ret, 1, &num_configs);
    FASTUIDRAWassert(num_configs != 0);
    return ret;
  }
}

egl_helper::
egl_helper(const fastuidraw::reference_counted_ptr<StreamHolder> &str,
           const params &P, SDL_Window *sdl):
  m_ctx(EGL_NO_CONTEXT),
  m_surface(EGL_NO_SURFACE),
  m_dpy(EGL_NO_DISPLAY),
  m_wl_window(nullptr)
{
  FASTUIDRAWunused(P);
  FASTUIDRAWunused(sdl);

  SDL_SysWMinfo wm;
  int egl_major(0), egl_minor(0);
  EGLNativeWindowType egl_window;
  EGLNativeDisplayType egl_display;

  SDL_VERSION(&wm.version);
  SDL_GetWindowWMInfo(sdl, &wm);

  switch (wm.subsystem)
    {
    case SDL_SYSWM_X11:
      egl_window = (EGLNativeWindowType)wm.info.x11.window;
      egl_display = (EGLNativeDisplayType)wm.info.x11.display;
      break;

    case SDL_SYSWM_WAYLAND:
      int width, height;
      SDL_GetWindowSize(sdl, &width, &height);
      m_wl_window = wl_egl_window_create(wm.info.wl.surface, width, height);
      egl_window = (EGLNativeWindowType)m_wl_window;
      egl_display = (EGLNativeDisplayType)wm.info.wl.display;
      break;

    default:
      FASTUIDRAWassert(!"Unsupported Platform for EGL\n");
      std::abort();
    }

  if (str)
    {
      m_logger = FASTUIDRAWnew Logger(str);
    }
  fastuidraw::egl_binding::get_proc_function(get_proc);
  m_dpy = fastuidraw_eglGetDisplay(egl_display);
  fastuidraw_eglInitialize(m_dpy, &egl_major, &egl_minor);

  /* find a config.
   */
  EGLConfig config;
  config = choose_config(m_dpy, P);
  m_surface = fastuidraw_eglCreateWindowSurface(m_dpy, config, egl_window, nullptr);

  EGLint context_attribs[32];
  int n(0);

  context_attribs[n++] = EGL_CONTEXT_MAJOR_VERSION;
  context_attribs[n++] = P.m_gles_major_version;
  if (P.m_gles_minor_version != 0)
    {
      context_attribs[n++] = EGL_CONTEXT_MINOR_VERSION;
      context_attribs[n++] = P.m_gles_minor_version;
    }
  context_attribs[n++] = EGL_NONE;

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      fastuidraw_eglBindAPI(EGL_OPENGL_ES_API);
    }
  #else
    {
      fastuidraw_eglBindAPI(EGL_OPENGL_API);
    }
  #endif

  std::cout << "\n\nUsing EGL!\n\n";

  m_ctx = fastuidraw_eglCreateContext(m_dpy, config, EGL_NO_CONTEXT, context_attribs);
  fastuidraw_eglMakeCurrent(m_dpy, m_surface, m_surface, m_ctx);
}

egl_helper::
~egl_helper()
{
  fastuidraw_eglMakeCurrent(m_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  fastuidraw_eglDestroyContext(m_dpy, m_ctx);
  fastuidraw_eglDestroySurface(m_dpy, m_surface);
  fastuidraw_eglTerminate(m_dpy);

  if (m_wl_window)
    {
      wl_egl_window_destroy(m_wl_window);
    }
}


void
egl_helper::
make_current(void)
{
  fastuidraw_eglMakeCurrent(m_dpy, m_surface, m_surface, m_ctx);
}

void
egl_helper::
swap_buffers(void)
{
  fastuidraw_eglSwapBuffers(m_dpy, m_surface);
}

void*
egl_helper::
egl_get_proc(fastuidraw::c_string name)
{
  return (void*)eglGetProcAddress(name);
}

void
egl_helper::
print_info(std::ostream &dst)
{
  dst << "\nEGL extensions: " << fastuidraw_eglQueryString(m_dpy, EGL_EXTENSIONS);
}

#endif
