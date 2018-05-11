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
#include <fastuidraw/painter/stroked_caps_joins.hpp>
#include <fastuidraw/painter/painter_shader_data.hpp>

namespace fastuidraw  {

///@cond
class TessellatedPath;
class Path;
class PainterAttribute;
class PainterAttributeData;
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
 */
class StrokedPath:
    public reference_counted<StrokedPath>::non_concurrent
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

  private:
    friend class StrokedPath;
    void *m_d;
  };

  /*!
   * Enumeration to select the chunk of all edges of the closing
   * edges or all edges of the non-closing edges of a StrokedPath.
   */
  enum chunk_selection
    {
      /*!
       * Select the chunk that holds all the edges of
       * ONLY the non-closing edges of the StrokedPath
       */
      all_non_closing = StrokedCapsJoins::all_non_closing,

      /*!
       * Select the chunk that holds all the edges of
       * ONLY the closing edges of the StrokedPath
       */
      all_closing = StrokedCapsJoins::all_closing,
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
   * Returns true if the \ref StrokedPath has arc.
   * If the stroked path has arcs, ALL of the attribute
   * data is packed \ref ArcStrokedPoint data,
   * if it has no arcs then ALL of the data is
   * packed \ref StrokedPoint data.
   */
  bool
  has_arcs(void) const;

  /*!
   * Given a set of clip equations in clip coordinates
   * and a tranformation from local coordiante to clip
   * coordinates, compute what chunks are not completely
   * culled by the clip equations.
   * \param scratch_space scratch space for computations
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
                 c_array<const vec3> clip_equations,
                 const float3x3 &clip_matrix_local,
                 const vec2 &recip_dimensions,
                 float pixels_additional_room,
                 float item_space_additional_room,
                 bool include_closing_edges,
                 unsigned int max_attribute_cnt,
                 unsigned int max_index_cnt,
                 ChunkSet &dst) const;

  /*!
   * Returns the data to draw the edges of a stroked path.
   * Note: the data is packed with ArcStrokedPoint::pack()
   * if has_arcs() returns true, otherwise the data is
   * packed with StrokedPoint::pack().
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
   * Returns the StrokedCapsJoins of the StrokedPath that
   * provides the attribute data for teh stroking of the
   * joins and caps.
   */
  const StrokedCapsJoins&
  caps_joins(void) const;

private:
  void *m_d;
};

/*! @} */

}
