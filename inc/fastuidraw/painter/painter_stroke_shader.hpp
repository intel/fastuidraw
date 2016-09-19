/*!
 * \file painter_stroke_shader.hpp
 * \brief file painter_stroke_shader.hpp
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
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{

/*!\addtogroup Painter
  @{
 */

  /*!
    A ChunkSelector provides an interface to know
    what chuck of a PainterAttributeData to grab
    for different data to stroke.
  */
  class StrokingChunkSelectorBase:
    public reference_counted<StrokingChunkSelectorBase>::default_base
  {
  public:
    /*!
      To be implemented by a derived class to return
      the chunk index, i.e. the value to feed
      \ref PainterAttributeData::attribute_data_chunk()
      and \ref PainterAttributeData::index_data_chunk(),
      for the named cap style.
      \param cp cap style
     */
    virtual
    unsigned int
    cap_chunk(enum PainterEnums::cap_style cp) const = 0;

    /*!
      To be implemented by a derived class to return
      the chunk index, i.e. the value to feed
      \ref PainterAttributeData::attribute_data_chunk()
      and \ref PainterAttributeData::index_data_chunk()
      for the edges.
      \param edge_closed if true, return the chunk that includes
                         the closing edge
     */
    virtual
    unsigned int
    edge_chunk(bool edge_closed) const = 0;

    /*!
      To be implemented by a derived class to return
      the chunk index, i.e. the value to feed
      \ref PainterAttributeData::attribute_data_chunk()
      and \ref PainterAttributeData::index_data_chunk(),
      for the named join style.
      \param js join style
      \param edge_closed if true, return the chunk that includes
                         the joins for the closing edge
     */
    virtual
    unsigned int
    join_chunk(enum PainterEnums::join_style js, bool edge_closed) const = 0;

      /*!
        To be implemented by a derived class to return
        the chunk index, i.e. the value to feed
        \ref PainterAttributeData::attribute_data_chunk()
        and \ref PainterAttributeData::index_data_chunk(),
        for the named join of a join style.
        \param js join style
        \param J (global) join index
       */
      virtual
      unsigned int
      named_join_chunk(enum PainterEnums::join_style js, unsigned int J) const = 0;

      /*!
        To be implemented by a derived class to return
        the chunk index, i.e. the value to feed
        \ref PainterAttributeData::attribute_data_chunk()
        and \ref PainterAttributeData::index_data_chunk(),
        for the cap joins
        \param J (global) join index
       */
      virtual
      unsigned int
      chunk_from_cap_join(unsigned int J) const = 0;
    };

  /*!
    A PainterStrokeShader hold shading for
    both stroking with and without anit-aliasing.
    The shader is to handle data as packed by
    PainterAttributeDataFillerPathStroked.
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

    /*!
      Returns a reference to the ChunkSelector to be used
      with the PainterStrokeShader
     */
    const reference_counted_ptr<StrokingChunkSelectorBase>&
    chunk_selector(void) const;

    /*!
      Set the value returned by chunk_selector(void) const.
      \param ch value to use
     */
    PainterStrokeShader&
    chunk_selector(const reference_counted_ptr<StrokingChunkSelectorBase> &ch);

  private:
    void *m_d;
  };

/*! @} */
}
