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

#include <fastuidraw/gl_backend/ngl_header.hpp>
#include "initialization.hpp"

//! [ExampleCustomBrushDefining]
#include <iostream>
#include <fstream>
#include <sstream>
#include <fastuidraw/util/static_resource.hpp>
#include <fastuidraw/painter/painter_brush_shader_data.hpp>
#include <fastuidraw/glsl/painter_brush_shader_glsl.hpp>

/* The data of a custom brush is represented by an object of type
 * derived from fastuidraw::PainterBrushShaderData.
 */
class ExampleCustomBrushData:public fastuidraw::PainterBrushShaderData
{
public:
  ExampleCustomBrushData(void):
    m_phase(0.0f),
    m_amplitude(0.0f),
    m_period(1.0f)
  {}

  /*
   * Data size is how many -scalar- values that the object will
   * add to the data store. This value must ALWAYS be a multiple
   * of 4.
   */
  unsigned int
  data_size(void) const override
  {
    /* Our object is just a few floats (m_phase, m_amplitude and m_period)
     * toegher with the data from a fastuidraw::PainterBrush. In order
     * to seemlessly use fastuidraw::PainterBrush as a dependency, its
     * data must also start on a multiple-of-4 boundary. So, we just
     * return 4 + how much data the PainterBrush neededs.
     */
    return 4 + m_brush_values.data_size();
  }

  /* Packing the data represents placing the data into the data store
   * which will be extacted on GPU by the shader.
   */
  void
  pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const override
  {
    /* We place the data of (m_phase, m_amplitude and m_period) first. */
    dst[0].f = m_phase;
    dst[1].f = m_period;
    dst[2].f = m_amplitude;

    /* then place the data of the PainterBrush after our data starting
     * at 4-boundary of generic_data.
     */
    m_brush_values.pack_data(dst.sub_array(4));
  }

  /* PainterBrushShaderData need to indicate how many resources they use.
   * A resource is any object that must stay alive for the custom brush
   * to correctly execute. Typically resources are just fastuidraw::Image
   * and fastuidraw::ColorStopSequenceOnAtlas objects. In this example
   * the resources are coming from only the PainterBrush m_brush_values,
   * so we use its return values.
   */
  unsigned int
  number_resources(void) const override
  {
    return m_brush_values.number_resources();
  }

  /* The function fill an array of size number_resources() to save the resource
   * references to gaurnatee that the resources stay in scope.
   */
  void
  save_resources(fastuidraw::c_array<fastuidraw::reference_counted_ptr<const fastuidraw::resource_base> > dst) const override
  {
    m_brush_values.save_resources(dst);
  }

  /* A PainterBrushShaderData also needs to inform FastUIDraw what Image objects
   * it uses that need to be bound by a 3D API call. Images that need to be bound
   * are just those images whose Image::type() is Image::context_texture2d. In this
   * example, the only image data that the brush has comes exactly from PainterBrush.
   */
  fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::Image> >
  bind_images(void) const override
  {
    return m_brush_values.bind_images();
  }

  /* The waviness of the brush is defined by these 3 values
   * - m_phase gives the phase of wave (so we can animate it)
   * - m_period gives the period of the brush
   * - m_amplitude gives the amplitude of the wave
   */
  float m_phase, m_period, m_amplitude;

  /* Our example builds off of the standard brush */
  fastuidraw::PainterBrush m_brush_values;
};

fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader>
create_wavy_custom_brush(fastuidraw::gl::PainterEngineGL *painter_engine_gl)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  using namespace fastuidraw::glsl;

  /* Get the brush shader for the default shader brush */
  const PainterShaderSet &default_shaders(painter_engine_gl->default_shaders());
  reference_counted_ptr<PainterBrushShader> default_brush(default_shaders.brush_shader());

  /* Our custom brush is going to use the default standard brush. We need to
   * declare in the PainterBrushShaderGLSL ctor what other brush shaders it
   * will use and how it will refer to them. Below we create (on the stack)
   * a DependencyList object and add to it that our custom shader will use
   * the default brush shader and refer via standard_brush. In the shader
   * code, we can call its shader by calling the function standrad_brush().
   * In addition, we can also read and write any of its varyings as well.
   * For each varying FOO of the default shader brush, we can access that
   * varying as standard_brush_FOO.
   */
  PainterBrushShaderGLSL::DependencyList deps;
  deps.add_shader("standard_brush", default_brush.static_cast_ptr<PainterBrushShaderGLSL>());

  /* The vertex shader of our custom brush, the main point of interest
   * is that it calls the vertext shader of the default brush by calling
   * standard_brush(). Note that we pass 0 as the sub_shader to the
   * standard brush and that we pass shader_data_offset + 1 as the
   * shader data offset. The "+1" is there because the PainterBrush
   * data was packed 4 generic_data elements AFTER the start of the
   * array passed to pack_data(). A single uvec4 value in GLSL corresponds
   * to 4 generic_data elements.
   */
  const char *custom_brush_vert_shader =
    "void\n"
    "fastuidraw_gl_vert_brush_main(in uint sub_shader,\n"
    "                              in uint shader_data_offset,\n"
    "                              in vec2 brush_p)\n"
    "{\n"
    "  standard_brush(0, shader_data_offset + 1, brush_p);\n"
    "}\n";

  /* The fragment shader of our custom brush. It first unpacks
   * m_phase, m_period and m_amplitude from the shader data.
   * From there is modifies the varying of the default brush,
   * fastuidraw_brush_p_x, by manipulating the value of
   * standard_brush_fastuidraw_brush_p_x. Internally, these
   * name aliases are implemented by macros. Finally, after
   * changing the brush position, it calls the default brush
   * by calling standard_brush().
   */
  const char *custom_brush_frag_shader =
    "vec4\n"
    "fastuidraw_gl_frag_brush_main(in uint sub_shader,\n"
    "                              in uint shader_data_offset)\n"
    "{\n"
    "   const float PI = 3.14159265358979323846;\n"
    "   uvec3 packed_value;\n"
    "   float phase, period, amplitude;\n"
    "   packed_value = fastuidraw_fetch_data(shader_data_offset).xyz;\n"
    "   phase = uintBitsToFloat(packed_value.x);\n"
    "   period = uintBitsToFloat(packed_value.y);\n"
    "   amplitude = uintBitsToFloat(packed_value.z);\n"
    "   standard_brush_fastuidraw_brush_p_x += amplitude * cos((2.0 * PI / period) * (phase + standard_brush_fastuidraw_brush_p_y));\n"
    "   return standard_brush(sub_shader, shader_data_offset + 1);\n"
    "}\n";

  /* When creating the ShaderSource objects to pass to the ctor of
   * PainterBrushShader, we can elect to take the sources from
   * a string, a file or a resource. When we take a string from
   * a file or resource, the uber-shader assembly can list what the
   * name of the file or resource if one dumps the shader source.
   * To aid in debugging it is usually BEST to take shader source
   * from a file or resource for just that reason.
   */
  generate_static_resource("custom_brush_vert_shader", custom_brush_vert_shader);
  generate_static_resource("custom_brush_frag_shader", custom_brush_frag_shader);

  ShaderSource vert, frag;

  vert.add_source("custom_brush_vert_shader", ShaderSource::from_resource);
  frag.add_source("custom_brush_frag_shader", ShaderSource::from_resource);

  /* The ctor of PainterBrushShaderGLSL needs to know how many
   * context textures the custom brushes shaders used DIRECTLY,
   * i.e. NOT counting the number coming from the dependencies.
   * In this example, our new shader code does not use any context
   * textures, so the value is 0.
   */
  unsigned int number_context_textures(0);

  /* The ctor of PainterBrushShaderGLSL needs to know the varyings
   * that our custom brush creates. This varying list does NOT
   * include the varyings from shaders listed in the DependencyList
   * object passed. In our simple example we have no varyings so
   * we do not add any elements to the varying_list object.
   */
  varying_list varyings;

  reference_counted_ptr<PainterBrushShader> return_value;
  return_value = FASTUIDRAWnew PainterBrushShaderGLSL(number_context_textures,
                                                      vert, frag,
                                                      varyings, deps);

  /* Before the shader can be used, it must be registered. */
  painter_engine_gl->painter_shader_registrar().register_shader(return_value);

  /* One big danger of having custom shaders is that if the GLSL code we gave
   * has a syntax error, then the entire uber-shader cannot be compiled by
   * the GL implementation. For debugging, we check if the uber-shader could
   * be compiled/linked and if not bail out and save the logs and built shader
   * source so that we can fix any errors in our shader code.
   */
  #ifdef FASTUIDRAW_DEBUG
    {
      reference_counted_ptr<Program> pr;

      /* There are several GLSL programs that the painter engine
       * actually has, For simplicity we grab the GLSL program
       * that is used to draw any item that is blended with a
       * blend shader that uses single-src blending.
       */
      pr = painter_engine_gl->program(PainterEngineGL::program_all, PainterBlendShader::single_src);
      if (!pr->link_success())
        {
          std::cout << "Uber GLSL-program failed to be linked, dumping logs and sources of it\n";

          std::ofstream program_log("program_log.txt");
          program_log << "Log:\n" << pr->log()
                      << "\nLinkLog:\n" << pr->link_log()
                      << "\n";

          /* For each shader of the program, dump its compile log
           * and original shader source.
           */
          GLenum tps[2] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
          for (GLenum tp : tps)
            {
              for (unsigned int i = 0, endi = pr->num_shaders(tp); i < endi; ++i)
                {
                  if (!pr->shader_compile_success(tp, i))
                    {
                      std::ostringstream name_common, name_log, name_src;
                      name_common << "compliled_failed.shader_" << i
                                  << "." << Shader::gl_shader_type_label(tp)
                                  << ".";
                      name_log << name_common.str() << "log";
                      name_src << name_common.str() << "glsl";

                      std::ofstream shader_log(name_log.str().c_str());
                      std::ofstream shader_src(name_src.str().c_str());

                      shader_log << pr->shader_compile_log(tp, i);

                      /* The shader_src_code() returns of the uber shader, that
                       * source code will also have a comment at the end of each
                       * line giving the line number of from where the source comes
                       * when the source comes from a file or a resource.
                       */
                      shader_src << pr->shader_src_code(tp, i);
                    }
                }
            }

          return_value = nullptr;
        }
    }
  #endif

  return return_value;
}
//! [ExampleCustomBrushDefining]

//! [ExampleCustomBrushUsing]

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
  if (!m_custom_brush_shader)
    {
      end_demo(-1);
    }
}

void
ExampleCustomBrush::
draw_frame(void)
{
  fastuidraw::vec2 window_dims(window_dimensions());
  fastuidraw::PainterSurface::Viewport vwp(0, 0, window_dims.x(), window_dims.y());

  m_surface_gl->viewport(vwp);
  m_painter->begin(m_surface_gl, fastuidraw::Painter::y_increases_downwards);

  /* For this simple example, we will use SDL_GetTicks() to
   * get the number of milliseconds since the application started.
   */
  uint32_t t;
  t = SDL_GetTicks() % 4000u;

  /* Create an instance of our custom brush on the stack. */
  ExampleCustomBrushData brush;

  /* first set PainterBrush from which our custom brush builds
   * to have a linear gradient with no color modulation applied.
   */
  brush.m_brush_values
    .color(1.0f, 1.0f, 1.0f, 1.0f)
    .linear_gradient(m_color_stops,
                     window_dims * 0.45f,
                     window_dims * 0.55f,
                     fastuidraw::PainterBrush::spread_mirror_repeat);

  /* Now set the values of the m_phase, m_amplitide to animate the wave.
   * The value of m_period is used to determine the period of the wave.
   */
  float tf;
  tf = 2.0f * FASTUIDRAW_PI / 4000.0f * static_cast<float>(t);
  brush.m_phase = window_dims.y() * static_cast<float>(t) / 4000.0f;
  brush.m_period = window_dims.y();
  brush.m_amplitude = 0.1f * window_dims.x() * fastuidraw::t_cos(tf);

  /* Create a fastuidraw::PainterCustomBrush that sources data from
   * our ExampleCustomBrushData instance, brush, and uses our custom
   * shader brush. Note that the -address- of ExampleCustomBrushData
   * is passed.
   */
  fastuidraw::PainterCustomBrush custom_brush(m_custom_brush_shader.get(),
                                              &brush);

  /* Fill a rect with out custom brush. */
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
