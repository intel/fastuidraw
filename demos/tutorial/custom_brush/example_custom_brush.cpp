/*!
 * \file example_custom_brush.cpp
 * \brief file example_custom_brush.cpp
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

//! [ExampleCustomBrushUsing]

#include <iostream>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/glsl/painter_brush_shader_glsl.hpp>
#include "initialization.hpp"

class ExampleCustomBrush:public Initialization
{
public:
  ExampleCustomBrush(DemoRunner *runner, int argc, char **argv);

  ~ExampleCustomBrush()
  {}

  virtual
  void
  draw_frame(void) override;

private:
  fastuidraw::reference_counted_ptr<const fastuidraw::ColorStopSequenceOnAtlas> m_color_stops;
  fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader> m_custom_brush_shader;
};
//! [ExampleCustomBrushUsing]

//! [ExampleCustomBrushDefining]
class ExampleCustomBrushData:public fastuidraw::PainterBrushShaderData
{
public:
  ExampleCustomBrushData(void):
    m_brush_values(),
    m_phase(0.0f),
    m_amplitude(0.0f),
    m_height(1.0f)
  {}

  unsigned int
  data_size(void) const override
  {
    return 4 + m_brush_values.data_size();
  }

  void
  pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const override
  {
    dst[0].f = m_phase;
    dst[1].f = m_height;
    dst[2].f = m_amplitude;
    m_brush_values.pack_data(dst.sub_array(4));
  }

  unsigned int
  number_resources(void) const override
  {
    return m_brush_values.number_resources();
  }

  void
  save_resources(fastuidraw::c_array<fastuidraw::reference_counted_ptr<const fastuidraw::resource_base> > dst) const override
  {
    m_brush_values.save_resources(dst);
  }

  fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::Image> >
  bind_images(void) const override
  {
    return m_brush_values.bind_images();
  }

  fastuidraw::PainterBrush m_brush_values;
  float m_phase, m_height, m_amplitude;
};

fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader>
create_wavy_custom_brush(fastuidraw::gl::PainterEngineGL *painter_engine_gl)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;

  const char *custom_brush_vert_shader =
    "void\n"
    "fastuidraw_gl_vert_brush_main(in uint sub_shader,\n"
    "                              in uint shader_data_offset,\n"
    "                              in vec2 brush_p)\n"
    "{\n"
    "  standard_brush(sub_shader, shader_data_offset + 1, brush_p);\n"
    "}\n";

  const char *custom_brush_frag_shader =
    "vec4\n"
    "fastuidraw_gl_frag_brush_main(in uint sub_shader,\n"
    "                              in uint shader_data_offset)\n"
    "{\n"
    "   const float PI = 3.14159265358979323846;\n"
    "   uvec3 packed_value;\n"
    "   float phase, height, amplitude;\n"
    "   packed_value = fastuidraw_fetch_data(shader_data_offset).xyz;\n"
    "   phase = uintBitsToFloat(packed_value.x);\n"
    "   height = uintBitsToFloat(packed_value.y);\n"
    "   amplitude = uintBitsToFloat(packed_value.z);\n"
    "   standard_brush_fastuidraw_brush_p_x += amplitude * cos((2.0 * PI / height) * (phase + standard_brush_fastuidraw_brush_p_y));\n"
    "   return standard_brush(sub_shader, shader_data_offset + 1);\n"
    "}\n";

  const PainterShaderSet &default_shaders(painter_engine_gl->default_shaders());
  reference_counted_ptr<PainterBrushShader> default_brush(default_shaders.brush_shader());
  reference_counted_ptr<PainterBrushShader> return_value;

  return_value = FASTUIDRAWnew PainterBrushShaderGLSL(0,
                                                      ShaderSource()
                                                      .add_source(custom_brush_vert_shader, ShaderSource::from_string),
                                                      ShaderSource()
                                                      .add_source(custom_brush_frag_shader, ShaderSource::from_string),
                                                      varying_list(),
                                                      1,
                                                      PainterBrushShaderGLSL::DependencyList()
                                                      .add_shader("standard_brush",
                                                                  default_brush.static_cast_ptr<PainterBrushShaderGLSL>())
                                                      );

  painter_engine_gl->painter_shader_registrar().register_shader(return_value);
  return return_value;
}

//! [ExampleCustomBrushDefining]

//! [ExampleCustomBrushUsing]
ExampleCustomBrush::
ExampleCustomBrush(DemoRunner *runner, int argc, char **argv):
  Initialization(runner, argc, argv)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;

  /* Make a simple color-stop-sequence with 4 color-stops. */
  ColorStopSequence seq;

  seq.add(ColorStop(u8vec4(0, 0, 255, 255), 0.0f));
  seq.add(ColorStop(u8vec4(255, 0, 0, 255), 0.5f));
  seq.add(ColorStop(u8vec4(0, 255, 0, 255), 0.75f));
  seq.add(ColorStop(u8vec4(255, 255, 255, 0), 1.0f));
  m_color_stops = FASTUIDRAWnew ColorStopSequenceOnAtlas(seq, m_painter_engine_gl->colorstop_atlas(), 8);
  m_custom_brush_shader = create_wavy_custom_brush(m_painter_engine_gl.get());
}

void
ExampleCustomBrush::
draw_frame(void)
{
  fastuidraw::vec2 window_dims(window_dimensions());
  fastuidraw::PainterSurface::Viewport vwp(0, 0, window_dims.x(), window_dims.y());

  m_surface_gl->viewport(vwp);
  m_painter->begin(m_surface_gl, fastuidraw::Painter::y_increases_downwards);

  ExampleCustomBrushData brush;
  brush.m_brush_values.color(1.0f, 1.0f, 1.0f, 1.0f);

  /* For this simple example, we will use SDL_GetTicks() to
   * get the number of milliseconds since the application started.
   */
  uint32_t t;
  t = SDL_GetTicks() % 4000u;

  brush.m_brush_values.linear_gradient(m_color_stops,
                                       window_dims * 0.45f,
                                       window_dims * 0.55f,
                                       fastuidraw::PainterBrush::spread_mirror_repeat);
  float tf;
  tf = 2.0f * FASTUIDRAW_PI / 4000.0f * static_cast<float>(t);
  brush.m_phase = window_dims.y() * static_cast<float>(t) / 4000.0f;
  brush.m_height = window_dims.y();
  brush.m_amplitude = 0.1f * window_dims.x() * fastuidraw::t_cos(tf);

  fastuidraw::PainterCustomBrush custom_brush(m_custom_brush_shader.get(),
                                              &brush);

  m_painter->fill_rect(custom_brush,
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
  return demo_runner.main<ExampleCustomBrush>(argc, argv);
}

//! [ExampleCustomBrushUsing]
