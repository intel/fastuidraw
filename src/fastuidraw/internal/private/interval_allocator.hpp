/*!
 * \file interval_allocator.hpp
 * \brief file interval_allocator.hpp
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

#include <map>
#include <set>
#include <fastuidraw/util/util.hpp>

namespace fastuidraw
{
  /*!\class interval_allocator
   * An interval_allocator gives a means to allocate and deallocate
   * ranges from a linear range. The implementation is simply
   * to make a list of free intervals sorted by size. Allocation
   * means taking from the list the smallest interval that can
   * accomodate the request. From that interval take the portion
   * needed and return the remainder as a new entry on the free
   * list. All operations are O(log N) where N is the number of
   * free intervals.
   */
  class interval_allocator:fastuidraw::noncopyable
  {
  public:
    /*!\enum interval_status_t
     */
    enum interval_status_t
      {
        /*!
         * Indicates interval is completely allocated
         */
        completely_allocated,

        /*!
         * Indicates interval is completely free
         */
        completely_free,

        /*!
         * Indicates the interval is partially allocated
         * and partially free.
         */
        partially_allocated,
      };

    /*!\fn
     * Ctor.
     * \param size gives the size from which to allocate intervals, essentially
     *             the \ref interval_allocator is initialized as having one
     *             free interval that starts at 0 with length equal to size
     */
    explicit
    interval_allocator(int size);

    /*!\fn
     * Reconstruct the \ref interval_allocator, i.e. clear all free intervals
     * \param size new size for the \ref interval_allocator
     */
    void
    reset(int size);

    /*!\fn
     * Resize the \ref interval_allocator. The new size must be atleast
     * as large as the old size.
     * \param size new size to which to size the \ref interval_allocator
     */
    void
    resize(int size);

    /*!\fn
     * Returns the "size" of the \ref interval_allocator, i.e. all intervals
     * allocated are in the range [0, size() ).
     */
    int
    size(void) const
    {
      return m_size;
    }

    /*!\fn
     * Allocate, returns the "begin" of the interval
     * allocated. Returns -1 on failure.
     * \param size length of interval to allocate
     */
    int
    allocate_interval(int size);

    /*!\fn
     * Free an interval.
     * \param location start of interval
     * \param size size of interval
     */
    void
    free_interval(int location, int size);

    /*!\fn
     * Returns the largest value that can be passed to allocate_interval()
     * and not fail.
     */
    int
    largest_free_interval(void) const
    {
      return m_sorted.empty() ?
        0 :
        m_sorted.rbegin()->first;
    }

    /*!\fn
     * Returns the allocation status of an interval
     * \param begin start of interval
     * \param size length of interval
     */
    interval_status_t
    interval_status(int begin, int size) const;

  private:
    typedef range_type<int> interval;
    typedef std::map<int, interval>::iterator interval_ref;

    class compare_interval_ref
    {
    public:
      bool
      operator()(interval_ref lhs, interval_ref rhs) const
      {
        FASTUIDRAWassert(lhs->first == lhs->second.m_end);
        FASTUIDRAWassert(rhs->first == rhs->second.m_end);
        return lhs->second.m_end < rhs->second.m_end;
      }
    };
    typedef std::set<interval_ref, compare_interval_ref> interval_ref_set;

    void
    remove_free_interval(interval_ref iter);

    void
    remove_free_interval(std::map<int, interval_ref_set>::iterator,
                         interval_ref iter);

    void
    remove_free_interval_from_sorted_only(std::map<int, interval_ref_set>::iterator sorted_iter,
                                          interval_ref iter);

    void
    remove_free_interval_from_sorted_only(interval_ref iter);

    int m_size;

    /* List of free intervals, stored in a map
     * keyed by interval::m_end
     */
    std::map<int, interval> m_free_intervals;

    /* Each element of m_sorted[size] refers
     * to an element in m_free_intervals. Map
     * is keyed by the size of the interval
     * pointed to by the elements in second.
     */
    std::map<int, interval_ref_set> m_sorted;
  };

}
