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
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/fill_rule.hpp>
#include <fastuidraw/painter/packing/painter_packer.hpp>

namespace fastuidraw  {

///@cond
class PainterAttributeData;
class TessellatedPath;
class Path;
///@endcond

/*!\addtogroup Core
  @{
 */

/*!
  A FilledPath represents the data needed to draw a path filled.
  It contains -all- the data needed to fill a path regardless of
  the fill rule.
 */
class FilledPath:
    public reference_counted<FilledPath>::non_concurrent
{
public:
  /*!
    A Subset represents a handle to a portion of a FilledPath.
    The handle is invalid once the FilledPath from which it
    comes goes out of scope. Do not save these handle values.
   */
  class Subset
  {
  public:
    /*!
      Returns the PainterAttributeData for the Subset
      Returns the PainterAttributeData for the portion of the
      FilledPath the Subset represents. The attribute data is
      packed as follows:
      - PainterAttribute::m_attrib0 .xy -> position of point in local coordinate (float)
      - PainterAttribute::m_attrib0 .zw -> 0 (free)
      - PainterAttribute::m_attrib1 .xyzw -> 0 (free)
      - PainterAttribute::m_attrib2 .xyzw -> 0 (free)
     */
    const PainterAttributeData&
    painter_data(void) const;

    /*!
      Returns an array listing what winding number values
      there are triangle in this Subset. To get the indices
      for those triangle with winding number N, use the chunk
      computed from chunk_from_winding_number(N). The same attribute
      chunk, 0, is used regardless of which index chunk.
    */
    const_c_array<int>
    winding_numbers(void) const;

    /*!
      Returns what chunk to pass PainterAttributeData::index_chunks()
      called on the PainterAttributeData returned by painter_data()
      to get the triangles of a specified winding number. The same
      attribute chunk, 0, is used regardless of which winding number.
     */
    static
    unsigned int
    chunk_from_winding_number(int w);

    /*!
      Returns what chunk to pass PainterAttributeData::index_chunks()
      called on the PainterAttributeData returned by painter_data()
      to get the triangles of a specified fill rule.
     */
    static
    unsigned int
    chunk_from_fill_rule(enum PainterEnums::fill_rule_t fill_rule);

    /*!
      Returns the chunk to use for drawing the anti-alias fuzz
      around the filled path caused by edges shared between
      winding number components. If one gives the value
      as the same winding number, then these are aa-edges
      of the winding number component that are NOT shared
      with any others.
     */
    static
    unsigned int
    chunk_for_aa_fuzz(int winding0, int winding1);

  private:
    friend class FilledPath;

    explicit
    Subset(void *d);

    void *m_d;
  };

  /*!
    Opaque object to hold work room needed for functions
    of FilledPath that require scratch space.
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
    A DataWriter is used by FilledPath to fill attribute
    and index data for the purpose of filling with
    anti-aliasing. The attribute data filled by a
    DataWriter is packed as follows:
      - PainterAttribute::m_attrib0 .xy -> position of point in local coordinate (float)
      - PainterAttribute::m_attrib0 .z -> 0 if vertex is on boundary of fill, 1 if it is on interior
      - PainterAttribute::m_attrib0 .w -> 0 (free)
      - PainterAttribute::m_attrib1 .xyzw -> 0 (free)
      - PainterAttribute::m_attrib2 .xyzw -> 0 (free)
   */
  class DataWriter:public PainterPacker::DataWriter
  {
  public:
    DataWriter(void);

    virtual
    ~DataWriter();

    virtual
    unsigned int
    number_attribute_chunks(void) const;

    virtual
    unsigned int
    number_attributes(unsigned int attribute_chunk) const;

    virtual
    unsigned int
    number_index_chunks(void) const;

    virtual
    unsigned int
    number_indices(unsigned int index_chunk) const;

    virtual
    unsigned int
    attribute_chunk_selection(unsigned int index_chunk) const;

    virtual
    void
    write_indices(c_array<PainterIndex> dst,
                  unsigned int index_offset_value,
                  unsigned int index_chunk) const;

    virtual
    void
    write_attributes(c_array<PainterAttribute> dst,
                     unsigned int attribute_chunk) const;

  private:
    friend class FilledPath;
    void *m_d;
  };

  /*!
    Ctor. Construct a FilledPath from the data
    of a TessellatedPath.
    \param P source TessellatedPath
   */
  explicit
  FilledPath(const TessellatedPath &P);

  ~FilledPath();

  /*!
    Returns the number of Subset objects of the FilledPath.
   */
  unsigned int
  number_subsets(void) const;

  /*!
    Return the named Subset object of the FilledPath.
   */
  Subset
  subset(unsigned int I) const;

  /*!
    Fetch those Subset objects that have triangles that
    intersect a region specified by clip equations.
    \param scratch_space scratch space for computations.
    \param clip_equations array of clip equations
    \param clip_matrix_local 3x3 transformation from local (x, y, 1)
                             coordinates to clip coordinates.
    \param max_attribute_cnt only allow those SubSet objects for which
                             Subset::painter_data() have no more than
                             max_attribute_cnt attributes.
    \param max_index_cnt only allow those SubSet objects for which
                         Subset::painter_data() have no more than
                         max_index_cnt attributes.
    \param[out] dst location to which to write the what SubSets
    \returns the number of chunks that intersect the clipping region,
             that number is guarnanteed to be no more than number_subsets().

   */
  unsigned int
  select_subsets(ScratchSpace &scratch_space,
                 const_c_array<vec3> clip_equations,
                 const float3x3 &clip_matrix_local,
                 unsigned int max_attribute_cnt,
                 unsigned int max_index_cnt,
                 c_array<unsigned int> dst) const;

  /*!
    Compute a DataWriter value for the purposes of filling
    a path against a fill rule WITH anti-aliasing applied.
    \param scratch_space scratch space for computations.
    \param fill_rule custom fill rule to apply to path
    \param clip_equations array of clip equations
    \param clip_matrix_local 3x3 transformation from local (x, y, 1)
                             coordinates to clip coordinates.
    \param max_attribute_cnt make so that the returned DataWriter
                             written to dst has that each attribute
                             chunk has no more than max_attribute_cnt
                             attributes.
    \param max_index_cnt make so that the returned DataWriter
                         written to dst has that each index
                         chunk has no more than max_index_cnt
                         indices.
    \param dst[output] location to which to write the DataWriter value
   */
  void
  compute_writer(ScratchSpace &scratch_space,
                 const CustomFillRuleBase &fill_rule,
                 const_c_array<vec3> clip_equations,
                 const float3x3 &clip_matrix_local,
                 unsigned int max_attribute_cnt,
                 unsigned int max_index_cnt,
                 DataWriter &dst) const;
private:
  void *m_d;
};

/*! @} */

} //namespace fastuidraw
