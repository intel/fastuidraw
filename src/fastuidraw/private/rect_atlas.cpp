/*!
 * \file rect_atlas.cpp
 * \brief file rect_atlas.cpp
 *
 * Adapted from: WRATHAtlasBase.cpp and WRATHAtlas.cpp of WRATH:
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

#include <algorithm>
#include <ciso646>

#include "rect_atlas.hpp"


namespace
{
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
  class tree_base
  {
  public:
    typedef std::pair<tree_base*, enum fastuidraw::return_code> add_return_value;
    typedef fastuidraw::detail::RectAtlas::rectangle rectangle;
    typedef fastuidraw::ivec2 ivec2;
    typedef fastuidraw::detail::SimplePool<4096> SimplePool;

    tree_base(const ivec2 &bl, const ivec2 &sz):
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

    virtual
    add_return_value
    add(SimplePool&, rectangle*) = 0;

  private:
    ivec2 m_minX_minY, m_size;
  };

  /* a tree_node_without_children represents
   * a Node which has NO child Node's
   * but may or maynot have a rectangle.
   */
  class tree_node_without_children:public tree_base
  {
  public:
    tree_node_without_children(const ivec2 &bl, const ivec2 &sz,
                               rectangle *rect = nullptr);

    virtual
    add_return_value
    add(SimplePool&, rectangle*);

    rectangle*
    data(void);

  private:
    rectangle *m_rectangle;
  };

  /* a tree node with children has _3_ children.
   * they are spawned when a tree_node_wihout_children
   * has a rectangle added but it already has a rectangle.
   */
  class tree_node_with_children:public tree_base
  {
  public:
    tree_node_with_children(SimplePool &pool,
                            tree_node_without_children *src,
                            bool split_x, bool split_y);

    virtual
    add_return_value
    add(SimplePool&, rectangle*);

  private:
    fastuidraw::vecN<tree_base*, 3> m_children;
  };

  class tree_sorter
  {
  public:
    bool
    operator()(tree_base *lhs, tree_base *rhs) const
    {
      /* we want to list the smallest "size" first
       * to avoid splitting large elements
       */
      return lhs->area() < rhs->area();
    }
  };
}

//////////////////////////////////////
// tree_node_without_children methods
tree_node_without_children::
tree_node_without_children(const ivec2 &bl, const ivec2 &sz,
                           rectangle *rect):
  tree_base(bl, sz),
  m_rectangle(rect)
{
}

tree_base::rectangle*
tree_node_without_children::
data(void)
{
  return m_rectangle;
}

tree_base::add_return_value
tree_node_without_children::
add(SimplePool &pool, rectangle *im)
{
  if (im->size().x() > size().x() || im->size().y() > size().y())
    {
      return add_return_value(this, fastuidraw::routine_fail);
    }

  if (m_rectangle == nullptr)
    {
      //do not have a rect so we take it (and move it).
      m_rectangle = im;
      m_rectangle->move(minX_minY());

      return add_return_value(this, fastuidraw::routine_success);
    }

  //we have a rectangle already, we need to check
  //if we can split in such a way to take im:
  int dx, dy;
  bool split_y_works, split_x_works;

  dx = size().x() - m_rectangle->size().x();
  dy = size().y() - m_rectangle->size().y();

  split_y_works = (dy >= im->size().y());
  split_x_works = (dx >= im->size().x());

  if (!split_x_works && !split_y_works)
    {
      return add_return_value(this, fastuidraw::routine_fail);
    }

  if (split_x_works && split_y_works)
    {
      //choose a split that is nicest
      //by making the other split false.

      //whoever has the most room left over is the split.
      if (dx > dy)
        {
          split_y_works = false;
        }
      else
        {
          split_x_works = false;
        }
    }

  tree_base *new_node;
  add_return_value R;

  //new_node will hold this->m_rectange:
  new_node = pool.create<tree_node_with_children>(pool, this, split_x_works, split_y_works);

  //add the new rectangle im to new_node:
  R = new_node->add(pool, im);
  FASTUIDRAWassert(R.second == fastuidraw::routine_success);

  if (R.first!=new_node)
    {
      new_node = R.first;
    }

  return add_return_value(new_node, fastuidraw::routine_success);
}

////////////////////////////////////
// tree_node_with_children methods
tree_node_with_children::
tree_node_with_children(SimplePool &pool,
                        tree_node_without_children *src,
                        bool split_x_works, bool split_y_works):
  tree_base(src->minX_minY(), src->size()),
  m_children(nullptr, nullptr, nullptr)
{
  rectangle *R(src->data());
  FASTUIDRAWassert(R != nullptr);

  m_children[2] = pool.create<tree_node_without_children>(R->minX_minY(), R->size(), R);

  /* Perhaps we should consider delaying creating m_children[0] and m_children[1]
   * until the first request come to this to add a rectangle so that we can
   * possibly take a bigger rectangle.
   */
  if (split_x_works)
    {
      m_children[0]
        = pool.create<tree_node_without_children>(ivec2(minX_minY().x(), minX_minY().y() + R->size().y()),
                                                  ivec2(R->size().x(), size().y() - R->size().y()) );

      m_children[1]
        = pool.create<tree_node_without_children>(ivec2(minX_minY().x() + R->size().x(), minX_minY().y()),
                                                  ivec2(size().x() - R->size().x(), size().y()) );
    }
  else
    {
      FASTUIDRAWassert(split_y_works);
      FASTUIDRAWunused(split_y_works);

      m_children[0]
        = pool.create<tree_node_without_children>(ivec2(minX_minY().x() + R->size().x(), minX_minY().y()),
                                                  ivec2(size().x() - R->size().x(), R->size().y()) );

      m_children[1]
        = pool.create<tree_node_without_children>(ivec2(minX_minY().x(), minX_minY().y() + R->size().y()),
                                                  ivec2(size().x(), size().y() - R->size().y()) );
    }

  std::sort(m_children.begin(), m_children.end(), tree_sorter());
}

tree_base::add_return_value
tree_node_with_children::
add(SimplePool &pool, rectangle *im)
{
  add_return_value R;

  for(int i = 0; i < 3; ++i)
    {
      R = m_children[i]->add(pool, im);
      if (R.second == fastuidraw::routine_success)
        {
          if (R.first != m_children[i])
            {
              m_children[i] = R.first;
            }
          return add_return_value(this, fastuidraw::routine_success);
        }
    }

  return add_return_value(this, fastuidraw::routine_fail);
}

////////////////////////////////////
// fastuidraw::detail::RectAtlas methods
fastuidraw::detail::RectAtlas::
RectAtlas(const ivec2 &dimensions):
  m_rejected_request_size(dimensions + ivec2(1, 1)),
  m_empty_rect(ivec2(0, 0))
{
  m_data = m_pool.create<tree_node_without_children>(ivec2(0,0), dimensions, nullptr);
}

fastuidraw::detail::RectAtlas::
~RectAtlas()
{
}

fastuidraw::ivec2
fastuidraw::detail::RectAtlas::
size(void) const
{
  tree_base *root;
  root = static_cast<tree_base*>(m_data);
  FASTUIDRAWassert(root != nullptr);
  return root->size();
}

void
fastuidraw::detail::RectAtlas::
clear(void)
{
  ivec2 dimensions(size());

  m_pool.clear();
  m_data = m_pool.create<tree_node_without_children>(ivec2(0,0), dimensions, nullptr);
  m_rejected_request_size = dimensions + ivec2(1, 1);
}

const fastuidraw::detail::RectAtlas::rectangle*
fastuidraw::detail::RectAtlas::
add_rectangle(const ivec2 &dimensions)
{
  rectangle *return_value(nullptr);
  tree_base *root;
  root = static_cast<tree_base*>(m_data);

  /* We are doing a very, very simple quick rejection
   * test where we reject any rectangle which has
   * a dimension atleast as large as the last rejected
   * rectangle. This will reject some rectangles that
   * could fit though.
   */
  if (dimensions.x() < m_rejected_request_size.x()
      && dimensions.y() < m_rejected_request_size.y())
    {
      tree_base::add_return_value R;

      if (dimensions.x() > 0 && dimensions.y() > 0)
        {
          //attempt to add the rect:
          return_value = m_pool.create<rectangle>(dimensions);
          R = root->add(m_pool, return_value);

          if (R.second == routine_success)
            {
              if (R.first != root)
                {
                  root = R.first;
                }
            }
          else
            {
              return_value = nullptr;
              m_rejected_request_size = dimensions;
            }
        }
      else
        {
          return_value = &m_empty_rect;
        }
    }

  m_data = root;
  return return_value;
}
