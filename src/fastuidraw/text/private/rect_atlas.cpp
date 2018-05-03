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

////////////////////////////////////////
// fastuidraw::detail::RectAtlas::tree_sorter methods
bool
fastuidraw::detail::RectAtlas::tree_sorter::
operator()(tree_base *lhs, tree_base *rhs) const
{
  /* we want to list the smallest "size" first
   *  to avoid splitting large elements
   */
  return lhs->area() < rhs->area();
}

///////////////////////////////////////////
// fastuidraw::detail::RectAtlas::rectangle methods
void
fastuidraw::detail::RectAtlas::rectangle::
build_parent_list(std::list<const tree_base*> &output) const
{
  const tree_base *p(m_tree);

  while(p != nullptr)
    {
      output.push_front(p);
      p = p->parent();
    }
}

//////////////////////////////////////
// fastuidraw::detail::RectAtlas::tree_base methods
fastuidraw::detail::RectAtlas::add_remove_return_value
fastuidraw::detail::RectAtlas::tree_base::
api_remove(const rectangle *p)
{
  std::list<const tree_base*> parentage;
  FASTUIDRAWassert(p != nullptr);

  p->build_parent_list(parentage);
  FASTUIDRAWassert(!parentage.empty());
  FASTUIDRAWassert(this == parentage.front());
  return remove(p, parentage);
}

//////////////////////////////////////
// fastuidraw::detail::RectAtlas::tree_node_without_children methods
fastuidraw::detail::RectAtlas::tree_node_without_children::
tree_node_without_children(const tree_base *pparent,
                           freesize_tracker *tr,
                           const ivec2 &bl, const ivec2 &sz,
                           rectangle *rect):
  tree_base(bl, sz, pparent, tr),
  m_rectangle(rect)
{
  if (m_rectangle != nullptr)
    {
      m_rectangle->m_tree = this;
    }
  update_tracking();
}


fastuidraw::detail::RectAtlas::tree_node_without_children::
~tree_node_without_children()
{
  if (m_rectangle!=nullptr)
    {
      FASTUIDRAWassert(m_rectangle->m_tree == this);
      FASTUIDRAWdelete(m_rectangle);
    }
  clear_from_tracking();
}

fastuidraw::detail::RectAtlas::rectangle*
fastuidraw::detail::RectAtlas::tree_node_without_children::
data(void)
{
  return m_rectangle;
}

void
fastuidraw::detail::RectAtlas::tree_node_without_children::
clear_from_tracking(void)
{
  for(std::vector<freesize_map::iterator>::iterator
        iter = m_sorted_by_x_iters.begin(), end = m_sorted_by_x_iters.end();
      iter != end; ++iter)
    {
      tracker()->m_sorted_by_x_size.erase(*iter);
    }

  for(std::vector<freesize_map::iterator>::iterator
        iter = m_sorted_by_y_iters.begin(), end = m_sorted_by_y_iters.end();
      iter != end; ++iter)
    {
      tracker()->m_sorted_by_y_size.erase(*iter);
    }

  m_sorted_by_x_iters.clear();
  m_sorted_by_y_iters.clear();
}

void
fastuidraw::detail::RectAtlas::tree_node_without_children::
update_tracking_helper(int x, int y)
{
  freesize_map::iterator iter_x, iter_y;

  iter_x = tracker()->m_sorted_by_x_size.insert(freesize_map::value_type(x, this));
  m_sorted_by_x_iters.push_back(iter_x);

  iter_y = tracker()->m_sorted_by_y_size.insert(freesize_map::value_type(y, this));
  m_sorted_by_y_iters.push_back(iter_y);
}


void
fastuidraw::detail::RectAtlas::tree_node_without_children::
update_tracking(void)
{
  clear_from_tracking();
  if (m_rectangle == nullptr)
    {
      update_tracking_helper(size().x(), size().y());
    }
  else
    {
      /*
       * we have two possible ways to split,
       * thus 2 entries:
       */
      update_tracking_helper(size().x(), size().y() - m_rectangle->size().y());
      update_tracking_helper(size().x() - m_rectangle->size().x(), size().y());
    }
}


fastuidraw::detail::RectAtlas::add_remove_return_value
fastuidraw::detail::RectAtlas::tree_node_without_children::
add(rectangle *im)
{
  if (im->size().x() > size().x() || im->size().y() > size().y())
    {
      return add_remove_return_value(this, routine_fail);
    }

  if (m_rectangle == nullptr)
    {
      //do not have a rect so we take it (and move it).
      m_rectangle = im;
      m_rectangle->m_tree = this;

      move_rectangle(m_rectangle, minX_minY());
      update_tracking();

      return add_remove_return_value(this, routine_success);
    }


  //we have a rectangle already, we need to check
  //if we can split in such a way to take im:
  int dx, dy;
  bool split_y_works, split_x_works;

  dx=size().x() - m_rectangle->size().x();
  dy=size().y() - m_rectangle->size().y();

  split_y_works = (dy >= im->size().y());
  split_x_works = (dx >= im->size().x());

  if (!split_x_works && !split_y_works)
    {
      return add_remove_return_value(this, routine_fail);
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
  add_remove_return_value R;

  //new_node will hold this->m_rectange:
  new_node = FASTUIDRAWnew tree_node_with_children(this, split_x_works, split_y_works);
  //set m_rectangle to nullptr since new_node "owns" it now,
  //the caller will delete this.
  m_rectangle = nullptr;

  //add the new rectangle im to new_node:
  R=new_node->add(im);
  FASTUIDRAWassert(R.second == routine_success);

  if (R.first!=new_node)
    {
      FASTUIDRAWdelete(new_node);
      new_node = R.first;
    }

  return add_remove_return_value(new_node, routine_success);
}

fastuidraw::detail::RectAtlas::add_remove_return_value
fastuidraw::detail::RectAtlas::tree_node_without_children::
remove(const fastuidraw::detail::RectAtlas::rectangle *im,
       std::list<const tree_base*> &parent_list)
{
  FASTUIDRAWassert(!parent_list.empty());
  if (parent_list.front() != this)
    {
      return add_remove_return_value(this, routine_fail);
    }

  FASTUIDRAWassert(m_rectangle == im);
  FASTUIDRAWassert(im->m_tree == this);
  FASTUIDRAWunused(im);
  FASTUIDRAWdelete(m_rectangle);
  m_rectangle = nullptr;
  update_tracking();

  return add_remove_return_value(this, routine_success);

}

bool
fastuidraw::detail::RectAtlas::tree_node_without_children::
empty(void)
{
  return m_rectangle == nullptr;
}

////////////////////////////////////
// fastuidraw::detail::RectAtlas::tree_node_with_children methods
fastuidraw::detail::RectAtlas::tree_node_with_children::
tree_node_with_children(fastuidraw::detail::RectAtlas::tree_node_without_children *src,
                        bool split_x, bool split_y):
  tree_base(src->minX_minY(), src->size(), src->parent(), src->tracker()),
  m_children(nullptr, nullptr, nullptr)
{
  rectangle *R(src->data());
  FASTUIDRAWassert(R != nullptr);

  m_children[2] = FASTUIDRAWnew tree_node_without_children(this, src->tracker(),
                                                          R->minX_minY(), R->size(), R);
  if (split_x)
    {
      m_children[0]
        = FASTUIDRAWnew tree_node_without_children( this, src->tracker(),
                                                   ivec2(minX_minY().x(), minX_minY().y() + R->size().y()),
                                                   ivec2(R->size().x(), size().y() - R->size().y()) );

      m_children[1]
        = FASTUIDRAWnew tree_node_without_children( this, src->tracker(),
                                                   ivec2(minX_minY().x() + R->size().x(), minX_minY().y()),
                                                   ivec2(size().x() - R->size().x(), size().y()) );
    }
  else
    {
      FASTUIDRAWassert(split_y);
      FASTUIDRAWunused(split_y);

      m_children[0]
        = FASTUIDRAWnew tree_node_without_children( this, src->tracker(),
                                                   ivec2(minX_minY().x() + R->size().x(), minX_minY().y()),
                                                   ivec2(size().x() - R->size().x(), R->size().y()) );

      m_children[1]
        = FASTUIDRAWnew tree_node_without_children( this, src->tracker(),
                                                   ivec2(minX_minY().x(), minX_minY().y() + R->size().y()),
                                                   ivec2(size().x(), size().y() - R->size().y()) );
    }

  std::sort(m_children.begin(), m_children.end(), tree_sorter());
}

fastuidraw::detail::RectAtlas::tree_node_with_children::
~tree_node_with_children()
{
  for(int i=0;i<3;++i)
    {
      FASTUIDRAWassert(m_children[i] != nullptr);
      FASTUIDRAWdelete(m_children[i]);
    }
}


fastuidraw::detail::RectAtlas::add_remove_return_value
fastuidraw::detail::RectAtlas::tree_node_with_children::
add(rectangle *im)
{
  add_remove_return_value R;

  for(int i=0;i<3;++i)
    {
      R = m_children[i]->add(im);
      if (R.second == routine_success)
        {
          if (R.first != m_children[i])
            {
              FASTUIDRAWdelete(m_children[i]);
              m_children[i] = R.first;
            }
          return add_remove_return_value(this, routine_success);
        }
    }

  return add_remove_return_value(this, routine_fail);
}

fastuidraw::detail::RectAtlas::add_remove_return_value
fastuidraw::detail::RectAtlas::tree_node_with_children::
remove(const rectangle *im,
       std::list<const tree_base*> &parent_list)
{
  FASTUIDRAWassert(!parent_list.empty());
  if (parent_list.front() != this)
    {
      return add_remove_return_value(this, routine_fail);
    }

  parent_list.pop_front();
  tree_base *null_tree_base(nullptr);
  add_remove_return_value R(null_tree_base, routine_fail);
  int delete_index(3);

  for(int i = 0; i < 3 && R.second == routine_fail; ++i)
    {
      FASTUIDRAWassert(m_children[i] != nullptr);

      R = m_children[i]->remove(im, parent_list);
      if (R.second == routine_success)
        {
          delete_index = i;
        }
    }

  FASTUIDRAWassert(R.second == routine_success);
  FASTUIDRAWassert(delete_index < 3);
  if (R.first != m_children[delete_index])
    {
      FASTUIDRAWdelete(m_children[delete_index]);
      m_children[delete_index] = R.first;
    }

  //now check the situation, if all children are
  //empty and thus we can reform to be a
  //tree_node_without_children
  if (empty())
    {
      tree_node_without_children *ptr;

      ptr = FASTUIDRAWnew tree_node_without_children(parent(), tracker(), minX_minY(), size());
      return add_remove_return_value(ptr, routine_success);
    }
  else
    {
      return add_remove_return_value(this, routine_success);
    }

}

bool
fastuidraw::detail::RectAtlas::tree_node_with_children::
empty(void)
{
  return m_children[0]->empty()
    && m_children[1]->empty()
    && m_children[2]->empty();
}

//////////////////////////////////////
// fastuidraw::detail::RectAtlas::freesize_tracker methods
bool
fastuidraw::detail::RectAtlas::freesize_tracker::
fast_check(ivec2 psize)
{

  return !m_sorted_by_x_size.empty()
    && m_sorted_by_x_size.rbegin()->first >= psize.x()
    && !m_sorted_by_y_size.empty()
    && m_sorted_by_y_size.rbegin()->first >= psize.y();

  /*
   * the fast check can cull quickly, but
   * it does not guarnantee that an element
   * can be fit, to that would require
   * checking if the sets
   * [iterx, m_sorted_by_x_size.end())
   * and
   * [itery, m_sorted_by_y_size.end())
   *
   * where iterx=m_sorted_by_x_size.lower_bound(psize.x())
   * and itery=m_sorted_by_y_size.lower_bound(psize.y())
   *
   * intersect as sets of pointers sorted by pointer
   * (not value). Such a check would induce
   * one to sort, which is O(nlogn),
   * which is likely much more than just walking
   * the tree structure.
   *
   * The fast test is O(1).
   */
}

////////////////////////////////////
// fastuidraw::detail::RectAtlas methods
fastuidraw::detail::RectAtlas::
RectAtlas(const ivec2 &dimensions):
  m_root(nullptr),
  m_empty_rect(this, ivec2(0, 0))
{
  m_root = FASTUIDRAWnew tree_node_without_children(nullptr, &m_tracker, ivec2(0,0), dimensions, nullptr);
}

fastuidraw::detail::RectAtlas::
~RectAtlas()
{
  FASTUIDRAWassert(m_root != nullptr);
  FASTUIDRAWdelete(m_root);
}

fastuidraw::ivec2
fastuidraw::detail::RectAtlas::
size(void) const
{
  FASTUIDRAWassert(m_root != nullptr);
  return m_root->size();
}

void
fastuidraw::detail::RectAtlas::
clear(void)
{
  ivec2 dimensions(m_root->size());

  m_mutex.lock();
  FASTUIDRAWdelete(m_root);
  m_root = FASTUIDRAWnew tree_node_without_children(nullptr, &m_tracker, ivec2(0,0), dimensions, nullptr);
  m_mutex.unlock();
}

const fastuidraw::detail::RectAtlas::rectangle*
fastuidraw::detail::RectAtlas::
add_rectangle(const ivec2 &dimensions,
              int left_padding, int right_padding,
              int top_padding, int bottom_padding)
{
  rectangle *return_value(nullptr);

  m_mutex.lock();
  if (m_tracker.fast_check(dimensions))
    {
      add_remove_return_value R;


      if (dimensions.x() > 0 && dimensions.y() > 0)
        {
          //attempt to add the rect:
          return_value = FASTUIDRAWnew rectangle(this, dimensions);
          R = m_root->add(return_value);

          if (R.second == routine_success)
            {
              if (R.first != m_root)
                {
                  FASTUIDRAWdelete(m_root);
                  m_root = R.first;
                }
            }
          else
            {
              FASTUIDRAWdelete(return_value);
              return_value = nullptr;
            }
        }
      else
        {
          return_value = &m_empty_rect;
        }
    }
  m_mutex.unlock();

  if (return_value != nullptr && return_value != &m_empty_rect)
    {
      return_value->finalize(left_padding, right_padding,
                             top_padding, bottom_padding);
    }

  return return_value;
}


enum fastuidraw::return_code
fastuidraw::detail::RectAtlas::
remove_rectangle_implement(const rectangle *im)
{
  add_remove_return_value R;

  FASTUIDRAWassert(im->atlas() == this);

  if (im->size().x() <= 0 || im->size().y() <= 0)
    {
      FASTUIDRAWassert(im == &im->atlas()->m_empty_rect);
      return routine_success;
    }
  else
    {
      m_mutex.lock();
      R = m_root->api_remove(im);
      if (R.second == routine_success && R.first != m_root)
        {
          FASTUIDRAWdelete(m_root);
          m_root = R.first;
        }
      m_mutex.unlock();
      return R.second;
    }
}



enum fastuidraw::return_code
fastuidraw::detail::RectAtlas::
delete_rectangle(const RectAtlas::rectangle *im)
{
  FASTUIDRAWassert(im);
  FASTUIDRAWassert(im->m_atlas != nullptr);
  return im->m_atlas->remove_rectangle_implement(im);
}
