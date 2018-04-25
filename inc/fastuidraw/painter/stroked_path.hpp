/*!
 * \file stroked_path.hpp
 * \brief file stroked_path.hpp
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

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/painter/stroked_point.hpp>
#include <fastuidraw/painter/painter_shader_data.hpp>

namespace fastuidraw  {

///@cond
class TessellatedPath;
class Path;
class PainterAttribute;
class PainterAttributeData;
class DashEvaluatorBase;
///@endcond

/*!\addtogroup Paths
 * @{
 */

/*!
 * \brief
 * A StrokedPath represents the data needed to draw a path stroked.
 * It contains -all- the data needed to stroke a path regardless of
 * stroking style. In particular, for a given TessellatedPath,
 * one only needs to construct a StrokedPath <i>once</i> regardless
 * of how one strokes the original path for drawing.
 *
 * The data is stored as \ref PainterAttributeData, a seperate
 * object for edges, each type of join and each type of caps.
 * What chunks to use from these objects is computed by
 * the member function compute_chunks(); the PainterAttributeData
 * chunking for joins and caps is the same regardless of
 * the cap and join type.
 */
class StrokedPath:
    public reference_counted<StrokedPath>::non_concurrent
{
public:
  typedef StrokedPoint point;

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
    friend class StrokedPath;
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
     * The list of chunks to take from StrokedPath::edges()
     * that have visible content.
     */
    c_array<const unsigned int>
    edge_chunks(void) const;

    /*!
     * The list of chunks to take from any of StrokedPath::bevel_joins(),
     * StrokedPath::miter_clip_joins(), StrokedPath::miter_bevel_joins(),
     * StrokedPath::miter_joins() or StrokedPath::rounded_joins() that
     * have visible content.
     */
    c_array<const unsigned int>
    join_chunks(void) const;

    /*!
     * The list of chunks to take from any of StrokedPath::square_caps(),
     * StrokedPath::adjustable_caps() or StrokedPath::rounded_caps()
     * that have visible content.
     */
    c_array<const unsigned int>
    cap_chunks(void) const;

  private:
    friend class StrokedPath;
    void *m_d;
  };

  /*!
   * Enumeration to select the chunk of all joins or edges
   * of the closing edges or non-closing edges of a
   * StrokedPath.
   */
  enum chunk_selection
    {
      /*!
       * Select the chunk that holds all the joins/edges
       * of ONLY the non-closing edges of the StrokedPath
       */
      all_non_closing,

      /*!
       * Select the chunk that holds all the joins/edges
       * of ONLY the closing edges of the StrokedPath
       */
      all_closing,
    };

  /*!
   * Ctor. Construct a StrokedPath from the data
   * of a TessellatedPath.
   * \param P source TessellatedPath
   */
  explicit
  StrokedPath(const TessellatedPath &P);

  ~StrokedPath();

  /*!
   * Given a set of clip equations in clip coordinates
   * and a tranformation from local coordiante to clip
   * coordinates, compute what chunks are not completely
   * culled by the clip equations.
   * \param scratch_space scratch space for computations
   * \param dash_evaluator if doing dashed stroking, the dash evalulator will cull
   *                       joins not to be drawn, if nullptr only those joins not in the
   *                       visible area defined by clip_equations are culled.
   * \param dash_data data to pass to dast evaluator
   * \param clip_equations array of clip equations
   * \param clip_matrix_local 3x3 transformation from local (x, y, 1)
   *                          coordinates to clip coordinates.
   * \param recip_dimensions holds the reciprocal of the dimensions of the viewport
   * \param pixels_additional_room amount in -pixels- to push clip equations by
   *                               to grab additional edges
   * \param item_space_additional_room amount in local coordinates to push clip
   *                              equations by to grab additional edges
   *                              draw the closing edges of each contour
   * \param include_closing_edges if true include the chunks needed to
   * \param max_attribute_cnt only allow those chunks for which have no more
   *                          than max_attribute_cnt attributes
   * \param max_index_cnt only allow those chunks for which have no more
   *                      than max_index_cnt indices
   * \param take_joins_outside_of_region if true, take even those joins outside of the region
   *                                     (this is for handling miter-joins where the weather
   *                                     or not a miter-join is included is also a function of
   *                                     the miter-limit when stroking).
   * \param[out] dst location to which to write output
   */
  void
  compute_chunks(ScratchSpace &scratch_space,
                 const DashEvaluatorBase *dash_evaluator,
                 const PainterShaderData::DataBase *dash_data,
                 c_array<const vec3> clip_equations,
                 const float3x3 &clip_matrix_local,
                 const vec2 &recip_dimensions,
                 float pixels_additional_room,
                 float item_space_additional_room,
                 bool include_closing_edges,
                 unsigned int max_attribute_cnt,
                 unsigned int max_index_cnt,
                 bool take_joins_outside_of_region,
                 ChunkSet &dst) const;

  /*!
   * Returns the number of joins of the StrokedPath
   * \param include_joins_of_closing_edge if false disclude from the count,
   *                                      this joins of the closing edges
   */
  unsigned int
  number_joins(bool include_joins_of_closing_edge) const;

  /*!
   * Returns a chunk value for Painter::attribute_data_chunk()
   * and related calls to feed the return value to any of
   * bevel_joins(), miter_clip_joins(), miter_bevel_joins(),
   * miter_joins() or rounded_joins() to fetch the chunk
   * of the named join.
   * \param J join ID with 0 <= J < number_joins(true)
   */
  unsigned int
  join_chunk(unsigned int J) const;

  /*!
   * Return the chunk to feed to any of bevel_joins(),
   * miter_clip_joins(), miter_bevel_joins(),
   * miter_joins() or rounded_joins() that holds all
   * the joins of the closing edges or all the joins
   * of the non-closing edges.
   * \param c select joins of closing or non-closing edges
   */
  unsigned int
  chunk_of_joins(enum chunk_selection c) const;

  /*!
   * Returns the data to draw the edges of a stroked path.
   */
  const PainterAttributeData&
  edges(void) const;

  /*!
   * Return the chunk to feed to edges() that holds all
   * the edges of the closing edges or all the edges
   * of the non-closing edges.
   * \param c select edges of closing or non-closing edges
   */
  unsigned int
  chunk_of_edges(enum chunk_selection c) const;

  /*!
   * Return the chunk to feed any of square_caps(),
   * adjustable_caps() or rounded_caps() that holds
   * data for all caps of this StrokedPath.
   */
  unsigned int
  chunk_of_caps(void) const;

  /*!
   * Returns the data to draw the square caps of a stroked path.
   */
  const PainterAttributeData&
  square_caps(void) const;

  /*!
   * Returns the data to draw the caps of a stroked path used
   * when stroking with a dash pattern.
   */
  const PainterAttributeData&
  adjustable_caps(void) const;

  /*!
   * Returns the data to draw the bevel joins of a stroked path.
   */
  const PainterAttributeData&
  bevel_joins(void) const;

  /*!
   * Returns the data to draw the miter joins of a stroked path,
   * if the miter-limit is exceeded on stroking, the miter-join
   * is clipped to the miter-limit.
   */
  const PainterAttributeData&
  miter_clip_joins(void) const;

  /*!
   * Returns the data to draw the miter joins of a stroked path,
   * if the miter-limit is exceeded on stroking, the miter-join
   * is to be drawn as a bevel join.
   */
  const PainterAttributeData&
  miter_bevel_joins(void) const;

  /*!
   * Returns the data to draw the miter joins of a stroked path,
   * if the miter-limit is exceeded on stroking, the miter-join
   * end point is clamped to the miter-distance.
   */
  const PainterAttributeData&
  miter_joins(void) const;

  /*!
   * Returns the data to draw rounded joins of a stroked path.
   * \param thresh will return rounded joins so that the distance
   *               between the approximation of the round and the
   *               actual round is no more than thresh.
   */
  const PainterAttributeData&
  rounded_joins(float thresh) const;

  /*!
   * Returns the data to draw rounded caps of a stroked path.
   * \param thresh will return rounded caps so that the distance
   *               between the approximation of the round and the
   *               actual round is no more than thresh.
   */
  const PainterAttributeData&
  rounded_caps(float thresh) const;

private:
  void *m_d;
};

/*! @} */

}
