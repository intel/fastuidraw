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
    const PainterAttributeData&
    painter_data(void) const;

    /*!
      Returns an array listing what winding number values
      there are triangle in this Subset. To get the indices
      for those triangle with winding number N = winding_numbers()[k],
      pass k to PainterAttributeData::index_chunks() called
      on the PainterAttributeData returned by painter_data().
      The same attribute chunk, 0, is used regardless of which
      index chunk.
    */
    const_c_array<int>
    winding_numbers(void) const;

    /*!
      Returns what chunk to pass PainterAttributeData::index_chunks()
      called on the PainterAttributeData returned by painter_data()
      to get the triangles of a specified winding number.
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

  private:
    friend class FilledPath;

    explicit
    Subset(void *d);

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
    \param dst[output] location to which to write the what SubSets
    \returns the number of chunks that intersect the clipping region,
             that number is guarnanteed to be no more than number_subsets().

   */
  unsigned int
  select_subsets(const_c_array<vec3> clip_equations,
                 const float3x3 &clip_matrix_local,
                 c_array<unsigned int> dst) const;
private:
  void *m_d;
};

/*! @} */

} //namespace fastuidraw
