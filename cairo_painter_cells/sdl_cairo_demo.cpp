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
  m_backend(backend_cairo_xlib_on_screen,
            enumerated_string_type<enum backend_cairo_t>()
            .add_entry("xlib_onscreen", backend_cairo_xlib_on_screen, "render directly to X window surface")
            .add_entry("xlib_offscreen", backend_cairo_xlib_off_screen, "render to X Pixmap then blit to window surface")
            .add_entry("sdl_surface", backend_cairo_sdl_surface,
                       "render to SDL_Surface (i.e. use cairo_image_surface_create_for_data "
                       "with data buffer from SDL_Surface"),
            "cairo_backend",
            "Select Cairo backend",
            *this),
  m_window(NULL),
  m_cairo_window_surface(NULL),
  m_cairo_offscreen_surface(NULL),
  m_pixmap(0),
  m_sdl_surface(NULL)
{}

sdl_cairo_demo::
~sdl_cairo_demo()
{
  if(m_cairo)
    {
      cairo_destroy(m_cairo);
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

  if(m_sdl_surface)
    {
      SDL_UnlockSurface(m_sdl_surface);
      SDL_FreeSurface(m_sdl_surface);
    }

  if(m_cairo_offscreen_surface)
    {
      cairo_surface_destroy(m_cairo_offscreen_surface);
    }

  if(m_window)
    {
      SDL_ShowCursor(SDL_ENABLE);
      SDL_SetWindowGrab(m_window, SDL_FALSE);

      SDL_DestroyWindow(m_window);
      SDL_Quit();
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

  if(m_fullscreen.m_value)
    {
      video_flags=video_flags | SDL_WINDOW_FULLSCREEN;
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
  m_cairo_window_surface = cairo_xlib_surface_create(m_x11_display, m_x11_window,
                                                     attribs.visual, w, h);

  switch(m_backend.m_value.m_value)
    {
    case backend_cairo_xlib_on_screen:
      {
        render_surface = m_cairo_window_surface;
      }
      break;

    case backend_cairo_xlib_off_screen:
      {
        m_pixmap = XCreatePixmap(m_x11_display, m_x11_window,
                                 w, h, attribs.depth);
        m_cairo_offscreen_surface =
          cairo_xlib_surface_create(m_x11_display, m_pixmap,
                                    attribs.visual, w, h);
        render_surface = m_cairo_offscreen_surface;
      }
      break;

    case backend_cairo_sdl_surface:
      {
        Uint32 flags(0);
        int bpp(32);
        Uint32 red_mask(0x00FF0000), green_mask(0x0000FF00);
        Uint32 blue_mask(0x000000FF), alpha_mask(0);
        unsigned char *pixels;
        m_sdl_surface = SDL_CreateRGBSurface(flags, w, h, bpp,
                                             red_mask, green_mask,
                                             blue_mask, alpha_mask);
        SDL_LockSurface(m_sdl_surface);
        pixels = static_cast<unsigned char*>(m_sdl_surface->pixels);
        m_cairo_offscreen_surface = cairo_image_surface_create_for_data(pixels,
                                                                        CAIRO_FORMAT_RGB24,
                                                                        m_sdl_surface->w,
                                                                        m_sdl_surface->h,
                                                                        m_sdl_surface->pitch);
        render_surface = m_cairo_offscreen_surface;
      }
      break;

    default:
      std::cerr << "Unsupported backend_cairo_t: " << m_backend.m_value.m_value
                << "\n";
      exit(-1);
    }

  m_cairo = cairo_create(render_surface);
  derived_init(w, h);
}

void
sdl_cairo_demo::
on_resize(int w, int h)
{
  /* resize surfaces..
   */
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

    case backend_cairo_sdl_surface:
    case backend_cairo_xlib_off_screen:
      {
        cairo_t *cr;
        cairo_surface_flush(m_cairo_offscreen_surface);
        cr = cairo_create(m_cairo_window_surface);
        cairo_set_source_surface (cr, m_cairo_offscreen_surface, 0, 0);
        cairo_paint(cr);
        cairo_surface_flush(m_cairo_window_surface);
        cairo_destroy(cr);
        }
      break;

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

std::pair<int, int>
sdl_cairo_demo::
dimensions(void)
{
  std::pair<int, int> wh;

  assert(m_window);
  SDL_GetWindowSize(m_window, &wh.first, &wh.second);
  return wh;
}
