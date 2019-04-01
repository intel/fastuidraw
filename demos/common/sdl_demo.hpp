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
#include <iomanip>
#include <set>
#include <list>
#include <map>
#include <algorithm>
#include <vector>

#include <SDL.h>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/math.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_binding.hpp>

#include "ostream_utility.hpp"
#include "generic_command_line.hpp"
#include "cast_c_array.hpp"

/*
  Notes:
    ~sdl_demo() destroy the window and GL context,
    thus it is safe to use GL functions in dtor
    of a derived class

    sdl_demo() DOES NOT create the window or GL
    context, so one cannot call GL functions
    from there; rather call GL init functions
    in init_gl().

 */
class sdl_demo:public command_line_register
{
public:
  explicit
  sdl_demo(const std::string &about_text=std::string(),
           bool dimensions_must_match_default_value = false);

  virtual
  ~sdl_demo();

  /*
    call this as your main, at main exit, demo is over.
   */
  int
  main(int argc, char **argv);

protected:

  virtual
  void
  init_gl(int w, int h)
  {
    (void)w;
    (void)h;
  }

  virtual
  void
  pre_draw_frame(void)
  {}

  virtual
  void
  draw_frame(void)
  {}

  virtual
  void
  post_draw_frame(void)
  {}

  virtual
  void
  handle_event(const SDL_Event&)
  {}

  void
  reverse_event_y(bool v);

  void
  end_demo(int return_value)
  {
    m_run_demo = false;
    m_return_value = return_value;
  }

  fastuidraw::ivec2
  dimensions(void);

  void
  swap_buffers(unsigned int count = 1);

protected:
  bool m_handle_events;

private:

  enum fastuidraw::return_code
  init_sdl(void);

  void
  set_sdl_gl_context_attributes(void);

  void
  create_sdl_gl_context(void);

  std::string m_about;
  command_separator m_common_label;
  command_line_argument_value<int> m_red_bits;
  command_line_argument_value<int> m_green_bits;
  command_line_argument_value<int> m_blue_bits;
  command_line_argument_value<int> m_alpha_bits;
  command_line_argument_value<int> m_depth_bits;
  command_line_argument_value<int> m_stencil_bits;
  command_line_argument_value<bool> m_fullscreen;
  command_line_argument_value<bool> m_hide_cursor;
  command_line_argument_value<bool> m_use_msaa;
  command_line_argument_value<int> m_msaa;
  command_line_argument_value<int> m_width;
  command_line_argument_value<int> m_height;
  command_line_argument_value<bool> m_dimensions_must_match;
  command_line_argument_value<int> m_bpp;
  command_line_argument_value<std::string> m_log_gl_commands;
  command_line_argument_value<bool> m_print_gl_info;
  command_line_argument_value<int> m_swap_interval;
  command_line_argument_value<int> m_gl_major, m_gl_minor;
#ifndef FASTUIDRAW_GL_USE_GLES
  command_line_argument_value<bool> m_gl_forward_compatible_context;
  command_line_argument_value<bool> m_gl_debug_context;
  command_line_argument_value<bool> m_gl_core_profile;
  command_line_argument_value<bool> m_try_to_get_latest_gl_version;
#endif
  command_line_argument_value<bool> m_show_framerate;

  fastuidraw::reference_counted_ptr<fastuidraw::gl_binding::CallbackGL> m_gl_logger;

  bool m_run_demo;
  int m_return_value;
  bool m_reverse_event_y;

  SDL_Window *m_window;
  SDL_GLContext m_ctx;
};
