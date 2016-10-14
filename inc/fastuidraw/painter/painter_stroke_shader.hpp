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
#include <fastuidraw/painter/painter_shader_data.hpp>

namespace fastuidraw
{

/*!\addtogroup Painter
  @{
 */

  ///@cond
  class PainterAttributeData;
  ///@endcond

  /*!
    A StrokingDataSelector is an interface to assist Painter
    to select correct LOD for rounded joins and caps when
    drawing rounded joins and caps.
   */
  class StrokingDataSelectorBase:
    public reference_counted<StrokingDataSelectorBase>::default_base
  {
  public:
    /*!
      To be implemented by a derived class to compute the
      the value for thresh to feed to StrokedPath::rounded_joins(float)
      and StrokedPath::rounded_caps(float) to get a good
      level of detail to draw rounded joins or caps.
      \param data PainterItemShaderData::DataBase object holding
                  the data to be sent to the shader
      \param thresh threshhold used to select the StrokedPath
     */
    virtual
    float
    compute_rounded_thresh(const PainterShaderData::DataBase *data,
                           float thresh) const = 0;

    /*!
      To be implemented by a derived class to give by how
      much the stroking gives thickness to the stroked path.
      These values are geometrically added together. The
      intersection test performed is to first inflate the
      bounding boxes in local coordinates by the output
      *out_item_space_distance, then to convert the box
      to clip-coordinates and then push each clip-equation
      by *out_pixel_space_distance
      \param out_clip_space_distance[output] how much geometry inflates in pixels
      \param out_item_space_distance[output] how much geometry inflates in local coordinates
     */
    virtual
    void
    stroking_distances(const PainterShaderData::DataBase *data,
                       float *out_pixel_space_distance,
                       float *out_item_space_distance) const = 0;
  };

  /*!
    A PainterStrokeShader holds shaders for
    stroking with and without anit-aliasing.
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
      Returns the StrokingDataSelectorBase associated to this
      PainterStrokeShader.
     */
    const reference_counted_ptr<const StrokingDataSelectorBase>&
    stroking_data_selector(void) const;

    /*!
      Set the value returned by stroking_data_selector(void) const.
      \param sh value to use
     */
    PainterStrokeShader&
    stroking_data_selector(const reference_counted_ptr<const StrokingDataSelectorBase> &sh);

  private:
    void *m_d;
  };

/*! @} */
}
