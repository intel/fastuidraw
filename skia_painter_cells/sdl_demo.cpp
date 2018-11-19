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

#include <GL/glcorearb.h>

#include "generic_command_line.hpp"
#include "simple_time.hpp"
#include "sdl_demo.hpp"

namespace
{
  template<typename T>
  T
  gl_get(GLenum pname)
  {
    T v(0);
    glGetIntegerv(pname, &v);
    return v;
  }

  bool
  is_help_request(const std::string &v)
  {
    return v==std::string("-help")
      or v==std::string("--help")
      or v==std::string("-h");
  }
}

sdl_demo::
sdl_demo(const std::string &about_text):
  m_handle_events(true),
  m_about(command_line_argument::tabs_to_spaces(command_line_argument::format_description_string("", about_text))),
  m_common_label("Screen and Context Option", *this),
  m_depth_bits(24, "depth_bits",
               "Bpp of depth buffer, non-positive values mean use SDL defaults",
               *this),
  m_stencil_bits(8, "stencil_bits",
                 "Bpp of stencil buffer, non-positive values mean use SDL defaults",
                 *this),
  m_fullscreen(false, "fullscreen", "fullscreen mode", *this),
  m_hide_cursor(false, "hide_cursor", "If true, hide the mouse cursor with a SDL call", *this),
  m_use_msaa(false, "enable_msaa", "If true enables MSAA", *this),
  m_msaa(4, "msaa_samples",
         "If greater than 0, specifies the number of samples "
         "to request for MSAA. If not, SDL will choose the "
         "sample count as the highest available value",
         *this),
  m_width(800, "width", "window width", *this),
  m_height(480, "height", "window height", *this),
  m_print_gl_info(false, "print_gl_info", "If true print to stdout GL information", *this),
  m_swap_interval(-1, "swap_interval",
                  "If set, pass the specified value to SDL_GL_SetSwapInterval, "
                  "a value of 0 means no vsync, a value of 1 means vsync and "
                  "a value of -1, if the platform supports, late swap tearing "
                  "as found in extensions GLX_EXT_swap_control_tear and "
                  "WGL_EXT_swap_control_tear. STRONG REMINDER: the value is "
                  "only passed to SDL_GL_SetSwapInterval if the value is set "
                  "at command line", *this),
  m_gl_major(3, "gl_major", "GL major version", *this),
  m_gl_minor(3, "gl_minor", "GL minor version", *this),
  m_gl_forward_compatible_context(false, "foward_context", "if true request forward compatible context", *this),
  m_gl_debug_context(false, "debug_context", "if true request a context with debug", *this),
  m_gl_core_profile(true, "core_context", "if true request a context which is core profile", *this),
  m_show_framerate(false, "show_framerate", "if true show the cumulative framerate at end", *this),
  m_window(NULL),
  m_ctx(NULL)
{}

sdl_demo::
~sdl_demo()
{
  if(m_window)
    {
      if(m_ctx)
        {
          SDL_GL_MakeCurrent(m_window, NULL);
          SDL_GL_DeleteContext(m_ctx);
        }

      SDL_ShowCursor(SDL_ENABLE);
      SDL_SetWindowGrab(m_window, SDL_FALSE);

      SDL_DestroyWindow(m_window);
      SDL_Quit();
    }
}

enum sdl_demo::return_code
sdl_demo::
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

  if(m_fullscreen.value())
    {
      video_flags=video_flags | SDL_WINDOW_FULLSCREEN;
    }

  video_flags |=SDL_WINDOW_OPENGL;

  /*
    set GL attributes:
  */
  SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
  if(m_stencil_bits.value()>=0)
    {
      SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, m_stencil_bits.value());
    }

  if(m_depth_bits.value()>=0)
    {
      SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, m_depth_bits.value());
    }

  SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8);

  if(m_use_msaa.value())
    {
      SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1);
      SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, m_msaa.value());
    }

  if(m_gl_major.value()>=3)
    {
      int context_flags(0);
      int profile_mask(0);

      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, m_gl_major.value());
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, m_gl_minor.value());

      if(m_gl_forward_compatible_context.value())
        {
          context_flags|=SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
        }

      if(m_gl_debug_context.value())
        {
          context_flags|=SDL_GL_CONTEXT_DEBUG_FLAG;
        }
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags);

      if(m_gl_core_profile.value())
        {
          profile_mask=SDL_GL_CONTEXT_PROFILE_CORE;
        }
      else
        {
          profile_mask=SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
        }

      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile_mask);
    }

  // Create the SDL window
  m_window = SDL_CreateWindow("", 0, 0,
                              m_width.value(),
                              m_height.value(),
                              video_flags);

  if(m_window==NULL)
    {
      /*
        abort
      */
      std::cerr << "\nFailed on SDL_SetVideoMode\n";
      return routine_fail;
    }

  m_ctx = SDL_GL_CreateContext(m_window);
  if(m_ctx==NULL)
    {
      std::cerr << "Unable to create GL context: " << SDL_GetError() << "\n";
      return routine_fail;
    }
  SDL_GL_MakeCurrent(m_window, m_ctx);

  if(m_swap_interval.set_by_command_line())
    {
      if(SDL_GL_SetSwapInterval(m_swap_interval.value()) != 0)
        {
          std::cerr << "Warning unable to set swap interval: "
                    << SDL_GetError() << "\n";
        };
    }

  if(m_print_gl_info.value())
    {
      std::cout << "\nGL_VERSION:" << glGetString(GL_VERSION)
                << "\nGL_VENDOR:" << glGetString(GL_VENDOR)
                << "\nGL_RENDERER:" << glGetString(GL_RENDERER)
                << "\nGL_SHADING_LANGUAGE_VERSION:" << glGetString(GL_SHADING_LANGUAGE_VERSION)
                << "\nGL_MAX_VARYING_COMPONENTS:" << gl_get<GLint>(GL_MAX_VARYING_COMPONENTS)
                << "\nGL_MAX_VERTEX_ATTRIBS:" << gl_get<GLint>(GL_MAX_VERTEX_ATTRIBS)
                << "\nGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:" << gl_get<GLint>(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS)
                << "\nGL_MAX_VERTEX_UNIFORM_BLOCKS:" << gl_get<GLint>(GL_MAX_VERTEX_UNIFORM_BLOCKS)
                << "\nGL_MAX_FRAGMENT_UNIFORM_BLOCKS:" << gl_get<GLint>(GL_MAX_FRAGMENT_UNIFORM_BLOCKS)
                << "\nGL_MAX_COMBINED_UNIFORM_BLOCKS:" << gl_get<GLint>(GL_MAX_COMBINED_UNIFORM_BLOCKS)
                << "\nGL_MAX_UNIFORM_BLOCK_SIZE:" << gl_get<GLint>(GL_MAX_UNIFORM_BLOCK_SIZE)
                << "\nGL_MAX_TEXTURE_SIZE: " << gl_get<GLint>(GL_MAX_TEXTURE_SIZE)
                << "\nGL_MAX_TEXTURE_BUFFER_SIZE: " << gl_get<GLint>(GL_MAX_TEXTURE_BUFFER_SIZE)
                << "\nGL_MAX_GEOMETRY_UNIFORM_BLOCKS:" << gl_get<GLint>(GL_MAX_GEOMETRY_UNIFORM_BLOCKS)
                << "\nGL_MAX_CLIP_DISTANCES:" << gl_get<GLint>(GL_MAX_CLIP_DISTANCES);

      int cnt;
      cnt = gl_get<GLint>(GL_NUM_EXTENSIONS);
      std::cout << "\nGL_EXTENSIONS(" << cnt << "):";
      for(int i = 0; i < cnt; ++i)
        {
          std::cout << "\n\t" << glGetStringi(GL_EXTENSIONS, i);
        }
      std::cout << "\n";
    }

  if(m_hide_cursor.value())
    {
      SDL_ShowCursor(SDL_DISABLE);
    }

  return routine_success;
}

void
sdl_demo::
swap_buffers(unsigned int count)
{
  for(unsigned int i=0; i<count; ++i)
    {
      SDL_GL_SwapWindow(m_window);
    }
}


int
sdl_demo::
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
  init_gl(w, h);

  num_frames = 0;
  while(m_run_demo)
    {
      if(num_frames == 0)
	{
	  render_time.restart();
	}

      draw_frame();
      swap_buffers();
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

  if(m_show_framerate.value())
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

SkPoint
sdl_demo::
dimensions(void)
{
  int w, h;

  assert(m_window);
  SDL_GetWindowSize(m_window, &w, &h);
  return SkPoint::Make(w, h);
}

int
sdl_demo::
stencil_bits(void)
{
  int return_value;
  SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &return_value);
  return return_value;
}

int
sdl_demo::
depth_bits(void)
{
  int return_value;
  SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &return_value);
  return return_value;
}

int
sdl_demo::
sample_count(void)
{
  int return_value(0), use_msaa(0);

  SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS, &use_msaa);
  if(use_msaa)
    {
      SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &return_value);
    }
  return return_value;
}
