/*!
 * \file painter_shader_set.hpp
 * \brief file painter_shader_set.hpp
 *
 * Copyright 2016 by Intel.
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
 *
 */


#pragma once

#include <fastuidraw/painter/painter_item_shader.hpp>
#include <fastuidraw/painter/painter_blend_shader.hpp>
#include <fastuidraw/text/glyph.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{

/*!\addtogroup Painter
  @{
 */

  /*!
    A PainterGlyphShader holds a shader pair
    for each glyph_type. The shaders are to
    handle attribute data as packed by
    PainterAttributeData.
   */
  class PainterGlyphShader
  {
  public:
    /*!
      Ctor, inits as all return value from shader(enum glyph_type)
      as a NULL PainterItemShader
     */
    PainterGlyphShader(void);

    /*!
      Copy ctor.
     */
    PainterGlyphShader(const PainterGlyphShader &obj);

    ~PainterGlyphShader();

    /*!
      Assignment operator.
     */
    PainterGlyphShader&
    operator=(const PainterGlyphShader &rhs);

    /*!
      Return the PainterItemShader for a given
      glyph_type.
      \param tp glyph type to render
     */
    const reference_counted_ptr<PainterItemShader>&
    shader(enum glyph_type tp) const;

    /*!
      Set the PainterItemShader for a given
      glyph_type.
      \param tp glyph type to render
      \param sh PainterItemShader to use for the glyph type
     */
    PainterGlyphShader&
    shader(enum glyph_type tp, const reference_counted_ptr<PainterItemShader> &sh);

    /*!
      Returns the one plus the largest value for which
      shader(enum glyph_type, PainterItemShader)
      was called.
     */
    unsigned int
    shader_count(void) const;

  private:
    void *m_d;
  };

  /*!
    A PainterBlendShaderSet represents a set of shaders
    for the blend modes enumerated by PainterEnums::blend_mode_t
   */
  class PainterBlendShaderSet
  {
  public:
    /*!
      Ctor, inits as all return value from shader(enum glyph_type)
      return a PainterItemShader with no shaders
     */
    PainterBlendShaderSet(void);

    /*!
      Copy ctor.
     */
    PainterBlendShaderSet(const PainterBlendShaderSet &obj);

    ~PainterBlendShaderSet();

    /*!
      Assignment operator.
     */
    PainterBlendShaderSet&
    operator=(const PainterBlendShaderSet &rhs);

    /*!
      Return the PainterShader for a given
      PainterEnums::blend_mode_t.
      \param tp blend mode
     */
    const reference_counted_ptr<PainterBlendShader>&
    shader(enum PainterEnums::blend_mode_t tp) const;

    /*!
      Set the PainterShader for a given
      PainterEnums::blend_mode_t.
      \param tp blend mode
      \param sh PainterShader to use for the blend mode
     */
    PainterBlendShaderSet&
    shader(enum PainterEnums::blend_mode_t tp,
           const reference_counted_ptr<PainterBlendShader> &sh);

    /*!
      Returns the one plus the largest value for which
      shader(enum PainterEnums::blend_mode_t, const reference_counted_ptr<PainterShader>&)
      was called.
     */
    unsigned int
    shader_count(void) const;

  private:
    void *m_d;
  };

  /*!
    A PainterStrokeShader hold shading for
    both stroking with and without anit-aliasing.
    The shader is to handle data as packed by
    PainterAttributeData for stroking of
    paths.
   */
  class PainterStrokeShader
  {
  public:

    /*!
      Specifies how a PainterStrokeShader implements anti-alias stroking.
     */
    enum type_t
      {
        /*!
          In this anti-aliasing mode, first the solid portions are drawn
          and then the anti-alias boundary is drawn. When anti-alias
          stroking is done this way, the depth-test is used to make
          sure that there is no overdraw when stroking the path.
          In this case, for aa_shader_pass1(), the vertex shader needs
          to emit the depth value of the z-value from the painter header
          (the value is Painter::current_z()) PLUS the value written
          to in PainterAttribute::m_uint_attrib.x() by PainterAttributeData.
          The vertex shader of aa_shader_pass2() should emit the depth
          value the same as the z-value from the painter header.
         */
        draws_solid_then_fuzz,

        /*!
          In this anti-aliasing mode, the first pass draws to an auxilary
          buffer the coverage values and in the second pass draws to
          the color buffer using the coverage buffer value to set the
          alpha. The second pass should also clear the coverage buffer
          too. Both passes have that the vertex shader should emit the
          depth value as the z-value from the painter header.
         */
        cover_then_draw,
      };

    /*!
      Ctor
     */
    PainterStrokeShader(void);

    /*!
      Copy ctor.
     */
    PainterStrokeShader(const PainterStrokeShader &obj);

    ~PainterStrokeShader();

    /*!
      Assignment operator.
     */
    PainterStrokeShader&
    operator=(const PainterStrokeShader &rhs);

    /*!
      Specifies how the stroke shader performs
      anti-aliased stroking.
     */
    enum type_t
    aa_type(void) const;

    /*!
      Set the value returned by aa_type(void) const.
      Initial value is draws_solid_then_fuzz.
      \param v value to use
     */
    PainterStrokeShader&
    aa_type(enum type_t v);

    /*!
      The 1st pass of stroking with anti-aliasing
      via alpha-coverage.
     */
    const reference_counted_ptr<PainterItemShader>&
    aa_shader_pass1(void) const;

    /*!
      Set the value returned by aa_shader_pass1(void) const.
      \param sh value to use
     */
    PainterStrokeShader&
    aa_shader_pass1(const reference_counted_ptr<PainterItemShader> &sh);

    /*!
      The 2nd pass of stroking with anti-aliasing
      via alpha-coverage.
     */
    const reference_counted_ptr<PainterItemShader>&
    aa_shader_pass2(void) const;

    /*!
      Set the value returned by aa_shader_pass2(void) const.
      \param sh value to use
     */
    PainterStrokeShader&
    aa_shader_pass2(const reference_counted_ptr<PainterItemShader> &sh);

    /*!
      Shader for rendering a stroked path without
      anti-aliasing. The depth value emitted
      in vertex shading should be z-value from
      the painter header (the value is
      Painter::current_z()) PLUS the value written
      to in PainterAttribute::m_uint_attrib.x()
      by PainterAttributeData.
     */
    const reference_counted_ptr<PainterItemShader>&
    non_aa_shader(void) const;

    /*!
      Set the value returned by non_aa_shader(void) const.
      \param sh value to use
     */
    PainterStrokeShader&
    non_aa_shader(const reference_counted_ptr<PainterItemShader> &sh);

  private:
    void *m_d;
  };


  /*!
    A PainterDashedStrokeShaderSet holds a collection of
    PainterStrokeShaderSet objects for the purpose of
    dashed stroking. The shaders within a
    PainterDashedStrokeShaderSet are expected to draw
    any caps of dashed stroking from using just the edge
    data. In particular, attributes/indices for caps are
    NEVER given to a shader within a PainterDashedStrokeShaderSet.
   */
  class PainterDashedStrokeShaderSet
  {
  public:
    /*!
      Ctor
     */
    PainterDashedStrokeShaderSet(void);

    /*!
      Copy ctor.
     */
    PainterDashedStrokeShaderSet(const PainterDashedStrokeShaderSet &obj);

    ~PainterDashedStrokeShaderSet();

    /*!
      Assignment operator.
     */
    PainterDashedStrokeShaderSet&
    operator=(const PainterDashedStrokeShaderSet &rhs);

    /*!
      Shader set for dashed stroking of paths where the stroking
      width is given in same units as the original path.
      The stroking parameters are given by PainterDashedStrokeParams.
      \param st cap style
     */
    const PainterStrokeShader&
    shader(enum PainterEnums::dashed_cap_style st) const;

    /*!
      Set the value returned by dashed_stroke_shader(enum PainterEnums::dashed_cap_style) const.
      \param st cap style
      \param sh value to use
     */
    PainterDashedStrokeShaderSet&
    shader(enum PainterEnums::dashed_cap_style st, const PainterStrokeShader &sh);

  private:
    void *m_d;
  };

  /*!
    A PainterShaderSet provides shaders for drawing
    each of the item types:
     - glyphs
     - stroking paths
     - filling paths
   */
  class PainterShaderSet
  {
  public:
    /*!
      Ctor, inits all as empty
     */
    PainterShaderSet(void);

    /*!
      Copy ctor.
      \param obj value from which to copy
     */
    PainterShaderSet(const PainterShaderSet &obj);

    ~PainterShaderSet();

    /*!
      Assignment operator.
      \param rhs value from which to copy
     */
    PainterShaderSet&
    operator=(const PainterShaderSet &rhs);

    /*!fn glyph_shader(void) const
      Shader set for rendering of glyphs with isotropic
      anti-aliasing.
     */
    const PainterGlyphShader&
    glyph_shader(void) const;

    /*!
      Set the value returned by glyph_shader(void) const.
      \param sh value to use
     */
    PainterShaderSet&
    glyph_shader(const PainterGlyphShader &sh);

    /*!
      Shader set for rendering glyphs with anisotropic
      anti-aliasing.
     */
    const PainterGlyphShader&
    glyph_shader_anisotropic(void) const;

    /*!
      Set the value returned by glyph_shader_anisotropic(void) const.
      \param sh value to use
     */
    PainterShaderSet&
    glyph_shader_anisotropic(const PainterGlyphShader &sh);

    /*!
      Shader set for stroking of paths where the stroking
      width is given in same units as the original path.
      The stroking parameters are given by PainterStrokeParams.
     */
    const PainterStrokeShader&
    stroke_shader(void) const;

    /*!
      Set the value returned by stroke_shader(void) const.
      \param sh value to use
     */
    PainterShaderSet&
    stroke_shader(const PainterStrokeShader &sh);

    /*!
      Shader set for stroking of paths where the stroking
      width is given in pixels. The stroking parameters are
      given by PainterStrokeParams.
     */
    const PainterStrokeShader&
    pixel_width_stroke_shader(void) const;

    /*!
      Set the value returned by
      pixel_width_stroke_shader(void) const.
      \param sh value to use
     */
    PainterShaderSet&
    pixel_width_stroke_shader(const PainterStrokeShader &sh);

    /*!
      Shader set for stroking of paths where the stroking
      width is given in same units as the original path.
      The stroking parameters are given by PainterStrokeParams.
     */
    const PainterDashedStrokeShaderSet&
    dashed_stroke_shader(void) const;

    /*!
      Set the value returned by stroke_shader(void) const.
      \param sh value to use
     */
    PainterShaderSet&
    dashed_stroke_shader(const PainterDashedStrokeShaderSet &sh);

    /*!
      Shader set for stroking of paths where the stroking
      width is given in pixels. The stroking parameters are
      given by PainterStrokeParams.
     */
    const PainterDashedStrokeShaderSet&
    pixel_width_dashed_stroke_shader(void) const;

    /*!
      Set the value returned by
      pixel_width_stroke_shader(void) const.
      \param sh value to use
     */
    PainterShaderSet&
    pixel_width_dashed_stroke_shader(const PainterDashedStrokeShaderSet &sh);

    /*!
      Shader for filling of paths. The vertex shader
      takes attribute data as formatted by
      PainterAttributeData.
     */
    const reference_counted_ptr<PainterItemShader>&
    fill_shader(void) const;

    /*!
      Set the value returned by fill_shader(void) const.
      \param sh value to use
     */
    PainterShaderSet&
    fill_shader(const reference_counted_ptr<PainterItemShader> &sh);

    /*!
      Blend shaders. If an element is a NULL shader, then that
      blend mode is not supported.
     */
    const PainterBlendShaderSet&
    blend_shaders(void) const;

    /*!
      Set the value returned by blend_shaders(void) const.
      \param sh value to use
     */
    PainterShaderSet&
    blend_shaders(const PainterBlendShaderSet &sh);

  private:
    void *m_d;
  };

/*! @} */
}
