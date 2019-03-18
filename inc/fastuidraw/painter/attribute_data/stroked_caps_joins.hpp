/*!
 * \file stroked_caps_joins.hpp
 * \brief file stroked_caps_joins.hpp
 *
 * Copyright 2018 by Intel.
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

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw  {

///@cond
class PainterAttributeData;
///@endcond

/*!\addtogroup Paths
 * @{
 */

/*!
 * \brief
 * A StrokedCapsJoins represents the data needed to draw a the joins
 * and caps of a stroked path. It contains -all- the data needed to
 * do regardless of stroking parameters.
 *
 * The data is stored as \ref PainterAttributeData, a seperate
 * object for each type of join and each type of cap.  What chunks
 * to use from these objects is computed by the member function
 * compute_chunks(); the PainterAttributeData chunking for joins
 * and caps is the same regardless of the cap and join type.
 */
class StrokedCapsJoins:noncopyable
{
public:
  /*!
   * \brief
   * Opaque object to hold work room needed for functions
   * of StrokedPath that require scratch space.
   */
  class ScratchSpace:fastuidraw::noncopyable
  {
  public:
    ScratchSpace(void);
    ~ScratchSpace();
  private:
    friend class StrokedCapsJoins;
    void *m_d;
  };

  /*!
   * \brief
   * Object to hold the output of a chunk query via compute_chunks().
   */
  class ChunkSet:fastuidraw::noncopyable
  {
  public:
    ChunkSet(void);

    ~ChunkSet();

    /*!
     * The list of chunks to take from any of StrokedCapsJoins::bevel_joins(),
     * StrokedCapsJoins::miter_clip_joins(), StrokedCapsJoins::miter_bevel_joins(),
     * StrokedCapsJoins::miter_joins() or StrokedCapsJoins::rounded_joins() that
     * have visible content.
     */
    c_array<const unsigned int>
    join_chunks(void) const;

    /*!
     * The list of chunks to take from any of StrokedCapsJoins::square_caps(),
     * StrokedCapsJoins::adjustable_caps() or StrokedCapsJoins::rounded_caps()
     * that have visible content.
     */
    c_array<const unsigned int>
    cap_chunks(void) const;

    /*!
     * Clear the values (i.e. make join_chunks() and cap_chunks()
     * empty).
     */
    void
    reset(void);

  private:
    friend class StrokedCapsJoins;
    void *m_d;
  };

  /*!
   * \brief
   * A Builder is used to specify the nature of the contours
   * from which to geneate joins and caps.
   */
  class Builder:fastuidraw::noncopyable
  {
  public:
    Builder(void);
    ~Builder();

    /*!
     * Begin a contour
     * \param start_pt starting point of contour
     * \param start_direction unit vector from start point to next point
     */
    void
    begin_contour(const vec2 &start_pt,
                  const vec2 &start_direction);


    /*!
     * Add a join
     * \param join_pt location of join
     * \param distance_from_previous_join distance from previous join
     * \param direction_into_join unit vector of path into the join
     * \param direction_leaving_join unit vector of path leaving the join
     */
    void
    add_join(const vec2 &join_pt,
             float distance_from_previous_join,
             const vec2 &direction_into_join,
             const vec2 &direction_leaving_join);

    /*!
     * Close the contour giving it the two joins of the closing edge
     * \param distance_from_previous_join distance from previous join
     * \param direction_into_join unit vector of path into the join that ends
     *                            the closing edge
     */
    void
    close_contour(float distance_from_previous_join,
                  const vec2 &direction_into_join);

    /*!
     * End the contour in caps giving it a cap at the start and end of the contour.
     */
    void
    end_contour(const vec2 &cap_pt,
                float distance_from_previous_join,
                const vec2 &direction_into_cap);

  private:
    friend class StrokedCapsJoins;
    void *m_d;
  };

  /*!
   * Ctor.
   * \param b Builder holding the data to generate joins and caps
   */
  explicit
  StrokedCapsJoins(const Builder &b);

  ~StrokedCapsJoins();

  /*!
   * Given a set of clip equations in clip coordinates
   * and a tranformation from local coordiante to clip
   * coordinates, compute what chunks are not completely
   * culled by the clip equations.
   * \param scratch_space scratch space for computations
   * \param clip_equations array of clip equations
   * \param clip_matrix_local 3x3 transformation from local (x, y, 1)
   *                          coordinates to clip coordinates.
   * \param one_pixel_width holds the size of a single pixel in
   *                        normalized device coordinates
   * \param geometry_inflation amount path geometry is inflated, array
   *                           is indexed by the enumeration \ref
   *                           StrokingDataSelectorBase::path_geometry_inflation_index_t
   * \param max_attribute_cnt only allow those chunks for which have no more
   *                          than max_attribute_cnt attributes
   * \param max_index_cnt only allow those chunks for which have no more
   *                      than max_index_cnt indices
   * \param js join style
   * \param[out] dst location to which to write output
   */
  void
  compute_chunks(ScratchSpace &scratch_space,
                 c_array<const vec3> clip_equations,
                 const float3x3 &clip_matrix_local,
                 const vec2 &one_pixel_width,
                 c_array<const float> geometry_inflation,
                 unsigned int max_attribute_cnt,
                 unsigned int max_index_cnt,
                 enum PainterEnums::join_style js,
                 ChunkSet &dst) const;

  /*!
   * Returns the number of joins of the StrokedPath
   */
  unsigned int
  number_joins(void) const;

  /*!
   * Returns a chunk value for Painter::attribute_data_chunk()
   * and related calls to feed the return value to any of
   * bevel_joins(), miter_clip_joins(), miter_bevel_joins(),
   * miter_joins() or rounded_joins() to fetch the chunk
   * of the named join.
   * \param J join ID with 0 <= J < number_joins()
   */
  unsigned int
  join_chunk(unsigned int J) const;

  /*!
   * Return the chunk to feed to any of bevel_joins(),
   * miter_clip_joins(), miter_bevel_joins(),
   * miter_joins() or rounded_joins() that holds all
   * the joins.
   */
  unsigned int
  chunk_of_joins(void) const;

  /*!
   * Return the chunk to feed any of square_caps(),
   * adjustable_caps() or rounded_caps() that holds
   * data for all caps of this StrokedPath.
   */
  unsigned int
  chunk_of_caps(void) const;

  /*!
   * Returns the data to draw the square caps of a stroked path.
   * The attribute data is packed \ref StrokedPoint data.
   */
  const PainterAttributeData&
  square_caps(void) const;

  /*!
   * Returns the data to draw the caps of a stroked path used
   * when stroking with a dash pattern. The attribute data is
   * packed \ref StrokedPoint data.
   */
  const PainterAttributeData&
  adjustable_caps(void) const;

  /*!
   * Returns the data to draw the bevel joins of a stroked path.
   * The attribute data is packed \ref StrokedPoint data.
   */
  const PainterAttributeData&
  bevel_joins(void) const;

  /*!
   * Returns the data to draw the miter joins of a stroked path,
   * if the miter-limit is exceeded on stroking, the miter-join
   * is clipped to the miter-limit. The attribute data is
   * packed \ref StrokedPoint data.
   */
  const PainterAttributeData&
  miter_clip_joins(void) const;

  /*!
   * Returns the data to draw the miter joins of a stroked path,
   * if the miter-limit is exceeded on stroking, the miter-join
   * is to be drawn as a bevel join. The attribute data is
   * packed \ref StrokedPoint data.
   */
  const PainterAttributeData&
  miter_bevel_joins(void) const;

  /*!
   * Returns the data to draw the miter joins of a stroked path,
   * if the miter-limit is exceeded on stroking, the miter-join
   * end point is clamped to the miter-distance. The attribute
   * data is packed \ref StrokedPoint data.
   */
  const PainterAttributeData&
  miter_joins(void) const;

  /*!
   * Returns the data to draw rounded joins of a stroked path.
   * The attribute data is packed \ref StrokedPoint data.
   * \param thresh will return rounded joins so that the distance
   *               between the approximation of the round and the
   *               actual round is no more than thresh.
   */
  const PainterAttributeData&
  rounded_joins(float thresh) const;

  /*!
   * Returns the data to draw rounded caps of a stroked path.
   * The attribute data is packed \ref StrokedPoint data.
   * \param thresh will return rounded caps so that the distance
   *               between the approximation of the round and the
   *               actual round is no more than thresh.
   */
  const PainterAttributeData&
  rounded_caps(float thresh) const;

  /*!
   * Returns the data to draw rounded joins of a stroked path
   * using the fragment shader to provide per-pixel coverage
   * computation. The attribute data is packed \ref
   * ArcStrokedPoint data.
   */
  const PainterAttributeData&
  arc_rounded_joins(void) const;

  /*!
   * Returns the data to draw rounded caps of a stroked path
   * using the fragment shader to provide per-pixel coverage
   * computation. The attribute data is packed \ref
   * ArcStrokedPoint data.
   */
  const PainterAttributeData&
  arc_rounded_caps(void) const;

private:
  void *m_d;
};

/*! @} */


}
