/*!
 * \file colorstop_atlas.cpp
 * \brief file colorstop_atlas.cpp
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


#include <vector>
#include <fastuidraw/colorstop_atlas.hpp>
#include "private/interval_allocator.hpp"
#include "private/util_private.hpp"

namespace
{
  class ColorInterpolator
  {
  public:
    ColorInterpolator(const fastuidraw::ColorStop &begin,
                      const fastuidraw::ColorStop &end):
      m_start(begin.m_place),
      m_coeff(1.0f / (end.m_place - begin.m_place)),
      m_startColor(begin.m_color),
      m_deltaColor(fastuidraw::vec4(end.m_color) - fastuidraw::vec4(begin.m_color))
    {}

    fastuidraw::u8vec4
    interpolate(float t)
    {
      float s;
      fastuidraw::vec4 value;

      s = (t - m_start) * m_coeff;
      s = std::min(1.0f, std::max(0.0f, s));
      value = m_startColor + s * m_deltaColor;

      return fastuidraw::u8vec4(value);
    };

  private:
    float m_start, m_coeff;
    fastuidraw::vec4 m_startColor, m_deltaColor;
  };

  typedef std::pair<fastuidraw::ivec2, int> delayed_free_entry;

  class ColorStopAtlasPrivate
  {
  public:
    explicit
    ColorStopAtlasPrivate(fastuidraw::reference_counted_ptr<fastuidraw::ColorStopBackingStore> pbacking_store);

    void
    remove_entry_from_available_layers(std::map<int, std::set<int> >::iterator, int y);

    void
    add_bookkeeping(int new_size);

    void
    deallocate_implement(fastuidraw::ivec2 location, int width);

    mutable fastuidraw::mutex m_mutex;
    int m_delayed_interval_freeing_counter;
    std::vector<delayed_free_entry> m_delayed_freed_intervals;

    fastuidraw::reference_counted_ptr<fastuidraw::ColorStopBackingStore> m_backing_store;
    int m_allocated;

    /* Each layer has an interval allocator to allocate
       and free "color stop arrays"
     */
    std::vector<fastuidraw::interval_allocator*> m_layer_allocator;

    /* m_available_layers[key] gives indices into m_layer_allocator
       for those layers for which largest_free_interval() returns
       key.
     */
    std::map<int, std::set<int> > m_available_layers;
  };

  class ColorStopBackingStorePrivate
  {
  public:
    ColorStopBackingStorePrivate(int w, int num_layers, bool presizable):
      m_dimensions(w, num_layers),
      m_width_times_height(m_dimensions.x() * m_dimensions.y()),
      m_resizeable(presizable)
    {}

    fastuidraw::ivec2 m_dimensions;
    int m_width_times_height;
    bool m_resizeable;
  };

  class ColorStopSequenceOnAtlasPrivate
  {
  public:
    fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas> m_atlas;
    fastuidraw::ivec2 m_texel_location;
    int m_width;
    int m_start_slack, m_end_slack;
  };
}

////////////////////////////////////////
// ColorStopAtlasPrivate methods
ColorStopAtlasPrivate::
ColorStopAtlasPrivate(fastuidraw::reference_counted_ptr<fastuidraw::ColorStopBackingStore> pbacking_store):
  m_delayed_interval_freeing_counter(0),
  m_backing_store(pbacking_store),
  m_allocated(0)
{
  FASTUIDRAWassert(m_backing_store);
  add_bookkeeping(m_backing_store->dimensions().y());
}

void
ColorStopAtlasPrivate::
remove_entry_from_available_layers(std::map<int, std::set<int> >::iterator iter, int y)
{
  FASTUIDRAWassert(iter != m_available_layers.end());
  FASTUIDRAWassert(iter->second.find(y) != iter->second.end());
  iter->second.erase(y);
  if (iter->second.empty())
    {
      m_available_layers.erase(iter);
    }
}

void
ColorStopAtlasPrivate::
add_bookkeeping(int new_size)
{
  int width(m_backing_store->dimensions().x());
  int old_size(m_layer_allocator.size());
  std::set<int> &S(m_available_layers[width]);

  FASTUIDRAWassert(new_size > old_size);
  m_layer_allocator.resize(new_size, nullptr);
  for(int y = old_size; y < new_size; ++y)
    {
      m_layer_allocator[y] = FASTUIDRAWnew fastuidraw::interval_allocator(width);
      S.insert(y);
    }
}

void
ColorStopAtlasPrivate::
deallocate_implement(fastuidraw::ivec2 location, int width)
{
  int y(location.y());
  FASTUIDRAWassert(m_layer_allocator[y]);
  FASTUIDRAWassert(m_delayed_interval_freeing_counter == 0);

  int old_max, new_max;

  old_max = m_layer_allocator[y]->largest_free_interval();
  m_layer_allocator[y]->free_interval(location.x(), width);
  new_max = m_layer_allocator[y]->largest_free_interval();

  if (old_max != new_max)
    {
      std::map<int, std::set<int> >::iterator iter;

      iter = m_available_layers.find(old_max);
      remove_entry_from_available_layers(iter, y);
      m_available_layers[new_max].insert(y);
    }
  m_allocated -= width;
}

/////////////////////////////////////
// fastuidraw::ColorStopBackingStore methods
fastuidraw::ColorStopBackingStore::
ColorStopBackingStore(int w, int num_layers, bool presizable)
{
  m_d = FASTUIDRAWnew ColorStopBackingStorePrivate(w, num_layers, presizable);
}

fastuidraw::ColorStopBackingStore::
ColorStopBackingStore(ivec2 wl, bool presizable)
{
  m_d = FASTUIDRAWnew ColorStopBackingStorePrivate(wl.x(), wl.y(), presizable);
}

fastuidraw::ColorStopBackingStore::
~ColorStopBackingStore()
{
  ColorStopBackingStorePrivate *d;
  d = static_cast<ColorStopBackingStorePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::ivec2
fastuidraw::ColorStopBackingStore::
dimensions(void) const
{
  ColorStopBackingStorePrivate *d;
  d = static_cast<ColorStopBackingStorePrivate*>(m_d);
  return d->m_dimensions;
}

int
fastuidraw::ColorStopBackingStore::
width_times_height(void) const
{
  ColorStopBackingStorePrivate *d;
  d = static_cast<ColorStopBackingStorePrivate*>(m_d);
  return d->m_width_times_height;
}

bool
fastuidraw::ColorStopBackingStore::
resizeable(void) const
{
  ColorStopBackingStorePrivate *d;
  d = static_cast<ColorStopBackingStorePrivate*>(m_d);
  return d->m_resizeable;
}

void
fastuidraw::ColorStopBackingStore::
resize(int new_num_layers)
{
  ColorStopBackingStorePrivate *d;
  d = static_cast<ColorStopBackingStorePrivate*>(m_d);
  FASTUIDRAWassert(d->m_resizeable);
  FASTUIDRAWassert(new_num_layers > d->m_dimensions.y());
  resize_implement(new_num_layers);
  d->m_dimensions.y() = new_num_layers;
  d->m_width_times_height = d->m_dimensions.x() * d->m_dimensions.y();
}

///////////////////////////////////////
// fastuidraw::ColorStopAtlas methods
fastuidraw::ColorStopAtlas::
ColorStopAtlas(reference_counted_ptr<ColorStopBackingStore> pbacking_store)
{
  m_d = FASTUIDRAWnew ColorStopAtlasPrivate(pbacking_store);
}

fastuidraw::ColorStopAtlas::
~ColorStopAtlas()
{
  ColorStopAtlasPrivate *d;
  d = static_cast<ColorStopAtlasPrivate*>(m_d);

  FASTUIDRAWassert(d->m_delayed_interval_freeing_counter == 0);
  FASTUIDRAWassert(d->m_allocated == 0);
  for(interval_allocator *q : d->m_layer_allocator)
    {
      FASTUIDRAWdelete(q);
    }
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::ColorStopAtlas::
delay_interval_freeing(void)
{
  ColorStopAtlasPrivate *d;
  d = static_cast<ColorStopAtlasPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  ++d->m_delayed_interval_freeing_counter;
}

void
fastuidraw::ColorStopAtlas::
undelay_interval_freeing(void)
{
  ColorStopAtlasPrivate *d;
  d = static_cast<ColorStopAtlasPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  FASTUIDRAWassert(d->m_delayed_interval_freeing_counter >= 1);
  --d->m_delayed_interval_freeing_counter;
  if (d->m_delayed_interval_freeing_counter == 0)
    {
      for(unsigned int i = 0, endi = d->m_delayed_freed_intervals.size(); i < endi; ++i)
        {
          d->deallocate_implement(d->m_delayed_freed_intervals[i].first,
                                  d->m_delayed_freed_intervals[i].second);
        }
      d->m_delayed_freed_intervals.clear();
    }
}

void
fastuidraw::ColorStopAtlas::
deallocate(ivec2 location, int width)
{
  ColorStopAtlasPrivate *d;
  d = static_cast<ColorStopAtlasPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  if (d->m_delayed_interval_freeing_counter == 0)
    {
      d->deallocate_implement(location, width);
    }
  else
    {
      d->m_delayed_freed_intervals.push_back(delayed_free_entry(location, width));
    }
}

void
fastuidraw::ColorStopAtlas::
flush(void) const
{
  ColorStopAtlasPrivate *d;
  d = static_cast<ColorStopAtlasPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  d->m_backing_store->flush();
}

int
fastuidraw::ColorStopAtlas::
total_available(void) const
{
  ColorStopAtlasPrivate *d;
  d = static_cast<ColorStopAtlasPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  return d->m_backing_store->width_times_height() - d->m_allocated;
}

int
fastuidraw::ColorStopAtlas::
largest_allocation_possible(void) const
{
  ColorStopAtlasPrivate *d;
  d = static_cast<ColorStopAtlasPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  if (d->m_available_layers.empty())
    {
      return 0;
    }

  return d->m_available_layers.rbegin()->first;
}

fastuidraw::ivec2
fastuidraw::ColorStopAtlas::
allocate(c_array<const u8vec4> data)
{
  ColorStopAtlasPrivate *d;
  d = static_cast<ColorStopAtlasPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);

  std::map<int, std::set<int> >::iterator iter;
  ivec2 return_value;
  int width(data.size());

  FASTUIDRAWassert(width > 0);
  FASTUIDRAWassert(width <= max_width());

  iter = d->m_available_layers.lower_bound(width);
  if (iter == d->m_available_layers.end())
    {
      if (d->m_backing_store->resizeable())
        {
          /* TODO: what should the resize algorithm be?
             Right now we double the size, but that might
             be excessive.
           */
          int new_size, old_size;
          old_size = d->m_backing_store->dimensions().y();
          new_size = std::max(1, old_size * 2);
          d->m_backing_store->resize(new_size);
          d->add_bookkeeping(new_size);

          iter = d->m_available_layers.lower_bound(width);
          FASTUIDRAWassert(iter != d->m_available_layers.end());
        }
      else
        {
          FASTUIDRAWassert(!"ColorStop atlas exhausted");
          return ivec2(-1, -1);
        }
    }

  FASTUIDRAWassert(!iter->second.empty());

  int y(*iter->second.begin());
  int old_max, new_max;

  old_max = d->m_layer_allocator[y]->largest_free_interval();
  return_value.x() = d->m_layer_allocator[y]->allocate_interval(width);
  FASTUIDRAWassert(return_value.x() >= 0);
  new_max = d->m_layer_allocator[y]->largest_free_interval();

  if (old_max != new_max)
    {
      d->remove_entry_from_available_layers(iter, y);
      d->m_available_layers[new_max].insert(y);
    }
  return_value.y() = y;

  d->m_backing_store->set_data(return_value.x(), return_value.y(),
                               width, data);
  d->m_allocated += width;
  return return_value;
}


int
fastuidraw::ColorStopAtlas::
max_width(void) const
{
  ColorStopAtlasPrivate *d;
  d = static_cast<ColorStopAtlasPrivate*>(m_d);
  return d->m_backing_store->dimensions().x();
}

fastuidraw::reference_counted_ptr<const fastuidraw::ColorStopBackingStore>
fastuidraw::ColorStopAtlas::
backing_store(void) const
{
  ColorStopAtlasPrivate *d;
  d = static_cast<ColorStopAtlasPrivate*>(m_d);
  return d->m_backing_store;
}

///////////////////////////////////////////
// fastuidraw::ColorStopSequenceOnAtlas methods
fastuidraw::ColorStopSequenceOnAtlas::
ColorStopSequenceOnAtlas(const ColorStopSequence &pcolor_stops,
                         reference_counted_ptr<ColorStopAtlas> atlas,
                         int pwidth)
{
  ColorStopSequenceOnAtlasPrivate *d;
  d = FASTUIDRAWnew ColorStopSequenceOnAtlasPrivate();
  m_d = d;

  d->m_atlas = atlas;
  d->m_width = pwidth;

  c_array<const ColorStop> color_stops(pcolor_stops.values());
  FASTUIDRAWassert(d->m_atlas);
  FASTUIDRAWassert(pwidth>0);

  if (pwidth >= d->m_atlas->max_width())
    {
      d->m_width = d->m_atlas->max_width();
      d->m_start_slack = 0;
      d->m_end_slack = 0;
    }
  else if (pwidth == d->m_atlas->max_width() - 1)
    {
      d->m_start_slack = 0;
      d->m_end_slack = 1;
    }
  else
    {
      d->m_start_slack = 1;
      d->m_end_slack = 1;
    }

  std::vector<u8vec4> data(d->m_width + d->m_start_slack + d->m_end_slack);

  /* Discretize and interpolate color_stops into data
   */
  {
    unsigned int data_i, color_stops_i;
    float current_t, delta_t;

    delta_t = 1.0f / static_cast<float>(d->m_width);
    current_t = static_cast<float>(-d->m_start_slack) * delta_t;

    for(data_i = 0; current_t <= color_stops[0].m_place; ++data_i, current_t += delta_t)
      {
        data[data_i] = color_stops[0].m_color;
      }

    for(color_stops_i = 1;  color_stops_i < color_stops.size(); ++color_stops_i)
      {
        ColorStop prev_color(color_stops[color_stops_i-1]);
        ColorStop next_color(color_stops[color_stops_i]);

        /* There are cases where an application might
           add two color stops with the same stop location;
           these are for the purpose of changing color
           immediately at the named location. Adding the
           check avoids a divide error. The next texel
           in the gradient will observe the dramatic change.
           However, passing an interpolate between the
           immediate change and the texel after it will
           have the gradient interpolate from before the
           change to after the change sadly.

           The only way to really handle "fast immediate"
           changes is to make an array of (stop, color)
           pair values packed into an array readable from
           the shader and the fragment shader does the
           search. This means that rather than a single
           texture() command we would have multiple buffer
           look up value in the frag shader to get the
           interpolate. We can optimize it some where we
           increase the array size to the nearest power of 2
           so that within a triangle there is no branching
           in the hunt, but that would mean log2(N) buffer
           reads per pixel. ICK.
         */
        if (current_t < next_color.m_place)
          {
            ColorInterpolator color_interpolate(prev_color, next_color);

            for(; current_t < next_color.m_place && data_i < data.size();
                ++data_i, current_t += delta_t)
              {
                data[data_i] = color_interpolate.interpolate(current_t);
              }
          }
      }

    for(;data_i < data.size(); ++data_i)
      {
        data[data_i] = color_stops.back().m_color;
      }
  }


  d->m_texel_location = d->m_atlas->allocate(make_c_array(data));

  /* Adjust m_texel_location to remove the start slack
   */
  d->m_texel_location.x() += d->m_start_slack;
}

fastuidraw::ColorStopSequenceOnAtlas::
~ColorStopSequenceOnAtlas(void)
{
  ColorStopSequenceOnAtlasPrivate *d;
  d = static_cast<ColorStopSequenceOnAtlasPrivate*>(m_d);

  ivec2 loc(d->m_texel_location);

  loc.x() -= d->m_start_slack;
  d->m_atlas->deallocate(loc, d->m_width + d->m_start_slack + d->m_end_slack);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::ivec2
fastuidraw::ColorStopSequenceOnAtlas::
texel_location(void) const
{
  ColorStopSequenceOnAtlasPrivate *d;
  d = static_cast<ColorStopSequenceOnAtlasPrivate*>(m_d);
  return d->m_texel_location;
}

int
fastuidraw::ColorStopSequenceOnAtlas::
width(void) const
{
  ColorStopSequenceOnAtlasPrivate *d;
  d = static_cast<ColorStopSequenceOnAtlasPrivate*>(m_d);
  return d->m_width;
}

fastuidraw::reference_counted_ptr<const fastuidraw::ColorStopAtlas>
fastuidraw::ColorStopSequenceOnAtlas::
atlas(void) const
{
  ColorStopSequenceOnAtlasPrivate *d;
  d = static_cast<ColorStopSequenceOnAtlasPrivate*>(m_d);
  return d->m_atlas;
}
