/*!
 * \file image.cpp
 * \brief file image.cpp
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


#include <boost/multi_array.hpp>
#include <fastuidraw/image.hpp>
#include "private/util_private.hpp"


namespace
{

  /*
    Copies from src the rectangle:
      [source_x, source_x + dest_dim) x [source_y, source_y + dest_dim)
    to dest.

    If source_x is negative, then pads each line with the value
    from src at x=0.

    If source_y is negative, the source for those lines is
    taken from src at y=0.

    For pixel (x,y) with x < 0, takes the value
    from source at (0, y).

    For pixel (x,y) with y < 0, takes the value
    from source at (x, 0).

    For pixels (x,y) with x >= src_dims.x(), takes the value
    from source at (src_dims.x() - 1, y).

    For pixels (x,y) with y >= src_dims.y(), takes the value
    from source at (x, src_dims.y() - 1).

    \param dest_dim width and height of destination
    \param src_dims width and height of source
    \param source_x, source_y location within src from which to copy
    \param src: src pixels
    \param dest: destination pixels

    If all texel are the same value, returns true.
   */
  template<typename T, typename S>
  bool
  copy_sub_data(fastuidraw::c_array<T> dest,
                int dest_dim,
                fastuidraw::const_c_array<S> src,
                int source_x, int source_y,
                fastuidraw::ivec2 src_dims)
  {
    bool return_value(true);
    T first_value;

    assert(dest_dim > 0);
    assert(src_dims.x() > 0);
    assert(src_dims.y() > 0);

    first_value = src[std::max(source_x, 0) + std::max(source_y, 0) * src_dims.x()];

    for(int src_y = source_y, dst_y = 0; dst_y < dest_dim; ++src_y, ++dst_y)
      {
        fastuidraw::const_c_array<S> line_src;
        fastuidraw::c_array<T> line_dest;
        int dst_x, src_x, src_start;

        line_dest = dest.sub_array(dst_y * dest_dim, dest_dim);
        if(src_y < 0)
          {
            src_start = 0;
          }
        else if(src_y >= src_dims.y())
          {
            src_start = (src_dims.y() - 1) * src_dims.x();
          }
        else
          {
            src_start = src_y * src_dims.x();
          }
        line_src = src.sub_array(src_start, src_dims.x());

        for(src_x = source_x, dst_x = 0; src_x < 0; ++src_x, ++dst_x)
          {
            line_dest[dst_x] = line_src[0];
          }

        for(src_x = std::max(0, source_x);
            src_x < src_dims.x() && dst_x < dest_dim;
            ++src_x, ++dst_x)
          {
            line_dest[dst_x] = line_src[src_x];
            return_value = return_value && (line_dest[dst_x] == first_value);
          }

        for(;dst_x < dest_dim; ++dst_x)
          {
            line_dest[dst_x] = line_src[src_dims.x() - 1];
          }
      }

    return return_value;
  }

  fastuidraw::ivec2
  divide_up(fastuidraw::ivec2 numerator, int denominator)
  {
    fastuidraw::ivec2 R;
    R = numerator / denominator;
    if(numerator.x() % denominator != 0)
      {
        ++R.x();
      }
    if(numerator.y() % denominator != 0)
      {
        ++R.y();
      }
    return R;
  }

  int
  number_index_tiles_needed(fastuidraw::ivec2 number_color_tiles,
                            int index_tile_size)
  {
    int return_value = 1;
    fastuidraw::ivec2 tile_count = divide_up(number_color_tiles, index_tile_size);

    while(tile_count.x() > 1 || tile_count.y() > 1)
      {
        return_value += tile_count.x() * tile_count.y();
        tile_count = divide_up(tile_count, index_tile_size);
      }

    return return_value;
  }

  /* TODO: take into account for repeated tile colors. */
  bool
  enough_room_in_atlas(fastuidraw::ivec2 number_color_tiles,
                       fastuidraw::ImageAtlas *C,
                       int &total_index)
  {
    int total_color;

    total_color = number_color_tiles.x() * number_color_tiles.y();
    total_index = number_index_tiles_needed(number_color_tiles, C->index_tile_size());

    //std::cout << "Need " << total_color << " have: " << C->number_free_color_tiles() << "\n"
    //        << "Need " << total_index << " have: " << C->number_free_index_tiles() << "\n";

    return total_color <= C->number_free_color_tiles()
      && total_index <= C->number_free_index_tiles();
  }

  class BackingStorePrivate
  {
  public:
    BackingStorePrivate(fastuidraw::ivec3 whl, bool presizable):
      m_dimensions(whl),
      m_resizeable(presizable)
    {}

    BackingStorePrivate(int w, int h, int num_layers, bool presizable):
      m_dimensions(w, h, num_layers),
      m_resizeable(presizable)
    {}

    fastuidraw::ivec3 m_dimensions;
    bool m_resizeable;
  };

  class inited_bool
  {
  public:
    bool m_value;
    inited_bool(bool b = false):m_value(b) {}
  };

  class tile_allocator
  {
  public:
    tile_allocator(int tile_size, fastuidraw::ivec3 store_dimensions);

    ~tile_allocator();

    fastuidraw::ivec3
    allocate_tile(void);

    void
    delete_tile(fastuidraw::ivec3 v);

    int
    number_free(void) const;

    bool
    resize_to_fit(int num_tiles);

    int m_tile_size;
    fastuidraw::ivec3 m_next_tile;
    fastuidraw::ivec3 m_num_tiles;
    std::vector<fastuidraw::ivec3> m_free_tiles;
    int m_tile_count;

    #ifdef DEBUG
    boost::multi_array<inited_bool, 3> m_tile_allocated;
    #endif
  };

  class ImageAtlasPrivate
  {
  public:
    ImageAtlasPrivate(int pcolor_tile_size, int pindex_tile_size,
                      fastuidraw::AtlasColorBackingStoreBase::handle pcolor_store,
                      fastuidraw::AtlasIndexBackingStoreBase::handle pindex_store):
      m_color_store(pcolor_store),
      m_color_tiles(pcolor_tile_size, pcolor_store->dimensions()),
      m_index_store(pindex_store),
      m_index_tiles(pindex_tile_size, pindex_store->dimensions()),
      m_resizeable(m_color_store->resizeable() && m_index_store->resizeable())
    {}

    boost::mutex m_mutex;

    fastuidraw::AtlasColorBackingStoreBase::handle m_color_store;
    tile_allocator m_color_tiles;

    fastuidraw::AtlasIndexBackingStoreBase::handle m_index_store;
    tile_allocator m_index_tiles;

    bool m_resizeable;
  };

  class per_color_tile
  {
  public:
    per_color_tile(const fastuidraw::ivec3 t, bool b):
      m_tile(t), m_non_repeat_color(b)
    {}

    operator fastuidraw::ivec3() const
    {
      return m_tile;
    }

    fastuidraw::ivec3 m_tile;
    bool m_non_repeat_color;
  };

  class ImagePrivate
  {
  public:
    ImagePrivate(fastuidraw::ImageAtlas::handle patlas,
                 int w, int h,
                 fastuidraw::const_c_array<fastuidraw::u8vec4> image_data,
                 unsigned int pslack);

    ~ImagePrivate();

    void
    create_color_tiles(fastuidraw::const_c_array<fastuidraw::u8vec4> image_data);

    void
    create_index_tiles(void);

    template<typename T>
    fastuidraw::ivec2
    create_index_layer(fastuidraw::const_c_array<T> src_tiles,
                       fastuidraw::ivec2 src_dims, int slack,
                       std::list<std::vector<fastuidraw::ivec3> > &destination);

    fastuidraw::ImageAtlas::handle m_atlas;
    fastuidraw::ivec2 m_dimensions;
    unsigned int m_slack;
    fastuidraw::ivec2 m_num_color_tiles;

    std::map<fastuidraw::u8vec4, fastuidraw::ivec3> m_repeated_tiles;
    std::vector<per_color_tile> m_color_tiles;
    std::list<std::vector<fastuidraw::ivec3> > m_index_tiles;

    fastuidraw::ivec3 m_master_index_tile;
    fastuidraw::vec2 m_master_index_tile_dims;
    unsigned int m_number_index_lookups;
    float m_dimensions_index_divisor;
  };
}

/////////////////////////////////////////////
//ImagePrivate methods
ImagePrivate::
ImagePrivate(fastuidraw::ImageAtlas::handle patlas,
             int w, int h,
             fastuidraw::const_c_array<fastuidraw::u8vec4> image_data,
             unsigned int pslack):
  m_atlas(patlas),
  m_dimensions(w,h),
  m_slack(pslack)
{
  assert(m_dimensions.x() > 0);
  assert(m_dimensions.y() > 0);
  assert(m_atlas);

  create_color_tiles(image_data);
  create_index_tiles();
}

ImagePrivate::
~ImagePrivate()
{
  for(std::vector<per_color_tile>::const_iterator iter = m_color_tiles.begin(),
        end = m_color_tiles.end(); iter != end; ++iter)
    {
      if (iter->m_non_repeat_color)
        {
          m_atlas->delete_color_tile(iter->m_tile);
        }
    }

  for(std::map<fastuidraw::u8vec4, fastuidraw::ivec3>::const_iterator iter = m_repeated_tiles.begin(),
        end = m_repeated_tiles.end(); iter != end; ++iter)
    {
      m_atlas->delete_color_tile(iter->second);
    }

  for(std::list<std::vector<fastuidraw::ivec3> >::const_iterator viter = m_index_tiles.begin(),
        vend = m_index_tiles.end(); viter != vend; ++viter)
    {
      for(std::vector<fastuidraw::ivec3>::const_iterator iter = viter->begin(),
            end = viter->end(); iter != end; ++iter)
        {
          m_atlas->delete_index_tile(*iter);
        }
    }
}

void
ImagePrivate::
create_color_tiles(fastuidraw::const_c_array<fastuidraw::u8vec4> image_data)
{
  int tile_interior_size;
  int color_tile_size;

  color_tile_size = m_atlas->color_tile_size();
  tile_interior_size = color_tile_size - 2 * m_slack;
  m_num_color_tiles = divide_up(m_dimensions, tile_interior_size);
  m_master_index_tile_dims = fastuidraw::vec2(m_dimensions) / static_cast<float>(tile_interior_size);
  m_dimensions_index_divisor = static_cast<float>(tile_interior_size);

  unsigned int savings(0);
  std::vector<fastuidraw::u8vec4> tile_data(color_tile_size * color_tile_size);
  for(int ty = 0, source_y = -m_slack;
      ty < m_num_color_tiles.y();
      ++ty, source_y += tile_interior_size)
    {
      for(int tx = 0, source_x = -m_slack;
          tx < m_num_color_tiles.x();
          ++tx, source_x += tile_interior_size)
        {
          fastuidraw::ivec3 new_tile;
          bool all_same_color;

          all_same_color = copy_sub_data<fastuidraw::u8vec4>(make_c_array(tile_data), color_tile_size,
                                                            image_data, source_x, source_y,
                                                            m_dimensions);
          if(all_same_color)
            {
              std::map<fastuidraw::u8vec4, fastuidraw::ivec3>::iterator iter;
              fastuidraw::u8vec4 same_color_value;

              same_color_value = tile_data[0];
              iter = m_repeated_tiles.find(same_color_value);
              if(iter != m_repeated_tiles.end())
                {
                  new_tile = iter->second;
                  ++savings;
                }
              else
                {
                  /* we only need to call std::fill if the color tile
                     is only partially filled, but I am lazy and just
                     call it always.
                   */
                  std::fill(tile_data.begin(), tile_data.end(), same_color_value);
                  new_tile = m_atlas->add_color_tile(make_c_array(tile_data));
                  m_repeated_tiles[same_color_value] = new_tile;
                }
            }
          else
            {
              new_tile = m_atlas->add_color_tile(make_c_array(tile_data));
            }

          m_color_tiles.push_back(per_color_tile(new_tile, !all_same_color) );
        }
    }

  FASTUIDRAWunused(savings);
  //std::cout << "Saved " << savings << " out of "
  //        << m_num_color_tiles.x() * m_num_color_tiles.y()
  //        << " tiles from repeat color magicks\n";
}


/*
  returns the number of index tiles needed to
  store the created index data.
*/
template<typename T>
fastuidraw::ivec2
ImagePrivate::
create_index_layer(fastuidraw::const_c_array<T> src_tiles,
                   fastuidraw::ivec2 src_dims, int slack,
                   std::list<std::vector<fastuidraw::ivec3> > &destination)
{
  int index_tile_size;
  fastuidraw::ivec2 num_index_tiles;

  index_tile_size = m_atlas->index_tile_size();
  num_index_tiles = divide_up(src_dims, index_tile_size);

  destination.push_back(std::vector<fastuidraw::ivec3>());

  std::vector<fastuidraw::ivec3> vtile_data(index_tile_size * index_tile_size);
  fastuidraw::c_array<fastuidraw::ivec3> tile_data;
  tile_data = fastuidraw::make_c_array(vtile_data);

  for(int source_y = 0; source_y < src_dims.y(); source_y += index_tile_size)
    {
      for(int source_x = 0; source_x < src_dims.x(); source_x += index_tile_size)
        {
          fastuidraw::ivec3 new_tile;
          copy_sub_data<fastuidraw::ivec3, T>(tile_data,
                                             index_tile_size,
                                             src_tiles,
                                             source_x, source_y,
                                             src_dims);
          if(slack == -1)
            {
              new_tile = m_atlas->add_index_tile_index_data(tile_data);
            }
          else
            {
              new_tile = m_atlas->add_index_tile(tile_data, slack);
            }

          destination.back().push_back(new_tile);
        }
    }
  return num_index_tiles;
}

void
ImagePrivate::
create_index_tiles(void)
{
  fastuidraw::ivec2 num_index_tiles;
  int level(2);
  float findex_tile_size;

  findex_tile_size = static_cast<float>(m_atlas->index_tile_size());
  num_index_tiles = create_index_layer<per_color_tile>(fastuidraw::make_c_array(m_color_tiles),
                                                       m_num_color_tiles,
                                                       m_slack,
                                                       m_index_tiles);

  for(level = 2; num_index_tiles.x() > 1 || num_index_tiles.y() > 1; ++level)
    {
      num_index_tiles = create_index_layer<fastuidraw::ivec3>(fastuidraw::make_c_array(m_index_tiles.back()),
                                                             num_index_tiles,
                                                             -1, //indicates from index tile
                                                             m_index_tiles);
      m_dimensions_index_divisor *= findex_tile_size;
      m_master_index_tile_dims /= findex_tile_size;
    }

  assert(m_index_tiles.back().size() == 1);
  m_master_index_tile = m_index_tiles.back()[0];
  m_number_index_lookups = m_index_tiles.size();
}

///////////////////////////////////////////
// tile_allocator methods
tile_allocator::
tile_allocator(int tile_size, fastuidraw::ivec3 store_dimensions):
  m_tile_size(tile_size),
  m_next_tile(0, 0, 0),
  m_num_tiles(store_dimensions.x() / m_tile_size,
              store_dimensions.y() / m_tile_size,
              store_dimensions.z()),
  m_tile_count(0)
{
  assert(store_dimensions.x() % m_tile_size == 0);
  assert(store_dimensions.y() % m_tile_size == 0);

  #ifdef DEBUG
    {
      m_tile_allocated.resize(boost::extents[m_num_tiles.x()][m_num_tiles.y()][m_num_tiles.z()]);
    }
  #endif
}


tile_allocator::
~tile_allocator()
{
  assert(m_tile_count == 0);
}

fastuidraw::ivec3
tile_allocator::
allocate_tile(void)
{
  fastuidraw::ivec3 return_value;
  if(m_free_tiles.empty())
    {
      return_value = m_next_tile;
      ++m_next_tile.x();
      if(m_next_tile.x() == m_num_tiles.x())
        {
          m_next_tile.x() = 0;
          ++m_next_tile.y();
          if(m_next_tile.y() == m_num_tiles.y())
            {
              m_next_tile.y() = 0;
              ++m_next_tile.z();
              if(m_next_tile.z() == m_num_tiles.z())
                {
                  /* RAN out of color tiles */
                  assert(!"Color tile room exhausted");
                  return fastuidraw::ivec3(-1, -1,-1);
                }
            }
        }
    }
  else
    {
      return_value = m_free_tiles.back();
      m_free_tiles.pop_back();
    }

  #ifdef DEBUG
    {
      assert(!m_tile_allocated[return_value.x()][return_value.y()][return_value.z()].m_value);
      m_tile_allocated[return_value.x()][return_value.y()][return_value.z()] = true;
    }
  #endif

  ++m_tile_count;
  return return_value;
}

void
tile_allocator::
delete_tile(fastuidraw::ivec3 v)
{
  #ifdef DEBUG
    {
      assert(m_tile_allocated[v.x()][v.y()][v.z()].m_value);
      m_tile_allocated[v.x()][v.y()][v.z()] = false;
    }
  #endif

  --m_tile_count;
  m_free_tiles.push_back(v);
}

int
tile_allocator::
number_free(void) const
{
  return m_num_tiles.x() * m_num_tiles.y() * m_num_tiles.z() - m_tile_count;
}

bool
tile_allocator::
resize_to_fit(int num_tiles)
{
  if(num_tiles > number_free())
    {
      /* resize to fit the number of needed tiles,
         compute how many more tiles needed and from
         there compute how many more layers needed.
       */
      int needed_tiles, tiles_per_layer, needed_layers;
      needed_tiles = num_tiles - number_free();
      tiles_per_layer = m_num_tiles.x() * m_num_tiles.y();
      needed_layers = needed_tiles / tiles_per_layer;
      if(needed_tiles > needed_layers * tiles_per_layer)
        {
          ++needed_layers;
        }

      /* TODO:
          Should we resize at powers of 2, or just to what is
          needed?
       */
      m_num_tiles.z() += needed_layers;
      #ifdef DEBUG
        {
          m_tile_allocated.resize(boost::extents[m_num_tiles.x()][m_num_tiles.y()][m_num_tiles.z()]);
        }
      #endif

      return true;
    }
  else
    {
      return false;
    }
}

///////////////////////////////////////////
// fastuidraw::AtlasColorBackingStoreBase methods
fastuidraw::AtlasColorBackingStoreBase::
AtlasColorBackingStoreBase(ivec3 whl, bool presizable)
{
  m_d = FASTUIDRAWnew BackingStorePrivate(whl, presizable);
}

fastuidraw::AtlasColorBackingStoreBase::
AtlasColorBackingStoreBase(int w, int h, int num_layers, bool presizable)
{
  m_d = FASTUIDRAWnew BackingStorePrivate(w, h, num_layers, presizable);
}

fastuidraw::AtlasColorBackingStoreBase::
~AtlasColorBackingStoreBase()
{
  BackingStorePrivate *d;
  d = reinterpret_cast<BackingStorePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::ivec3
fastuidraw::AtlasColorBackingStoreBase::
dimensions(void) const
{
  BackingStorePrivate *d;
  d = reinterpret_cast<BackingStorePrivate*>(m_d);
  return d->m_dimensions;
}

bool
fastuidraw::AtlasColorBackingStoreBase::
resizeable(void) const
{
  BackingStorePrivate *d;
  d = reinterpret_cast<BackingStorePrivate*>(m_d);
  return d->m_resizeable;
}

void
fastuidraw::AtlasColorBackingStoreBase::
resize(int new_num_layers)
{
  BackingStorePrivate *d;

  d = reinterpret_cast<BackingStorePrivate*>(m_d);
  assert(d->m_resizeable);
  assert(new_num_layers > d->m_dimensions.z());
  resize_implement(new_num_layers);
  d->m_dimensions.z() = new_num_layers;
}

///////////////////////////////////////////////
// fastuidraw::AtlasIndexBackingStoreBase methods
fastuidraw::AtlasIndexBackingStoreBase::
AtlasIndexBackingStoreBase(ivec3 whl, bool presizable)
{
  m_d = FASTUIDRAWnew BackingStorePrivate(whl, presizable);
}

fastuidraw::AtlasIndexBackingStoreBase::
AtlasIndexBackingStoreBase(int w, int h, int l, bool presizable)
{
  m_d = FASTUIDRAWnew BackingStorePrivate(w, h, l, presizable);
}

fastuidraw::AtlasIndexBackingStoreBase::
~AtlasIndexBackingStoreBase()
{
  BackingStorePrivate *d;
  d = reinterpret_cast<BackingStorePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::ivec3
fastuidraw::AtlasIndexBackingStoreBase::
dimensions(void) const
{
  BackingStorePrivate *d;
  d = reinterpret_cast<BackingStorePrivate*>(m_d);
  return d->m_dimensions;
}

bool
fastuidraw::AtlasIndexBackingStoreBase::
resizeable(void) const
{
  BackingStorePrivate *d;
  d = reinterpret_cast<BackingStorePrivate*>(m_d);
  return d->m_resizeable;
}

void
fastuidraw::AtlasIndexBackingStoreBase::
resize(int new_num_layers)
{
  BackingStorePrivate *d;

  d = reinterpret_cast<BackingStorePrivate*>(m_d);
  assert(d->m_resizeable);
  assert(new_num_layers > d->m_dimensions.z());
  resize_implement(new_num_layers);
  d->m_dimensions.z() = new_num_layers;
}



//////////////////////////////
// fastuidraw::ImageAtlas methods
fastuidraw::ImageAtlas::
ImageAtlas(int pcolor_tile_size, int pindex_tile_size,
           AtlasColorBackingStoreBase::handle pcolor_store,
           AtlasIndexBackingStoreBase::handle pindex_store)
{
  m_d = FASTUIDRAWnew ImageAtlasPrivate(pcolor_tile_size, pindex_tile_size,
                                       pcolor_store, pindex_store);
}

fastuidraw::ImageAtlas::
~ImageAtlas()
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

int
fastuidraw::ImageAtlas::
index_tile_size(void) const
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  return d->m_index_tiles.m_tile_size;
}

int
fastuidraw::ImageAtlas::
color_tile_size(void) const
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  return d->m_color_tiles.m_tile_size;
}

int
fastuidraw::ImageAtlas::
number_free_index_tiles(void) const
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  autolock_mutex M(d->m_mutex);
  return d->m_index_tiles.number_free();
}

fastuidraw::ivec3
fastuidraw::ImageAtlas::
add_index_tile(fastuidraw::const_c_array<fastuidraw::ivec3> data, int slack)
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);

  ivec3 return_value;
  autolock_mutex M(d->m_mutex);

  /* TODO:
       have the idea of sub-index tiles (which are squares)but size is
       in each dimension, half of m_index_tiles.m_tile_size.
       The motivation is to reduce the overhead of small but
       non-tiny images. A single index tile would hold 4 such tiles
       and we could also recurse such.
   */
  return_value = d->m_index_tiles.allocate_tile();
  d->m_index_store->set_data(return_value.x() * d->m_index_tiles.m_tile_size,
                             return_value.y() * d->m_index_tiles.m_tile_size,
                             return_value.z(),
                             d->m_index_tiles.m_tile_size,
                             d->m_index_tiles.m_tile_size,
                             data,
                             slack,
                             d->m_color_store.get(),
                             d->m_color_tiles.m_tile_size);

  return return_value;
}

fastuidraw::ivec3
fastuidraw::ImageAtlas::
add_index_tile_index_data(fastuidraw::const_c_array<fastuidraw::ivec3> data)
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);

  ivec3 return_value;
  autolock_mutex M(d->m_mutex);

  return_value = d->m_index_tiles.allocate_tile();
  d->m_index_store->set_data(return_value.x() * d->m_index_tiles.m_tile_size,
                             return_value.y() * d->m_index_tiles.m_tile_size,
                             return_value.z(),
                             d->m_index_tiles.m_tile_size,
                             d->m_index_tiles.m_tile_size,
                             data);

  return return_value;
}

void
fastuidraw::ImageAtlas::
delete_index_tile(fastuidraw::ivec3 tile)
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  autolock_mutex M(d->m_mutex);
  d->m_index_tiles.delete_tile(tile);
}


int
fastuidraw::ImageAtlas::
number_free_color_tiles(void) const
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  autolock_mutex M(d->m_mutex);
  return d->m_color_tiles.number_free();
}


fastuidraw::ivec3
fastuidraw::ImageAtlas::
add_color_tile(fastuidraw::const_c_array<u8vec4> data)
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  ivec3 return_value;
  autolock_mutex M(d->m_mutex);

  return_value = d->m_color_tiles.allocate_tile();
  d->m_color_store->set_data(return_value.x() * d->m_color_tiles.m_tile_size,
                             return_value.y() * d->m_color_tiles.m_tile_size,
                             return_value.z(),
                             d->m_color_tiles.m_tile_size,
                             d->m_color_tiles.m_tile_size,
                             data);
  return return_value;
}

void
fastuidraw::ImageAtlas::
delete_color_tile(fastuidraw::ivec3 tile)
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  autolock_mutex M(d->m_mutex);
  d->m_color_tiles.delete_tile(tile);
}

void
fastuidraw::ImageAtlas::
flush(void) const
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  autolock_mutex M(d->m_mutex);
  d->m_index_store->flush();
  d->m_color_store->flush();
}

fastuidraw::AtlasColorBackingStoreBase::const_handle
fastuidraw::ImageAtlas::
color_store(void) const
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  return d->m_color_store;
}

fastuidraw::AtlasIndexBackingStoreBase::const_handle
fastuidraw::ImageAtlas::
index_store(void) const
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  return d->m_index_store;
}

bool
fastuidraw::ImageAtlas::
resizeable(void) const
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  return d->m_resizeable;
}

void
fastuidraw::ImageAtlas::
resize_to_fit(int num_color_tiles, int num_index_tiles)
{
  ImageAtlasPrivate *d;
  d = reinterpret_cast<ImageAtlasPrivate*>(m_d);
  assert(d->m_resizeable);
  if(d->m_color_tiles.resize_to_fit(num_color_tiles))
    {
      d->m_color_store->resize(d->m_color_tiles.m_num_tiles.z());
    }
  if(d->m_index_tiles.resize_to_fit(num_index_tiles))
    {
      d->m_index_store->resize(d->m_index_tiles.m_num_tiles.z());
    }
}


//////////////////////////////////////
// fastuidraw::Image methods
fastuidraw::Image::handle
fastuidraw::Image::
create(ImageAtlas::handle atlas, int w, int h,
       const_c_array<u8vec4> image_data, unsigned int pslack)
{
  int tile_interior_size;
  int color_tile_size;
  ivec2 num_color_tiles;
  int index_tiles;

  if(w <= 0 || h <= 0)
    {
      return handle();
    }

  color_tile_size = atlas->color_tile_size();
  tile_interior_size = color_tile_size - 2 * pslack;

  if(tile_interior_size <= 0)
    {
      return handle();
    }

  num_color_tiles = divide_up(ivec2(w, h), tile_interior_size);
  if(!enough_room_in_atlas(num_color_tiles, atlas.get(), index_tiles))
    {
      /*TODO:
         there actually might be enough room if we take into account
         the savings from repeated tiles. The correct thing is to
         delay this until iamge construction, check if it succeeded
         and if not then delete it and return an invalid handle.
       */
      if(atlas->resizeable())
        {
          atlas->resize_to_fit(num_color_tiles.x() * num_color_tiles.y(), index_tiles);
        }
      else
        {
          return handle();
        }
    }

  return FASTUIDRAWnew Image(atlas, w, h, image_data, pslack);
}

fastuidraw::Image::
Image(fastuidraw::ImageAtlas::handle patlas,
      int w, int h,
      fastuidraw::const_c_array<fastuidraw::u8vec4> image_data,
      unsigned int pslack)
{
  m_d = FASTUIDRAWnew ImagePrivate(patlas, w, h, image_data, pslack);
}

fastuidraw::Image::
~Image()
{
  ImagePrivate *d;
  d = reinterpret_cast<ImagePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}


unsigned int
fastuidraw::Image::
number_index_lookups(void) const
{
  ImagePrivate *d;
  d = reinterpret_cast<ImagePrivate*>(m_d);
  return d->m_number_index_lookups;
}

fastuidraw::ivec2
fastuidraw::Image::
dimensions(void) const
{
  ImagePrivate *d;
  d = reinterpret_cast<ImagePrivate*>(m_d);
  return d->m_dimensions;
}

unsigned int
fastuidraw::Image::
slack(void) const
{
  ImagePrivate *d;
  d = reinterpret_cast<ImagePrivate*>(m_d);
  return d->m_slack;
}

fastuidraw::ivec3
fastuidraw::Image::
master_index_tile(void) const
{
  ImagePrivate *d;
  d = reinterpret_cast<ImagePrivate*>(m_d);
  return d->m_master_index_tile;
}

fastuidraw::vec2
fastuidraw::Image::
master_index_tile_dims(void) const
{
  ImagePrivate *d;
  d = reinterpret_cast<ImagePrivate*>(m_d);
  return d->m_master_index_tile_dims;
}

float
fastuidraw::Image::
dimensions_index_divisor(void) const
{
  ImagePrivate *d;
  d = reinterpret_cast<ImagePrivate*>(m_d);
  return d->m_dimensions_index_divisor;
}

fastuidraw::ImageAtlas::const_handle
fastuidraw::Image::
atlas(void) const
{
  ImagePrivate *d;
  d = reinterpret_cast<ImagePrivate*>(m_d);
  return d->m_atlas;
}
