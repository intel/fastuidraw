/*!
 * \file example_framework.hpp
 * \brief file example_framework.hpp
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

#pragma once

//! [ExampleFramework]

#include <SDL.h>
#include <SDL_video.h>
#include <fastuidraw/util/vecN.hpp>

class example_framework
{
public:
  explicit
  example_framework(void);

  virtual
  ~example_framework();

  /* call this as your main, at main exit, demo is over. */
  int
  main(int argc, char **argv);

  /* Using SDL, fetch the dimensions of the window */
  fastuidraw::ivec2
  window_dimensions(void);

  /* End the event loop of the demo */
  void
  end_demo(void)
  {
    m_run_demo = false;
  }

protected:
  /* To be implemented by a derived class to render the
   * contents of the current frame.
   */
  virtual
  void
  draw_frame(void)
  {}

  /* To be implemented by a derived class to handle an event.
   * Default implementation handls SDL_QUIT event to end the
   * demo.
   */
  virtual
  void
  handle_event(const SDL_Event &ev);

  /* To be implemented by a derived class to perform any
   * one-time initialization that is needed after the
   * GL context is created and made current.
   */
  virtual
  void
  derived_init(int argc, char **argv)
  {
    FASTUIDRAWunused(argc);
    FASTUIDRAWunused(argv);
  }

private:
  enum fastuidraw::return_code
  init_sdl(void);

  bool m_run_demo;
  SDL_Window *m_window;
  SDL_GLContext m_ctx;
};
//! [ExampleFramework]
