/*!
 * \file rect_atlas.hpp
 * \brief file rect_atlas.hpp
 *
 * Adapted from: WRATHAtlasBase.hpp and WRATHAtlas.hpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */

#pragma once

#include <list>
#include <map>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>

#include "util_private.hpp"
#include "simple_pool.hpp"


namespace fastuidraw {
namespace detail {

/*!\class RectAtlas
 * Provides an interface to allocate and free rectangle
 * regions from a large rectangle.
 */
class RectAtlas:public fastuidraw::noncopyable
{
public:
  /*!
   * Ctor
   * \param dimensions dimension of the atlas, this is then the return value to size().
   */
  explicit
  RectAtlas(const ivec2 &dimensions);

  virtual
  ~RectAtlas();

  /*!
   * Returns the location where the rectangle is palced
   * in the RectAtlas. Failure is indicated by if any
   * of the coordinates of the returned value are negative.
   * \param dimension width and height of the rectangle
   */
  ivec2
  add_rectangle(const ivec2 &dimension);

  /*!
   * Clears the RectAtlas, in doing so deleting
   * all recranges allocated by \ref add_rectangle().
   * After clear(), all rectangle objects
   * returned by add_rectangle() are deleted, and as
   * such the pointers are then wild-invalid.
   */
  void
  clear(void);

  /*!
   * Clears the RectAtlas, in doing so deleting
   * all recranges allocated by \ref add_rectangle().
   * After clear(), all rectangle objects
   * returned by add_rectangle() are deleted, and as
   * such the pointers are then wild-invalid.
   * \param new_dimensions new dimensions of the RectAtlas.
   */
  void
  clear(ivec2 new_dimensions);

  /*!
   * Returns the size of the \ref RectAtlas,
   * i.e. the value passed as dimensions
   * in RectAtlas().
   */
  ivec2
  size(void) const;

private:
  void *m_data;
  SimplePool<4096> m_pool;
};

} //namespace detail

} //namespace fastuidraw
