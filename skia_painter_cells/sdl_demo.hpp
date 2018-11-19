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
#include <assert.h>
#include <SDL.h>

#include "SkPoint.h"

#include "generic_command_line.hpp"

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
  enum return_code
    {
      routine_fail,
      routine_success,
    };


  explicit
  sdl_demo(const std::string &about_text=std::string());

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
  draw_frame(void)
  {}

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

  SkPoint
  dimensions(void);

  int
  stencil_bits(void);

  int
  depth_bits(void);

  int
  sample_count(void);

  void
  swap_buffers(unsigned int count=1);

protected:
  bool m_handle_events;

private:

  enum return_code
  init_sdl(void);

  std::string m_about;
  command_separator m_common_label;

protected:
  command_line_argument_value<int> m_depth_bits;
  command_line_argument_value<int> m_stencil_bits;
  command_line_argument_value<bool> m_fullscreen;
  command_line_argument_value<bool> m_hide_cursor;
  command_line_argument_value<bool> m_use_msaa;
  command_line_argument_value<int> m_msaa;

private:
  command_line_argument_value<int> m_width;
  command_line_argument_value<int> m_height;
  command_line_argument_value<bool> m_print_gl_info;
  command_line_argument_value<int> m_swap_interval;
  command_line_argument_value<int> m_gl_major, m_gl_minor;
  command_line_argument_value<bool> m_gl_forward_compatible_context;
  command_line_argument_value<bool> m_gl_debug_context;
  command_line_argument_value<bool> m_gl_core_profile;

  command_line_argument_value<bool> m_show_framerate;

  bool m_run_demo;
  int m_return_value;

  SDL_Window *m_window;
  SDL_GLContext m_ctx;
};
