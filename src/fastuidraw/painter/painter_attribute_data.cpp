/*!
 * \file painter_attribute_data.cpp
 * \brief file painter_attribute_data.cpp
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


#include <vector>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include "../private/util_private.hpp"

namespace
{
  class PainterAttributeDataPrivate
  {
  public:
    void
    ready_non_empty_index_data_chunks(void);

    std::vector<fastuidraw::PainterAttribute> m_attribute_data;
    std::vector<fastuidraw::PainterIndex> m_index_data;

    std::vector<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > m_attribute_chunks;
    std::vector<fastuidraw::const_c_array<fastuidraw::PainterIndex> > m_index_chunks;
    std::vector<fastuidraw::range_type<int> > m_z_ranges;
    std::vector<unsigned int> m_non_empty_index_data_chunks;
    std::vector<int> m_index_adjust_chunks;
  };
}

void
PainterAttributeDataPrivate::
ready_non_empty_index_data_chunks(void)
{
  m_non_empty_index_data_chunks.clear();
  for(unsigned int i = 0; i < m_index_chunks.size(); ++i)
    {
      if(!m_index_chunks[i].empty())
        {
          m_non_empty_index_data_chunks.push_back(i);
        }
    }
}

//////////////////////////////////////////////
// fastuidraw::PainterAttributeData methods
fastuidraw::PainterAttributeData::
PainterAttributeData(void)
{
  m_d = FASTUIDRAWnew PainterAttributeDataPrivate();
}

fastuidraw::PainterAttributeData::
~PainterAttributeData()
{
  PainterAttributeDataPrivate *d;
  d = static_cast<PainterAttributeDataPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::PainterAttributeData::
set_data(const PainterAttributeDataFiller &filler)
{
  PainterAttributeDataPrivate *d;
  d = static_cast<PainterAttributeDataPrivate*>(m_d);

  unsigned int number_attributes(0), number_indices(0);
  unsigned int number_attribute_chunks(0), number_index_chunks(0);
  unsigned int number_z_ranges(0);

  filler.compute_sizes(number_attributes, number_indices,
                       number_attribute_chunks, number_index_chunks,
                       number_z_ranges);

  d->m_attribute_data.resize(number_attributes);
  d->m_index_data.resize(number_indices);

  d->m_attribute_chunks.clear();
  d->m_attribute_chunks.resize(number_attribute_chunks);

  d->m_index_chunks.clear();
  d->m_index_chunks.resize(number_index_chunks);
  d->m_index_adjust_chunks.resize(number_index_chunks);

  d->m_z_ranges.clear();
  d->m_z_ranges.resize(number_z_ranges, range_type<int>(0, 0));

  filler.fill_data(make_c_array(d->m_attribute_data),
                   make_c_array(d->m_index_data),
                   make_c_array(d->m_attribute_chunks),
                   make_c_array(d->m_index_chunks),
                   make_c_array(d->m_z_ranges),
                   make_c_array(d->m_index_adjust_chunks));

  d->ready_non_empty_index_data_chunks();
}

fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> >
fastuidraw::PainterAttributeData::
attribute_data_chunks(void) const
{
  PainterAttributeDataPrivate *d;
  d = static_cast<PainterAttributeDataPrivate*>(m_d);
  return make_c_array(d->m_attribute_chunks);
}

fastuidraw::const_c_array<fastuidraw::PainterAttribute>
fastuidraw::PainterAttributeData::
attribute_data_chunk(unsigned int i) const
{
  PainterAttributeDataPrivate *d;
  d = static_cast<PainterAttributeDataPrivate*>(m_d);
  return (i < d->m_attribute_chunks.size()) ?
    d->m_attribute_chunks[i] :
    const_c_array<PainterAttribute>();
}

fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> >
fastuidraw::PainterAttributeData::
index_data_chunks(void) const
{
  PainterAttributeDataPrivate *d;
  d = static_cast<PainterAttributeDataPrivate*>(m_d);
  return make_c_array(d->m_index_chunks);
}

fastuidraw::const_c_array<fastuidraw::PainterIndex>
fastuidraw::PainterAttributeData::
index_data_chunk(unsigned int i) const
{
  PainterAttributeDataPrivate *d;
  d = static_cast<PainterAttributeDataPrivate*>(m_d);
  return (i < d->m_index_chunks.size()) ?
    d->m_index_chunks[i] :
    const_c_array<PainterIndex>();
}

fastuidraw::const_c_array<int>
fastuidraw::PainterAttributeData::
index_adjust_chunks(void) const
{
  PainterAttributeDataPrivate *d;
  d = static_cast<PainterAttributeDataPrivate*>(m_d);
  return make_c_array(d->m_index_adjust_chunks);
}

int
fastuidraw::PainterAttributeData::
index_adjust_chunk(unsigned int i) const
{
  PainterAttributeDataPrivate *d;
  d = static_cast<PainterAttributeDataPrivate*>(m_d);
  return (i < d->m_index_adjust_chunks.size()) ?
    d->m_index_adjust_chunks[i] :
    0;
}

fastuidraw::const_c_array<fastuidraw::range_type<int> >
fastuidraw::PainterAttributeData::
z_ranges(void) const
{
  PainterAttributeDataPrivate *d;
  d = static_cast<PainterAttributeDataPrivate*>(m_d);
  return make_c_array(d->m_z_ranges);
}

fastuidraw::range_type<int>
fastuidraw::PainterAttributeData::
z_range(unsigned int i) const
{
  PainterAttributeDataPrivate *d;
  d = static_cast<PainterAttributeDataPrivate*>(m_d);
  return (i < d->m_z_ranges.size()) ?
    d->m_z_ranges[i] :
    range_type<int>(0, 0);
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::PainterAttributeData::
non_empty_index_data_chunks(void) const
{
  PainterAttributeDataPrivate *d;
  d = static_cast<PainterAttributeDataPrivate*>(m_d);
  return make_c_array(d->m_non_empty_index_data_chunks);
}
