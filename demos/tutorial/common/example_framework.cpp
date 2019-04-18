/*!
 * \file example_framework.cpp
 * \brief file example_framework.cpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 */

//! [ExampleFramework]

#include <iostream>
#include "example_framework.hpp"

////////////////////////////
//example_framework  methods
example_framework::
example_framework(void):
  m_window(nullptr),
  m_ctx(nullptr),
  m_run_demo(true)
{
}

example_framework::
~example_framework()
{
  if (m_window)
    {
      if (m_ctx)
        {
          SDL_GL_MakeCurrent(m_window, nullptr);
          SDL_GL_DeleteContext(m_ctx);
        }

      SDL_ShowCursor(SDL_ENABLE);
      SDL_SetWindowGrab(m_window, SDL_FALSE);

      SDL_DestroyWindow(m_window);
      SDL_Quit();
    }
}

enum fastuidraw::return_code
example_framework::
init_sdl(void)
{
  /* With SDL:
   *   1) Create a window
   *   2) Create a GL context
   *   3) make the GL context current
   */
  if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
      std::cerr << "\nFailed on SDL_Init\n";
      return fastuidraw::routine_fail;
    }

  int window_width(800), window_height(600);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  /* FastUIDraw requires for GL atleast 3.30 core profile
   * and for GLES atleast GLES 3.0. Some GL implementations
   * will only give a GL 3.0 compatibility context unless
   * we ask SDL to create a core profile. If using the
   * GLES backend, the macro FASTUIDRAW_GL_USE_GLES will
   * be defined.
   */
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    }
  #else
    {
      /* DANGER: there are GL implementations that will give
       * you JUST the version requested when creating a GL
       * context rather than the highest version it could give.
       */
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    }
  #endif

  m_window = SDL_CreateWindow("",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              window_width,
                              window_height,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

  if (m_window == nullptr)
    {
      std::cerr << "\nFailed on SDL_SetVideoMode\n";
      return fastuidraw::routine_fail;
    }

  m_ctx = SDL_GL_CreateContext(m_window);
  if (m_ctx == nullptr)
    {
      std::cerr << "Unable to create GL context: " << SDL_GetError() << "\n";
      return fastuidraw::routine_fail;
    }
  SDL_GL_MakeCurrent(m_window, m_ctx);

  return fastuidraw::routine_success;
}

fastuidraw::ivec2
example_framework::
window_dimensions(void)
{
  fastuidraw::ivec2 return_value;

  FASTUIDRAWassert(m_window);
  SDL_GetWindowSize(m_window, &return_value.x(), &return_value.y());
  return return_value;
}

void
example_framework::
handle_event(const SDL_Event &ev)
{
  switch (ev.type)
    {
      case SDL_QUIT:
        end_demo();
        break;
    }
}

int
example_framework::
main(int argc, char **argv)
{
  enum fastuidraw::return_code R;

  R = init_sdl();
  if (R == fastuidraw::routine_fail)
    {
      return -1;
    }

  derived_init(argc, argv);
  while(m_run_demo)
    {
      draw_frame();
      SDL_GL_SwapWindow(m_window);

      if (m_run_demo)
        {
          SDL_Event ev;
          while(m_run_demo && SDL_PollEvent(&ev))
            {
              handle_event(ev);
            }
        }
    }

  return 0;
}

//! [ExampleFramework]
