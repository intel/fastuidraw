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

  /* a NodeWithoutChildren represents
   * a Node which has NO child Node's
   * but may or maynot have a rectangle.
   */

  /* a tree node with children has _3_ children.
   * they are spawned when a tree_node_wihout_children
   * has a rectangle added but it already has a rectangle.
   */
}

////////////////////////////////
// fastuidraw::detail::RectAtlas::NodeBase methods
fastuidraw::detail::RectAtlas::NodeBase*
fastuidraw::detail::RectAtlas::NodeBase::
add(MemoryPool &pool, Rectangle *rect)
{
  /* We are doing a very, very simple quick rejection
   * test where we reject any rectangle which has
   * area larger than this's area() or if any of the
   * dimensions are greater than this's size. This
   * is far from ideal, and the correct thing would be
   * to create bins (perhaps keyed by log2) in each
   * dimension to quickly choose a node to split.
   * This would prevent the tree walk entirely and
   * then the root would need a list of nodes
   * available and the ability to remove elements
   * from the lists fast (perhaps std::map<K, std::list>)
   * would work. Only the creation of objects of
   * type NodeWithoutChildren would modify the
   * list, as would their deconsuction.
   */
  if (rect->area() <= biggest_possible_rectangle()->biggest_possible()
      && rect->size().x() <= widest_possible_rectangle()->widest_possible()
      && rect->size().y() <= tallest_possible_rectangle()->tallest_possible())
    {
      NodeBase *return_value;

      return_value = add_implement(pool, rect);
      return return_value;
    }
  else
    {
      return nullptr;
    }
}

//////////////////////////////////////
// fastuidraw::detail::RectAtlas::NodeWithoutChildren methods
fastuidraw::detail::RectAtlas::NodeWithoutChildren::
NodeWithoutChildren(const ivec2 &bl, const ivec2 &sz, Rectangle *rect):
  NodeBase(bl, sz),
  m_rectangle(rect)
{
  m_widest = m_tallest = m_biggest = this;
}

fastuidraw::detail::RectAtlas::Rectangle*
fastuidraw::detail::RectAtlas::NodeWithoutChildren::
data(void)
{
  return m_rectangle;
}

fastuidraw::detail::RectAtlas::NodeBase*
fastuidraw::detail::RectAtlas::NodeWithoutChildren::
add_implement(MemoryPool &pool, Rectangle *im)
{
  FASTUIDRAWassert(im->size().x() <= size().x());
  FASTUIDRAWassert(im->size().y() <= size().y());

  if (m_rectangle == nullptr)
    {
      //do not have a rect so we take it (and move it).
      m_rectangle = im;
      m_rectangle->move(minX_minY());

      return this;
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
      return nullptr;
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

  NodeBase *new_node;

  //new_node will hold this->m_rectange:
  new_node = pool.create<NodeWithChildren>(pool, this, split_x_works, split_y_works);

  //add the new rectangle im to new_node:
  new_node = new_node->add(pool, im);
  FASTUIDRAWassert(new_node);

  return new_node;
}

////////////////////////////////////
// NodeWithChildren methods
fastuidraw::detail::RectAtlas::NodeWithChildren::
NodeWithChildren(MemoryPool &pool,
                 NodeWithoutChildren *src,
                 bool split_x_works, bool split_y_works):
  NodeBase(src->minX_minY(), src->size()),
  m_children(nullptr, nullptr, nullptr)
{
  Rectangle *R(src->data());
  FASTUIDRAWassert(R != nullptr);

  m_children[2] = pool.create<NodeWithoutChildren>(R->minX_minY(), R->size(), R);

  /* Perhaps we should consider delaying creating m_children[0] and m_children[1]
   * until the first request come to this to add a rectangle so that we can
   * possibly take a bigger rectangle.
   */
  if (split_x_works)
    {
      m_children[0]
        = pool.create<NodeWithoutChildren>(ivec2(minX_minY().x(), minX_minY().y() + R->size().y()),
                                           ivec2(R->size().x(), size().y() - R->size().y()) );

      m_children[1]
        = pool.create<NodeWithoutChildren>(ivec2(minX_minY().x() + R->size().x(), minX_minY().y()),
                                           ivec2(size().x() - R->size().x(), size().y()) );
    }
  else
    {
      FASTUIDRAWassert(split_y_works);
      FASTUIDRAWunused(split_y_works);

      m_children[0]
        = pool.create<NodeWithoutChildren>(ivec2(minX_minY().x() + R->size().x(), minX_minY().y()),
                                           ivec2(size().x() - R->size().x(), R->size().y()) );

      m_children[1]
        = pool.create<NodeWithoutChildren>(ivec2(minX_minY().x(), minX_minY().y() + R->size().y()),
                                           ivec2(size().x(), size().y() - R->size().y()) );
    }

  std::sort(m_children.begin(), m_children.end(), NodeSorter());
  recompute_possible();
}

fastuidraw::detail::RectAtlas::NodeBase*
fastuidraw::detail::RectAtlas::NodeWithChildren::
add_implement(MemoryPool &pool, Rectangle *im)
{
  NodeBase *R;
  for(int i = 0; i < 3; ++i)
    {
      R = m_children[i]->add(pool, im);
      if (R)
        {
          m_children[i] = R;
          recompute_possible();
          return this;
        }
    }

  return nullptr;
}

void
fastuidraw::detail::RectAtlas::NodeWithChildren::
recompute_possible(void)
{
  m_widest = m_children[0]->widest_possible_rectangle();
  m_tallest = m_children[0]->tallest_possible_rectangle();
  m_biggest = m_children[0]->biggest_possible_rectangle();

  for (int i = 1; i < 3; ++i)
    {
      NodeWithoutChildren *p;

      p = m_children[i]->widest_possible_rectangle();
      if (p->widest_possible() > m_widest->widest_possible())
        {
          m_widest = p;
        }

      p = m_children[i]->tallest_possible_rectangle();
      if (p->tallest_possible() > m_tallest->tallest_possible())
        {
          m_tallest = p;
        }

      p = m_children[i]->biggest_possible_rectangle();
      if (p->biggest_possible() > m_biggest->biggest_possible())
        {
          m_biggest = p;
        }
    }
}

////////////////////////////////////
// fastuidraw::detail::RectAtlas methods
fastuidraw::detail::RectAtlas::
RectAtlas(const ivec2 &dimensions)
{
  m_root = m_pool.create<NodeWithoutChildren>(ivec2(0,0), dimensions, nullptr);
}

fastuidraw::detail::RectAtlas::
~RectAtlas()
{
}

fastuidraw::ivec2
fastuidraw::detail::RectAtlas::
size(void) const
{
  return m_root->size();
}

void
fastuidraw::detail::RectAtlas::
clear(void)
{
  clear(size());
}

void
fastuidraw::detail::RectAtlas::
clear(ivec2 dimensions)
{
  m_pool.clear();
  m_root = m_pool.create<NodeWithoutChildren>(ivec2(0,0), dimensions, nullptr);
}

fastuidraw::ivec2
fastuidraw::detail::RectAtlas::
add_rectangle(const ivec2 &dimensions)
{
  Rectangle *return_value(nullptr);
  NodeBase *R;

  if (dimensions.x() > 0 && dimensions.y() > 0)
    {
      //attempt to add the rect:
      return_value = m_pool.create<Rectangle>(dimensions);
      R = m_root->add(m_pool, return_value);

      if (R)
        {
          m_root = R;
        }
      else
        {
          return_value = nullptr;
        }
    }
  else
    {
      return ivec2(0, 0);
    }

  if (return_value)
    {
      return return_value->minX_minY();
    }
  else
    {
      return ivec2(-1, -1);
    }
}
