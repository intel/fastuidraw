/*!
 * \file example_path.cpp
 * \brief file example_path.cpp
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

//! [ExamplePath]

#include "initialization.hpp"

class ExamplePath:public Initialization
{
public:
  ExamplePath(DemoRunner *runner, int argc, char **argv);

  virtual
  void
  draw_frame(void) override;

  virtual
  void
  handle_event(const SDL_Event &ev) override;

private:
  fastuidraw::Path m_path;
  fastuidraw::vec2 m_translate;
  float m_scale;
};

ExamplePath::
ExamplePath(DemoRunner *runner, int argc, char **argv):
  Initialization(runner, argc, argv),
  m_translate(200.0f, 200.0f),
  m_scale(1.0f)
{
  /* A fastuidraw::Path is composed of multiple fastuidraw::PathContour
   * objects. A fastuidraw::PathContour is essentially a sequence of
   * points with interpolator objects between them to define how the
   * contour walks between sequential points. A contour is open if
   * there is no interpolator from the last point of the contour to
   * the first point of a contour. If there is such an interpolator,
   * then the contour is closed. Stroking a Path essentially means
   * drawing each of the contours. The fastuidraw::Path interface supports
   * directly line segments, arcs of a circle and Bezier curves of
   * any degree directly.
   */

  /* add a contour to m_path that outline a 60 degree sector of a circle */
  m_path
    .move(fastuidraw::vec2(0.0f, 0.0f)) //move to the origin
    .line_to(fastuidraw::vec2(100.0f, 0.0f)) //add a single line from (0, 0) to (100, 0)
    .arc_to(60.0f * FASTUIDRAW_PI / 180.0f, fastuidraw::vec2(50.0f, 25.0f)) //add an arc of 60 degree from (100, 0) to (50, 25)
    .close_contour(); //close the contour with a line segment.

  /* add an open contour defined by several curves. When stroking,
   * there will be no closing edge. However, the contour does
   * affect filling. When the path is filled the contour is added
   * to the path and closed by a line segment from the ending
   * point to the starting point.
   */
  m_path
    .move(fastuidraw::vec2(200.0f, 200.0f))
    .quadratic_to(fastuidraw::vec2(50.0f, 150.0f),
                  fastuidraw::vec2(0.0f, 200.0f))
    .cubic_to(fastuidraw::vec2(30.0f, 75.0f),
              fastuidraw::vec2(-30.0f, 150.0f),
              fastuidraw::vec2(0.0f, 0.0f));

  /* add another contours whose closing edge is an arc */
  m_path
    .move(fastuidraw::vec2(300.0f, 300.0f))
    .line_to(fastuidraw::vec2(300.0f, 200.0f))
    .line_to(fastuidraw::vec2(200.0f, 300.0f))
    .close_contour_arc(90.0f * FASTUIDRAW_PI / 180.0f);
}

void
ExamplePath::
draw_frame(void)
{
  fastuidraw::vec2 window_dims(window_dimensions());
  fastuidraw::PainterSurface::Viewport vwp(0, 0, window_dims.x(), window_dims.y());

  m_surface_gl->viewport(vwp);
  m_painter->begin(m_surface_gl, fastuidraw::Painter::y_increases_downwards);

  /* translate and scale as according to m_translate and m_scale */
  m_painter->translate(m_translate);
  m_painter->scale(m_scale);

  /* first fill the path with the color red, using the odd-even fill rule */
  m_painter->fill_path(fastuidraw::PainterBrush()
                       .color(1.0f, 0.0f, 0.0f, 1.0f),
                       m_path, fastuidraw::Painter::odd_even_fill_rule);

  /* then stroke the path with transparent orange, applying a
   * - stroking width of 8.0
   * - rounded joins
   * - rounded caps
   */
  m_painter->stroke_path(fastuidraw::PainterBrush()
                         .color(1.0f, 0.6f, 0.0f, 0.8f),
                         fastuidraw::PainterStrokeParams()
                         .width(8.0f),
                         m_path,
                         fastuidraw::StrokingStyle()
                         .join_style(fastuidraw::Painter::rounded_joins)
                         .cap_style(fastuidraw::Painter::rounded_caps));

  m_painter->end();

  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface_gl->blit_surface(GL_NEAREST);
}

void
ExamplePath::
handle_event(const SDL_Event &ev)
{
  switch (ev.type)
    {
    case SDL_KEYDOWN:
      switch (ev.key.keysym.sym)
        {
        case SDLK_UP:
          m_translate.y() += 16.0f;
          break;

        case SDLK_DOWN:
          m_translate.y() -= 16.0f;
          break;

        case SDLK_LEFT:
          m_translate.x() += 16.0f;
          break;

        case SDLK_RIGHT:
          m_translate.x() -= 16.0f;
          break;

        case SDLK_PAGEUP:
          m_scale += 0.2f;
          break;

        case SDLK_PAGEDOWN:
          m_scale -= 0.2f;
          break;

        case SDLK_SPACE:
          m_scale = 1.0f;
          m_translate.x() = 0.0f;
          m_translate.y() = 0.0f;
          break;
        }
      break;
    }
  Initialization::handle_event(ev);
}

int
main(int argc, char **argv)
{
  DemoRunner demo;
  return demo.main<ExamplePath>(argc, argv);
}

//! [ExamplePath]
