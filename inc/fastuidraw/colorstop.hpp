/*!
 * \file colorstop.hpp
 * \brief file colorstop.hpp
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

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/reference_counted.hpp>

namespace fastuidraw
{
///@cond
class ColorStopAtlas;
///@endcond

/*!\addtogroup Imaging
 * @{
 */

  /*!
   * \brief
   * A ColorStop is a pair consisting of an RGBA value and a place. The
   * value of the place is a floating point value in the range [0, 1].
   */
  class ColorStop
  {
  public:
    /*!
     * Default ctor. Initializes m_color as (0, 0, 0, 0)
     * and m_place as 0.0f.
     */
    ColorStop(void):
      m_color(0, 0, 0, 0),
      m_place(0.0f)
    {}

    /*!
     * Ctor
     * \param c value with which to initialize m_color
     * \param p value with which to initialize m_place
     */
    ColorStop(u8vec4 c, float p):
      m_color(c),
      m_place(p)
    {}

    /*!
     * Comparison operator to sort ColorStop values by m_place.
     * \param rhs value to which to compare
     */
    bool
    operator<(const ColorStop &rhs) const
    {
      return m_place < rhs.m_place;
    }

    /*!
     * The RGBA value of the color of the ColorStop.
     */
    u8vec4 m_color;

    /*!
     * The place of the ColorStop.
     */
    float m_place;
  };

  /*!
   * \brief
   * A ColorStopArray is a sequence of ColorStop values used to
   * define the color stops of a gradient.
   *
   * The values are sorted by ColorStop::m_place and each ColorStop
   * value of a ColorStopArray must have a unique value for
   * ColorStop::m_place. A color is computed (in drawing) from a
   * ColorStopArray at a point q as follows. First the color stops
   * S and T are found so that q is in the range [S.m_place, T.m_place].
   * The color value is then given by the value
   * (1-t) * S.m_color + t * S.m_color where t is given by
   * (q-S.m_place) / (T.m_place - S.m_place).
   */
  class ColorStopArray:noncopyable
  {
  public:
    /*!
     * Ctor, inits ColorStopArray as empty
     */
    ColorStopArray(void);

    /*!
     * Ctor, inits ColorStopArray as empty
     * but pre-reserves memory for color stops
     * added via add().
     * \param reserve number of slots to reserve in memory
     */
    explicit
    ColorStopArray(int reserve);

    ~ColorStopArray();

    /*!
     * Add a ColorStop to this ColorStopArray
     * \param c value to add
     */
    void
    add(const ColorStop &c);

    /*!
     * Add a sequence of stops.
     * \tparam iterator type to ColorStop
     * \param begin iterator to 1st stop to add
     * \param end iterator to one past the last stop to add
     */
    template<typename iterator>
    void
    add(iterator begin, iterator end)
    {
      for(; begin != end; ++begin)
        {
          add(*begin);
        }
    }

    /*!
     * Clear all stops from this ColorStopArray.
     */
    void
    clear(void);

    /*!
     * Returns the values added by add() sorted by ColorStop::m_place.
     * Sorting is done lazily, i.e. on calling values().
     */
    c_array<const ColorStop>
    values(void) const;

  private:
    void *m_d;
  };

  /*!
   * \brief
   * A ColorStopSequence is a \ref ColorStopArray on a \ref ColorStopAtlas.
   * A \ref ColorStopAtlas is backed by a 1D texture array with linear filtering.
   * The values of ColorStop::m_place are discretized. Values in between the
   * \ref ColorStop 's of a \ref ColorStopArray are interpolated.
   */
  class ColorStopSequence:public resource_base
  {
  public:
    ~ColorStopSequence();

    /*!
     * Returns the location in the backing store to
     * the logical start of the ColorStopSequence.
     * A ColorStopSequence is added to an atlas
     * so that the first and last texel are repeated, thus
     * allowing for implementation to use linear texture
     * filtering to implement color interpolation quickly
     * in a shader.
     */
    ivec2
    texel_location(void) const;

    /*!
     * Returns the number of texels NOT including repeating the
     * boundary texels used in the backing store.
     */
    int
    width(void) const;

    /*!
     * Returns the atlas on which the object resides
     */
    ColorStopAtlas&
    atlas(void) const;

  private:
    friend class ColorStopAtlas;

    ColorStopSequence(const ColorStopArray &color_stops,
                      ColorStopAtlas& atlas, unsigned int pwidth);
    void *m_d;
  };

/*! @} */
}
