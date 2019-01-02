/*!
 * \file painter_packer.cpp
 * \brief file painter_packer.cpp
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
#include <list>
#include <cstring>

#include <fastuidraw/painter/backend/painter_header.hpp>
#include "painter_packer.hpp"
#include "../../../private/util_private.hpp"

namespace
{
  class PainterShaderGroupValues
  {
  public:
    uint32_t m_composite_group;
    uint32_t m_item_group;
    uint32_t m_blend_group;
    uint32_t m_brush;
    fastuidraw::BlendMode m_composite_mode;
  };

  template<typename T>
  const T&
  fetch_value(const fastuidraw::PainterData::value<T> &obj)
  {
    if (obj.m_packed_value)
      {
        return obj.m_packed_value.value();
      }

    if (obj.m_value != nullptr)
      {
        return *obj.m_value;
      }

    static T default_value;
    return default_value;
  }

  class DataCallBackPrivate
  {
  public:
    DataCallBackPrivate(void):
      m_list(nullptr)
    {}

    std::list<fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> >::iterator m_iterator;
    std::list<fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> > *m_list;
  };

  class PainterShaderGroupPrivate:
    public fastuidraw::PainterShaderGroup,
    public PainterShaderGroupValues
  {
  public:
    PainterShaderGroupPrivate(void)
    {}

    PainterShaderGroupPrivate(const PainterShaderGroupPrivate &obj):
      PainterShaderGroupValues(obj)
    {}

    void
    operator=(const PainterShaderGroupPrivate &obj)
    {
      PainterShaderGroupValues::operator=(obj);
    }
  };

  /* QUESTION
   *  - does the reference count to a pool need to be thread safe?
   */
  class PoolBase:public fastuidraw::reference_counted<PoolBase>::non_concurrent
  {
  public:
    enum
      {
        pool_size = 1024
      };

    PoolBase(void):
      m_free_slots_back(pool_size - 1)
    {
      for(unsigned int i = 0; i < pool_size; ++i)
        {
          m_free_slots[i] = pool_size - 1 - i;
        }
    }

    ~PoolBase()
    {
      FASTUIDRAWassert(m_free_slots_back == pool_size - 1);
    }

    int
    aquire_slot(void)
    {
      /* Should we make this thread safe? We can make it
       * thread safe by using atomic operation on m_free_slots_back.
       * The following code would make it thread safe.
       *
       * return_value = m_free_slots_back.fetch_sub(1, std::memory_order_aquire)
       * if (return_value < 0)
       *   {
       *      m_free_slots_back.fetch_add(1, std::memory_order_aquire);
       *   }
       * return return_value;
       */
      int return_value(-1);

      if (m_free_slots_back >= 0)
        {
          return_value = m_free_slots[m_free_slots_back];
          --m_free_slots_back;
        }
      return return_value;
    }

    void
    release_slot(int v)
    {
      /* Should we make this thread safe? We can make it
       * thread safe by using atomic operation on m_free_slots_back.
       * The following code would make it thread safe.
       *
       * int S;
       * S = m_free_slots_back.fetch_add(1, std::memory_order_release);
       * std::atomic_thread_fence(std::memory_order_acquire);
       * FASTUIDRAWassert(S < pool_size);
       * m_free_slots[S] = v;
       */
      FASTUIDRAWassert(v >= 0);
      FASTUIDRAWassert(v < pool_size);

      ++m_free_slots_back;

      FASTUIDRAWassert(m_free_slots_back < pool_size);
      m_free_slots[m_free_slots_back] = v;
    }

  private:
    int m_free_slots_back;
    fastuidraw::vecN<int, pool_size> m_free_slots;
  };

  class EntryBase
  {
  public:

    EntryBase(void):
      m_raw_value(nullptr),
      m_pool_slot(-1)
    {}

    void
    aquire(void)
    {
      FASTUIDRAWassert(m_pool);
      FASTUIDRAWassert(m_pool_slot >= 0);
      m_count.add_reference();
    }

    void
    release(void)
    {
      FASTUIDRAWassert(m_pool);
      FASTUIDRAWassert(m_pool_slot >= 0);
      if (m_count.remove_reference())
        {
          m_pool->release_slot(m_pool_slot);
          m_pool_slot = -1;
          m_pool = nullptr;
        }
    }

    const void*
    raw_value(void)
    {
      return m_raw_value;
    }

    /* To what painter and where in data store buffer
     * already packed into PainterDraw::m_store
     */
    const fastuidraw::PainterPacker *m_painter;
    std::vector<fastuidraw::generic_data> m_data;
    int m_begin_id;
    unsigned int m_draw_command_id, m_offset;

  protected:
    /* pointer to raw state member held
     * by derived class
     */
    const void *m_raw_value;

    /* location in pool and what pool
     */
    fastuidraw::reference_counted_ptr<PoolBase> m_pool;
    int m_pool_slot;

  private:
    /* Entry reference count is not thread safe because
     * the objects themselves are not.
     */
    fastuidraw::reference_count_non_concurrent m_count;
  };

  template<typename T>
  class Entry:public EntryBase
  {
  public:
    Entry(void)
    {
      m_raw_value = &m_state;
    }

    void
    set(const T &st, PoolBase *p, int slot)
    {
      FASTUIDRAWassert(p);
      FASTUIDRAWassert(slot >= 0);

      m_pool = p;
      m_state = st;
      m_pool_slot = slot;

      this->m_begin_id = -1;
      this->m_draw_command_id = 0;
      this->m_offset = 0;
      this->m_painter = nullptr;
      this->m_data.resize(m_state.data_size());
      m_state.pack_data(fastuidraw::make_c_array(this->m_data));
    }

    T m_state;
  };

  template<typename T>
  class Pool:public PoolBase
  {
  public:
    /* Returning nullptr indicates no free entries left in the pool
     */
    Entry<T>*
    allocate(const T &st)
    {
      Entry<T> *return_value(nullptr);
      int slot;

      slot = this->aquire_slot();
      if (slot >= 0)
        {
          return_value = &m_data[slot];
          return_value->set(st, this, slot);
        }
      return return_value;
    }

  private:
    fastuidraw::vecN<Entry<T>, PoolBase::pool_size> m_data;
  };

  template<typename T>
  class PoolSet:fastuidraw::noncopyable
  {
  public:
    PoolSet(void)
    {
      m_pools.push_back(FASTUIDRAWnew Pool<T>());
    }

    Entry<T>*
    allocate(const T &st)
    {
      Entry<T> *return_value;

      return_value = m_pools.back()->allocate(st);
      if (!return_value)
        {
          m_pools.push_back(FASTUIDRAWnew Pool<T>());
          return_value = m_pools.back()->allocate(st);
        }

      FASTUIDRAWassert(return_value);
      return return_value;
    }

  private:
    std::vector<fastuidraw::reference_counted_ptr<Pool<T> > > m_pools;
  };

  class PainterPackedValuePoolPrivate
  {
  public:
    PoolSet<fastuidraw::PainterBrush> m_brush_pool;
    PoolSet<fastuidraw::PainterClipEquations> m_clip_equations_pool;
    PoolSet<fastuidraw::PainterItemMatrix> m_item_matrix_pool;
    PoolSet<fastuidraw::PainterItemShaderData> m_item_shader_data_pool;
    PoolSet<fastuidraw::PainterCompositeShaderData> m_composite_shader_data_pool;
    PoolSet<fastuidraw::PainterBlendShaderData> m_blend_shader_data_pool;
  };

  class painter_state_location
  {
  public:
    uint32_t m_clipping_data_loc;
    uint32_t m_item_matrix_data_loc;
    uint32_t m_brush_shader_data_loc;
    uint32_t m_item_shader_data_loc;
    uint32_t m_composite_shader_data_loc;
    uint32_t m_blend_shader_data_loc;
  };

  class PainterPackerPrivate;

  class per_draw_command
  {
  public:
    per_draw_command(const fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw> &r,
                     const fastuidraw::PainterBackend::ConfigurationBase &config);

    unsigned int
    attribute_room(void)
    {
      FASTUIDRAWassert(m_attributes_written <= m_draw_command->m_attributes.size());
      return m_draw_command->m_attributes.size() - m_attributes_written;
    }

    unsigned int
    index_room(void)
    {
      FASTUIDRAWassert(m_indices_written <= m_draw_command->m_indices.size());
      return m_draw_command->m_indices.size() - m_indices_written;
    }

    unsigned int
    store_room(void)
    {
      unsigned int s;
      s = store_written();
      FASTUIDRAWassert(s <= m_draw_command->m_store.size());
      return m_draw_command->m_store.size() - s;
    }

    unsigned int
    store_written(void)
    {
      return current_block() * 4;
    }

    void
    unmap(void)
    {
      m_draw_command->unmap(m_attributes_written, m_indices_written, store_written());
    }

    void
    pack_painter_state(const fastuidraw::PainterPackerData &state,
                       PainterPackerPrivate *p, painter_state_location &out_data);

    bool //returns true if a draw break was needed
    pack_header(unsigned int header_size,
                uint32_t brush_shader,
                const fastuidraw::reference_counted_ptr<fastuidraw::PainterBlendShader> &blend_shader,
                const fastuidraw::reference_counted_ptr<fastuidraw::PainterCompositeShader> &composite_shader,
                fastuidraw::BlendMode mode,
                const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &item_shader,
                int z,
                const painter_state_location &loc,
                const std::list<fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> > &call_backs,
                unsigned int *header_location);

    bool
    draw_break(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> &action)
    {
      if (action)
        {
          m_draw_command->draw_break(action, m_indices_written);
          return true;
        }
      return false;
    }

    fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw> m_draw_command;
    unsigned int m_attributes_written, m_indices_written;

  private:
    fastuidraw::c_array<fastuidraw::generic_data>
    allocate_store(unsigned int num_elements);

    unsigned int
    current_block(void)
    {
      return m_store_blocks_written;
    }

    void
    pack_state_data(PainterPackerPrivate *p, EntryBase *st_d, uint32_t &location);

    template<typename T>
    void
    pack_state_data_from_value(const T &st, uint32_t &location)
    {
      fastuidraw::c_array<fastuidraw::generic_data> dst;
      unsigned int data_sz;

      location = current_block();
      data_sz = st.data_size();
      dst = allocate_store(data_sz);
      st.pack_data(dst);
    }

    template<typename T>
    void
    pack_state_data(PainterPackerPrivate *p,
                    const fastuidraw::PainterData::value<T> &obj,
                    uint32_t &location)
    {
      if (obj.m_packed_value)
        {
          EntryBase *e;
          e = static_cast<EntryBase*>(obj.m_packed_value.opaque_data());
          pack_state_data(p, e, location);
        }
      else if (obj.m_value != nullptr)
        {
          pack_state_data_from_value(*obj.m_value, location);
        }
      else
        {
          static T v;
          pack_state_data_from_value(v, location);
        }
    }

    unsigned int m_store_blocks_written;
    uint32_t m_brush_shader_mask;
    PainterShaderGroupPrivate m_prev_state;
    fastuidraw::BlendMode m_prev_composite_mode;
  };

  class PainterPackerPrivateWorkroom
  {
  public:
    std::vector<unsigned int> m_attribs_loaded;
  };

  class AttributeIndexSrcFromArray
  {
  public:
    AttributeIndexSrcFromArray(fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
                               fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                               fastuidraw::c_array<const int> index_adjusts,
                               fastuidraw::c_array<const unsigned int> attrib_chunk_selector):
      m_attrib_chunks(attrib_chunks),
      m_index_chunks(index_chunks),
      m_index_adjusts(index_adjusts),
      m_attrib_chunk_selector(attrib_chunk_selector)
    {
      FASTUIDRAWassert((m_attrib_chunk_selector.empty() && m_attrib_chunks.size() == m_index_chunks.size())
             || (m_attrib_chunk_selector.size() == m_index_chunks.size()) );
      FASTUIDRAWassert(m_index_adjusts.size() == m_index_chunks.size() || m_index_adjusts.empty());
    }

    unsigned int
    number_attribute_chunks(void) const
    {
      return m_attrib_chunks.size();
    }

    unsigned int
    number_attributes(unsigned int attribute_chunk) const
    {
      FASTUIDRAWassert(attribute_chunk < m_attrib_chunks.size());
      return m_attrib_chunks[attribute_chunk].size();
    }

    unsigned int
    number_index_chunks(void) const
    {
      return m_index_chunks.size();
    }

    unsigned int
    number_indices(unsigned int index_chunk) const
    {
      FASTUIDRAWassert(index_chunk < m_index_chunks.size());
      return m_index_chunks[index_chunk].size();
    }

    unsigned int
    attribute_chunk_selection(unsigned int index_chunk) const
    {
      FASTUIDRAWassert(m_attrib_chunk_selector.empty() || index_chunk < m_attrib_chunk_selector.size());
      return m_attrib_chunk_selector.empty() ?
        index_chunk :
        m_attrib_chunk_selector[index_chunk];
    }

    void
    write_indices(fastuidraw::c_array<fastuidraw::PainterIndex> dst,
                  unsigned int index_offset_value,
                  unsigned int index_chunk) const
    {
      fastuidraw::c_array<const fastuidraw::PainterIndex> src;

      FASTUIDRAWassert(index_chunk < m_index_chunks.size());
      src = m_index_chunks[index_chunk];

      FASTUIDRAWassert(dst.size() == src.size());
      for(unsigned int i = 0; i < dst.size(); ++i)
        {
	  int adjust;

	  adjust = (m_index_adjusts.empty()) ? 0 : m_index_adjusts[index_chunk];
          FASTUIDRAWassert(int(src[i]) + adjust >= 0);
          dst[i] = int(src[i] + index_offset_value) + adjust;
        }
    }

    void
    write_attributes(fastuidraw::c_array<fastuidraw::PainterAttribute> dst,
                     unsigned int attribute_chunk) const
    {
      fastuidraw::c_array<const fastuidraw::PainterAttribute> src;

      FASTUIDRAWassert(attribute_chunk < m_attrib_chunks.size());
      src = m_attrib_chunks[attribute_chunk];

      FASTUIDRAWassert(dst.size() == src.size());
      std::memcpy(dst.c_ptr(), src.c_ptr(), sizeof(fastuidraw::PainterAttribute) * dst.size());
    }

    fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > m_attrib_chunks;
    fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > m_index_chunks;
    fastuidraw::c_array<const int> m_index_adjusts;
    fastuidraw::c_array<const unsigned int> m_attrib_chunk_selector;
  };

  class PainterPackerPrivate
  {
  public:
    explicit
    PainterPackerPrivate(fastuidraw::PainterPackedValuePool &pool,
                         fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend> backend,
                         fastuidraw::PainterPacker *p);

    void
    start_new_command(void);

    void
    upload_draw_state(const fastuidraw::PainterPackerData &draw_state);

    unsigned int
    compute_room_needed_for_packing(const fastuidraw::PainterPackerData &draw_state);

    template<typename T>
    unsigned int
    compute_room_needed_for_packing(const fastuidraw::PainterData::value<T> &obj)
    {
      if (obj.m_packed_value)
        {
          EntryBase *d;
          d = static_cast<EntryBase*>(obj.m_packed_value.opaque_data());
          if (d->m_painter == m_p && d->m_begin_id == m_number_begins
             && d->m_draw_command_id == m_accumulated_draws.size())
            {
              return 0;
            }
          else
            {
              return d->m_data.size();
            }
        }
      else if (obj.m_value != nullptr)
        {
          return obj.m_value->data_size();
        }
      else
        {
          static T v;
          return v.data_size();
        }
    };

    template<typename T>
    void
    draw_generic_implement(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                           const fastuidraw::PainterPackerData &data,
                           const T &src,
                           int z);

    fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend> m_backend;
    fastuidraw::PainterShaderSet m_default_shaders;
    fastuidraw::PainterData::value<fastuidraw::PainterBrush> m_default_brush;
    unsigned int m_header_size;

    fastuidraw::reference_counted_ptr<fastuidraw::PainterBlendShader> m_blend_shader;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterCompositeShader> m_composite_shader;
    fastuidraw::BlendMode m_composite_mode;
    painter_state_location m_painter_state_location;
    int m_number_begins;

    fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend::Surface> m_surface;
    bool m_clear_color_buffer;
    std::vector<per_draw_command> m_accumulated_draws;
    fastuidraw::reference_counted_ptr<const fastuidraw::Image> m_last_binded_image;
    fastuidraw::PainterPacker *m_p;

    PainterPackerPrivateWorkroom m_work_room;
    fastuidraw::vecN<unsigned int, fastuidraw::PainterEnums::num_stats> m_stats;

    std::list<fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> > m_callback_list;
  };
}

//////////////////////////////////////////
// per_draw_command methods
per_draw_command::
per_draw_command(const fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw> &r,
                 const fastuidraw::PainterBackend::ConfigurationBase &config):
  m_draw_command(r),
  m_attributes_written(0),
  m_indices_written(0),
  m_store_blocks_written(0),
  m_brush_shader_mask(config.brush_shader_mask())
{
  m_prev_state.m_item_group = 0;
  m_prev_state.m_brush = 0;
  m_prev_state.m_composite_group = 0;
}

fastuidraw::c_array<fastuidraw::generic_data>
per_draw_command::
allocate_store(unsigned int num_elements)
{
  fastuidraw::c_array<fastuidraw::generic_data> return_value;
  FASTUIDRAWassert(num_elements % 4 == 0);
  return_value = m_draw_command->m_store.sub_array(store_written(), num_elements);
  m_store_blocks_written += num_elements >> 2;
  return return_value;
}

void
per_draw_command::
pack_state_data(PainterPackerPrivate *p,
                EntryBase *d, uint32_t &location)
{
  if (d->m_painter == p->m_p && d->m_begin_id == p->m_number_begins
     && d->m_draw_command_id == p->m_accumulated_draws.size())
    {
      location = d->m_offset;
      return;
    }

  /* data not in current data store add
   * it to the current store.
   */
  fastuidraw::c_array<const fastuidraw::generic_data> src;
  fastuidraw::c_array<fastuidraw::generic_data> dst;

  location = current_block();
  src = fastuidraw::make_c_array(d->m_data);
  dst = allocate_store(src.size());
  std::copy(src.begin(), src.end(), dst.begin());

  d->m_painter = p->m_p;
  d->m_begin_id = p->m_number_begins;
  d->m_draw_command_id = p->m_accumulated_draws.size();
  d->m_offset = location;
}

void
per_draw_command::
pack_painter_state(const fastuidraw::PainterPackerData &state,
                   PainterPackerPrivate *p, painter_state_location &out_data)
{
  pack_state_data(p, state.m_clip, out_data.m_clipping_data_loc);
  pack_state_data(p, state.m_matrix, out_data.m_item_matrix_data_loc);
  pack_state_data(p, state.m_item_shader_data, out_data.m_item_shader_data_loc);
  pack_state_data(p, state.m_composite_shader_data, out_data.m_composite_shader_data_loc);
  pack_state_data(p, state.m_blend_shader_data, out_data.m_blend_shader_data_loc);

  if (state.m_brush.has_data())
    {
      pack_state_data(p, state.m_brush, out_data.m_brush_shader_data_loc);
    }
  else
    {
      pack_state_data(p, p->m_default_brush, out_data.m_brush_shader_data_loc);
    }
}

bool
per_draw_command::
pack_header(unsigned int header_size,
            uint32_t brush_shader,
            const fastuidraw::reference_counted_ptr<fastuidraw::PainterBlendShader> &blend_shader,
            const fastuidraw::reference_counted_ptr<fastuidraw::PainterCompositeShader> &composite_shader,
            fastuidraw::BlendMode composite_mode,
            const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &item_shader,
            int z,
            const painter_state_location &loc,
            const std::list<fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> > &call_backs,
            unsigned int *header_location)
{
  bool return_value(false);
  fastuidraw::c_array<fastuidraw::generic_data> dst;
  fastuidraw::PainterHeader header;

  *header_location = current_block();
  dst = allocate_store(header_size);

  PainterShaderGroupPrivate current;
  fastuidraw::PainterShader::Tag composite;
  fastuidraw::PainterShader::Tag blend;

  if (composite_shader)
    {
      composite = composite_shader->tag();
    }

  if (blend_shader)
    {
      blend = blend_shader->tag();
    }

  current.m_item_group = item_shader->group();
  current.m_blend_group = blend.m_group;
  current.m_brush = brush_shader;
  current.m_composite_group = composite.m_group;
  current.m_composite_mode = composite_mode;

  header.m_clip_equations_location = loc.m_clipping_data_loc;
  header.m_item_matrix_location = loc.m_item_matrix_data_loc;
  header.m_brush_shader_data_location = loc.m_brush_shader_data_loc;
  header.m_item_shader_data_location = loc.m_item_shader_data_loc;
  header.m_composite_shader_data_location = loc.m_composite_shader_data_loc;
  header.m_blend_shader_data_location = loc.m_blend_shader_data_loc;
  header.m_item_shader = item_shader->ID();
  header.m_brush_shader = current.m_brush;
  header.m_composite_shader = composite.m_ID;
  header.m_blend_shader = blend.m_ID;
  header.m_z = z;
  header.m_flags = 0u;
  header.pack_data(dst);

  if (current.m_item_group != m_prev_state.m_item_group
      || current.m_composite_group != m_prev_state.m_composite_group
      || current.m_blend_group != m_prev_state.m_blend_group
      || (m_brush_shader_mask & (current.m_brush ^ m_prev_state.m_brush)) != 0u
      || current.m_composite_mode != m_prev_state.m_composite_mode)
    {
      return_value = m_draw_command->draw_break(m_prev_state, current,
                                                m_indices_written);
    }

  m_prev_state = current;

  for (const auto &call_back: call_backs)
    {
      call_back->header_added(m_draw_command, header, dst);
    }

  return return_value;
}

///////////////////////////////////////////
// PainterPackerPrivate methods
PainterPackerPrivate::
PainterPackerPrivate(fastuidraw::PainterPackedValuePool &pool,
                     fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend> backend,
                     fastuidraw::PainterPacker *p):
  m_backend(backend),
  m_clear_color_buffer(false),
  m_p(p)
{
  m_header_size = fastuidraw::PainterHeader::data_size();
  // By calling PainterBackend::default_shaders(), we make the shaders
  // registered. By setting m_default_shaders to its return value,
  // and using that for the return value of PainterPacker::default_shaders(),
  // we skip the check in PainterBackend::default_shaders() to register
  // the shaders as well.
  m_default_shaders = m_backend->default_shaders();
  m_number_begins = 0;
  m_default_brush.make_packed(pool);
}

void
PainterPackerPrivate::
start_new_command(void)
{
  if (!m_accumulated_draws.empty())
    {
      per_draw_command &c(m_accumulated_draws.back());

      m_stats[fastuidraw::PainterEnums::num_attributes] += c.m_attributes_written;
      m_stats[fastuidraw::PainterEnums::num_indices] += c.m_indices_written;
      m_stats[fastuidraw::PainterEnums::num_generic_datas] += c.store_written();
      m_stats[fastuidraw::PainterEnums::num_draws] += 1u;

      c.unmap();
    }

  fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw> r;
  r = m_backend->map_draw();
  m_accumulated_draws.push_back(per_draw_command(r, m_backend->configuration_base()));
}

unsigned int
PainterPackerPrivate::
compute_room_needed_for_packing(const fastuidraw::PainterPackerData &draw_state)
{
  unsigned int R(0);
  R += compute_room_needed_for_packing(draw_state.m_clip);
  R += compute_room_needed_for_packing(draw_state.m_matrix);
  R += compute_room_needed_for_packing(draw_state.m_brush);
  R += compute_room_needed_for_packing(draw_state.m_item_shader_data);
  R += compute_room_needed_for_packing(draw_state.m_composite_shader_data);
  R += compute_room_needed_for_packing(draw_state.m_blend_shader_data);
  return R;
}

void
PainterPackerPrivate::
upload_draw_state(const fastuidraw::PainterPackerData &draw_state)
{
  using namespace fastuidraw;

  unsigned int needed_room;

  FASTUIDRAWassert(!m_accumulated_draws.empty());
  needed_room = compute_room_needed_for_packing(draw_state);
  if (needed_room > m_accumulated_draws.back().store_room())
    {
      start_new_command();
    }
  m_accumulated_draws.back().pack_painter_state(draw_state, this, m_painter_state_location);

  if (draw_state.m_brush.has_data())
    {
      const PainterBrush &brush(draw_state.m_brush.data());
      if (brush.image_requires_binding() && brush.image() != m_last_binded_image)
        {
          reference_counted_ptr<PainterDraw::Action> action;

          m_last_binded_image = brush.image();
          action = m_backend->bind_image(m_last_binded_image);
          if (m_accumulated_draws.back().draw_break(action))
            {
              ++m_stats[fastuidraw::PainterEnums::num_draws];
            }
        }
    }
}

template<typename T>
void
PainterPackerPrivate::
draw_generic_implement(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                       const fastuidraw::PainterPackerData &draw,
                       const T &src,
                       int z)
{
  bool allocate_header;
  unsigned int header_loc;
  const unsigned int NOT_LOADED = ~0u;
  unsigned int number_index_chunks, number_attribute_chunks;

  number_index_chunks = src.number_index_chunks();
  number_attribute_chunks = src.number_attribute_chunks();
  if (!shader || number_index_chunks == 0 || number_attribute_chunks == 0)
    {
      /* should we emit a warning message that the PainterItemShader
       * was missing the item shader value?
       */
      return;
    }

  m_work_room.m_attribs_loaded.clear();
  m_work_room.m_attribs_loaded.resize(number_attribute_chunks, NOT_LOADED);

  FASTUIDRAWassert(shader);

  upload_draw_state(draw);
  allocate_header = true;

  for(unsigned chunk = 0; chunk < number_index_chunks; ++chunk)
    {
      unsigned int attrib_room, index_room, data_room;
      unsigned int attrib_src, needed_attrib_room;
      unsigned int num_attribs, num_indices;

      attrib_room = m_accumulated_draws.back().attribute_room();
      index_room = m_accumulated_draws.back().index_room();
      data_room = m_accumulated_draws.back().store_room();

      attrib_src = src.attribute_chunk_selection(chunk);
      FASTUIDRAWassert(attrib_src < number_attribute_chunks);

      num_attribs = src.number_attributes(attrib_src);
      num_indices = src.number_indices(chunk);
      if (num_attribs == 0 || num_indices == 0)
        {
          continue;
        }

      needed_attrib_room = (m_work_room.m_attribs_loaded[attrib_src] == NOT_LOADED) ?
        num_attribs : 0;

      if (attrib_room < needed_attrib_room || index_room < num_indices
         || (allocate_header && data_room < m_header_size))
        {
          start_new_command();
          upload_draw_state(draw);

          /* reset attribs_loaded[] and recompute needed_attrib_room
           */
          std::fill(m_work_room.m_attribs_loaded.begin(), m_work_room.m_attribs_loaded.end(), NOT_LOADED);
          needed_attrib_room = num_attribs;

          attrib_room = m_accumulated_draws.back().attribute_room();
          index_room = m_accumulated_draws.back().index_room();
          data_room = m_accumulated_draws.back().store_room();
          allocate_header = true;

          if (attrib_room < needed_attrib_room || index_room < num_indices)
            {
              FASTUIDRAWassert(!"Unable to fit chunk into freshly allocated draw command, not good!");
              continue;
            }

          FASTUIDRAWassert(data_room >= m_header_size);
        }

      per_draw_command &cmd(m_accumulated_draws.back());
      if (allocate_header)
        {
          bool draw_break_added;

          ++m_stats[fastuidraw::PainterEnums::num_headers];
          allocate_header = false;
          draw_break_added = cmd.pack_header(m_header_size,
                                             fetch_value(draw.m_brush).shader(),
                                             m_blend_shader,
                                             m_composite_shader,
                                             m_composite_mode,
                                             shader,
                                             z, m_painter_state_location,
                                             m_callback_list,
                                             &header_loc);
          if (draw_break_added)
            {
              ++m_stats[fastuidraw::PainterEnums::num_draws];
            }
        }

      /* copy attribute data and get offset into attribute buffer
       * where attributes are copied
       */
      unsigned int attrib_offset;

      if (needed_attrib_room > 0)
        {
          fastuidraw::c_array<fastuidraw::PainterAttribute> attrib_dst_ptr;
          fastuidraw::c_array<uint32_t> header_dst_ptr;

          attrib_dst_ptr = cmd.m_draw_command->m_attributes.sub_array(cmd.m_attributes_written, num_attribs);
          header_dst_ptr = cmd.m_draw_command->m_header_attributes.sub_array(cmd.m_attributes_written, num_attribs);

          src.write_attributes(attrib_dst_ptr, attrib_src);
          std::fill(header_dst_ptr.begin(), header_dst_ptr.end(), header_loc);

          FASTUIDRAWassert(m_work_room.m_attribs_loaded[attrib_src] == NOT_LOADED);
          m_work_room.m_attribs_loaded[attrib_src] = cmd.m_attributes_written;

          attrib_offset = cmd.m_attributes_written;
          cmd.m_attributes_written += attrib_dst_ptr.size();
        }
      else
        {
          FASTUIDRAWassert(m_work_room.m_attribs_loaded[attrib_src] != NOT_LOADED);
          attrib_offset = m_work_room.m_attribs_loaded[attrib_src];
        }

      /* copy and adjust the index value by incrementing them by attrib_offset
       */
      fastuidraw::c_array<fastuidraw::PainterIndex> index_dst_ptr;

      index_dst_ptr = cmd.m_draw_command->m_indices.sub_array(cmd.m_indices_written, num_indices);
      src.write_indices(index_dst_ptr, attrib_offset, chunk);
      cmd.m_indices_written += index_dst_ptr.size();
    }
}

//////////////////////////////////////////////////
// fastuidraw::PainterPacker::DataCallBack methods
fastuidraw::PainterPacker::DataCallBack::
DataCallBack(void)
{
  m_d = FASTUIDRAWnew DataCallBackPrivate();
}

fastuidraw::PainterPacker::DataCallBack::
~DataCallBack()
{
  DataCallBackPrivate *d;

  d = static_cast<DataCallBackPrivate*>(m_d);
  FASTUIDRAWassert(d->m_list == nullptr);
  FASTUIDRAWdelete(d);
}

bool
fastuidraw::PainterPacker::DataCallBack::
active(void) const
{
  DataCallBackPrivate *d;

  d = static_cast<DataCallBackPrivate*>(m_d);
  return d->m_list != nullptr;
}

////////////////////////////////////////////
// fastuidraw::PainterPacker methods
fastuidraw::PainterPacker::
PainterPacker(PainterPackedValuePool &pool,
              reference_counted_ptr<PainterBackend> backend)
{
  FASTUIDRAWassert(backend);
  m_d = FASTUIDRAWnew PainterPackerPrivate(pool, backend, this);
}

fastuidraw::PainterPacker::
~PainterPacker()
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::PainterPacker::
add_callback(const reference_counted_ptr<DataCallBack> &callback)
{
  DataCallBackPrivate *cd;
  PainterPackerPrivate *pd;

  FASTUIDRAWassert(callback && !callback->active());
  if (!callback || callback->active())
    {
      return;
    }

  cd = static_cast<DataCallBackPrivate*>(callback->m_d);
  pd = static_cast<PainterPackerPrivate*>(m_d);

  cd->m_list = &pd->m_callback_list;
  cd->m_iterator = cd->m_list->insert(cd->m_list->begin(), callback);
}

void
fastuidraw::PainterPacker::
remove_callback(const reference_counted_ptr<DataCallBack> &callback)
{
  DataCallBackPrivate *cd;
  PainterPackerPrivate *pd;

  FASTUIDRAWassert(callback && callback->active());
  if (!callback || !callback->active())
    {
      return;
    }

  cd = static_cast<DataCallBackPrivate*>(callback->m_d);
  pd = static_cast<PainterPackerPrivate*>(m_d);

  FASTUIDRAWassert(cd->m_list == &pd->m_callback_list);
  if (cd->m_list != &pd->m_callback_list)
    {
      return;
    }

  cd->m_list->erase(cd->m_iterator);
  cd->m_list = nullptr;
}

void
fastuidraw::PainterPacker::
begin(const reference_counted_ptr<PainterBackend::Surface> &surface,
      bool clear_color_buffer)
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);

  FASTUIDRAWassert(d->m_accumulated_draws.empty());
  std::fill(d->m_stats.begin(), d->m_stats.end(), 0u);
  d->m_surface = surface;
  d->m_clear_color_buffer = clear_color_buffer;
  d->start_new_command();
  d->m_last_binded_image = nullptr;
  ++d->m_number_begins;
}

unsigned int
fastuidraw::PainterPacker::
query_stat(enum PainterEnums::query_stats_t st) const
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);

  vecN<unsigned int, PainterEnums::num_stats> tmp(0);
  if (!d->m_accumulated_draws.empty())
    {
      per_draw_command &c(d->m_accumulated_draws.back());
      tmp[PainterEnums::num_attributes] = c.m_attributes_written;
      tmp[PainterEnums::num_indices] = c.m_indices_written;
      tmp[PainterEnums::num_generic_datas] = c.store_written();
      tmp[PainterEnums::num_draws] = 0u;
    }
  return d->m_stats[st] + tmp[st];
}

void
fastuidraw::PainterPacker::
end(void)
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  if (!d->m_accumulated_draws.empty())
    {
      per_draw_command &c(d->m_accumulated_draws.back());

      d->m_stats[PainterEnums::num_attributes] += c.m_attributes_written;
      d->m_stats[PainterEnums::num_indices] += c.m_indices_written;
      d->m_stats[PainterEnums::num_generic_datas] += c.store_written();
      d->m_stats[PainterEnums::num_draws] += 1u;

      c.unmap();
    }

  d->m_backend->on_pre_draw(d->m_surface, d->m_clear_color_buffer);
  for(per_draw_command &cmd : d->m_accumulated_draws)
    {
      FASTUIDRAWassert(cmd.m_draw_command->unmapped());
      cmd.m_draw_command->draw();
    }
  d->m_backend->on_post_draw();
  d->m_accumulated_draws.clear();
  d->m_surface.clear();
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend::Surface>&
fastuidraw::PainterPacker::
surface(void) const
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  return d->m_surface;
}

void
fastuidraw::PainterPacker::
draw_break(const reference_counted_ptr<const PainterDraw::Action> &action)
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  if (d->m_accumulated_draws.back().draw_break(action))
    {
      ++d->m_stats[PainterEnums::num_draws];
    }
}

void
fastuidraw::PainterPacker::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
             const PainterPackerData &draw,
             c_array<const c_array<const PainterAttribute> > attrib_chunks,
             c_array<const c_array<const PainterIndex> > index_chunks,
             c_array<const int> index_adjusts,
             int z)
{
  draw_generic(shader, draw, attrib_chunks, index_chunks,
               index_adjusts, c_array<const unsigned int>(),
               z);
}

void
fastuidraw::PainterPacker::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
             const PainterPackerData &draw,
             c_array<const c_array<const PainterAttribute> > attrib_chunks,
             c_array<const c_array<const PainterIndex> > index_chunks,
             c_array<const int> index_adjusts,
             c_array<const unsigned int> attrib_chunk_selector,
             int z)
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);

  AttributeIndexSrcFromArray src(attrib_chunks, index_chunks, index_adjusts, attrib_chunk_selector);
  d->draw_generic_implement(shader, draw, src, z);
}

void
fastuidraw::PainterPacker::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
             const PainterPackerData &data,
             const PainterAttributeWriter &src,
             int z)
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  d->draw_generic_implement(shader, data, src, z);
}

const fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas>&
fastuidraw::PainterPacker::
glyph_atlas(void) const
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  return d->m_backend->glyph_atlas();
}

const fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas>&
fastuidraw::PainterPacker::
image_atlas(void) const
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  return d->m_backend->image_atlas();
}

const fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas>&
fastuidraw::PainterPacker::
colorstop_atlas(void) const
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  return d->m_backend->colorstop_atlas();
}

fastuidraw::reference_counted_ptr<fastuidraw::PainterShaderRegistrar>
fastuidraw::PainterPacker::
painter_shader_registrar(void) const
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  return d->m_backend->painter_shader_registrar();
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterCompositeShader>&
fastuidraw::PainterPacker::
composite_shader(void) const
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  return d->m_composite_shader;
}

fastuidraw::BlendMode
fastuidraw::PainterPacker::
composite_mode(void) const
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  return d->m_composite_mode;
}

void
fastuidraw::PainterPacker::
composite_shader(const fastuidraw::reference_counted_ptr<PainterCompositeShader> &h,
                 BlendMode pcomposite_mode)
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  d->m_composite_shader = h;
  d->m_composite_mode = pcomposite_mode;
}


const fastuidraw::reference_counted_ptr<fastuidraw::PainterBlendShader>&
fastuidraw::PainterPacker::
blend_shader(void) const
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  return d->m_blend_shader;
}

void
fastuidraw::PainterPacker::
blend_shader(const fastuidraw::reference_counted_ptr<PainterBlendShader> &h)
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  d->m_blend_shader = h;
}

const fastuidraw::PainterShaderSet&
fastuidraw::PainterPacker::
default_shaders(void) const
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  return d->m_default_shaders;
}

const fastuidraw::PainterBackend::PerformanceHints&
fastuidraw::PainterPacker::
hints(void)
{
  PainterPackerPrivate *d;
  d = static_cast<PainterPackerPrivate*>(m_d);
  return d->m_backend->hints();
}

void*
fastuidraw::PainterPacker::
create_painter_packed_value_pool_d(void)
{
  return FASTUIDRAWnew PainterPackedValuePoolPrivate();
}

void
fastuidraw::PainterPacker::
delete_painter_packed_value_pool_d(void *md)
{
  PainterPackedValuePoolPrivate *d;
  d = static_cast<PainterPackedValuePoolPrivate*>(md);
  FASTUIDRAWdelete(d);
}

void*
fastuidraw::PainterPacker::
create_packed_value(void *md, const PainterBrush &value)
{
  PainterPackedValuePoolPrivate *d;
  Entry<PainterBrush> *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(md);
  e = d->m_brush_pool.allocate(value);
  return e;
}

void*
fastuidraw::PainterPacker::
create_packed_value(void *md, const PainterClipEquations &value)
{
  PainterPackedValuePoolPrivate *d;
  Entry<PainterClipEquations> *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(md);
  e = d->m_clip_equations_pool.allocate(value);
  return e;
}

void*
fastuidraw::PainterPacker::
create_packed_value(void *md, const PainterItemMatrix &value)
{
  PainterPackedValuePoolPrivate *d;
  Entry<PainterItemMatrix> *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(md);
  e = d->m_item_matrix_pool.allocate(value);
  return e;
}

void*
fastuidraw::PainterPacker::
create_packed_value(void *md, const PainterItemShaderData &value)
{
  PainterPackedValuePoolPrivate *d;
  Entry<PainterItemShaderData> *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(md);
  e = d->m_item_shader_data_pool.allocate(value);
  return e;
}

void*
fastuidraw::PainterPacker::
create_packed_value(void *md, const PainterCompositeShaderData &value)
{
  PainterPackedValuePoolPrivate *d;
  Entry<PainterCompositeShaderData> *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(md);
  e = d->m_composite_shader_data_pool.allocate(value);
  return e;
}

void*
fastuidraw::PainterPacker::
create_packed_value(void *md, const PainterBlendShaderData &value)
{
  PainterPackedValuePoolPrivate *d;
  Entry<PainterBlendShaderData> *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(md);
  e = d->m_blend_shader_data_pool.allocate(value);
  return e;
}

void
fastuidraw::PainterPacker::
acquire_packed_value(void *md)
{
  EntryBase *d;
  d = static_cast<EntryBase*>(md);
  d->aquire();
}

void
fastuidraw::PainterPacker::
release_packed_value(void *md)
{
  EntryBase *d;
  d = static_cast<EntryBase*>(md);
  d->release();
}

const void*
fastuidraw::PainterPacker::
raw_data_of_packed_value(void *md)
{
  EntryBase *d;
  d = static_cast<EntryBase*>(md);
  FASTUIDRAWassert(d);
  FASTUIDRAWassert(d->raw_value());
  return d->raw_value();
}

uint32_t
fastuidraw::PainterPacker::
composite_group(const PainterShaderGroup *md)
{
  const PainterShaderGroupPrivate *d;
  d = static_cast<const PainterShaderGroupPrivate*>(md);
  return d->m_composite_group;
}

uint32_t
fastuidraw::PainterPacker::
blend_group(const PainterShaderGroup *md)
{
  const PainterShaderGroupPrivate *d;
  d = static_cast<const PainterShaderGroupPrivate*>(md);
  return d->m_blend_group;
}

uint32_t
fastuidraw::PainterPacker::
item_group(const PainterShaderGroup *md)
{
  const PainterShaderGroupPrivate *d;
  d = static_cast<const PainterShaderGroupPrivate*>(md);
  return d->m_item_group;
}

uint32_t
fastuidraw::PainterPacker::
brush(const PainterShaderGroup *md)
{
  const PainterShaderGroupPrivate *d;
  d = static_cast<const PainterShaderGroupPrivate*>(md);
  return d->m_brush;
}

fastuidraw::BlendMode
fastuidraw::PainterPacker::
composite_mode(const PainterShaderGroup *md)
{
  const PainterShaderGroupPrivate *d;
  d = static_cast<const PainterShaderGroupPrivate*>(md);
  return d->m_composite_mode;
}
