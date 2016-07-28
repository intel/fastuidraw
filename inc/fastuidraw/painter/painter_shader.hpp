/*!
 * \file painter_shader.hpp
 * \brief file painter_shader.hpp
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
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/text/glyph.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{
  class PainterBackend;

/*!\addtogroup Painter
  @{
 */

  /*!
    A PainterShader encapsulates what a Painter needs to
    use a painter to assemble data to send to a PainterBackend
    to draw. The real meat of a PainterShader is dependent
    on the backend. Typically it is a source code -fragment-
    that is placed into a large uber-shader.
   */
  class PainterShader:
    public reference_counted<PainterShader>::default_base
  {
  public:

    /*!
      A Tag is how a PainterShader is described for
      and by a PainterBackend.
     */
    class Tag
    {
    public:
      /*!
        Ctor, initializes \ref m_ID and \ref m_group
        to 0.
       */
      Tag(void):
        m_ID(0),
        m_group(0)
      {}

      /*!
        The ID of a PainterShader is unique.
        Typically, \ref m_ID is used in a switch
        statement of an uber-shader.
       */
      uint32_t m_ID;

      /*!
        The group of a PainterShader is
        used to classify PainterShader objects
        into groups for the cases when draw call breaks
        are needed either to improve performance (to
        prevent divergent branching in shaders) or to
        insert API state changes. The value 0 is used
        to indicate "default" shader group. The NULL
        shader belongs to group 0.
       */
      uint32_t m_group;
    };

    /*!
      Ctor.
     */
    explicit
    PainterShader(void);

    virtual
    ~PainterShader();

    /*!
      Returns the ID of the shader, the shader
      must be registered to a PainterBackend
      to have an ID.
     */
    uint32_t
    ID(void) const;

    /*!
      Returns the shader group to which the shader belongs.
      A different value in group() triggers a call to
      PainterDrawCommand:draw_break() to note that
      the shader group changed.
     */
    uint32_t
    group(void) const;

    /*!
      Returns the Tag of the shader which holds
      the value for ID() in Tag::m_ID and group()
      in Tag::m_group.
     */
    Tag
    tag(void) const;

    /*!
      Called by a PainterBackend to register the shader to it.
      A PainterShader may only be registered once.
     */
    void
    register_shader(Tag tg, const PainterBackend *p);

    /*!
      Returns the PainterBackend to which the shader
      is registed. If not yet registered, returns NULL.
     */
    const PainterBackend*
    registered_to(void) const;

  private:
    void *m_d;
  };

  /*!
    A PainterItemShader encapsulates the vertex and
    fragment shader to draw an item type.
   */
  class PainterItemShader
  {
  public:
    /*!
      Default (empty) ctor.
     */
    PainterItemShader(void);

    /*!
      Copy ctor
      \param obj value from which to copy
     */
    PainterItemShader(const PainterItemShader &obj);

    ~PainterItemShader();

    /*!
      Assignment operator
      \param rhs value from which to copy
     */
    PainterItemShader&
    operator=(const PainterItemShader &rhs);

    /*!
      Vertex shader for the item.
     */
    const reference_counted_ptr<PainterShader>&
    vert_shader(void) const;

    /*!
      Set the value returned by vert_shader(void) const.
      \param sh value to use
     */
    PainterItemShader&
    vert_shader(const reference_counted_ptr<PainterShader> &sh);

    /*!
      Fragment shader for the item.
     */
    const reference_counted_ptr<PainterShader>&
    frag_shader(void) const;

    /*!
      Set the value returned by frag_shader(void) const.
      \param sh value to use
     */
    PainterItemShader&
    frag_shader(const reference_counted_ptr<PainterShader> &sh);

  private:
    void *m_d;
  };

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
      return a PainterItemShader with no shaders
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
    const PainterItemShader&
    shader(enum glyph_type tp) const;

    /*!
      Set the PainterItemShader for a given
      glyph_type.
      \param tp glyph type to render
      \param sh PainterItemShader to use for the glyph type
     */
    void
    shader(enum glyph_type tp, const PainterItemShader &sh);

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
    const reference_counted_ptr<PainterShader>&
    shader(enum PainterEnums::blend_mode_t tp) const;

    /*!
      Set the PainterShader for a given
      PainterEnums::blend_mode_t.
      \param tp blend mode
      \param sh PainterShader to use for the blend mode
     */
    void
    shader(enum PainterEnums::blend_mode_t tp,
           const reference_counted_ptr<PainterShader> &sh);

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
    const PainterItemShader&
    aa_shader_pass1(void) const;

    /*!
      Set the value returned by aa_shader_pass1(void) const.
      \param sh value to use
     */
    PainterStrokeShader&
    aa_shader_pass1(const PainterItemShader &sh);

    /*!
      The 2nd pass of stroking with anti-aliasing
      via alpha-coverage.
     */
    const PainterItemShader&
    aa_shader_pass2(void) const;

    /*!
      Set the value returned by aa_shader_pass2(void) const.
      \param sh value to use
     */
    PainterStrokeShader&
    aa_shader_pass2(const PainterItemShader &sh);

    /*!
      Shader for rendering a stroked path without
      anti-aliasing. The depth value emitted
      in vertex shading should be z-value from
      the painter header (the value is
      Painter::current_z()) PLUS the value written
      to in PainterAttribute::m_uint_attrib.x()
      by PainterAttributeData.
     */
    const PainterItemShader&
    non_aa_shader(void) const;

    /*!
      Set the value returned by non_aa_shader(void) const.
      \param sh value to use
     */
    PainterStrokeShader&
    non_aa_shader(const PainterItemShader &sh);

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
      The stroking parameters are given by StrokeParams.
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
      given by StrokeParams.
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
      Shader for filling of paths. The vertex shader
      takes attribute data as formatted by
      PainterAttributeData.
     */
    const PainterItemShader&
    fill_shader(void) const;

    /*!
      Set the value returned by fill_shader(void) const.
      \param sh value to use
     */
    PainterShaderSet&
    fill_shader(const PainterItemShader &sh);

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
