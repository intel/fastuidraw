/*!
 * \file painter_attribute_data_filler_path_fill.hpp
 * \brief file painter_attribute_data_filler_path_fill.hpp
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
#include <fastuidraw/painter/painter_fill_shader.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/filled_path.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */
  /*!
    A PainterAttributeDataFillerPathFill is an implementation for
    PainterAttributeDataFiller to construct index and attribute
    data to fill a path. The enumeration values of
    PainterEnums::fill_rule_t provide the indices into
    PainterAttributeData::attribute_data_chunks() for the fill rules.
    To get the index data for the component of a filled path with a
    given winding number, use the static function
    index_chunk_from_winding_number(int). The attribute data,
    regardless of winding number or fill rule is
    the same value, the 0'th chunk. Data for filling is packed
    as follows:
      - PainterAttribute::m_attrib0 .xy    -> coordinate of point (float)
      - PainterAttribute::m_attrib0 .zw    -> 0 (free)
      - PainterAttribute::m_attrib1 .xyz -> 0 (free)
      - PainterAttribute::m_attrib1 .w   -> 0 (free)
      - PainterAttribute::m_attrib2 .x -> 0 (free)
      - PainterAttribute::m_attrib2 .y -> 0 (free)
      - PainterAttribute::m_attrib2 .z -> 0 (free)
      - PainterAttribute::m_attrib2 .w -> 0 (free)
   */
  class PainterAttributeDataFillerPathFill:public PainterAttributeDataFiller
  {
  public:
    /*!
      Ctor.
      \param path FilledPath from which to construct attribute and index data.
     */
    explicit
    PainterAttributeDataFillerPathFill(const reference_counted_ptr<const FilledPath> &path);

    ~PainterAttributeDataFillerPathFill();

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
              c_array<unsigned int> zincrements,
              c_array<int> index_adjusts) const;

    /*!
      Returns the FilledPath from which this object fills
      data (set at ctor).
     */
    const reference_counted_ptr<const FilledPath>&
    path(void) const;

    /*!
      Create and return a WindingSelectorChunkBase
      compatible with PainterAttributeDataFillerPathFill
     */
    static
    reference_counted_ptr<WindingSelectorChunkBase>
    chunk_selector(void);

  private:
    void *m_d;
  };
/*! @} */
}
