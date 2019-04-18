/*!
 * \file initialization.hpp
 * \brief file initialization.hpp
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

//! [ExampleInitialization]

#include <fastuidraw/painter/painter.hpp>
#include <fastuidraw/gl_backend/painter_engine_gl.hpp>
#include <fastuidraw/gl_backend/painter_surface_gl.hpp>
#include "example_framework.hpp"

class initialization:public example_framework
{
public:
  initialization(void);
  ~initialization();

protected:
  virtual
  void
  draw_frame(void) override;

  virtual
  void
  derived_init(int argc, char **argv) override;

  virtual
  void
  handle_event(const SDL_Event &ev) override;

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
