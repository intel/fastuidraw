/*!
 * \file painter_stroke_shader.hpp
 * \brief file painter_stroke_shader.hpp
 *
 * Copyright 2016 by Intel.
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
 *
 */


#ifndef FASTUIDRAW_PAINTER_STROKE_SHADER_HPP
#define FASTUIDRAW_PAINTER_STROKE_SHADER_HPP

#include <fastuidraw/path_enums.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/shader_data/painter_shader_data.hpp>
#include <fastuidraw/painter/shader/painter_item_shader.hpp>
#include <fastuidraw/painter/backend/painter_draw.hpp>

namespace fastuidraw
{

/*!\addtogroup PainterShaders
 * @{
 */

  ///@cond
  class PainterAttributeData;
  ///@endcond

  /*!
   * \brief
   * A StrokingDataSelector is an interface to assist Painter
   * to select correct LOD for rounded joins and caps when
   * drawing rounded joins and caps. In addition it also
   * informs Painter if stroking via arcs is possible with
   * the stroking parameters.
   */
  class StrokingDataSelectorBase:
    public reference_counted<StrokingDataSelectorBase>::concurrent,
    public PathEnums
  {
  public:
    /*!
     * To be implemented by a derived class to compute the value
     * used to select rounded join level of detail (\ref
     * StrokedPath::rounded_joins()) and rounded cap level of detail
     * (\ref StrokedPath::rounded_caps()).
     * \param data packed data to be sent to the shader
     * \param path_magnification by how much the path is magnified
     *                           from its native coordiantes to pixel
     *                           coordinates.
     * \param curve_flatness curve flatness
     */
    virtual
    float
    compute_thresh(c_array<const uvec4 > data,
                   float path_magnification,
                   float curve_flatness) const = 0;

    /*!
     * To be implemented by a derived class to give by how
     * much the stroking gives thickness to the stroked path.
     * These values are geometrically added together. The
     * intersection test performed is to first inflate the
     * bounding boxes in local coordinates by the output
     * out_item_space_distance, then to convert the box
     * to clip-coordinates and then push each clip-equation
     * by *out_pixel_space_distance
     * \param data packed data to be sent to the shader
     * \param[out] out_values output with array indexed as according
     *                        to \ref path_geometry_inflation_index_t
     */
    virtual
    void
    stroking_distances(c_array<const uvec4 > data,
                       c_array<float> out_values) const = 0;

    /*!
     * To be implemented by a derived class to specify if
     * arc-stroking is possible with the stroking parameters.
     * \param data packed data to be sent to the shader
     */
    virtual
    bool
    arc_stroking_possible(c_array<const uvec4 > data) const = 0;

    /*!
     * To be implemented by a derived class to specify if the passed
     * data is suitable for it.
     * \param data packed data to be sent to the shader
     */
    virtual
    bool
    data_compatible(c_array<const uvec4 > data) const = 0;
  };

  /*!
   * \brief
   * A PainterStrokeShader holds shaders for stroking. It is to hold
   * shaders for stroking paths linearly or via arcs with and without
   * anti-aliasing along with meta-data to inform what shading is faster.
   */
  class PainterStrokeShader
  {
  public:
    /*!
     * Enumeration to specify what shader
     */
    enum shader_type_t
      {
        /*!
         * Specify the shader for rendering a stroked path without
         * anti-aliasing. The depth value emitted in the item's
         * vertex shader should be z-value to guarantee that there
         * is no overdraw, see \ref StrokedPoint::depth() and \ref
         * ArcStrokedPoint::depth().
         */
        non_aa_shader,

        /*!
         * Specifies a two-pass shader where the first pass renders to the
         * deferred coverage buffer (via PainterItemShader::coverage_shader())
         * and the second pass reads from it. The depth value emitted in the
         * item's vertex shader should be z-value to guarantee that
         * there is no overdraw, see \ref StrokedPoint::depth() and \ref
         * ArcStrokedPoint::depth().
         */
        aa_shader,

        number_shader_types,
      };

    /*!
     * Ctor
     */
    PainterStrokeShader(void);

    /*!
     * Copy ctor.
     */
    PainterStrokeShader(const PainterStrokeShader &obj);

    ~PainterStrokeShader();

    /*!
     * Assignment operator.
     */
    PainterStrokeShader&
    operator=(const PainterStrokeShader &rhs);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(PainterStrokeShader &obj);

    /*!
     * Returns the \ref PainterItemShader for a given pass of a given
     * type of stroking.
     * \param tp specify to return a shader for arc or linear stroking
     * \param sh spcify which shader to return
     */
    const reference_counted_ptr<PainterItemShader>&
    shader(enum PainterEnums::stroking_method_t tp, enum shader_type_t sh) const;

    /*!
     * Set the value returned by shader(enum PainterEnums::stroking_method_t, enum shader_type_t) const.
     * \param tp specify to return shader for arc or linear stroking
     * \param sh spcify which shader to return
     * \param v value to use
     */
    PainterStrokeShader&
    shader(enum PainterEnums::stroking_method_t tp, enum shader_type_t sh,
           const reference_counted_ptr<PainterItemShader> &v);

    /*!
     * Return what PainterEnums::stroking_method_t is fastest to
     * stroke with anti-aliasing.
     */
    enum PainterEnums::stroking_method_t
    fastest_anti_aliased_stroking_method(void) const;

    /*!
     * Set the value returned by fastest_anti_aliased_stroking_method(void) const.
     * \param v value to use
     */
    PainterStrokeShader&
    fastest_anti_aliased_stroking_method(enum PainterEnums::stroking_method_t v);

    /*!
     * Return the fastest stroking method to use when stroking
     * without anti-aliasing.
     */
    enum PainterEnums::stroking_method_t
    fastest_non_anti_aliased_stroking_method(void) const;

    /*!
     * Set the value returned by fastest_non_anti_aliased_stroking_method(void) const.
     * \param v value to use
     */
    PainterStrokeShader&
    fastest_non_anti_aliased_stroking_method(enum PainterEnums::stroking_method_t v);

    /*!
     * Returns the StrokingDataSelectorBase associated to this
     * PainterStrokeShader.
     */
    const reference_counted_ptr<const StrokingDataSelectorBase>&
    stroking_data_selector(void) const;

    /*!
     * Set the value returned by stroking_data_selector(void) const.
     * \param sh value to use
     */
    PainterStrokeShader&
    stroking_data_selector(const reference_counted_ptr<const StrokingDataSelectorBase> &sh);

  private:
    void *m_d;
  };

/*! @} */
}

#endif
