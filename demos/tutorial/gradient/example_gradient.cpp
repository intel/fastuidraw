/*!
 * \file example_gradient.cpp
 * \brief file example_gradient.cpp
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

//! [ExampleGradient]

#include <iostream>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include "initialization.hpp"

class ExampleGradient:public Initialization
{
public:
  ExampleGradient(DemoRunner *runner, int argc, char **argv);

  ~ExampleGradient()
  {}

  virtual
  void
  draw_frame(void) override;

  virtual
  void
  handle_event(const SDL_Event &ev) override;

private:
  enum gradient_type
    {
      linear_gradient,
      radial_gradient,
      sweep_gradient,

      number_gradient_types
    };

  unsigned int m_gradient_type;
  fastuidraw::reference_counted_ptr<const fastuidraw::ColorStopSequenceOnAtlas> m_color_stops;
};

ExampleGradient::
ExampleGradient(DemoRunner *runner, int argc, char **argv):
  Initialization(runner, argc, argv),
  m_gradient_type(linear_gradient)
{
  std::cout << "Press any key to change gradient\n";

  /* Create the color stop sequence object that the fastuidraw::PainterBrush
   * will consume fro drawing gradients. The object fastuidraw::ColorStopSequence
   * species the location and color of each of the colro stops and the
   * object ColorStopSequenceOnAtlas is the object realized in the
   * backend for drawing.
   */

  /* Make a simple color-stop-sequence with 4 color-stops. */
  fastuidraw::ColorStopSequence seq;

  seq.add(fastuidraw::ColorStop(fastuidraw::u8vec4(0, 0, 255, 255), 0.0f));
  seq.add(fastuidraw::ColorStop(fastuidraw::u8vec4(255, 0, 0, 255), 0.5f));
  seq.add(fastuidraw::ColorStop(fastuidraw::u8vec4(0, 255, 0, 255), 0.75f));
  seq.add(fastuidraw::ColorStop(fastuidraw::u8vec4(255, 255, 255, 0), 1.0f));

  /* create the fastuidraw::ColorStopSequenceOnAtlas, the trickiest
   * argument to set correctly is the last argument, pwidth() which
   * specifies how many texels the color-stop will occupy on a texture.
   * For this example, our color stops are evenly spaced at 0.25
   * increments, so taking a width as 8 will capture the color stop
   * values.
   */
  m_color_stops = FASTUIDRAWnew fastuidraw::ColorStopSequenceOnAtlas(seq,
                                                                     m_painter_engine_gl->colorstop_atlas(),
                                                                     8);
}

void
ExampleGradient::
handle_event(const SDL_Event &ev)
{
  switch (ev.type)
    {
    case SDL_KEYDOWN:
      ++m_gradient_type;
      break;
    }
  m_gradient_type = m_gradient_type % number_gradient_types;
  Initialization::handle_event(ev);
}

void
ExampleGradient::
draw_frame(void)
{
  fastuidraw::vec2 window_dims(window_dimensions());
  fastuidraw::PainterSurface::Viewport vwp(0, 0, window_dims.x(), window_dims.y());

  m_surface_gl->viewport(vwp);
  m_painter->begin(m_surface_gl, fastuidraw::Painter::y_increases_downwards);

  fastuidraw::PainterBrush brush;
  brush.color(1.0f, 1.0f, 1.0f, 1.0f);

  /* For this simple example, we will use SDL_GetTicks() to
   * get the number of milliseconds since the application started.
   * We want a time value that is mirror-periodic to make the
   * animation trivially smooth.
   */
  uint32_t t;
  t = SDL_GetTicks() % 4000u;
  if (t >= 2000u)
    {
      t = 4000u - t;
    }

  switch (m_gradient_type)
    {
    case linear_gradient:
      {
        /* Set the PainterBrush to have a linear gradient
         * using the color stops of m_color_stops. We make
         * the starting point of the gradient the center
         * of the screen and then end point a point rotating
         * about the center. We use SDL_GetTicks() to get the
         * number of milliseconds since the application started.
         */
        fastuidraw::vec2 p0, p1;
        float c, s, tf, r;

        p0 = window_dims * 0.5f;
        tf = 2.0f * FASTUIDRAW_PI / 2000.0f * static_cast<float>(t);
        c = fastuidraw::t_cos(tf);
        s = fastuidraw::t_sin(tf);
        r = fastuidraw::t_min(p0.x(), p0.y());
        p1 = p0 + r * fastuidraw::vec2(c, s);

        brush.linear_gradient(m_color_stops, p0, p1,
                              fastuidraw::PainterBrush::spread_mirror_repeat);
      }
      break;

    case radial_gradient:
      {
        /* Set the PainterBrush to have a radial gradient
         * using the color stops of m_color_stops. We make
         * the starting circle the center of the screen with
         * radius 0 and the ending circle to also be the center
         * but have the ending radius animated.
         */
        fastuidraw::vec2 p0, p1;
        float r0, r1;

        p0 = p1 = window_dims * 0.5f;
        r0 = 0.0f;
        r1 = 10.0f + fastuidraw::t_max(window_dims.x(), window_dims.y()) * static_cast<float>(t) / 4000.0f;

        brush.radial_gradient(m_color_stops, p0, r0, p1, r1,
                              fastuidraw::PainterBrush::spread_mirror_repeat);
      }
      break;

    case sweep_gradient:
      {
        /* Set the PainterBrush to have a sweep gradient
         * using the color stops of m_color_stops. We make
         * the center of the screen the sweep center and
         * rotate the angle according to t scaled and set
         * the patten to repeat several times.
         */
        fastuidraw::vec2 p;
        float angle, repeat;

        p = window_dims * 0.5f;
        angle = 2.0f * FASTUIDRAW_PI / 2000.0f * static_cast<float>(t);
        repeat = 1.0f;

        brush.sweep_gradient(m_color_stops, p, angle - FASTUIDRAW_PI, repeat,
                             fastuidraw::PainterBrush::spread_mirror_repeat);
      }
      break;

    }

  m_painter->fill_rect(brush,
                       fastuidraw::Rect()
                       .min_point(0.0f, 0.0f)
                       .max_point(window_dims));

  m_painter->end();

  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface_gl->blit_surface(GL_NEAREST);
}

int
main(int argc, char **argv)
{
  DemoRunner demo_runner;
  return demo_runner.main<ExampleGradient>(argc, argv);
}

//! [ExampleGradient]
