/*!
 * \file example_image.cpp
 * \brief file example_image.cpp
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

//! [ExampleImage]

#include <iostream>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include "initialization.hpp"
#include "image_loader.hpp"

class ExampleImage:public Initialization
{
public:
  ExampleImage(DemoRunner *runner, int argc, char **argv);

  ~ExampleImage()
  {}

  virtual
  void
  draw_frame(void) override;

private:
  fastuidraw::reference_counted_ptr<const fastuidraw::Image> m_image;
};

ExampleImage::
ExampleImage(DemoRunner *runner, int argc, char **argv):
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
}

void
ExampleImage::
draw_frame(void)
{
  fastuidraw::vec2 window_dims(window_dimensions());
  fastuidraw::PainterSurface::Viewport vwp(0, 0, window_dims.x(), window_dims.y());

  m_surface_gl->viewport(vwp);
  m_painter->begin(m_surface_gl, fastuidraw::Painter::y_increases_downwards);

  /* Set the fastuidraw::PainterBrush to use m_image as the
   * image source. The value of color() sets by what color
   * the image is modulated (in this case a value of (1, 1, 1, 1)
   * indicates that the image is not modulated.
   */
  fastuidraw::PainterBrush brush;
  brush
    .color(1.0f, 1.0f, 1.0f, 1.0f)
    .image(m_image);

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
  if (argc < 2)
    {
      std::cerr << "Usage: " << argv[0] << " image_file\n";
      return -1;
    }
  return demo_runner.main<ExampleImage>(argc, argv);
}

//! [ExampleImage]
