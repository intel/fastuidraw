/*!
 * \file example_custom_path_shading.cpp
 * \brief file example_custom_path_shading.cpp
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

#include <iostream>
#include <map>
#include <fastuidraw/util/math.hpp>
#include "initialization.hpp"

//! [ExampleCustomPathShaderDefining]

/* The data for the custom shader is a set of numbers followed
 * by a fastuidraw::PainterStrokeParams object.
 */
class ExampleItemData:public fastuidraw::PainterItemShaderData
{
public:
  /* This function is what FastUIDraw calls to pack data for
   * the shader to read. In this example we pack first the data
   * to implement the wavy effect followed by the the data of
   * a fastuidraw::PainterStrokeParams object.
   */
  virtual
  void
  pack_data(fastuidraw::c_array<fastuidraw::uvec4> dst) const override
  {
    float sum(0.0f);

    for (int i = 0; i < 4; ++i)
      {
        sum += fastuidraw::t_abs(m_cos_coeffs[i]);
        sum += fastuidraw::t_abs(m_sin_coeffs[i]);
      }

    dst[0] = fastuidraw::pack_vec4(m_cos_coeffs);
    dst[1] = fastuidraw::pack_vec4(m_sin_coeffs);
    dst[2].x() = fastuidraw::pack_float(m_domain_coeff);
    dst[2].y() = fastuidraw::pack_float(1.0f / sum);
    dst[2].z() = fastuidraw::pack_float(m_phase);
    dst[2].w() = fastuidraw::pack_float(m_stroke_params.width());
    m_stroke_params.pack_data(dst.sub_array(3));
  }

  /* This function is what FastUIDraw calls to know what is the
   * size of the data to pack.
   */
  virtual
  unsigned int
  data_size(void) const override
  {
    return 3 + m_stroke_params.data_size();
  }

  /* The numbers to control the wavy effect */
  float m_domain_coeff, m_phase;
  fastuidraw::vecN<float, 4> m_cos_coeffs;
  fastuidraw::vecN<float, 4> m_sin_coeffs;

  /* the values specifying to the base-shader how to stroke */
  fastuidraw::PainterStrokeParams m_stroke_params;
};

/* We want to create a PainterItemShaderGLSL that has a dependee
 * a PainterItemShaderGLSL that does the actual stroking. However,
 * an item-shader can (but does not need to) have a coverage shader
 * that is called first to draw to an auxiliary coverage buffer that
 * the actual item shaders uses for a coverage value for shader-based
 * anti-aliasing.
 *
 * Rather than duplicating the code, we will use template function to
 * do the shader generation. However, the ctor of PainterItemShaderGLSL
 * and PainterItemCoverageShaderGLSL are different in their argument
 * lists, so we rely on C++ function overloading to call the ctor via
 * a function between, call_ctor.
 */

fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL>
call_ctor(bool uses_discard,
          const fastuidraw::glsl::ShaderSource &vert_src,
          const fastuidraw::glsl::ShaderSource &frag_src,
          const fastuidraw::glsl::PainterItemShaderGLSL::DependencyList deps,
          unsigned int num_subshaders)
{
  /* The first argument to PainterItemShaderGLSL is if the the item shader
   * uses dicard. The GL/GLES backends of FastUIDraw seperate the item
   * shaders that use and don't use discard from each other into seperate
   * GL programs. This is done because shaders that have discard stop a
   * GPU from performing a variety of optimiations (the biggest ones being
   * early-depth-test and hierarchical occlusion.
   */
  return FASTUIDRAWnew fastuidraw::glsl::PainterItemShaderGLSL(uses_discard,
                                                               vert_src,
                                                               frag_src,
                                                               fastuidraw::glsl::varying_list(),
                                                               deps,
                                                               num_subshaders);
}

fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemCoverageShaderGLSL>
call_ctor(bool,
          const fastuidraw::glsl::ShaderSource &vert_src,
          const fastuidraw::glsl::ShaderSource &frag_src,
          const fastuidraw::glsl::PainterItemCoverageShaderGLSL::DependencyList deps,
          unsigned int num_subshaders)
{
  return FASTUIDRAWnew fastuidraw::glsl::PainterItemCoverageShaderGLSL(vert_src,
                                                                       frag_src,
                                                                       fastuidraw::glsl::varying_list(),
                                                                       deps,
                                                                       num_subshaders);
}

bool
uses_discard(const fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL> &sh,
             bool apply_wavy_effect)
{
  return apply_wavy_effect || sh->uses_discard();
}

bool
uses_discard(const fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemCoverageShaderGLSL>&,
             bool)
{
  return false;
}

/* This beast of a function is used to generate the custom item and coverage shaders
 * that apply a wavy effect to stroking. The stroking shaders of FastUIDraw's GL/GLES
 * backends operate as follows:
 *   - the non-anti-aliased shader does not have a PainterItemCoverageShader. Thus
 *     the fragment shader of the non-anti-aliased shader is the one that performs
 *     any possible stroking computation.
 *   - the anti-aliased shader has a coverage shader. The coverage shader is the
 *     shader that performs the diffucult computation (in the case of arc-stroking
 *     or dashed stroking) of if a fragment is covered and if so, what it the coverage.
 *     The item shader simply reads from the coverage buffer and emits the value.
 *     Hence, in the anti-aliased case, the item shader does NOT apply the wavy effect,
 *     but the coverage shader does. However, both read from the shader data and we
 *     placed teh wavy effect values before the stroking parameters, thus we still
 *     need a custom shader to place the shader data read location at the correct
 *     location for the base shader to read.
 */
template<typename T>
fastuidraw::reference_counted_ptr<T>
create_custom_shader(const fastuidraw::reference_counted_ptr<T> &stroking_shader,
                     bool add_wavy_effect)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;

  /* Add the dependency of the base stroking shader and refer to as
   * stroke_shader in the custom item shader.
   */
  typename T::DependencyList deps;
  deps.add_shader("stroke_shader", stroking_shader);

  /* The custom vertex shader is essentially just a pass through to
   * the vertex shader of the stroking shader, except that is needs
   * to adjust the shader_data_block to where the stroking parameters
   * start.
   *
   * The signature for the vertex-shaders for PainterItemShaderGLSL and
   * PainterItemCoverageShaderGLSL is different in the argument list.
   * FastUIDraw will define the symbol FASTUIDRAW_RENDER_TO_COLOR_BUFFER
   * for non-coverage shaders. This way, an application can share more
   * shader code internally more easily and we take advantage of that
   * here.
   */
  const char *custom_vert_shader =
    "void\n"
    "fastuidraw_gl_vert_main(in uint sub_shader,\n"
    "                        in uvec4 in_attrib0,\n"
    "                        in uvec4 in_attrib1,\n"
    "                        in uvec4 in_attrib2,\n"
    "                        inout uint shader_data_block,\n"
    "                        #ifdef FASTUIDRAW_RENDER_TO_COLOR_BUFFER\n"
    "                        out int z_add,\n"
    "                        out vec2 out_brush_p,\n"
    "                        #endif\n"
    "                        out vec3 out_clip_p)\n"
    "{\n"
    "    shader_data_block += 3u;\n"
    "    stroke_shader(sub_shader, in_attrib0, in_attrib1, in_attrib2,\n"
    "                  shader_data_block,\n"
    "                  #ifdef FASTUIDRAW_RENDER_TO_COLOR_BUFFER\n"
    "                  z_add, out_brush_p,\n"
    "                  #endif\n"
    "                  out_clip_p);\n"
    "}\n";

  /* This is the fragment shader that applies to wave-effect to the stroking.
   * Similair to the vertex shader, the signatures for the fragment shader
   * for PainterItemShaderGLSL and PainterItemCoverageShaderGLSL are different
   * (the former returning a vec4 color value and the latter just a scalar float
   * coverage value). Again, we test for the symbol FASTUIDRAW_RENDER_TO_COLOR_BUFFER
   * to know if we are doing a PainterItemShaderGLSL or PainterItemCoverageShaderGLSL.
   */
  const char *custom_wavy_frag_shader =
    "#ifdef FASTUIDRAW_RENDER_TO_COLOR_BUFFER\n"
    "#define return_type vec4\n"
    "#else\n"
    "#define return_type float\n"
    "#endif\n"
    "return_type\n"
    "fastuidraw_gl_frag_main(in uint sub_shader,\n"
    "                        inout uint shader_data_block)\n"
    "{\n"
    "   vec4 cos_coeffs, sin_coeffs;\n"
    "   uvec4 tmp;\n"
    "   float coeff, inverse_sum, phase, width;\n"
    "   return_type return_value;\n"
    "   cos_coeffs = uintBitsToFloat(fastuidraw_fetch_data(shader_data_block));\n"
    "   sin_coeffs = uintBitsToFloat(fastuidraw_fetch_data(shader_data_block + 1u));\n"
    "   tmp = fastuidraw_fetch_data(shader_data_block + 2u);\n"
    "   coeff = uintBitsToFloat(tmp.x);\n"
    "   inverse_sum = uintBitsToFloat(tmp.y);\n"
    "   phase = uintBitsToFloat(tmp.z);\n"
    "   width = uintBitsToFloat(tmp.w);\n"
    "   shader_data_block += 3u;\n"
    "\n"
    "   return_value = stroke_shader(sub_shader, shader_data_block);\n"
    // use the symbols fastuidraw_stroking_relative_distance_from_center
    // and fastuidraw_stroking_distance to know where and how far the fragment
    // is from the path and how far along it is on the path
    "   float a, r;\n"
    "   vec4 cos_tuple, sin_tuple;\n"
    "   r = coeff * stroke_shader::fastuidraw_stroking_distance + phase;\n"
    "   cos_tuple = vec4(cos(r), cos(2.0 * r), cos(3.0 * r), cos(4.0 * r));\n"
    "   sin_tuple = vec4(sin(r), sin(2.0 * r), sin(3.0 * r), sin(4.0 * r));\n"
    "   a = inverse_sum * (dot(cos_coeffs, cos_tuple) + dot(sin_coeffs, sin_tuple));\n"
    "   a = abs(a);\n"
    "#ifdef FASTUIDRAW_RENDER_TO_COLOR_BUFFER\n"
    "   if (a < stroke_shader::fastuidraw_stroking_relative_distance_from_center)\n"
    "     FASTUIDRAW_DISCARD;\n"
    "#else\n"
    "   float q, dd;\n"
    // we want a coverage value so that if a < stroke_shader::fastuidraw_stroking_relative_distance_from_center
    // then we get zero, but we want to apply some anti-aliasing as well.
    "   q = max(a - stroke_shader::fastuidraw_stroking_relative_distance_from_center);\n"
    "   dd = max(q, stroke_shader::fastuidraw_stroking_relative_distance_from_center_fwidth);\n"
    "   return_value *= q / dd;\n"
    "#endif\n"
    "   return return_value;\n"
    "}\n"
    "#undef return_type\n";

  /* This shader string is used if the fragment shader does not apply the wavy effect
   * and just needs to pass down to the stroking shader's fragment shader. As for the
   * vertex shader case, its main purpose then is to just update the location of
   * the shader data to where the stroking parameters start.
   */
  const char *custom_pass_through_frag_shader =
    "#ifdef FASTUIDRAW_RENDER_TO_COLOR_BUFFER\n"
    "vec4\n"
    "#else\n"
    "float\n"
    "#endif\n"
    "fastuidraw_gl_frag_main(in uint sub_shader,\n"
    "                        inout uint shader_data_block)\n"
    "{\n"
    "   shader_data_block += 3u;"
    "   return stroke_shader(sub_shader, shader_data_block);\n"
    "}\n";

  const char *custom_frag_shader;

  custom_frag_shader = (add_wavy_effect) ?
    custom_wavy_frag_shader:
    custom_pass_through_frag_shader;

  return call_ctor(uses_discard(stroking_shader, add_wavy_effect),
                   ShaderSource().add_source(custom_vert_shader, ShaderSource::from_string),
                   ShaderSource().add_source(custom_frag_shader, ShaderSource::from_string),
                   deps,
                   stroking_shader->number_sub_shaders());
}


/* The next block of code is essentially ugly boilerplate code to handle
 * the that item shaders of a fastuidraw::PainterStrokeShader are realized
 * as sub-shaders of a parent shader and we wish to follow the that
 * structure.
 */
template<typename T>
class CustomShaderGenerator:fastuidraw::noncopyable
{
public:
  typedef fastuidraw::reference_counted_ptr<T> shader_ref;
  typedef std::map<shader_ref, shader_ref> shader_map;

  shader_ref
  fetch_generate_custom_shader(const shader_ref &src, bool add_wavy_effect)
  {
    typename shader_map::iterator iter;
    shader_ref return_value;

    iter = m_shaders.find(src);
    if (iter != m_shaders.end())
      {
        return_value = iter->second;
      }
    else
      {
        return_value = create_custom_shader<T>(src, add_wavy_effect);
        m_shaders[src] = return_value;
      }
    return return_value;
  }

private:
  shader_map m_shaders;
};

/* Finally, the code that generates the item shader from a base shader.
 * The arguments item_shaders and cvg_shaders are for re-using parent
 * shaders if the passed source shader, src, is a sub-shader.
 */
fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>
generate_item_shader(CustomShaderGenerator<fastuidraw::glsl::PainterItemShaderGLSL> &item_shaders,
                     CustomShaderGenerator<fastuidraw::glsl::PainterItemCoverageShaderGLSL> &cvg_shaders,
                     fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> src)
{
  using namespace fastuidraw;
  using namespace glsl;

  reference_counted_ptr<PainterItemCoverageShader> src_cvg, use_cvg;
  reference_counted_ptr<PainterItemShaderGLSL> src_glsl, use;

  /* Get the actual item shader that implements the code. If a shader
   * is realized as a sub-shader, then parent() will be a non-null
   * reference to the shader that implements the code (which then for
   * the GL and GLES backends is then a PainterItemShaderGLSL).
   */
  if (src->parent())
    {
      src_glsl = src->parent().dynamic_cast_ptr<PainterItemShaderGLSL>();
    }
  else
    {
      src_glsl = src.dynamic_cast_ptr<PainterItemShaderGLSL>();
    }
  FASTUIDRAWassert(src_glsl);

  /* Check if the item-shader uses a coverage shader, if it does, then
   * it is the coverage shader that implements the wavy-effect.
   */
  src_cvg = src->coverage_shader();
  if (src_cvg)
    {
      fastuidraw::reference_counted_ptr<PainterItemCoverageShaderGLSL> src_cvg_glsl;

      if (src_cvg->parent())
        {
          src_cvg_glsl = src_cvg->parent().dynamic_cast_ptr<PainterItemCoverageShaderGLSL>();
        }
      else
        {
          src_cvg_glsl = src_cvg.dynamic_cast_ptr<PainterItemCoverageShaderGLSL>();
        }
      FASTUIDRAWassert(src_cvg_glsl);

      /* the coverage shader does the pixel coverage */
      use_cvg = cvg_shaders.fetch_generate_custom_shader(src_cvg_glsl, true);
      use = item_shaders.fetch_generate_custom_shader(src_glsl, false);

      /* use correct sub-shader from use_cvg */
      use_cvg = FASTUIDRAWnew PainterItemCoverageShader(use_cvg, src_cvg->sub_shader());
    }
  else
    {
      /* Fragment's item shader does the pixel computation */
      use = item_shaders.fetch_generate_custom_shader(src_glsl, true);
    }

  return FASTUIDRAWnew PainterItemShader(use, src->sub_shader(), use_cvg);
}

/* A PainterStrokeShader consists of 4 items shaders. There is a stroking
 * methods of how to stroke (see Painter::stroking_method_t) and if shader
 * based anti-aliasing is appled (see PainterStrokeShader::shader_type_t).
 * This function is just a conveniance function to make typing less awful.
 */
void
set_shader(fastuidraw::PainterStrokeShader &dst,
           CustomShaderGenerator<fastuidraw::glsl::PainterItemShaderGLSL> &item_shaders,
           CustomShaderGenerator<fastuidraw::glsl::PainterItemCoverageShaderGLSL> &cvg_shaders,
           const fastuidraw::PainterStrokeShader &src,
           enum fastuidraw::Painter::stroking_method_t tp,
           enum fastuidraw::PainterStrokeShader::shader_type_t sh)
{
  fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> new_shader, src_shader;

  src_shader = src.shader(tp, sh);
  new_shader = generate_item_shader(item_shaders, cvg_shaders, src_shader);
  FASTUIDRAWassert(new_shader);

  dst.shader(tp, sh, new_shader);
}

/* Attached to a PainterStrokeShader is also a StrokingDataSelectorBase
 * that FastUIDraw uses to realize how much the path geometry is inflated
 * on stroking. These objects use the -packed- data of the PainterItemShaderData
 * to compute this. Since the stroking data starts after the wavy-effect data,
 * we need to have a StrokingDataSelectorBase that takes this into account
 * before passing the data to the "real" StrokingDataSelectorBase object.
 */
class CustomStrokingDataSelector:public fastuidraw::StrokingDataSelectorBase
{
public:
  explicit
  CustomStrokingDataSelector(const fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase> &base):
    m_base(base)
  {}

  virtual
  float
  compute_thresh(fastuidraw::c_array<const fastuidraw::uvec4> data,
                 float path_magnification,
                 float curve_flatness) const override
  {
    return m_base->compute_thresh(data.sub_array(3),
                                  path_magnification,
                                  curve_flatness);
  }

  virtual
  void
  stroking_distances(fastuidraw::c_array<const fastuidraw::uvec4 > data,
                     fastuidraw::c_array<float> out_values) const override
  {
    m_base->stroking_distances(data.sub_array(3), out_values);
  }

  virtual
  bool
  arc_stroking_possible(fastuidraw::c_array<const fastuidraw::uvec4 > data) const override
  {
    return m_base->arc_stroking_possible(data.sub_array(3));
  }

  virtual
  bool
  data_compatible(fastuidraw::c_array<const fastuidraw::uvec4 > data) const override
  {
    return m_base->data_compatible(data.sub_array(3));
  }

  fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase> m_base;
};

/* Finally, the actual function that generates the custom PainterStrokeShader.
 * This function needs to assign each of the 4 different shaders of its return
 * value correct, set the StrokingDataSelectorBase that matches with the shader
 * data and also set meta-data that FastUIDraw uses.
 */
fastuidraw::PainterStrokeShader
generate_stroke_shader(const fastuidraw::PainterStrokeShader &src)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;

  CustomShaderGenerator<PainterItemShaderGLSL> item_shaders;
  CustomShaderGenerator<PainterItemCoverageShaderGLSL> cvg_shaders;
  PainterStrokeShader return_value;

  set_shader(return_value, item_shaders, cvg_shaders,
             src, Painter::stroking_method_linear,
             PainterStrokeShader::non_aa_shader);

  set_shader(return_value, item_shaders, cvg_shaders,
             src, Painter::stroking_method_arc,
             PainterStrokeShader::non_aa_shader);

  set_shader(return_value, item_shaders, cvg_shaders,
             src, Painter::stroking_method_linear,
             PainterStrokeShader::aa_shader);

  set_shader(return_value, item_shaders, cvg_shaders,
             src, Painter::stroking_method_arc,
             PainterStrokeShader::aa_shader);

  /* The meta-data, PainterStrokeShader::fastest_anti_aliased_stroking_method() and
   * PainterStrokeShader::fastest_non_anti_aliased_stroking_method() is used by
   * FastUIDraw to select what stroking method to use when passed the value
   * PainterEnum::stroking_method_fastest.
   */
  return_value
    .stroking_data_selector(FASTUIDRAWnew CustomStrokingDataSelector(src.stroking_data_selector()))
    .fastest_non_anti_aliased_stroking_method(src.fastest_non_anti_aliased_stroking_method())
    .fastest_anti_aliased_stroking_method(src.fastest_anti_aliased_stroking_method());

  return return_value;
}
//! [ExampleCustomPathShaderDefining]

//! [ExampleCustomPathShading]
class ExampleCustomPathShading:public Initialization
{
public:
  ExampleCustomPathShading(DemoRunner *runner, int argc, char **argv);

  virtual
  void
  draw_frame(void) override;

  virtual
  void
  handle_event(const SDL_Event &ev) override;

private:
  fastuidraw::Path m_path;
  fastuidraw::Rect m_path_bounds;
  fastuidraw::PainterStrokeShader m_stroke_shader;
};

ExampleCustomPathShading::
ExampleCustomPathShading(DemoRunner *runner, int argc, char **argv):
  Initialization(runner, argc, argv)
{
  /* In this example we build a complicated path using the
   * operator<< overloads that fastuidraw::Path defines.
   */
  m_path << fastuidraw::Path::contour_start(fastuidraw::vec2( 460, 60 ))
         << fastuidraw::vec2( 644, 134 )
         << fastuidraw::vec2( 544, 367 )
         << fastuidraw::Path::contour_end()
         << fastuidraw::Path::contour_start(fastuidraw::vec2( 560, 60 ))
         << fastuidraw::vec2( 644, 367 )
         << fastuidraw::vec2( 744, 134 )
         << fastuidraw::Path::contour_end()
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
         << fastuidraw::Path::contour_end()
         << fastuidraw::Path::contour_start(fastuidraw::vec2( 59, 9 ))
         << fastuidraw::vec2( 59, -209 )
         << fastuidraw::vec2( 519, -209 )
         << fastuidraw::vec2( 519, 9 )
         << fastuidraw::Path::arc_degrees(-180, fastuidraw::vec2(100, -209))
         << fastuidraw::Path::contour_start(fastuidraw::vec2( 160, 60 ))
         << fastuidraw::vec2( 344, 134 )
         << fastuidraw::vec2( 244, 367 )
         << fastuidraw::Path::contour_end()
         << fastuidraw::Path::contour_start(fastuidraw::vec2( 260, 60 ))
         << fastuidraw::vec2( 344, 367 )
         << fastuidraw::vec2( 444, 134 )
         << fastuidraw::Path::contour_end()
         << fastuidraw::Path::contour_start(fastuidraw::vec2( 360, 60 ))
         << fastuidraw::vec2( 544, 134 )
         << fastuidraw::vec2( 444, 367 )
         << fastuidraw::Path::contour_end()
         << fastuidraw::Path::contour_start(fastuidraw::vec2( -60, -60 ))
         << fastuidraw::vec2( -100, 300 )
         << fastuidraw::vec2( 60, 500 )
         << fastuidraw::vec2( 200, 570 )
         << fastuidraw::Path::arc_degrees(80, fastuidraw::vec2(300, 100))
         << fastuidraw::Path::contour_end();


  /* Get the approximate bounding box for the path. This approximate
   * bounding box computation is a cheap function returning cached
   * values.
   */
  m_path.approximate_bounding_box(&m_path_bounds);

  m_stroke_shader = generate_stroke_shader(m_painter_engine_gl->default_shaders().stroke_shader());
  m_painter_engine_gl->register_shader(m_stroke_shader);
}

void
ExampleCustomPathShading::
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

  ExampleItemData data;
  uint32_t ms;
  float fc, fs, fc2, fs2;

  ms = SDL_GetTicks() % 4000;

  data.m_phase = static_cast<float>(ms) / 2000.0f * FASTUIDRAW_PI;
  fc = fastuidraw::t_cos(data.m_phase);
  fs = fastuidraw::t_sin(data.m_phase);
  fc2 = fastuidraw::t_cos(2.0f * data.m_phase);
  fs2 = fastuidraw::t_sin(2.0f * data.m_phase);

  data.m_domain_coeff = 10.0f * FASTUIDRAW_PI / (m_path_bounds.width() + m_path_bounds.height());
  data.m_cos_coeffs = fastuidraw::vec4(+8.0f * fc,
                                       -4.0f * fs,
                                       +2.0f * fs2,
                                       -1.0f * fc2);
  data.m_sin_coeffs = fastuidraw::vec4(+1.3f * fs,
                                       -2.0f * fc,
                                       +4.0f * fs2,
                                       -8.0f * fc2);
  data.m_stroke_params.width(40.0f);

  fastuidraw::PainterBrush brush;
  brush.color(1.0f, 0.6f, 0.0f, 0.8f);

  m_painter->stroke_path(m_stroke_shader,
                         fastuidraw::PainterData(&brush, &data),
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
ExampleCustomPathShading::
handle_event(const SDL_Event &ev)
{
  switch (ev.type)
    {
    case SDL_KEYDOWN:
      switch (ev.key.keysym.sym)
        {
        }
      break;
    }
  Initialization::handle_event(ev);
}

int
main(int argc, char **argv)
{
  DemoRunner demo;
  return demo.main<ExampleCustomPathShading>(argc, argv);
}

//! [ExampleCustomPathShading]
