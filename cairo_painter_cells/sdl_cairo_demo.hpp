/*!
 * \file sdl_demo.hpp
 * \brief file sdl_demo.hpp
 *
 * Adapted from: sdl_demo.hpp of WRATH:
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


#pragma once

#include <iostream>
#include <sys/time.h>
#include <vector>
#include <utility>
#include <cairo.h>
#include <cairo-xlib.h>
#include <X11/Xutil.h>
#include <SDL.h>

#include "generic_command_line.hpp"
#include "vec2.hpp"

/*
  Notes:
    sdl_cairo_demo() DOES NOT create the window,
    cairo stuff, or GL stuff. They are made later
    and are avaliable at derived_init()
 */
class sdl_cairo_demo:public command_line_register
{
public:
  enum return_code
    {
      routine_fail,
      routine_success,
    };


  explicit
  sdl_cairo_demo(const std::string &about_text=std::string());

  virtual
  ~sdl_cairo_demo();

  /*
    call this as your main, at main exit, demo is over.
   */
  int
  main(int argc, char **argv);

protected:

  virtual
  void
  draw_frame(void)
  {}

  void
  on_resize(int, int);

  virtual
  void
  handle_event(const SDL_Event&)
  {}

  void
  end_demo(int return_value)
  {
    m_run_demo=false;
    m_return_value=return_value;
  }

  ivec2
  dimensions(void);

  virtual
  void
  derived_init(int, int)
  {}

  bool m_handle_events;
  cairo_t *m_cairo;

private:

  void
  present(void);

  enum backend_cairo_t
    {
      backend_cairo_xlib_on_screen,
      backend_cairo_xlib_off_screen,
      backend_cairo_offscreen_data_surface,
#if HAVE_CAIRO_GL
      backend_cairo_gl,
#endif
    };

  enum return_code
  init_sdl(void);

  void
  init_cairo(int w, int h);

  void
  cleanup_cairo(void);

  std::string m_about;
  command_separator m_common_label;
  command_line_argument_value<bool> m_fullscreen;
  command_line_argument_value<bool> m_hide_cursor;
  command_line_argument_value<int> m_width;
  command_line_argument_value<int> m_height;
  command_line_argument_value<bool> m_show_framerate;
  enumerated_command_line_argument_value<enum backend_cairo_t> m_backend;
#if HAVE_CAIRO_GL
  command_separator m_gl_options;
  command_line_argument_value<int> m_depth_bits;
  command_line_argument_value<int> m_stencil_bits;
  command_line_argument_value<bool> m_use_msaa;
  command_line_argument_value<int> m_msaa;
  command_line_argument_value<int> m_swap_interval;
#endif
  command_separator m_demo_options;

  bool m_run_demo;
  int m_return_value;

  SDL_Window *m_window;
  cairo_surface_t *m_cairo_window_surface;

  /* X stuff taken from m_window
   */
  Display *m_x11_display;
  Window m_x11_window;

  /* use if rendering to offscreen surface first.
   */
  cairo_surface_t *m_cairo_offscreen_surface;

  /* offscreen surface for X
   */
  Pixmap m_pixmap;

  /* offscreen surface for CPU rendering
   */
  std::vector<unsigned char> m_offscreen_data_pixels;

  /* Only active for GL backends!
   */
  SDL_GLContext m_sdl_gl_ctx;
  cairo_device_t *m_cairo_gl_device;
};
