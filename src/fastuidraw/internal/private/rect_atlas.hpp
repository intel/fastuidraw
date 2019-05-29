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
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
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

#include <private/util_private.hpp>
#include <private/simple_pool.hpp>


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
  enum node_num_t
    {
      num_rects,
      num_with_children,
      num_without_children,

      node_num_count
    };

  class Rectangle;
  class NodeBase;
  class NodeWithoutChildren;
  class NodeWithChildren;
  class MemoryPool;
  typedef vecN<int, node_num_count> NodeSizeCount;

  class Rectangle:public fastuidraw::noncopyable
  {
  public:
    explicit
    Rectangle(const ivec2 &psize):
      m_minX_minY(0, 0),
      m_size(psize)
    {}

    const fastuidraw::ivec2&
    minX_minY(void) const
    {
      return m_minX_minY;
    }

    int
    area(void) const
    {
      return m_size.x() * m_size.y();
    }

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

  class NodeBase
  {
  public:
    NodeBase(const ivec2 &bl, const ivec2 &sz):
      m_minX_minY(bl), m_size(sz)
    {}

    const ivec2&
    size(void) const
    {
      return m_size;
    }

    int
    area(void) const
    {
      return m_size.x() * m_size.y();
    }

    const ivec2&
    minX_minY(void) const
    {
      return m_minX_minY;
    }

    NodeWithoutChildren*
    widest_possible_rectangle(void) { return m_widest; }

    NodeWithoutChildren*
    tallest_possible_rectangle(void) { return m_tallest; }

    NodeWithoutChildren*
    biggest_possible_rectangle(void) { return m_biggest; }

    virtual
    NodeSizeCount
    count(void) const = 0;

    /* returns nullptr if failed to add rectangle,
     * otherwise returns what the caller pointer
     * should be updated to.
     */
    NodeBase*
    add(MemoryPool &pool, Rectangle *rect);

  protected:
    NodeWithoutChildren *m_widest, *m_tallest, *m_biggest;

  private:
    /*
     * A return value of nullptr indicates failure;
     * a return value of non-null but different, means
     * change pointer to callee.
     */
    virtual
    NodeBase*
    add_implement(MemoryPool&, Rectangle*) = 0;

    ivec2 m_minX_minY, m_size;
  };

  class NodeWithoutChildren:public NodeBase
  {
  public:
    NodeWithoutChildren(const ivec2 &bl, const ivec2 &sz,
                        Rectangle *rect);

    Rectangle*
    data(void);

    int
    widest_possible(void) const
    {
      return size().x();
    }

    int
    tallest_possible(void) const
    {
      return size().y();
    }

    int
    biggest_possible(void) const
    {
      int A(area());

      return m_rectangle ?
        A - m_rectangle->area() :
        A;
    }

    virtual
    NodeSizeCount
    count(void) const
    {
      NodeSizeCount return_value;

      return_value[num_rects] = (m_rectangle) ? 1u : 0u;
      return_value[num_with_children] = 0;
      return_value[num_without_children] = 1;

      return return_value;
    }

  private:
    virtual
    NodeBase*
    add_implement(MemoryPool&, Rectangle*);

    Rectangle *m_rectangle;
  };

  class NodeWithChildren:public NodeBase
  {
  public:
    NodeWithChildren(MemoryPool &pool,
                     NodeWithoutChildren *src,
                     bool split_x, bool split_y);

    virtual
    NodeSizeCount
    count(void) const
    {
      NodeSizeCount return_value;

      return_value[num_rects] = 0u;
      return_value[num_with_children] = 1;
      return_value[num_without_children] = 0;

      return return_value
        + m_children[0]->count()
        + m_children[1]->count()
        + m_children[2]->count();
    }

  private:
    virtual
    NodeBase*
    add_implement(MemoryPool&, Rectangle*);

    void
    recompute_possible(void);

    vecN<NodeBase*, 3> m_children;
  };

  class NodeSorter
  {
  public:
    bool
    operator()(NodeBase *lhs, NodeBase *rhs) const
    {
      /* we want to list the smallest "size" first
       * to avoid splitting large elements
       */
      return lhs->area() < rhs->area();
    }
  };

  class MemoryPool:fastuidraw::noncopyable
  {
  public:
    Rectangle*
    create_rectangle(const ivec2 &psize)
    {
      return m_rect_allocator.create(psize);
    }

    NodeWithoutChildren*
    create_node_without_children(const ivec2 &bl, const ivec2 &sz,
                                 Rectangle *rect = nullptr)
    {
      return m_node_without_children_allocator.create(bl, sz, rect);
    }

    NodeWithChildren*
    create_node_with_children(NodeWithoutChildren *src,
                              bool split_x, bool split_y)
    {
      return m_node_with_children_allocator.create(*this, src, split_x, split_y);
    }

    void
    clear(void)
    {
      m_rect_allocator.clear();
      m_node_without_children_allocator.clear();
      m_node_with_children_allocator.clear();
    }

  private:
    SimplePool<Rectangle, 512> m_rect_allocator;
    SimplePool<NodeWithoutChildren, 512> m_node_without_children_allocator;
    SimplePool<NodeWithChildren, 512> m_node_with_children_allocator;
  };

  NodeBase *m_root;
  MemoryPool m_pool;
};

} //namespace detail

} //namespace fastuidraw
