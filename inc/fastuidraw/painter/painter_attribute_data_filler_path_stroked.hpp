/*!
 * \file painter_attribute_data_filler_path_stroked.hpp
 * \brief file painter_attribute_data_filler_path_stroked.hpp
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

#include <fastuidraw/painter/painter_attribute_data_filler.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter_stroke_shader.hpp>
#include <fastuidraw/stroked_path.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

  /*!
    A PainterAttributeDataFillerPathStroked is an implementation for
    PainterAttributeDataFiller to construct index and attribute
    data to stroke a path. The data is so that it provides all that
    is needed to stroke a path regardless of join style, cap style,
    stroking width, stroking miter limit or even dash pattern.

    Data for stroking is packed as follows:
    - PainterAttribute::m_attrib0 .xy -> StrokedPath::point::m_position (float)
    - PainterAttribute::m_attrib0 .zw -> StrokedPath::point::m_pre_offset (float)
    - PainterAttribute::m_attrib1 .x -> StrokedPath::point::m_distance_from_edge_start (float)
    - PainterAttribute::m_attrib1 .y -> StrokedPath::point::m_distance_from_contour_start (float)
    - PainterAttribute::m_attrib1 .zw -> StrokedPath::point::m_auxilary_offset (float)
    - PainterAttribute::m_attrib2 .x -> StrokedPath::point::m_packed_data (uint)
    - PainterAttribute::m_attrib2 .y -> StrokedPath::point::m_edge_length (float)
    - PainterAttribute::m_attrib2 .z -> StrokedPath::point::m_open_contour_length (float)
    - PainterAttribute::m_attrib2 .w -> StrokedPath::point::m_closed_contour_length (float)
   */
  class PainterAttributeDataFillerPathStroked:public PainterAttributeDataFiller
  {
  public:
    /*!
      Ctor.
      \param path FilledPath from which to construct attribute and index data.
     */
    explicit
    PainterAttributeDataFillerPathStroked(const reference_counted_ptr<const StrokedPath> &path);

    ~PainterAttributeDataFillerPathStroked();

    virtual
    void
    compute_sizes(unsigned int &number_attributes,
                  unsigned int &number_indices,
                  unsigned int &number_attribute_chunks,
                  unsigned int &number_index_chunks,
                  unsigned int &number_z_increments) const;
    virtual
    void
    fill_data(c_array<PainterAttribute> attributes,
              c_array<PainterIndex> indices,
              c_array<const_c_array<PainterAttribute> > attrib_chunks,
              c_array<const_c_array<PainterIndex> > index_chunks,
              c_array<unsigned int> zincrements) const;

    /*!
      Returns the StrokedPath from which this object fills
      data (set at ctor).
     */
    const reference_counted_ptr<const StrokedPath>&
    path(void) const;

    /*!
      Create and return a StrokingChunkSelectorBase
      compatible with PainterAttributeDataFillerPathStroked
     */
    static
    reference_counted_ptr<StrokingChunkSelectorBase>
    chunk_selector(void);

    /*!
      Unpack a StrokedPath::point from a PainterAttribute
      as packed by PainterAttributeDataFillerPathStroked.
      \param attrib PainterAttribute from which to unpack
             a StrokedPath::point
     */
    static
    StrokedPath::point
    unpack_point(const PainterAttribute &attrib);

    /*!
      Pack a StrokedPath::point into a PainterAttribute
      as packed by PainterAttributeDataFillerPathStroked.
      \param pt StrokedPath::point from which to pack a
                PainterAttribute
     */
    static
    PainterAttribute
    pack_point(const StrokedPath::point &pt);

  private:
    void *m_d;
  };
/*! @} */
}
