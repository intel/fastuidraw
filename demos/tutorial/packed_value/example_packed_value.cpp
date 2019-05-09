/*!
 * \file example_image.cpp
 * \brief file example_image.cpp
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

//! [ExamplePackedValue]

#include <iostream>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include "initialization.hpp"
#include "image_loader.hpp"

class ExamplePackedValue:public Initialization
{
public:
  ExamplePackedValue(DemoRunner *runner, int argc, char **argv);

  ~ExamplePackedValue()
  {}

  virtual
  void
  draw_frame(void) override;

private:
  fastuidraw::reference_counted_ptr<const fastuidraw::Image> m_image;
  fastuidraw::PainterPackedValue<fastuidraw::PainterBrushShaderData> m_packed_brush;
};

ExamplePackedValue::
ExamplePackedValue(DemoRunner *runner, int argc, char **argv):
  Initialization(runner, argc, argv)
{
  /* Create the fastuidraw::Image using ImageSourceSDL to
   * direct SDL2_image to do the heavy lifting of loading
   * the pixel data.
   */
  ImageSourceSDL image_loader(argv[1]);
  m_image = m_painter_engine_gl->image_atlas().create(image_loader.width(),
                                                      image_loader.height(),
                                                      image_loader);

  /* Create a PainterPackedValue from a PainterBrush. A PainterPackedValue
   * allows Painter to resuse shader data across Painter draw methods.
   * This can be quite useful if one is drawing many items with the same
   * brush.
   *
   * First set the values for a PainterBrush.
   */
  fastuidraw::PainterBrush brush;
  brush.image(m_image);

  /* Second, get the PainterPackedValuePool from the Painter used to draw */
  fastuidraw::PainterPackedValuePool &pool(m_painter->packed_value_pool());

  /* Lastly, use the values of brush to create the PainterPackedValue.
   * PainterPackedValue also supports creating packed values for the
   * other types of shader data: PainterItemShaderData and
   * PainterBlendShaderData.
   */
  m_packed_brush = pool.create_packed_value(brush);
}

void
ExamplePackedValue::
draw_frame(void)
{
  fastuidraw::vec2 window_dims(window_dimensions());
  fastuidraw::PainterSurface::Viewport vwp(0, 0, window_dims.x(), window_dims.y());

  m_surface_gl->viewport(vwp);
  m_painter->begin(m_surface_gl, fastuidraw::Painter::y_increases_downwards);

  const unsigned int num_rects_x(8), num_rects_y(6);
  unsigned int x, y;
  float xpos, ypos;
  float xpos_delta = window_dims.x() / static_cast<float>(num_rects_x);
  float ypos_delta = window_dims.y() / static_cast<float>(num_rects_y);
  float xpad = window_dims.x() / static_cast<float>(4 * num_rects_x);
  float ypad = window_dims.x() / static_cast<float>(4 * num_rects_y);
  fastuidraw::vec2 image_size(m_image->dimensions());
  fastuidraw::vec2 shear(0.5f * xpos_delta / image_size.x(),
                         0.5f * ypos_delta / image_size.y());
  uint32_t time_ms;
  time_ms = SDL_GetTicks() % 4000u;

  for (y = 0, ypos = 0.0f; y < num_rects_y; ++y, ypos += ypos_delta)
    {
      for (x = 0, xpos = 0.0f; x < num_rects_x; ++x, xpos += xpos_delta)
        {
          /* Save the current state of the Painter for later restore.
           * This state also includes the current transformation applied
           * to items.
           */
          m_painter->save();

          /* Translate to (xpos, ypos) */
          m_painter->translate(fastuidraw::vec2(xpos, ypos));

          /* squish so that m_image->dimensions() is squished-sheared
           * to rect_size
           */
          m_painter->shear(shear.x(), shear.y());

          /* For demo-effecs, lets rotate the rect as well. First
           * translate to the center.
           */
          m_painter->translate(image_size * 0.5f);

          /* now apply a rotation dependent on time. The angle to
           * be given to Painter is in RADIANS.
           */
          m_painter->rotate(FASTUIDRAW_PI * static_cast<float>(time_ms) / 2000.0f);

          /* translate back. */
          m_painter->translate(-image_size * 0.5f);

          /* Draw the rect. Note that in local coordinate the size of the
           * rectangle is image_size. However, the shearing done above makes
           * the rect drawn with size image_size * shear pixels. Also,
           * the brush coordinates are local to the drawn item, i.e.
           * the brush coordinate will from from (0, 0) to image_size
           */
          m_painter->fill_rect(m_packed_brush,
                               fastuidraw::Rect()
                               .min_point(0.0f, 0.0f)
                               .max_point(image_size));

          /* restore the Painter state to what it was as the last call
           * to Painter::save().
           */
          m_painter->restore();
        }
    }

  m_painter->end();

  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface_gl->blit_surface(GL_NEAREST);
}

int
main(int argc, char **argv)
{
  DemoRunner demo_runner;
  if (argc < 2)
    {
      std::cerr << "Usage: " << argv[0] << " image_file\n";
      return -1;
    }
  return demo_runner.main<ExamplePackedValue>(argc, argv);
}

//! [ExamplePackedValue]
