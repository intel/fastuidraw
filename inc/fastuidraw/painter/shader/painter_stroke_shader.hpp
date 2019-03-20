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


#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter_shader_data.hpp>
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
    public reference_counted<StrokingDataSelectorBase>::default_base
  {
  public:
    /*!
     * Enumeration to specify indexes into a \ref c_array<float>
     * on how much a path's geometry is inflated by stroking.
     */
    enum path_geometry_inflation_index_t
      {
        /*!
         * Index into \ref c_array<float> to indicate how much
         * the path geometry is inflated in pixels after its
         * inflation in in item coordinates and having the current
         * transformation matrix applied.
         */
        pixel_space_distance = 0,

        /*!
         * Index into \ref c_array<float> to indicate how much
         * the path geometry is inflated in path cordinates
         * before the transformation matrix applied or the
         * inflation by \ref pixel_space_distance is applied
         */
        item_space_distance,

        /*!
         * Index into \ref c_array<float> to indicate how much
         * the bounding box for taking miter-joins is inflated
         * in pixels after its inflation in in item coordinates
         * and having the current transformation matrix applied.
         */
        pixel_space_distance_miter_joins,

        /*!
         * Index into \ref c_array<float> to indicate how much
         * the bounding box for taking miter-joins is inflated
         * in path cordinates before the transformation matrix
         * applied or the inflation by \ref pixel_space_distance_joins
         * is applied
         */
        item_space_distance_miter_joins,

        path_geometry_inflation_index_count,
      };

    /*!
     * To be implemented by a derived class to compute the value
     * used to select rounded join level of detail (\ref
     * StrokedCapsJoins::rounded_joins()) and rounded cap level of detail
     * (\ref StrokedCapsJoins::rounded_caps()).
     * \param data PainterItemShaderData::DataBase object holding
     *             the data to be sent to the shader
     * \param path_magnification by how much the path is magnified
     *                           from its native coordiantes to pixel
     *                           coordinates.
     * \param curve_flatness curve flatness
     */
    virtual
    float
    compute_thresh(const PainterShaderData::DataBase *data,
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
     * \param data PainterItemShaderData::DataBase object holding
     *             the data to be sent to the shader
     * \param js join style
     * \param[out] out_values output with array indexed as according
     *                        to \ref path_geometry_inflation_index_t
     */
    virtual
    void
    stroking_distances(const PainterShaderData::DataBase *data,
                       c_array<float> out_values) const = 0;

    /*!
     * To be implemented by a derived class to specify if
     * arc-stroking is possible with the stroking parameters.
     * \param data PainterItemShaderData::DataBase object holding
     *             the data to be sent to the shader
     */
    virtual
    bool
    arc_stroking_possible(const PainterShaderData::DataBase *data) const = 0;

    /*!
     * To be implemented by a derived class to specify if the passed
     * data is suitable for it.
     */
    virtual
    bool
    data_compatible(const PainterShaderData::DataBase *data) const = 0;
  };

  /*!
   * \brief
   * A PainterStrokeShader holds shaders for stroking. Both
   * \ref PainterEnums::shader_anti_alias_immediate_coverage and
   * \ref PainterEnums::shader_anti_alias_simple are two pass
   * solutions. For PainterEnums::shader_anti_alias_simple,
   * the first pass draws the portions of the path that have
   * 100% coverage and the 2nd pass draws those portions with
   * less than 100% coverage; both of these passes rely on
   * depth testing to prevent overdraw. For \ref
   * PainterEnums::shader_anti_alias_immediate_coverage, the first
   * pass draws to an offscreen coverage buffer the coverage
   * values so that when a fragment is hit multiple times
   * the largest coverage value is retained and the 2nd pass
   * reads and clears the coverage values using the value
   * from the coverage buffer for the coverage.
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
         * Specify the shader for the 1st pass of anti-alias stroking
         * for \ref PainterEnums::shader_anti_alias_simple which draws
         * the portions of the stroked path that cover 100% of the
         * sample area of a fragment. The depth value emitted in the
         * item's vertex shader should be z-value to guarantee that
         * there is no overdraw, see \ref StrokedPoint::depth() and \ref
         * ArcStrokedPoint::depth().
         */
        simple_aa_shader_pass1,

        /*!
         * Specify the shader for the 2nd pass of anti-alias stroking
         * for \ref PainterEnums::shader_anti_alias_simple which draws
         * the portions of the stroked path that cover less than 100%
         * of the sample area of a fragment. The depth value emitted in the
         * item's vertex shader should be z-value to guarantee that
         * there is no overdraw, see \ref StrokedPoint::depth() and \ref
         * ArcStrokedPoint::depth(). An implementation must provide
         * this shader for \ref linear_stroke_type atleast.
         */
        simple_aa_shader_pass2,

        /*!
         * Specify the shader for the 1st pass of anti-alias stroking
         * for \ref PainterEnums::shader_anti_alias_immediate_coverage which
         * draws to a the immediate coverage buffer the coverage of a
         * fragment area by the stroked path. The item's vertex shader
         * it to emit a depth value of 0. It is optional to support
         * this shader type.
         */
        aa_shader_immediate_coverage_pass1,

        /*!
         * Specify the shader for the 2nd pass of anti-alias stroking
         * for \ref PainterEnums::shader_anti_alias_immediate_coverage which
         * draws emits the coverage value from the immediated coverage
         * buffer and clears the value from the immediate coverage buffer
         * as well. The item's vertex shader it to emit a depth value of
         * 0.  It is optional to support this shader type.
         */
        aa_shader_immediate_coverage_pass2,

        /*!
         * Specifies a two-pass shader where the first pass renders to the
         * deferred coverage buffer (via PainterItemShader::coverage_shader())
         * and the second pass reads from it. The depth value emitted in the
         * item's vertex shader should be z-value to guarantee that
         * there is no overdraw, see \ref StrokedPoint::depth() and \ref
         * ArcStrokedPoint::depth().
         */
        aa_shader_deferred_coverage,

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
     * Returns the fastest anti-aliasing mode for stroking with a
     * given \ref PainterEnums::stroking_method_t
     * \param tp stroking method to query
     */
    enum PainterEnums::shader_anti_alias_t
    fastest_anti_aliasing(enum PainterEnums::stroking_method_t tp) const;

    /*!
     * Set the value returned by fastest_stroking_method(enum PainterEnums::stroking_method_t) const.
     * \param tp stroking method to set
     * \param v value to use
     */
    PainterStrokeShader&
    fastest_anti_aliasing(enum PainterEnums::stroking_method_t tp,
                          enum PainterEnums::shader_anti_alias_t v);

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
     * Provided as a conveniance; equivalent to
     * \code
     * shader(tp, aa_shader_immediate_coverage_pass1)
     *   && shader(tp, aa_shader_immediate_coverage_pass2)
     * \endcode
     * \param tp arc or linear stroking to query
     */
    bool
    aa_shader_immediate_coverage_supported(enum PainterEnums::stroking_method_t tp) const;

    /*!
     * Returns the action to be called before the 1st high quality
     * pass, a return value of nullptr indicates to not have an action
     * (and thus no draw-call break).
     */
    const reference_counted_ptr<const PainterDraw::Action>&
    aa_action_pass1(void) const;

    /*!
     * Set the value returned by aa_action_pass1(void) const.
     * Initial value is nullptr.
     * \param a value to use
     */
    PainterStrokeShader&
    aa_action_pass1(const reference_counted_ptr<const PainterDraw::Action> &a);

    /*!
     * Returns the action to be called before the 2nd high quality
     * pass a return value of nullptr indicates to not have an action
     * (and thus no draw-call break).
     */
    const reference_counted_ptr<const PainterDraw::Action>&
    aa_action_pass2(void) const;

    /*!
     * Set the value returned by aa_action_pass2(void) const.
     * Initial value is nullptr.
     * \param a value to use
     */
    PainterStrokeShader&
    aa_action_pass2(const reference_counted_ptr<const PainterDraw::Action> &a);

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
