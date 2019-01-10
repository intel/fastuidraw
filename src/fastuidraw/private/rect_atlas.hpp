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
   * An rectangle gives the location (i.e size and
   * position) of a rectangle within a RectAtlas.
   * The location of a rectangle does not change for the
   * lifetime of the rectangle after returned by
   * add_rectangle().
   */
  class rectangle:public fastuidraw::noncopyable
  {
  public:
    explicit
    rectangle(const ivec2 &psize):
      m_minX_minY(0, 0),
      m_size(psize)
    {}

    /*!
     * Returns the minX_minY of the rectangle.
     */
    const ivec2&
    minX_minY(void) const
    {
      return m_minX_minY;
    }

    int
    area(void) const
    {
      return m_size.x() * m_size.y();
    }

    /*!
     * Returns the size of the rectangle.
     */
    const ivec2&
    size(void) const
    {
      return m_size;
    }

    void
    move(const ivec2 &moveby)
    {
      m_minX_minY += moveby;
    }

  private:
    ivec2 m_minX_minY, m_size;
  };

  /*!
   * Ctor
   * \param dimensions dimension of the atlas, this is then the return value to size().
   */
  explicit
  RectAtlas(const ivec2 &dimensions);

  virtual
  ~RectAtlas();

  /*!
   * Returns a pointer to the a newly created rectangle
   * of the requested size. Returns nullptr on failure.
   * The rectangle is owned by this RectAtlas.
   * An implementation may not change the location
   * (or size) of a rectangle once it has been
   * returned by add_rectangle().
   * \param dimension width and height of the rectangle
   */
  const rectangle*
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
  rectangle m_empty_rect;
};

} //namespace detail

} //namespace fastuidraw
