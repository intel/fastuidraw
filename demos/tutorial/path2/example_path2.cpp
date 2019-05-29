/*!
 * \file example_path2.cpp
 * \brief file example_path2.cpp
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

//! [ExamplePath2]

#include <iostream>
#include "initialization.hpp"

/* FastUIDraw allows applications to provide custom fill rules
 * to fastuidraw::Painter::fill_path(). A custom fill rule
 * derives from fastuiraw::CustomFillRuleBase and needs to
 * implement bool operator(int) const which is to return true
 * if and only if the portion of the path with the passed
 * winding number should be filled.
 */
class ExampleFillRule:public fastuidraw::CustomFillRuleBase
{
public:
  ExampleFillRule(void):
    m_winding_value(0),
    m_fill_equal(false)
  {}

  /* In our example fill rule, we have two parameters:
   *  - a reference value, m_winding_value
   *  - a boolean, m_fill_equal, which indicates to
   *    test for equality or in-equality
   */
  virtual
  bool
  operator()(int winding_number) const override
  {
    return (m_fill_equal) ?
      m_winding_value == winding_number :
      m_winding_value != winding_number;
  }

  int m_winding_value;
  bool m_fill_equal;
};

class ExamplePath2:public Initialization
{
public:
  ExamplePath2(DemoRunner *runner, int argc, char **argv);

  virtual
  void
  draw_frame(void) override;

  virtual
  void
  handle_event(const SDL_Event &ev) override;

private:
  fastuidraw::Path m_path;
  fastuidraw::Rect m_path_bounds;
  ExampleFillRule m_example_fill_rule;
};

ExamplePath2::
ExamplePath2(DemoRunner *runner, int argc, char **argv):
  Initialization(runner, argc, argv)
{
  /* In this example we build a complicated path using the
   * operator<< overloads that fastuidraw::Path defines.
   */
  m_path << fastuidraw::Path::contour_start(fastuidraw::vec2( 460, 60 ))
         << fastuidraw::vec2( 644, 134 )
         << fastuidraw::vec2( 544, 367 )
         << fastuidraw::Path::contour_close()
         << fastuidraw::Path::contour_start(fastuidraw::vec2( 560, 60 ))
         << fastuidraw::vec2( 644, 367 )
         << fastuidraw::vec2( 744, 134 )
         << fastuidraw::Path::contour_close()
         << fastuidraw::Path::contour_start(fastuidraw::vec2( 0, 0 ))
         << fastuidraw::Path::control_point(fastuidraw::vec2( 100, -100 ))
         << fastuidraw::Path::control_point(fastuidraw::vec2( 200, 100 ))
         << fastuidraw::vec2( 300, 0 )
         << fastuidraw::Path::arc_degrees(233, fastuidraw::vec2( 500, 0 ))
         << fastuidraw::vec2( 500, 100 )
         << fastuidraw::Path::arc_degrees(212, fastuidraw::vec2( 500, 300 ))
         << fastuidraw::Path::control_point(fastuidraw::vec2( 250, 200 ))
         << fastuidraw::Path::control_point(fastuidraw::vec2( 125, 400 ))
         << fastuidraw::vec2( 90, 120 )
         << fastuidraw::Path::arc_degrees(290, fastuidraw::vec2( 20, 150 ))
         << fastuidraw::vec2( -40, 160 )
         << fastuidraw::Path::contour_close()
         << fastuidraw::Path::contour_start(fastuidraw::vec2( 59, 9 ))
         << fastuidraw::vec2( 59, -209 )
         << fastuidraw::vec2( 519, -209 )
         << fastuidraw::vec2( 519, 9 )
         << fastuidraw::Path::contour_close_arc_degrees(-180)
         << fastuidraw::Path::contour_start(fastuidraw::vec2( 160, 60 ))
         << fastuidraw::vec2( 344, 134 )
         << fastuidraw::vec2( 244, 367 )
         << fastuidraw::Path::contour_close()
         << fastuidraw::Path::contour_start(fastuidraw::vec2( 260, 60 ))
         << fastuidraw::vec2( 344, 367 )
         << fastuidraw::vec2( 444, 134 )
         << fastuidraw::Path::contour_close()
         << fastuidraw::Path::contour_start(fastuidraw::vec2( 360, 60 ))
         << fastuidraw::vec2( 544, 134 )
         << fastuidraw::vec2( 444, 367 )
         << fastuidraw::Path::contour_close()
         << fastuidraw::Path::contour_start(fastuidraw::vec2( -60, -60 ))
         << fastuidraw::vec2( -100, 300 )
         << fastuidraw::vec2( 60, 500 )
         << fastuidraw::vec2( 200, 570 )
         << fastuidraw::vec2( 300, 100 )
         << fastuidraw::Path::contour_close_arc_degrees(80);

  /* Get the approximate bounding box for the path. This approximate
   * bounding box computation is a cheap function returning cached
   * values.
   */
  m_path.approximate_bounding_box(&m_path_bounds);

  std::cout << "Press space to toggle fill rule comparison operator between equal and not equal\n"
            << "Press any other key to increment the winding comparison value\n";
}

void
ExamplePath2::
draw_frame(void)
{
  fastuidraw::vec2 translate, scale, window_dims(window_dimensions());
  fastuidraw::PainterSurface::Viewport vwp(0, 0, window_dims.x(), window_dims.y());
  float stroke_width, border;

  m_surface_gl->viewport(vwp);
  m_painter->begin(m_surface_gl, fastuidraw::Painter::y_increases_downwards);

  /* Set the translate and scale so that the path is stretched across
   * the entire window, but have some border around the path so that
   * all of the stroking is visible.
   */
  stroke_width = 8.0f;
  border = 3.0f * stroke_width;

  scale = window_dims / (m_path_bounds.size() + fastuidraw::vec2(2.0f * border));
  translate = -m_path_bounds.m_min_point + fastuidraw::vec2(border);
  m_painter->shear(scale.x(), scale.y());
  m_painter->translate(translate);

  /* first fill the path with the color red, using the odd-even fill rule */
  m_painter->fill_path(fastuidraw::PainterBrush()
                       .color(1.0f, 0.0f, 0.0f, 1.0f),
                       m_path, m_example_fill_rule);

  /* then stroke the path with transparent orange, applying a
   * - stroking width of 8.0
   * - rounded joins
   * - rounded caps
   */
  m_painter->stroke_path(fastuidraw::PainterBrush()
                         .color(1.0f, 0.6f, 0.0f, 0.8f),
                         fastuidraw::PainterStrokeParams()
                         .width(stroke_width),
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
ExamplePath2::
handle_event(const SDL_Event &ev)
{
  switch (ev.type)
    {
    case SDL_KEYDOWN:
      switch (ev.key.keysym.sym)
        {
        case SDLK_SPACE:
          m_example_fill_rule.m_fill_equal = !m_example_fill_rule.m_fill_equal;
          std::cout << "Winding comparison operator set to ";
          if (m_example_fill_rule.m_fill_equal)
            {
              std::cout << "equality.\n";
            }
          else
            {
              std::cout << "inequality.\n";
            }
          break;

        default:
          ++m_example_fill_rule.m_winding_value;

          /* From looking at the path geometry, we know that
           * its winding values are in the range [-1, 3].
           */
          if (m_example_fill_rule.m_winding_value > 3)
            {
              m_example_fill_rule.m_winding_value = -1;
            }
          std::cout << "Winding reference value set to "
                    << m_example_fill_rule.m_winding_value
                    << "\n";
        }
      break;
    }
  Initialization::handle_event(ev);
}

int
main(int argc, char **argv)
{
  DemoRunner demo;
  return demo.main<ExamplePath2>(argc, argv);
}

//! [ExamplePath2]
