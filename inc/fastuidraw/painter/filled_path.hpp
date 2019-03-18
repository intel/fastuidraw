/*!
 * \file filled_path.hpp
 * \brief file filled_path.hpp
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
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/rect.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/fill_rule.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/painter_attribute_writer.hpp>

namespace fastuidraw  {

///@cond
class PainterAttributeData;
class TessellatedPath;
class Path;
///@endcond

/*!\addtogroup Paths
 * @{
 */

/*!
 * \brief
 * A FilledPath represents the data needed to draw a path filled.
 * It contains -all- the data needed to fill a path regardless of
 * the fill rule.
 */
class FilledPath:
    public reference_counted<FilledPath>::non_concurrent
{
public:
  /*!
   * \brief
   * A Subset represents a handle to a portion of a FilledPath.
   * The handle is invalid once the FilledPath from which it
   * comes goes out of scope. Do not save these handle values
   * without also saving a handle of the FilledPath from which
   * they come.
   */
  class Subset
  {
  public:
    /*!
     * Enumeration to specify type for an attribute of
     * aa_fuzz_painter_data().
     */
    enum aa_fuzz_type_t
      {
        /*!
         * Point is a point on the path.
         */
        aa_fuzz_type_on_path,

        /*!
         * Point is a point on the boundary of the aa-fuzz
         */
        aa_fuzz_type_on_boundary,

        /*!
         * Point is a point on the boundary of the aa-fuzz
         * as a miter-join point.
         */
        aa_fuzz_type_on_boundary_miter,
      };

    /*!
     * Returns the PainterAttributeData to draw the triangles
     * for the portion of the FilledPath the Subset represents.
     * The attribute data is packed as follows:
     * - PainterAttribute::m_attrib0 .xy -> position of point in local coordinate (float)
     * - PainterAttribute::m_attrib0 .zw -> 0 (free)
     * - PainterAttribute::m_attrib1 .xyzw -> 0 (free)
     * - PainterAttribute::m_attrib2 .xyzw -> 0 (free)
     */
    const PainterAttributeData&
    painter_data(void) const;

    /*!
     * Returns the PainterAttributeData to draw the anti-alias fuzz
     * for the portion of the FilledPath the Subset represents.
     * The aa-fuzz is drawn as a quad (of two triangles) per edge
     * of the boudnary of a filled component.
     * The attribute data is packed as follows:
     * - PainterAttribute::m_attrib0 .xy -> position of point in local coordinate (float)
     * - PainterAttribute::m_attrib0 .z  -> (uint) classification, given by \ref aa_fuzz_type_t
     * - PainterAttribute::m_attrib0 .w  -> the z-offset value (uint)
     * - PainterAttribute::m_attrib1 .xy -> normal vector to edge
     * - PainterAttribute::m_attrib1 .zw -> normal vector to next edge
     * - PainterAttribute::m_attrib2 .xyzw -> 0 (free)
     */
    const PainterAttributeData&
    aa_fuzz_painter_data(void) const;

    /*!
     * Returns an array listing what winding number values
     * there are triangle in this Subset. To get the indices
     * for those triangle with winding number N, use the chunk
     * computed from chunk_from_winding_number(N). The same attribute
     * chunk, 0, is used regardless of which index chunk.
     */
    c_array<const int>
    winding_numbers(void) const;

    /*!
     * Returns the bounding box realized as a \ref Path.
     */
    const Path&
    bounding_path(void) const;

    /*!
     * Returns the bounding box of the Subset.
     */
    const Rect&
    bounding_box(void) const;

    /*!
     * Returns what chunk to pass PainterAttributeData::index_data_chunk()
     * called on the \ref PainterAttributeData returned by painter_data()
     * to get the triangles of a specified winding number. The same
     * attribute chunk, 0, is used regardless of which winding number.
     * \param w winding number
     */
    static
    unsigned int
    fill_chunk_from_winding_number(int w);

    /*!
     * Returns what chunk to pass PainterAttributeData::index_data_chunk()
     * called on the \ref PainterAttributeData returned by painter_data()
     * to get the triangles of a specified fill rule.
     */
    static
    unsigned int
    fill_chunk_from_fill_rule(enum PainterEnums::fill_rule_t fill_rule);

    /*!
     * Returns the chunk to pass PainterAttributeData::index_data_chunk()
     * and PainterAttributeData::attribute_data_chunk() on the
     * \ref PainterAttributeData returned by aa_fuzz_painter_data().
     * NOTE that this value is NOT the same as returned by
     * fill_chunk_from_winding_number(int).
     * \param w winding number
     */
    static
    unsigned int
    aa_fuzz_chunk_from_winding_number(int w);

  private:
    friend class FilledPath;

    explicit
    Subset(void *d);

    void *m_d;
  };

  /*!
   * \brief
   * Opaque object to hold work room needed for functions
   * of FilledPath that require scratch space.
   */
  class ScratchSpace:fastuidraw::noncopyable
  {
  public:
    ScratchSpace(void);
    ~ScratchSpace();
  private:
    friend class FilledPath;
    void *m_d;
  };

  /*!
   * Ctor. Construct a FilledPath from the data
   * of a TessellatedPath.
   * \param P source TessellatedPath
   */
  explicit
  FilledPath(const TessellatedPath &P);

  ~FilledPath();

  /*!
   * Returns the the bounding box of the \ref FilledPath.
   */
  const Rect&
  bounding_box(void) const;

  /*!
   * Returns the number of Subset objects of the FilledPath.
   */
  unsigned int
  number_subsets(void) const;

  /*!
   * Return the named Subset object of the FilledPath.
   */
  Subset
  subset(unsigned int I) const;

  /*!
   * Fetch those Subset objects that have triangles that
   * intersect a region specified by clip equations.
   * \param scratch_space scratch space for computations.
   * \param clip_equations array of clip equations
   * \param clip_matrix_local 3x3 transformation from local (x, y, 1)
   *                          coordinates to clip coordinates.
   * \param max_attribute_cnt only allow those \ref Subset objects for which
   *                          Subset::painter_data() have no more than
   *                          max_attribute_cnt attributes.
   * \param max_index_cnt only allow those \ref Subset objects for which
   *                      Subset::painter_data() have no more than
   *                      max_index_cnt attributes.
   * \param[out] dst location to which to write the \ref Subset ID values
   * \returns the number of Subset object ID's written to dst, that
   *          number is guaranteed to be no more than number_subsets().
   *
   */
  unsigned int
  select_subsets(ScratchSpace &scratch_space,
                 c_array<const vec3> clip_equations,
                 const float3x3 &clip_matrix_local,
                 unsigned int max_attribute_cnt,
                 unsigned int max_index_cnt,
                 c_array<unsigned int> dst) const;

  /*!
   * In contrast to select_subsets() which performs hierarchical
   * culling against a set of clip equations, this routine performs
   * no culling and returns the subsets needed to draw all of the
   * FilledPath.
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
private:
  void *m_d;
};

/*! @} */

} //namespace fastuidraw
