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


namespace fastuidraw {
namespace detail {

/*!\class RectAtlas
 * Provides an interface to allocate and free rectangle
 * regions from a large rectangle.
 */
class RectAtlas:public fastuidraw::noncopyable
{
private:
  class tree_base;
  class tree_node_with_children;
  class tree_node_without_children;
  typedef std::pair<tree_base*, enum return_code> add_return_value;

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
    /*!
     * Returns the minX_minY of the rectangle.
     */
    const ivec2&
    minX_minY(void) const
    {
      return m_minX_minY;
    }

    /*!
     * Returns the size of the rectangle.
     */
    const ivec2&
    size(void) const
    {
      return m_size;
    }

    const ivec2&
    unpadded_minX_minY(void) const
    {
      return m_unpadded_minX_minY;
    }

    const ivec2&
    unpadded_size(void) const
    {
      return m_unpadded_size;
    }

    /*!
     * Returns the owning RectAtlas of this
     * rectangle.
     */
    const RectAtlas*
    atlas(void) const
    {
      return m_atlas;
    }

    virtual
    ~rectangle()
    {}

  private:
    friend class RectAtlas;
    friend class tree_base;

    rectangle(RectAtlas *p, const ivec2 &psize):
      m_atlas(p),
      m_minX_minY(0, 0),
      m_size(psize),
      m_tree(nullptr)
    {}

    void
    finalize(int left, int right,
             int top, int bottom)
    {
      m_unpadded_minX_minY = m_minX_minY - ivec2(left, top);
      m_unpadded_size = m_size - ivec2(left + right, top + bottom);
    }

    RectAtlas *m_atlas;
    ivec2 m_minX_minY, m_size;
    ivec2 m_unpadded_minX_minY, m_unpadded_size;
    tree_base *m_tree;
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
  add_rectangle(const ivec2 &dimension,
                int left_padding, int right_padding,
                int top_padding, int bottom_padding);

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
   * Returns the size of the \ref RectAtlas,
   * i.e. the value passed as dimensions
   * in RectAtlas().
   */
  ivec2
  size(void) const;

private:
  /*
   * Tree structure to construct the texture atlas,
   * basic idea is very simple: walk the tree until one finds
   * a node where the image can fit.
   *
   * if .second is routine_fail, then the routine
   * failed.
   *
   * If .first of the return value of add or remove
   * is not the same as the object, then the return value
   * represents a new child and the old object should be deleted.
   *
   * if .first of the return value of add or remove
   * is the same as the object, then the routine succeeded
   * and the object should not be deleted.
   */
  class tree_sorter
  {
  public:
    bool
    operator()(tree_base *lhs, tree_base *rhs) const;
  };

  class tree_base
  {
  public:
    tree_base(const ivec2 &bl, const ivec2 &sz,
              const tree_base *pparent):
      m_minX_minY(bl), m_size(sz),
      m_parent(pparent)
    {}

    virtual
    ~tree_base(void)
    {}

    const ivec2&
    size(void) const
    {
      return m_size;
    }

    int
    area(void) const
    {
      return m_size.x()*m_size.y();
    }

    const ivec2&
    minX_minY(void) const
    {
      return m_minX_minY;
    }

    const tree_base*
    parent(void) const
    {
      return m_parent;
    }

    virtual
    add_return_value
    add(rectangle*) = 0;

    virtual
    bool
    empty(void) = 0;

  private:
    ivec2 m_minX_minY, m_size;
    const tree_base *m_parent;
  };

  //a tree_node_without_children represents
  //a Node which has NO child Node's
  //but may or maynot have a rectangle.
  class tree_node_without_children:public tree_base
  {
  public:
    tree_node_without_children(const tree_base *pparent,
                               const ivec2 &bl, const ivec2 &sz,
                               rectangle *rect=nullptr);
    ~tree_node_without_children();

    virtual
    add_return_value
    add(rectangle*);

    virtual
    bool
    empty(void);

    rectangle*
    data(void);

  private:
    rectangle *m_rectangle;
  };

  //a tree node with children has _3_ children.
  //they are spawned when a tree_node_wihout_children
  //has a rectangle added but it already has a rectangle.
  class tree_node_with_children:public tree_base
  {
  public:
    tree_node_with_children(tree_node_without_children *src,
                            bool split_x, bool split_y);
    ~tree_node_with_children();

    virtual
    add_return_value
    add(rectangle*);

    virtual
    bool
    empty(void);

  private:
    vecN<tree_base*,3> m_children;
  };

  static
  void
  move_rectangle(rectangle *rect, const ivec2 &moveby)
  {
    FASTUIDRAWassert(rect);
    rect->m_minX_minY += moveby;
  }

  static
  void
  set_minX_minY(rectangle *rect, const ivec2 &bl)
  {
    FASTUIDRAWassert(rect);
    rect->m_minX_minY = bl;
  }

  tree_base *m_root;
  ivec2 m_rejected_request_size;
  rectangle m_empty_rect;
};

} //namespace detail_private

} //namespace fastuidraw
