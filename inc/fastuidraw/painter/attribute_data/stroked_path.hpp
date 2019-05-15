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
#include <fastuidraw/path.hpp>
#include <fastuidraw/partitioned_tessellated_path.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute_data.hpp>
#include <fastuidraw/painter/attribute_data/stroked_caps_joins.hpp>

namespace fastuidraw  {

/*!\addtogroup PainterAttribute
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
   * A Subset represents a handle to a portion of a StrokedPath.
   * The handle is invalid once the StrokedPath from which it
   * comes goes out of scope. Do not save these handle values
   * without also saving a handle of the StrokedPath from which
   * they come.
   */
  class Subset
  {
  private:
    typedef bool (Subset::*unspecified_bool_type)(void) const;

  public:
    /*!
     * Ctor to initialize value to "null" handle.
     */
    Subset(void):
      m_d(nullptr)
    {}

    /*!
     * Allows one to legally write to test if Subset
     * is a null-handle:
     * \code
     * Subset p;
     *
     * if (p)
     *   {
     *     // p does refers to data
     *   }
     *
     * if (!p)
     *   {
     *     // p does not referr to any data
     *   }
     * \endcode
     */
    operator unspecified_bool_type() const
    {
      return m_d ? &Subset::has_children : 0;
    }

    /*!
     * Returns the segments that are within this
     * \ref Subset
     */
    c_array<const PartitionedTessellatedPath::segment_chain>
    segment_chains(void) const;

    /*!
     * Returns the PainterAttributeData to draw the triangles
     * for the portion of the StrokedPath the Subset represents.
     * Note: the data is packed with ArcStrokedPoint::pack()
     * if StrokedPath::has_arcs() returns true, otherwise the
     * data is packed with StrokedPoint::pack(). There is only
     * one chunk of the returned object, chunk 0.
     */
    const PainterAttributeData&
    painter_data(void) const;

    /*!
     * Returns the bounding box.
     */
    const Rect&
    bounding_box(void) const;

    /*!
     * Returns the bounding box realized as a \ref Path.
     */
    const Path&
    bounding_path(void) const;

    /*!
     * Returns the ID of this Subset, i.e. the value to
     * feed to \ref PartitionedTessellatedPath::subset()
     * to get this \ref Subset.
     */
    unsigned int
    ID(void) const;

    /*!
     * Returns true if this Subset has child Subset
     */
    bool
    has_children(void) const;

    /*!
     * Returns the children of this Subset. It is an
     * error to call this if \ref has_children() returns
     * false.
     */
    vecN<Subset, 2>
    children(void) const;

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
   * Returns the root-subset of the StrokedPath, this
   * is the \ref Subset that includes the entire
   * StrokedPath.
   */
  Subset
  root_subset(void) const;

  /*!
   * Given a set of clip equations in clip coordinates
   * and a tranformation from local coordiante to clip
   * coordinates, compute what Subset are not completely
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
   * \param[out] dst location to which to write the \ref Subset ID values
   * \returns the number of Subset object ID's written to dst, that
   *          number is guaranteed to be no more than number_subsets().
   */
  unsigned int
  select_subsets(ScratchSpace &scratch_space,
                 c_array<const vec3> clip_equations,
                 const float3x3 &clip_matrix_local,
                 const vec2 &one_pixel_width,
                 c_array<const float> geometry_inflation,
                 unsigned int max_attribute_cnt,
                 unsigned int max_index_cnt,
                 c_array<unsigned int> dst) const;

  /*!
   * In contrast to select_subsets() which performs hierarchical
   * culling against a set of clip equations, this routine performs
   * no culling and returns the subsets needed to draw all of the
   * StrokedPath.
   * \param max_attribute_cnt only allow those chunks for which have no more
   *                          than max_attribute_cnt attributes
   * \param max_index_cnt only allow those chunks for which have no more
   *                      than max_index_cnt indices
   * \param[out] dst location to which to write the \ref Subset ID values
   * \returns the number of Subset object ID's written to dst, that
   *          number is guaranteed to be no more than number_subsets().
   */
  unsigned int
  select_subsets_no_culling(unsigned int max_attribute_cnt,
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
