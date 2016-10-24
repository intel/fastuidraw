/*!
 * \file sdl_demo.cpp
 * \brief file sdl_demo.cpp
 *
 * Adapted from: sdl_demo.cpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */


#include <typeinfo>
#include <fstream>
#include <SDL_syswm.h>
#include <SDL_video.h>
#include <SDL_render.h>
#include <SDL_surface.h>

#if HAVE_CAIRO_GL
  #include <cairo-gl.h>
#endif

#include "generic_command_line.hpp"
#include "simple_time.hpp"
#include "sdl_cairo_demo.hpp"

namespace
{
  bool
  is_help_request(const std::string &v)
  {
    return v==std::string("-help")
      or v==std::string("--help")
      or v==std::string("-h");
  }
}

sdl_cairo_demo::
sdl_cairo_demo(const std::string &about_text):
  m_handle_events(true),
  m_cairo(NULL),
  m_about(command_line_argument::tabs_to_spaces(command_line_argument::format_description_string("", about_text))),
  m_common_label("Screen Option", *this),
  m_fullscreen(false, "fullscreen", "fullscreen mode", *this),
  m_hide_cursor(false, "hide_cursor", "If true, hide the mouse cursor with a SDL call", *this),
  m_width(800, "width", "window width", *this),
  m_height(480, "height", "window height", *this),
  m_show_framerate(false, "show_framerate", "if true show the cumulative framerate at end", *this),
  m_backend(backend_cairo_offscreen_data_surface,
            enumerated_string_type<enum backend_cairo_t>()
            .add_entry("xlib_onscreen", backend_cairo_xlib_on_screen,
                       "render directly to X window surface")
            .add_entry("xlib_offscreen", backend_cairo_xlib_off_screen,
                       "render to X Pixmap then blit to window surface")
#if HAVE_CAIRO_GL
            .add_entry("gl", backend_cairo_gl, "use Cairo GL backend")
#endif
            .add_entry("offscreen_data", backend_cairo_offscreen_data_surface,
                       "render to memory buffer(i.e. use cairo_image_surface_create_for_data)"),
            "cairo_backend",
            "Select Cairo backend",
            *this),
#if HAVE_CAIRO_GL
  m_gl_options("GL options (only active if cairo_backend is gl)", *this),
  m_depth_bits(24, "depth_bits",
               "Bpp of depth buffer of GL, non-positive values mean use SDL defaults",
               *this),
  m_stencil_bits(8, "stencil_bits",
                 "Bpp of stencil buffer of GL, non-positive values mean use SDL defaults",
                 *this),
  m_use_msaa(false, "enable_msaa", "If true enables MSAA for GL", *this),
  m_msaa(4, "msaa_samples",
         "If greater than 0, specifies the number of samples "
         "to request for MSAA for GL. If not, SDL will choose the "
         "sample count as the highest available value",
         *this),
  m_swap_interval(-1, "swap_interval",
                  "If set, pass the specified value to SDL_GL_SetSwapInterval, "
                  "a value of 0 means no vsync, a value of 1 means vsync and "
                  "a value of -1, if the platform supports, late swap tearing "
                  "as found in extensions GLX_EXT_swap_control_tear and "
                  "WGL_EXT_swap_control_tear. STRONG REMINDER: the value is "
                  "only passed to SDL_GL_SetSwapInterval if the value is set "
                  "at command line", *this),
#endif
  m_demo_options("Demo Options", *this),
  m_window(NULL),
  m_cairo_window_surface(NULL),
  m_cairo_offscreen_surface(NULL),
  m_present_cairo(NULL),
  m_pixmap(0),
  m_sdl_gl_ctx(NULL),
  m_cairo_gl_device(NULL)
{}

sdl_cairo_demo::
~sdl_cairo_demo()
{
  cleanup_cairo();

  /* Destroying the Cairo-GL stuff apparently also
     destroys the GL context, thus we do NOT call
     SDL_GL_DeleteContext()
   */

  if(m_window)
    {
      SDL_ShowCursor(SDL_ENABLE);
      SDL_SetWindowGrab(m_window, SDL_FALSE);

      SDL_DestroyWindow(m_window);
      SDL_Quit();
    }
}

void
sdl_cairo_demo::
cleanup_cairo(void)
{
  if(m_cairo)
    {
      cairo_destroy(m_cairo);
    }

  if(m_present_cairo)
    {
      cairo_destroy(m_present_cairo);
    }

  if(m_cairo_window_surface)
    {
      cairo_surface_destroy(m_cairo_window_surface);
    }

  if(m_pixmap)
    {
      XFreePixmap(m_x11_display, m_pixmap);
      m_pixmap = 0;
    }

  if(m_cairo_offscreen_surface)
    {
      cairo_surface_destroy(m_cairo_offscreen_surface);
    }

  if(m_cairo_gl_device)
    {
      cairo_device_destroy(m_cairo_gl_device);
    }
}

enum sdl_cairo_demo::return_code
sdl_cairo_demo::
init_sdl(void)
{
  // Init SDL
  if(SDL_Init(SDL_INIT_EVERYTHING)<0)
    {
      /*
        abort!
       */
      std::cerr << "\nFailed on SDL_Init\n";
      return routine_fail;
    }

  int video_flags;
  video_flags = SDL_WINDOW_RESIZABLE;

  #if HAVE_CAIRO_GL
    {
      if(m_backend.m_value.m_value == backend_cairo_gl)
        {
          video_flags |= SDL_WINDOW_OPENGL;

          SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
          if(m_stencil_bits.m_value>=0)
            {
              SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, m_stencil_bits.m_value);
            }

          if(m_depth_bits.m_value>=0)
            {
              SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, m_depth_bits.m_value);
            }

          SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8);
          SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8);
          SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8);
          SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8);

          if(m_use_msaa.m_value)
            {
              SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1);
              SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, m_msaa.m_value);
            }
        }
    }
  #endif

  if(m_fullscreen.m_value)
    {
      video_flags = video_flags | SDL_WINDOW_FULLSCREEN;
    }

  // Create the SDL window
  m_window = SDL_CreateWindow("", 0, 0,
                              m_width.m_value,
                              m_height.m_value,
                              video_flags);

  if(m_window==NULL)
    {
      /*
        abort
      */
      std::cerr << "\nFailed on SDL_SetVideoMode\n";
      return routine_fail;
    }

  #if HAVE_CAIRO_GL
    {
      if(m_backend.m_value.m_value == backend_cairo_gl)
        {
          m_sdl_gl_ctx = SDL_GL_CreateContext(m_window);
          if(m_sdl_gl_ctx == NULL)
            {
              std::cerr << "Unable to create GL context: " << SDL_GetError() << "\n";
              return routine_fail;
            }
          SDL_GL_MakeCurrent(m_window, m_sdl_gl_ctx);
        }
    }
  #endif

  if(m_hide_cursor.m_value)
    {
      SDL_ShowCursor(SDL_DISABLE);
    }

  return routine_success;
}

void
sdl_cairo_demo::
init_cairo(int w, int h)
{
  SDL_SysWMinfo wm;
  cairo_surface_t *render_surface;

  SDL_VERSION(&wm.version);
  SDL_GetWindowWMInfo(m_window, &wm);

  XWindowAttributes attribs;
  m_x11_display = wm.info.x11.display;
  m_x11_window = wm.info.x11.window;

  XGetWindowAttributes(m_x11_display, m_x11_window, &attribs);

  switch(m_backend.m_value.m_value)
    {
    case backend_cairo_xlib_on_screen:
      {
        m_cairo_window_surface = cairo_xlib_surface_create(m_x11_display, m_x11_window,
                                                           attribs.visual, w, h);
        render_surface = m_cairo_window_surface;
      }
      break;

    case backend_cairo_xlib_off_screen:
      {
        m_cairo_window_surface = cairo_xlib_surface_create(m_x11_display, m_x11_window,
                                                           attribs.visual, w, h);
        m_pixmap = XCreatePixmap(m_x11_display, m_x11_window,
                                 w, h, attribs.depth);
        m_cairo_offscreen_surface =
          cairo_xlib_surface_create(m_x11_display, m_pixmap,
                                    attribs.visual, w, h);
        render_surface = m_cairo_offscreen_surface;
      }
      break;

    case backend_cairo_offscreen_data_surface:
      {
        unsigned int stride;
        cairo_format_t fmt;

        m_cairo_window_surface = cairo_xlib_surface_create(m_x11_display, m_x11_window,
                                                           attribs.visual, w, h);
        fmt = CAIRO_FORMAT_ARGB32; //or should we use CAIRO_FORMAT_RGB24?
        stride = cairo_format_stride_for_width(fmt, w);
        m_offscreen_data_pixels.resize(stride * h);
        m_cairo_offscreen_surface = cairo_image_surface_create_for_data(&m_offscreen_data_pixels[0],
                                                                        fmt, w, h, stride);
        render_surface = m_cairo_offscreen_surface;
      }
      break;

#if HAVE_CAIRO_GL
    case backend_cairo_gl:
      {
        GLXContext ctx;
        int double_buffered(0);

        ctx = glXGetCurrentContext();
        assert(ctx);
        m_cairo_gl_device = cairo_glx_device_create(m_x11_display, ctx);
        SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &double_buffered);
        m_cairo_window_surface = cairo_gl_surface_create_for_window(m_cairo_gl_device,
                                                                    m_x11_window, w, h);
        render_surface = m_cairo_window_surface;
      }
      break;
#endif

    default:
      std::cerr << "Unsupported backend_cairo_t: " << m_backend.m_value.m_value
                << "\n";
      exit(-1);
    }

  m_cairo = cairo_create(render_surface);
  if(render_surface != m_cairo_window_surface)
    {
      m_present_cairo = cairo_create(m_cairo_window_surface);
    }
}

void
sdl_cairo_demo::
on_resize(int w, int h)
{
  #if HAVE_CAIRO_GL
    {
      if(m_sdl_gl_ctx)
        {
          cairo_gl_surface_set_size(m_cairo_window_surface, w, h);
          return;
        }
    }
  #endif

  cleanup_cairo();
  init_cairo(w, h);
}

void
sdl_cairo_demo::
present(void)
{
  const enum backend_cairo_t bk(m_backend.m_value.m_value);
  switch(bk)
    {
    case backend_cairo_xlib_on_screen:
      cairo_surface_flush(m_cairo_window_surface);
      break;

    case backend_cairo_offscreen_data_surface:
    case backend_cairo_xlib_off_screen:
      {
        cairo_set_source_surface(m_present_cairo, m_cairo_offscreen_surface, 0, 0);
        cairo_paint(m_present_cairo);
        /* should we flush the surface? It does not seem to be
           needed, but from the point of view of API, it looks
           like it should be flushed before doing swap buffers.
         */
        //cairo_surface_flush(m_cairo_window_surface);
        }
      break;
#if HAVE_CAIRO_GL
    case backend_cairo_gl:
      {
        /* should we flush the surface? It does not seem to be
           needed, but from the point of view of API, it looks
           like it should be flushed before doing swap buffers.
         */
        //cairo_surface_flush(m_cairo_window_surface);
        SDL_GL_SwapWindow(m_window);
      }
      break;
#endif

    default:
      std::cerr << "Unsupported backend_cairo_t: " << bk << "\n";
      exit(-1);
    }
}


int
sdl_cairo_demo::
main(int argc, char **argv)
{
  simple_time render_time;
  unsigned int num_frames;

  if(argc == 2 and is_help_request(argv[1]))
    {
      std::cout << m_about << "\n\nUsage: " << argv[0];
      print_help(std::cout);
      print_detailed_help(std::cout);
      return 0;
    }


  std::cout << "\n\nRunning: \"";
  for(int i=0;i<argc;++i)
    {
      std::cout << argv[i] << " ";
    }

  parse_command_line(argc, argv);
  std::cout << "\n\n" << std::flush;

  enum return_code R;
  R = init_sdl();
  int w, h;

  if(R == routine_fail)
    {
      return -1;
    }

  m_run_demo = true;
  SDL_GetWindowSize(m_window, &w, &h);
  init_cairo(w, h);
  derived_init(w, h);

  num_frames = 0;
  while(m_run_demo)
    {
      if(num_frames == 0)
	{
	  render_time.restart();
	}

      draw_frame();
      present();
      ++num_frames;

      if(m_run_demo && m_handle_events)
        {
          SDL_Event ev;
          while(m_run_demo && m_handle_events && SDL_PollEvent(&ev))
            {
              handle_event(ev);
            }
        }
    }

  if(m_show_framerate.m_value)
    {
      int32_t ms;
      float msf, numf;

      ms = render_time.elapsed();
      numf = static_cast<float>(std::max(1u, num_frames));
      msf = static_cast<float>(std::max(1, ms));
      std::cout << "Rendered " << num_frames << " in " << ms << " ms.\n"
		<< "ms/frame = " << msf / numf  << "\n"
		<< "FPS = " << 1000.0f * numf / msf << "\n";
    }

  return m_return_value;
}

ivec2
sdl_cairo_demo::
dimensions(void)
{
  ivec2 wh;

  assert(m_window);
  SDL_GetWindowSize(m_window, &wh.x(), &wh.y());
  return wh;
}
