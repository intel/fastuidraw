/*!
 * \file initialization.cpp
 * \brief file initialization.cpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 */

//! [ExampleInitialization]

#include <fastuidraw/painter/painter.hpp>
#include <fastuidraw/gl_backend/gl_binding.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/painter_engine_gl.hpp>
#include <fastuidraw/gl_backend/painter_surface_gl.hpp>

#include "demo_framework.hpp"

class Initialization:public Demo
{
public:
  Initialization(DemoRunner *runner, int argc, char **argv);
  ~Initialization();

  virtual
  void
  draw_frame(void) override;

  virtual
  void
  handle_event(const SDL_Event &ev) override;

protected:
  /* A PainterEngine represents how a Painter will issue commands to
   * a 3D API
   */
  fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterEngineGL> m_painter_engine_gl;

  /* A Painter is the interface with which to render 2D content with FastUIDraw
   */
  fastuidraw::reference_counted_ptr<fastuidraw::Painter> m_painter;

  /* A PainterSurface is to where to render content via a Painter.
   */
  fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterSurfaceGL> m_surface_gl;
};

//! [ExampleInitialization]
