/*!
 * \file stroked_path.hpp
 * \brief file stroked_path.hpp
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


#pragma once

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/partitioned_tessellated_path.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute_data.hpp>

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
   * they come. The region of a \ref Subset is the exact same
   * region as a \ref PartitionedTessellatedPath::Subset object.
   * Also, the \ref ID() value for a Subset is the same value as
   * \ref PartitionedTessellatedPath::Subset::ID() as well.
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
    c_array<const TessellatedPath::segment_chain>
    segment_chains(void) const;

    /*!
     * Returns the joins within this \ref Subset
     */
    c_array<const TessellatedPath::join>
    joins(void) const;

    /*!
     * Returns the caps within this \ref Subset
     */
    c_array<const TessellatedPath::cap>
    caps(void) const;

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
     * Return the join chunk to feed the \ref PainterAttributeData
     * returned by \ref bevel_joins(), \ref miter_clip_joins() \ref
     * miter_bevel_joins(), \ref miter_joins(), \ref rounded_joins()
     * or \ref arc_rounded_joins() to get the attribute and index
     * data representing the joins within this subset. A return value
     * of -1 indicates that there are no joins within this Subset.
     */
    int
    join_chunk(void) const;

    /*!
     * Return the join chunk to feed the \ref PainterAttributeData
     * returned by \ref adjustable_caps(), \ref square_caps() \ref
     * rounded_caps(), \ref flat_caps(), \ref arc_rounded_caps() to
     * get the attribute and index data representing the caps
     * within this subset. A return value of -1 indicates that there
     * are no caps within this Subset.
     */
    int
    cap_chunk(void) const;

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
     * feed to \ref StrokedPath::subset() to get this
     * \ref Subset.
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
   * A SubsetSelection represents what \ref Subset
   * objects intersect a clipped region.
   */
  class SubsetSelection:fastuidraw::noncopyable
  {
  public:
    /*!
     * Ctor.
     */
    SubsetSelection();
    ~SubsetSelection();

    /*!
     * The ID's of what Subset objects are selected.
     */
    c_array<const unsigned int>
    subset_ids(void) const;

    /*!
     * ID's of what Subset objects are selected for joins.
     * This value is different from \ref subset_ids() only
     * when Stroked::select_subset_ids() was specified to
     * enlarge the join footprints for miter-join stroking.
     */
    c_array<const unsigned int>
    join_subset_ids(void) const;

    /*!
     * Returns the source for the data.
     */
    const reference_counted_ptr<const StrokedPath>&
    source(void) const;

    /*!
     * Clears the SubsetSelection to be empty.
     * \param src value for \ref source() to return
     */
    void
    clear(const reference_counted_ptr<const StrokedPath> &src =
          reference_counted_ptr<const StrokedPath>());

  private:
    friend class StrokedPath;

    void *m_d;
  };

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
   * Returns the source \ref PartitionedTessellatedPath of this
   * \ref StrokedPath.
   */
  const PartitionedTessellatedPath&
  partitioned_path(void) const;

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
   * Returns the data to draw the square caps of a stroked path.
   * The attribute data is packed \ref StrokedPoint data.
   */
  const PainterAttributeData&
  square_caps(void) const;

  /*!
   * Returns the data to draw the flat caps of a stroked path.
   * The attribute data is packed \ref StrokedPoint data.
   */
  const PainterAttributeData&
  flat_caps(void) const;

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
   *               actual round is no more than thresh when the
   *               path is stroked with a stroking radius of one.
   */
  const PainterAttributeData&
  rounded_joins(float thresh) const;

  /*!
   * Returns the data to draw rounded caps of a stroked path.
   * The attribute data is packed \ref StrokedPoint data.
   * \param thresh will return rounded caps so that the distance
   *               between the approximation of the round and the
   *               actual round is no more than thresh when the
   *               path is stroked with a stroking radius of one.
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

  /*!
   * Given a set of clip equations in clip coordinates
   * and a tranformation from local coordiante to clip
   * coordinates, compute what Subset are not completely
   * culled by the clip equations.
   * \param clip_equations array of clip equations
   * \param clip_matrix_local 3x3 transformation from local (x, y, 1)
   *                          coordinates to clip coordinates.
   * \param one_pixel_width holds the size of a single pixel in
   *                        normalized device coordinates
   * \param geometry_inflation amount path geometry is inflated, array
   *                           is indexed by the enumeration \ref
   *                           PathEnums::path_geometry_inflation_index_t
   * \param max_attribute_cnt only allow those chunks for which have no more
   *                          than max_attribute_cnt attributes
   * \param max_index_cnt only allow those chunks for which have no more
   *                      than max_index_cnt indices
   * \param select_miter_joins if true, when selecting what joins are in
   *                           the area, enlarge the join footprint for if
   *                           the joins are stroked as a type of miter join.
   * \param[out] dst location to which to write the subset-selection.
   */
  void
  select_subsets(c_array<const vec3> clip_equations,
                 const float3x3 &clip_matrix_local,
                 const vec2 &one_pixel_width,
                 c_array<const float> geometry_inflation,
                 unsigned int max_attribute_cnt,
                 unsigned int max_index_cnt,
                 bool select_miter_joins,
                 SubsetSelection &dst) const;

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
   */
  void
  select_subsets_no_culling(unsigned int max_attribute_cnt,
                            unsigned int max_index_cnt,
                            SubsetSelection &dst) const;

private:
  friend class TessellatedPath;

  // only a TessellatedPath can construct a StrokedPath
  explicit
  StrokedPath(const TessellatedPath &P);

  void *m_d;
};

/*! @} */

}
