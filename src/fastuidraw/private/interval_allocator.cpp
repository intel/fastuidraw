/*!
 * \file interval_allocator.cpp
 * \brief file interval_allocator.cpp
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

#include <algorithm>

#include <fastuidraw/util/util.hpp>
#include "interval_allocator.hpp"


fastuidraw::interval_allocator::
interval_allocator(int size)
{
  reset(size);
}

void
fastuidraw::interval_allocator::
reset(int size)
{
  FASTUIDRAWassert(size >= 0);

  m_size = std::max(0, size);
  m_sorted.clear();
  m_free_intervals.clear();
  if (m_size > 0)
    {
      free_interval(0, m_size);
    }
}

void
fastuidraw::interval_allocator::
resize(int size)
{
  FASTUIDRAWassert(size >= m_size);
  if (size > m_size)
    {
      int old_size(m_size);
      m_size = size;
      free_interval(old_size, size - old_size);
    }
}


fastuidraw::interval_allocator::interval_status_t
fastuidraw::interval_allocator::
interval_status(int begin, int size) const
{
  FASTUIDRAWassert(begin >= 0);
  FASTUIDRAWassert(size > 0);

  int end(begin + size);
  FASTUIDRAWassert(end <= m_size);

  std::map<int, interval>::const_iterator begin_iter;

  /* begin_iter points to the first free interval I
   * for which I.m_end >= begin
   */
  begin_iter = m_free_intervals.upper_bound(begin);

  if (begin_iter == m_free_intervals.end())
    {
      /* All free intervals end at or before begin,
       * thus the interval is completely allocated
       */
      return completely_allocated;
    }

  interval I(begin_iter->second);
  FASTUIDRAWassert(I.m_end > begin);

  if (I.m_end >= end && I.m_begin <= begin)
    {
      /* the free Interval I completely contains
       * the interval [begin, end)
       */
      return completely_free;
    }

  if (I.m_begin > begin)
    {
      /* the queried interval begins before I.
       * Note that the free interval previous
       * to I (call it J) has that J.m_end < begin,
       * i.e. J ends before begin, and the range
       * [J.m_end, I.m_begin) is completely
       * allocated.
       */
      if (end <= I.m_begin)
        {
          /* end is before I even starts, thus
           * [begin, end) is completely allocated
           */
          return completely_allocated;
        }
      else
        {
          return partially_allocated;
        }
    }

  FASTUIDRAWassert(I.m_end < end);
  return partially_allocated;

}

int
fastuidraw::interval_allocator::
allocate_interval(int size)
{
  if (size <= 0)
    {
      return -1;
    }

  std::map<int, interval_ref_set>::iterator iter;

  iter = m_sorted.lower_bound(size);
  if (iter == m_sorted.end())
    {
      return -1;
    }

  interval_ref interval_reference(*iter->second.begin());
  interval I(interval_reference->second);
  interval return_value(I.m_begin, I.m_begin + size);

  FASTUIDRAWassert(interval_reference->second.m_end == interval_reference->first);
  FASTUIDRAWassert(I.m_end - I.m_begin == iter->first);

  iter->second.erase(iter->second.begin());
  if (iter->second.empty())
    {
      m_sorted.erase(iter);
    }

  /* Now take away the room from the interval
   * pointed to by interval_reference that
   * we used in the allocation. We can do
   * this because the map is keyed by
   * m_end of interval.
   */
  interval_reference->second.m_begin += size;

  FASTUIDRAWassert(interval_reference->second.m_begin <= interval_reference->second.m_end);
  if (interval_reference->second.m_begin == interval_reference->second.m_end)
    {
      /* if the new interval is empty, then we delete it
       */
      m_free_intervals.erase(interval_reference);
    }
  else
    {
      /* is not empty, we need to add it to m_sorted
       */
      int sz(interval_reference->second.m_end - interval_reference->second.m_begin);
      m_sorted[sz].insert(interval_reference);
    }

  return return_value.m_begin;
}


void
fastuidraw::interval_allocator::
free_interval(int location, int size)
{
  FASTUIDRAWassert(size > 0);
  FASTUIDRAWassert(interval_status(location, size) == completely_allocated);

  int end(location + size);

  /* see if location corresponds to m_end
   * of an existing free block.
   */
  interval_ref iter;
  iter = m_free_intervals.find(location);
  if (iter != m_free_intervals.end())
    {
      /* in this case we need to enlarge
       * the block pointed to by iter,
       * however that means we need to
       * remove it since we are changing
       * m_end to end;
       */
      location = iter->second.m_begin;
      size = end - location;
      remove_free_interval(iter);
    }

  iter = m_free_intervals.lower_bound(end);
  if (iter != m_free_intervals.end() && iter->second.m_begin == end)
    {
      /* end is the start of an existing block,
       * so just make that existing block bigger.
       * First, remove it from m_sorted.
       */
      remove_free_interval_from_sorted_only(iter);

      /* Second, make it bigger and add it to m_sorted
       */
      iter->second.m_begin = location;
      m_sorted[iter->second.m_end - iter->second.m_begin].insert(iter);
      return;
    }

  /* the element to add has that its end is not
   * the start of an existing free interval and
   * that its beginning is not the end of an
   * existing one either, so just add it:
   */
  interval I(location, end);
  std::pair<interval_ref, bool> R;

  R = m_free_intervals.insert( std::pair<int, interval>(end, I));
  FASTUIDRAWassert(R.second);
  m_sorted[size].insert(R.first);
}

void
fastuidraw::interval_allocator::
remove_free_interval(interval_ref iter)
{
  int sz(iter->second.m_end - iter->second.m_begin);
  std::map<int, interval_ref_set>::iterator sorted_iter;

  sorted_iter = m_sorted.find(sz);
  FASTUIDRAWassert(sorted_iter != m_sorted.end());

  remove_free_interval(sorted_iter, iter);
}

void
fastuidraw::interval_allocator::
remove_free_interval(std::map<int, interval_ref_set>::iterator sorted_iter,
                     interval_ref iter)
{
  /* we need to remove iter from m_sorted
   * and m_free_intervals
   */
  remove_free_interval_from_sorted_only(sorted_iter, iter);
  m_free_intervals.erase(iter);
}

void
fastuidraw::interval_allocator::
remove_free_interval_from_sorted_only(std::map<int, interval_ref_set>::iterator sorted_iter,
                                      interval_ref iter)
{
  FASTUIDRAWassert(iter->second.m_end == iter->first);
  FASTUIDRAWassert(iter->second.m_end - iter->second.m_begin == sorted_iter->first);
  FASTUIDRAWassert(sorted_iter != m_sorted.end());
  FASTUIDRAWassert(sorted_iter->second.find(iter) != sorted_iter->second.end());

  sorted_iter->second.erase(iter);
  if (sorted_iter->second.empty())
    {
      m_sorted.erase(sorted_iter);
    }
}

void
fastuidraw::interval_allocator::
remove_free_interval_from_sorted_only(interval_ref iter)
{
  int sz(iter->second.m_end - iter->second.m_begin);
  std::map<int, interval_ref_set>::iterator sorted_iter;

  sorted_iter = m_sorted.find(sz);
  remove_free_interval_from_sorted_only(sorted_iter, iter);
}
