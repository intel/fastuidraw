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
#include <fastuidraw/util/reference_counted.hpp>

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
    Ctor. Construct a FilledPath from the data
    of a TessellatedPath.
    \param P source TessellatedPath
   */
  explicit
  FilledPath(const TessellatedPath &P);

  ~FilledPath();

  /*!
    Returns the points of the FilledPath
   */
  const_c_array<vec2>
  points(void) const;

  /*!
    Returns an array listing what winding number values
    for which indices() will return a non-empty
    array. The array is sorted in ascending order.
   */
  const_c_array<int>
  winding_numbers(void) const;

  /*!
    Return indices into points() to draw
    triangles of the FilledPath of a given
    winding number.
   */
  const_c_array<unsigned int>
  indices(int winding_number) const;

  /*!
    Returns indices into points() to draw
    triangles of the FilledPath with non-zero
    winding number.
   */
  const_c_array<unsigned int>
  nonzero_winding_indices(void) const;

  /*!
    Returns indices into points() to draw
    triangles of the FilledPath with an
    odd winding number.
   */
  const_c_array<unsigned int>
  odd_winding_indices(void) const;

  /*!
    Returns indices into points() to draw
    triangles of the FilledPath with zero
    winding number. The unbounded portion is
    realized as bounded by the bounding box of
    the creating TessellatedPath.
   */
  const_c_array<unsigned int>
  zero_winding_indices(void) const;

  /*!
    Returns indices into points() to draw
    triangles of the FilledPath with even
    winding number. The unbounded portion is
    realized as bounded by the bounding box of
    the creating TessellatedPath.
   */
  const_c_array<unsigned int>
  even_winding_indices(void) const;

  /*!
    Returns data that can be passed to a PainterPacker
    to fill a path.
   */
  const PainterAttributeData&
  painter_data(void) const;

private:
  void *m_d;
};

/*! @} */

} //namespace fastuidraw
