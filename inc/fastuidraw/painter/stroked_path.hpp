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
   * Chunk enumeration for those PainterAttributeData
   * objects returned by Subset::painter_data().
   */
  enum chunk_selection
    {
      /*!
       * Select the chunk that holds both closing and
       * non-closing edges of the StrokedPath.
       */
      all_edges = 0,

      /*!
       * Select the chunk that holds the edges of
       * ONLY the non-closing edges of the StrokedPath
       */
      only_non_closing_edges,
    };

  /*!
   * \brief
   * A Subset represents a handle to a portion of a StrokedPath.
   * The handle is invalid once the StrokedPath from which it
   * comes goes out of scope. Do not save these handle values
   * without also saving a handle of the StrokedPath from which
   * they come.
   */
  class Subset
  {
  public:
    /*!
     * Returns the PainterAttributeData to draw the triangles
     * for the portion of the StrokedPath the Subset represents.
     * Note: the data is packed with ArcStrokedPoint::pack()
     * if StrokedPath::has_arcs() returns true, otherwise the
     * data is packed with StrokedPoint::pack(). The values of
     * \ref chunk_selection are to be used to selects chunks.
     */
    const PainterAttributeData&
    painter_data(void) const;

    /*!
     * Returns the bounding box realized as a \ref Path.
     */
    const Path&
    bounding_path(void) const;

  private:
    friend class StrokedPath;

    explicit
    Subset(void *d);

    void *m_d;
  };

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
   * Returns the number of Subset objects of the StrokedPath.
   */
  unsigned int
  number_subsets(void) const;

  /*!
   * Return the named Subset object of the StrokedPath.
   */
  Subset
  subset(unsigned int I) const;

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
   * \param max_attribute_cnt only allow those chunks for which have no more
   *                          than max_attribute_cnt attributes
   * \param max_index_cnt only allow those chunks for which have no more
   *                      than max_index_cnt indices
   * \param[out] dst location to which to write the \ref Subset object values
   * \returns the number of Subset objects that intersect the clipping region,
   *          that number is guarnanteed to be no more than number_subsets().
   */
  unsigned int
  select_subsets(ScratchSpace &scratch_space,
                 c_array<const vec3> clip_equations,
                 const float3x3 &clip_matrix_local,
                 const vec2 &recip_dimensions,
                 float pixels_additional_room,
                 float item_space_additional_room,
                 unsigned int max_attribute_cnt,
                 unsigned int max_index_cnt,
                 c_array<unsigned int> dst) const;

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
