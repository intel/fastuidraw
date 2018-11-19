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


#include <list>
#include <map>
#include <mutex>
#include <fastuidraw/image.hpp>
#include "private/array3d.hpp"
#include "private/util_private.hpp"


namespace
{

  /*
   * Copies from src the rectangle:
   *  [source_x, source_x + dest_dim) x [source_y, source_y + dest_dim)
   * to dest.
   *
   * If source_x is negative, then pads each line with the value
   * from src at x=0.
   *
   * If source_y is negative, the source for those lines is
   * taken from src at y=0.
   *
   * For pixel (x,y) with x < 0, takes the value
   * from source at (0, y).
   *
   * For pixel (x,y) with y < 0, takes the value
   * from source at (x, 0).
   *
   * For pixels (x,y) with x >= src_dims.x(), takes the value
   * from source at (src_dims.x() - 1, y).
   *
   * For pixels (x,y) with y >= src_dims.y(), takes the value
   * from source at (x, src_dims.y() - 1).
   *
   * \param dest_dim width and height of destination
   * \param src_dims width and height of source
   * \param source_x, source_y location within src from which to copy
   * \param src: src pixels
   * \param dest: destination pixels
   *
   * If all texel are the same value, returns true.
   */
  template<typename T, typename S>
  void
  copy_sub_data(fastuidraw::c_array<T> dest,
                int w, int h,
                fastuidraw::c_array<const S> src,
                int source_x, int source_y,
                fastuidraw::ivec2 src_dims)
  {
    using namespace fastuidraw;

    FASTUIDRAWassert(w > 0);
    FASTUIDRAWassert(h > 0);
    FASTUIDRAWassert(src_dims.x() > 0);
    FASTUIDRAWassert(src_dims.y() > 0);

    for(int src_y = source_y, dst_y = 0; dst_y < h; ++src_y, ++dst_y)
      {
        c_array<const S> line_src;
        c_array<T> line_dest;
        int dst_x, src_x, src_start;

        line_dest = dest.sub_array(dst_y * w, w);
        if (src_y < 0)
          {
            src_start = 0;
          }
        else if (src_y >= src_dims.y())
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

        for(src_x = t_max(0, source_x);
            src_x < src_dims.x() && dst_x < w;
            ++src_x, ++dst_x)
          {
            line_dest[dst_x] = line_src[src_x];
          }

        for(;dst_x < w; ++dst_x)
          {
            line_dest[dst_x] = line_src[src_dims.x() - 1];
          }
      }
  }

  fastuidraw::ivec2
  divide_up(fastuidraw::ivec2 numerator, int denominator)
  {
    fastuidraw::ivec2 R;
    R = numerator / denominator;
    if (numerator.x() % denominator != 0)
      {
        ++R.x();
      }
    if (numerator.y() % denominator != 0)
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

    void
    delay_tile_freeing(void);

    void
    undelay_tile_freeing(void);

    int
    tile_size(void) const
    {
      return m_tile_size;
    }

    const fastuidraw::ivec3&
    num_tiles(void) const
    {
      return m_num_tiles;
    }

  private:
    void
    delete_tile_implement(fastuidraw::ivec3 v);

    int m_tile_size;
    fastuidraw::ivec3 m_next_tile;
    fastuidraw::ivec3 m_num_tiles;
    std::vector<fastuidraw::ivec3> m_free_tiles;
    int m_tile_count;

    int m_delay_tile_freeing_counter;
    std::vector<fastuidraw::ivec3> m_delayed_free_tiles;

    #ifdef FASTUIDRAW_DEBUG
    fastuidraw::array3d<inited_bool> m_tile_allocated;
    #endif
  };

  class ImageAtlasPrivate
  {
  public:
    ImageAtlasPrivate(int pcolor_tile_size, int pindex_tile_size,
                      fastuidraw::reference_counted_ptr<fastuidraw::AtlasColorBackingStoreBase> pcolor_store,
                      fastuidraw::reference_counted_ptr<fastuidraw::AtlasIndexBackingStoreBase> pindex_store):
      m_color_store(pcolor_store),
      m_color_tiles(pcolor_tile_size, pcolor_store->dimensions()),
      m_index_store(pindex_store),
      m_index_tiles(pindex_tile_size, pindex_store->dimensions()),
      m_resizeable(m_color_store->resizeable() && m_index_store->resizeable())
    {}

    std::mutex m_mutex;

    fastuidraw::reference_counted_ptr<fastuidraw::AtlasColorBackingStoreBase> m_color_store;
    tile_allocator m_color_tiles;

    fastuidraw::reference_counted_ptr<fastuidraw::AtlasIndexBackingStoreBase> m_index_store;
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
    ImagePrivate(fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> patlas,
                 int w, int h,
                 const fastuidraw::ImageSourceBase &image_data,
                 unsigned int pslack);

    ImagePrivate(int w, int h, unsigned int m, fastuidraw::Image::type_t t, uint64_t handle):
      m_dimensions(w, h),
      m_num_mipmap_levels(m),
      m_type(t),
      m_slack(~0u),
      m_num_color_tiles(-1, -1),
      m_master_index_tile(-1, -1, -1),
      m_master_index_tile_dims(-1.0f, -1.0f),
      m_number_index_lookups(0),
      m_dimensions_index_divisor(-1.0f),
      m_bindless_handle(handle)
    {}

    ~ImagePrivate();

    void
    create_color_tiles(const fastuidraw::ImageSourceBase &image_data);

    void
    create_index_tiles(void);

    template<typename T>
    fastuidraw::ivec2
    create_index_layer(fastuidraw::c_array<const T> src_tiles,
                       fastuidraw::ivec2 src_dims, int slack,
                       std::list<std::vector<fastuidraw::ivec3> > &destination);

    fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> m_atlas;
    fastuidraw::ivec2 m_dimensions;
    int m_num_mipmap_levels;

    enum fastuidraw::Image::type_t m_type;

    /* Data for when the image has type on_atlas */
    unsigned int m_slack;
    fastuidraw::ivec2 m_num_color_tiles;
    std::map<fastuidraw::u8vec4, fastuidraw::ivec3> m_repeated_tiles;
    std::vector<per_color_tile> m_color_tiles;
    std::list<std::vector<fastuidraw::ivec3> > m_index_tiles;
    fastuidraw::ivec3 m_master_index_tile;
    fastuidraw::vec2 m_master_index_tile_dims;
    unsigned int m_number_index_lookups;
    float m_dimensions_index_divisor;

    /* data for when image has different type than on_atlas */
    uint64_t m_bindless_handle;
  };
}

/////////////////////////////////////////////
//ImagePrivate methods
ImagePrivate::
ImagePrivate(fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> patlas,
             int w, int h,
             const fastuidraw::ImageSourceBase &image_data,
             unsigned int pslack):
  m_atlas(patlas),
  m_dimensions(w, h),
  m_num_mipmap_levels(image_data.num_mipmap_levels()),
  m_type(fastuidraw::Image::on_atlas),
  m_slack(pslack),
  m_bindless_handle(-1)
{
  FASTUIDRAWassert(m_dimensions.x() > 0);
  FASTUIDRAWassert(m_dimensions.y() > 0);
  FASTUIDRAWassert(m_atlas);

  create_color_tiles(image_data);
  create_index_tiles();
}

ImagePrivate::
~ImagePrivate()
{
  for(const per_color_tile &C : m_color_tiles)
    {
      if (C.m_non_repeat_color)
        {
          m_atlas->delete_color_tile(C.m_tile);
        }
    }

  for(const auto &C : m_repeated_tiles)
    {
      m_atlas->delete_color_tile(C.second);
    }

  for(const auto &tile_array: m_index_tiles)
    {
      for(const fastuidraw::ivec3 &index_tile : tile_array)
        {
          m_atlas->delete_index_tile(index_tile);
        }
    }
}

void
ImagePrivate::
create_color_tiles(const fastuidraw::ImageSourceBase &image_data)
{
  int tile_interior_size;
  int color_tile_size;

  color_tile_size = m_atlas->color_tile_size();
  tile_interior_size = color_tile_size - 2 * m_slack;
  m_num_color_tiles = divide_up(m_dimensions, tile_interior_size);
  m_master_index_tile_dims = fastuidraw::vec2(m_dimensions) / static_cast<float>(tile_interior_size);
  m_dimensions_index_divisor = static_cast<float>(tile_interior_size);

  unsigned int savings(0);
  for(int ty = 0, source_y = -m_slack;
      ty < m_num_color_tiles.y();
      ++ty, source_y += tile_interior_size)
    {
      for(int tx = 0, source_x = -m_slack;
          tx < m_num_color_tiles.x();
          ++tx, source_x += tile_interior_size)
        {
          fastuidraw::ivec3 new_tile;
          fastuidraw::ivec2 src_xy(source_x, source_y);
          bool all_same_color;
          fastuidraw::u8vec4 same_color_value;

          all_same_color = image_data.all_same_color(src_xy, color_tile_size, &same_color_value);

          if (all_same_color)
            {
              std::map<fastuidraw::u8vec4, fastuidraw::ivec3>::iterator iter;

              iter = m_repeated_tiles.find(same_color_value);
              if (iter != m_repeated_tiles.end())
                {
                  new_tile = iter->second;
                  ++savings;
                }
              else
                {
                  new_tile = m_atlas->add_color_tile(same_color_value);
                  m_repeated_tiles[same_color_value] = new_tile;
                }
            }
          else
            {
              new_tile = m_atlas->add_color_tile(src_xy, image_data);
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
 * returns the number of index tiles needed to
 * store the created index data.
 */
template<typename T>
fastuidraw::ivec2
ImagePrivate::
create_index_layer(fastuidraw::c_array<const T> src_tiles,
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
          copy_sub_data<fastuidraw::ivec3, T>(tile_data, index_tile_size, index_tile_size,
                                              src_tiles, source_x, source_y,
                                              src_dims);
          if (slack == -1)
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

  FASTUIDRAWassert(m_index_tiles.back().size() == 1);
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
  m_tile_count(0),
  m_delay_tile_freeing_counter(0)
#ifdef FASTUIDRAW_DEBUG
  ,
  m_tile_allocated(m_num_tiles.x(), m_num_tiles.y(), m_num_tiles.z())
#endif
{
  FASTUIDRAWassert(store_dimensions.x() % m_tile_size == 0);
  FASTUIDRAWassert(store_dimensions.y() % m_tile_size == 0);
}

tile_allocator::
~tile_allocator()
{
  FASTUIDRAWassert(m_delay_tile_freeing_counter == 0);
  FASTUIDRAWassert(m_tile_count == 0);
}

fastuidraw::ivec3
tile_allocator::
allocate_tile(void)
{
  fastuidraw::ivec3 return_value;
  if (m_free_tiles.empty())
    {
      if (m_next_tile.x() < m_num_tiles.x() || m_next_tile.y() < m_num_tiles.y() || m_next_tile.z() < m_num_tiles.z())
        {
          return_value = m_next_tile;
          ++m_next_tile.x();
          if (m_next_tile.x() == m_num_tiles.x())
            {
              m_next_tile.x() = 0;
              ++m_next_tile.y();
              if (m_next_tile.y() == m_num_tiles.y())
                {
                  m_next_tile.y() = 0;
                  ++m_next_tile.z();
                }
            }
        }
      else
        {
          FASTUIDRAWassert(!"Color tile room exhausted");
          return fastuidraw::ivec3(-1, -1,-1);
        }
    }
  else
    {
      return_value = m_free_tiles.back();
      m_free_tiles.pop_back();
    }

  #ifdef FASTUIDRAW_DEBUG
    {
      FASTUIDRAWassert(!m_tile_allocated(return_value.x(), return_value.y(), return_value.z()).m_value);
      m_tile_allocated(return_value.x(), return_value.y(), return_value.z()) = true;
    }
  #endif

  ++m_tile_count;
  return return_value;
}

void
tile_allocator::
delay_tile_freeing(void)
{
  ++m_delay_tile_freeing_counter;
}

void
tile_allocator::
undelay_tile_freeing(void)
{
  FASTUIDRAWassert(m_delay_tile_freeing_counter >= 1);
  --m_delay_tile_freeing_counter;
  if (m_delay_tile_freeing_counter == 0)
    {
      for(unsigned int i = 0, endi = m_delayed_free_tiles.size(); i < endi; ++i)
        {
          delete_tile_implement(m_delayed_free_tiles[i]);
        }
      m_delayed_free_tiles.clear();
    }
}

void
tile_allocator::
delete_tile(fastuidraw::ivec3 v)
{
  if (m_delay_tile_freeing_counter == 0)
    {
      delete_tile_implement(v);
    }
  else
    {
      m_delayed_free_tiles.push_back(v);
    }
}

void
tile_allocator::
delete_tile_implement(fastuidraw::ivec3 v)
{
  FASTUIDRAWassert(m_delay_tile_freeing_counter == 0);
  #ifdef FASTUIDRAW_DEBUG
    {
      FASTUIDRAWassert(m_tile_allocated(v.x(), v.y(), v.z()).m_value);
      m_tile_allocated(v.x(), v.y(), v.z()) = false;
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
  if (num_tiles > number_free())
    {
      /* resize to fit the number of needed tiles,
       * compute how many more tiles needed and from
       * there compute how many more layers needed.
       */
      int needed_tiles, tiles_per_layer, needed_layers;
      needed_tiles = num_tiles - number_free();
      tiles_per_layer = m_num_tiles.x() * m_num_tiles.y();
      needed_layers = needed_tiles / tiles_per_layer;
      if (needed_tiles > needed_layers * tiles_per_layer)
        {
          ++needed_layers;
        }

      /* TODO:
       *  Should we resize at powers of 2, or just to what is
       *  needed?
       */
      m_num_tiles.z() += needed_layers;
      #ifdef FASTUIDRAW_DEBUG
        {
          m_tile_allocated.resize(m_num_tiles.x(), m_num_tiles.y(), m_num_tiles.z());
        }
      #endif

      return true;
    }
  else
    {
      return false;
    }
}



////////////////////////////////////////
// fastuidraw::ImageSourceCArray methods
fastuidraw::ImageSourceCArray::
ImageSourceCArray(uvec2 dimensions,
                  c_array<const c_array<const u8vec4> > pdata):
  m_dimensions(dimensions),
  m_data(pdata)
{}

bool
fastuidraw::ImageSourceCArray::
all_same_color(ivec2 location, int square_size, u8vec4 *dst) const
{
  location.x() = t_max(location.x(), 0);
  location.y() = t_max(location.y(), 0);

  location.x() = t_min(int(m_dimensions.x()) - 1, location.x());
  location.y() = t_min(int(m_dimensions.y()) - 1, location.y());

  square_size = t_min(int(m_dimensions.x()) - location.x(), square_size);
  square_size = t_min(int(m_dimensions.y()) - location.y(), square_size);

  *dst = m_data[0][location.x() + location.y() * m_dimensions.x()];
  for (int y = 0, sy = location.y(); y < square_size; ++y, ++sy)
    {
      for (int x = 0, sx = location.x(); x < square_size; ++x, ++sx)
        {
          if (*dst != m_data[0][sx + sy * m_dimensions.x()])
            {
              return false;
            }
        }
    }
  return true;
}

unsigned int
fastuidraw::ImageSourceCArray::
num_mipmap_levels(void) const
{
  return m_data.size();
}

void
fastuidraw::ImageSourceCArray::
fetch_texels(unsigned int mipmap_level, ivec2 location,
             unsigned int w, unsigned int h,
             c_array<u8vec4> dst) const
{
  if (mipmap_level >= m_data.size())
    {
      std::fill(dst.begin(), dst.end(), u8vec4(255u, 255u, 0u, 255u));
    }
  else
    {
      copy_sub_data(dst, w, h,
                    m_data[mipmap_level],
                    location.x(), location.y(),
                    ivec2(m_dimensions.x() >> mipmap_level,
                          m_dimensions.y() >> mipmap_level));
    }
}

//////////////////////////////////////////////////
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
  d = static_cast<BackingStorePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::ivec3
fastuidraw::AtlasColorBackingStoreBase::
dimensions(void) const
{
  BackingStorePrivate *d;
  d = static_cast<BackingStorePrivate*>(m_d);
  return d->m_dimensions;
}

bool
fastuidraw::AtlasColorBackingStoreBase::
resizeable(void) const
{
  BackingStorePrivate *d;
  d = static_cast<BackingStorePrivate*>(m_d);
  return d->m_resizeable;
}

void
fastuidraw::AtlasColorBackingStoreBase::
resize(int new_num_layers)
{
  BackingStorePrivate *d;

  d = static_cast<BackingStorePrivate*>(m_d);
  FASTUIDRAWassert(d->m_resizeable);
  FASTUIDRAWassert(new_num_layers > d->m_dimensions.z());
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
  d = static_cast<BackingStorePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::ivec3
fastuidraw::AtlasIndexBackingStoreBase::
dimensions(void) const
{
  BackingStorePrivate *d;
  d = static_cast<BackingStorePrivate*>(m_d);
  return d->m_dimensions;
}

bool
fastuidraw::AtlasIndexBackingStoreBase::
resizeable(void) const
{
  BackingStorePrivate *d;
  d = static_cast<BackingStorePrivate*>(m_d);
  return d->m_resizeable;
}

void
fastuidraw::AtlasIndexBackingStoreBase::
resize(int new_num_layers)
{
  BackingStorePrivate *d;

  d = static_cast<BackingStorePrivate*>(m_d);
  FASTUIDRAWassert(d->m_resizeable);
  FASTUIDRAWassert(new_num_layers > d->m_dimensions.z());
  resize_implement(new_num_layers);
  d->m_dimensions.z() = new_num_layers;
}



//////////////////////////////
// fastuidraw::ImageAtlas methods
fastuidraw::ImageAtlas::
ImageAtlas(int pcolor_tile_size, int pindex_tile_size,
           fastuidraw::reference_counted_ptr<AtlasColorBackingStoreBase> pcolor_store,
           fastuidraw::reference_counted_ptr<AtlasIndexBackingStoreBase> pindex_store)
{
  m_d = FASTUIDRAWnew ImageAtlasPrivate(pcolor_tile_size, pindex_tile_size,
                                       pcolor_store, pindex_store);
}

fastuidraw::ImageAtlas::
~ImageAtlas()
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::ImageAtlas::
delay_tile_freeing(void)
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);

  std::lock_guard<std::mutex> M(d->m_mutex);
  d->m_color_tiles.delay_tile_freeing();
  d->m_index_tiles.delay_tile_freeing();
}

void
fastuidraw::ImageAtlas::
undelay_tile_freeing(void)
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);

  std::lock_guard<std::mutex> M(d->m_mutex);
  d->m_color_tiles.undelay_tile_freeing();
  d->m_index_tiles.undelay_tile_freeing();
}

int
fastuidraw::ImageAtlas::
index_tile_size(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  return d->m_index_tiles.tile_size();
}

int
fastuidraw::ImageAtlas::
color_tile_size(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  return d->m_color_tiles.tile_size();
}

int
fastuidraw::ImageAtlas::
number_free_index_tiles(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  std::lock_guard<std::mutex> M(d->m_mutex);
  return d->m_index_tiles.number_free();
}

fastuidraw::ivec3
fastuidraw::ImageAtlas::
add_index_tile(fastuidraw::c_array<const fastuidraw::ivec3> data, int slack)
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);

  ivec3 return_value;
  std::lock_guard<std::mutex> M(d->m_mutex);

  /* TODO:
   *   have the idea of sub-index tiles (which are squares)but size is
   *   in each dimension, half of m_index_tiles.tile_size().
   *   The motivation is to reduce the overhead of small but
   *   non-tiny images. A single index tile would hold 4 such tiles
   *   and we could also recurse such.
   */
  return_value = d->m_index_tiles.allocate_tile();
  d->m_index_store->set_data(return_value.x() * d->m_index_tiles.tile_size(),
                             return_value.y() * d->m_index_tiles.tile_size(),
                             return_value.z(),
                             d->m_index_tiles.tile_size(),
                             d->m_index_tiles.tile_size(),
                             data,
                             slack,
                             d->m_color_store.get(),
                             d->m_color_tiles.tile_size());

  return return_value;
}

fastuidraw::ivec3
fastuidraw::ImageAtlas::
add_index_tile_index_data(fastuidraw::c_array<const fastuidraw::ivec3> data)
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);

  ivec3 return_value;
  std::lock_guard<std::mutex> M(d->m_mutex);

  return_value = d->m_index_tiles.allocate_tile();
  d->m_index_store->set_data(return_value.x() * d->m_index_tiles.tile_size(),
                             return_value.y() * d->m_index_tiles.tile_size(),
                             return_value.z(),
                             d->m_index_tiles.tile_size(),
                             d->m_index_tiles.tile_size(),
                             data);

  return return_value;
}

void
fastuidraw::ImageAtlas::
delete_index_tile(fastuidraw::ivec3 tile)
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  std::lock_guard<std::mutex> M(d->m_mutex);
  d->m_index_tiles.delete_tile(tile);
}

int
fastuidraw::ImageAtlas::
number_free_color_tiles(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  std::lock_guard<std::mutex> M(d->m_mutex);
  return d->m_color_tiles.number_free();
}

fastuidraw::ivec3
fastuidraw::ImageAtlas::
add_color_tile(u8vec4 color_data)
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);

  ivec3 return_value;
  std::lock_guard<std::mutex> M(d->m_mutex);
  ivec2 dst_xy;
  int sz;

  return_value = d->m_color_tiles.allocate_tile();
  dst_xy.x() = return_value.x() * d->m_color_tiles.tile_size();
  dst_xy.y() = return_value.y() * d->m_color_tiles.tile_size();
  sz = d->m_color_tiles.tile_size();

  for (int level = 0; sz > 0; ++level, sz /= 2, dst_xy /= 2)
    {
      d->m_color_store->set_data(level, dst_xy, return_value.z(),
                                 sz, color_data);
    }

  return return_value;
}

fastuidraw::ivec3
fastuidraw::ImageAtlas::
add_color_tile(ivec2 src_xy, const ImageSourceBase &image_data)
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  ivec3 return_value;
  std::lock_guard<std::mutex> M(d->m_mutex);
  ivec2 dst_xy;
  int sz, level, end_level;

  return_value = d->m_color_tiles.allocate_tile();
  dst_xy.x() = return_value.x() * d->m_color_tiles.tile_size();
  dst_xy.y() = return_value.y() * d->m_color_tiles.tile_size();
  sz = d->m_color_tiles.tile_size();
  end_level = image_data.num_mipmap_levels();

  for (level = 0; level < end_level && sz > 0; ++level, sz /= 2, dst_xy /= 2, src_xy /= 2)
    {
      d->m_color_store->set_data(level, dst_xy, return_value.z(), src_xy, sz, image_data);
    }

  for (; sz > 0; ++level, sz /= 2, dst_xy /= 2, src_xy /= 2, sz /= 2)
    {
      d->m_color_store->set_data(level, dst_xy, return_value.z(), sz,
                                 u8vec4(255u, 255u, 0u, 255u));
    }

  return return_value;
}

void
fastuidraw::ImageAtlas::
delete_color_tile(fastuidraw::ivec3 tile)
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  std::lock_guard<std::mutex> M(d->m_mutex);
  d->m_color_tiles.delete_tile(tile);
}

void
fastuidraw::ImageAtlas::
flush(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  std::lock_guard<std::mutex> M(d->m_mutex);
  d->m_index_store->flush();
  d->m_color_store->flush();
}

fastuidraw::reference_counted_ptr<const fastuidraw::AtlasColorBackingStoreBase>
fastuidraw::ImageAtlas::
color_store(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  return d->m_color_store;
}

fastuidraw::reference_counted_ptr<const fastuidraw::AtlasIndexBackingStoreBase>
fastuidraw::ImageAtlas::
index_store(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  return d->m_index_store;
}

bool
fastuidraw::ImageAtlas::
resizeable(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  return d->m_resizeable;
}

void
fastuidraw::ImageAtlas::
resize_to_fit(int num_color_tiles, int num_index_tiles)
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  FASTUIDRAWassert(d->m_resizeable);
  if (d->m_color_tiles.resize_to_fit(num_color_tiles))
    {
      d->m_color_store->resize(d->m_color_tiles.num_tiles().z());
    }
  if (d->m_index_tiles.resize_to_fit(num_index_tiles))
    {
      d->m_index_store->resize(d->m_index_tiles.num_tiles().z());
    }
}


//////////////////////////////////////
// fastuidraw::Image methods
fastuidraw::reference_counted_ptr<fastuidraw::Image>
fastuidraw::Image::
create(reference_counted_ptr<ImageAtlas> atlas, int w, int h,
       c_array<const u8vec4> image_data, unsigned int pslack)
{
  if (w <= 0 || h <= 0)
    {
      return reference_counted_ptr<Image>();
    }

  c_array<const c_array<const u8vec4> > data(&image_data, 1);
  return create(atlas, w, h, ImageSourceCArray(uvec2(w, h), data), pslack);
}

fastuidraw::reference_counted_ptr<fastuidraw::Image>
fastuidraw::Image::
create(reference_counted_ptr<ImageAtlas> atlas, int w, int h,
       const ImageSourceBase &image_data, unsigned int pslack)
{
  int tile_interior_size;
  int color_tile_size;
  ivec2 num_color_tiles;
  int index_tiles;

  if (w <= 0 || h <= 0)
    {
      return reference_counted_ptr<Image>();
    }

  color_tile_size = atlas->color_tile_size();
  tile_interior_size = color_tile_size - 2 * pslack;

  if (tile_interior_size <= 0)
    {
      return reference_counted_ptr<Image>();
    }

  num_color_tiles = divide_up(ivec2(w, h), tile_interior_size);
  if (!enough_room_in_atlas(num_color_tiles, atlas.get(), index_tiles))
    {
      /*TODO:
       * there actually might be enough room if we take into account
       * the savings from repeated tiles. The correct thing is to
       * delay this until iamge construction, check if it succeeded
       * and if not then delete it and return an invalid handle.
       */
      if (atlas->resizeable())
        {
          atlas->resize_to_fit(num_color_tiles.x() * num_color_tiles.y(), index_tiles);
        }
      else
        {
          return reference_counted_ptr<Image>();
        }
    }

  return FASTUIDRAWnew Image(atlas, w, h, image_data, pslack);
}

fastuidraw::reference_counted_ptr<fastuidraw::Image>
fastuidraw::Image::
create_bindless(int w, int h, unsigned int m, enum type_t type, uint64_t handle)
{
  Image *p(nullptr);
  if (type != on_atlas)
    {
      p = FASTUIDRAWnew Image(w, h, m, type, handle);
    }
  return p;
}

fastuidraw::Image::
Image(int w, int h, unsigned int m, enum type_t type, uint64_t handle)
{
  m_d = FASTUIDRAWnew ImagePrivate(w, h, m, type, handle);
}

fastuidraw::Image::
Image(reference_counted_ptr<ImageAtlas> patlas,
      int w, int h,
      const ImageSourceBase &image_data,
      unsigned int pslack)
{
  m_d = FASTUIDRAWnew ImagePrivate(patlas, w, h, image_data, pslack);
}

fastuidraw::Image::
~Image()
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}


unsigned int
fastuidraw::Image::
number_index_lookups(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_number_index_lookups;
}

fastuidraw::ivec2
fastuidraw::Image::
dimensions(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_dimensions;
}

unsigned int
fastuidraw::Image::
number_mipmap_levels(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_num_mipmap_levels;
}

unsigned int
fastuidraw::Image::
slack(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_slack;
}

fastuidraw::ivec3
fastuidraw::Image::
master_index_tile(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_master_index_tile;
}

fastuidraw::vec2
fastuidraw::Image::
master_index_tile_dims(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_master_index_tile_dims;
}

float
fastuidraw::Image::
dimensions_index_divisor(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_dimensions_index_divisor;
}

enum fastuidraw::Image::type_t
fastuidraw::Image::
type(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_type;
}

uint64_t
fastuidraw::Image::
bindless_handle(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_bindless_handle;
}

const fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas>&
fastuidraw::Image::
atlas(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_atlas;
}
