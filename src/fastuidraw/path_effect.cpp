/*!
 * \file path_effect.cpp
 * \brief file path_effect.cpp
 *
 * Copyright 2019 by Intel.
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
#include <fastuidraw/path_effect.hpp>
#include <private/util_private.hpp>

namespace
{
  class Chain
  {
  public:
    /* if true, the previous segment is located at
     * m_segment_storage[m_range.m_begin - 1].
     */
    bool m_has_prev;
    fastuidraw::range_type<unsigned int> m_range;
  };

  class StoragePrivate
  {
  public:
    std::vector<fastuidraw::PathEffect::segment> m_segment_storage;
    std::vector<fastuidraw::PathEffect::join> m_join_storage;
    std::vector<fastuidraw::PathEffect::cap> m_cap_storage;
    std::vector<Chain> m_chains;
  };
}

///////////////////////////////////////////////////////
// fastuidraw::PathEffect::Storage methods
fastuidraw::PathEffect::Storage::
Storage(void)
{
  m_d = FASTUIDRAWnew StoragePrivate();
}

fastuidraw::PathEffect::Storage::
~Storage(void)
{
  StoragePrivate *d;
  d = static_cast<StoragePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::PathEffect::Storage::
clear(void)
{
  StoragePrivate *d;
  d = static_cast<StoragePrivate*>(m_d);

  d->m_segment_storage.clear();
  d->m_join_storage.clear();
  d->m_cap_storage.clear();
  d->m_chains.clear();
}

fastuidraw::PathEffect::Storage&
fastuidraw::PathEffect::Storage::
begin_chain(const segment *prev_segment)
{
  StoragePrivate *d;
  Chain C;
  unsigned int sz;

  d = static_cast<StoragePrivate*>(m_d);
  sz = d->m_segment_storage.size();
  if (!d->m_chains.empty())
    {
      d->m_chains.back().m_range.m_end = sz;
    }

  C.m_range.m_begin = sz;
  C.m_range.m_end = ~0u;
  if (prev_segment)
    {
      d->m_segment_storage.push_back(*prev_segment);
      C.m_has_prev = true;
      ++C.m_range.m_begin;
    }
  else
    {
      C.m_has_prev = false;
    }

  d->m_chains.push_back(C);
  return *this;
}

fastuidraw::PathEffect::Storage&
fastuidraw::PathEffect::Storage::
add_segment(const segment &segment)
{
  StoragePrivate *d;
  d = static_cast<StoragePrivate*>(m_d);
  d->m_segment_storage.push_back(segment);
  return *this;
}

fastuidraw::PathEffect::Storage&
fastuidraw::PathEffect::Storage::
add_join(const join &join)
{
  StoragePrivate *d;
  d = static_cast<StoragePrivate*>(m_d);
  d->m_join_storage.push_back(join);
  return *this;
}

fastuidraw::PathEffect::Storage&
fastuidraw::PathEffect::Storage::
add_cap(const cap &cap)
{
  StoragePrivate *d;
  d = static_cast<StoragePrivate*>(m_d);
  d->m_cap_storage.push_back(cap);
  return *this;
}

unsigned int
fastuidraw::PathEffect::Storage::
number_chains(void) const
{
  StoragePrivate *d;
  d = static_cast<StoragePrivate*>(m_d);
  return d->m_chains.size();
}

fastuidraw::PathEffect::segment_chain
fastuidraw::PathEffect::Storage::
chain(unsigned int I) const
{
  StoragePrivate *d;
  segment_chain return_value;
  range_type<unsigned int> R;

  d = static_cast<StoragePrivate*>(m_d);
  R = d->m_chains[I].m_range;
  if (I + 1u == d->m_chains.size())
    {
      R.m_end = d->m_segment_storage.size();
    }

  return_value.m_segments = make_c_array(d->m_segment_storage).sub_array(R);
  return_value.m_prev_to_start = (d->m_chains[I].m_has_prev) ?
    &d->m_segment_storage[R.m_begin - 1] :
    nullptr;

  return return_value;
}

fastuidraw::c_array<const fastuidraw::PathEffect::join>
fastuidraw::PathEffect::Storage::
joins(void) const
{
  StoragePrivate *d;
  d = static_cast<StoragePrivate*>(m_d);
  return make_c_array(d->m_join_storage);
}

fastuidraw::c_array<const fastuidraw::PathEffect::cap>
fastuidraw::PathEffect::Storage::
caps(void) const
{
  StoragePrivate *d;
  d = static_cast<StoragePrivate*>(m_d);
  return make_c_array(d->m_cap_storage);
}

///////////////////////////////////////////////////////
// fastuidraw::PathEffect methods
void
fastuidraw::PathEffect::
process_selection(const PartitionedTessellatedPath::SubsetSelection &selection,
                  Storage &dst) const
{
  if (!selection.source())
    {
      return;
    }

  const PartitionedTessellatedPath &path(*selection.source());
  for (unsigned int id : selection.subset_ids())
    {
      PartitionedTessellatedPath::Subset S(path.subset(id));
      c_array<const segment_chain> segment_chains(S.segment_chains());
      c_array<const PartitionedTessellatedPath::cap> caps(S.caps());

      process_chains(segment_chains.begin(), segment_chains.end(), dst);
      process_caps(caps.begin(), caps.end(), dst);
    }

  for (unsigned int id : selection.join_subset_ids())
    {
      PartitionedTessellatedPath::Subset S(path.subset(id));
      c_array<const PartitionedTessellatedPath::join> joins(S.joins());
      process_joins(joins.begin(), joins.end(), dst);
    }
}
