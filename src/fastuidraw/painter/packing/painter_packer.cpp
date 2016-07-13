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

#include <fastuidraw/painter/packing/painter_packer.hpp>
#include <fastuidraw/painter/packing/painter_packing_enums.hpp>
#include "../../private/util_private.hpp"

namespace
{
  class PainterShaderGroupValues
  {
  public:
    uint32_t m_blend_group;
    uint32_t m_vert_group;
    uint32_t m_frag_group;
    uint32_t m_brush;
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
      - does the reference count to a pool need to be thread safe?
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
      assert(m_free_slots_back == pool_size - 1);
    }

    int
    aquire_slot(void)
    {
      /* Should we make this thread safe? We can make it
         thread safe by using atomic operation on m_free_slots_back.
         The following code would make it thread safe.

         return_value = m_free_slots_back.fetch_sub(1, boost::memory_order_aquire)
         if(return_value < 0)
           {
              m_free_slots_back.fetch_add(1, boost::memory_order_aquire);
           }
         return return_value;
       */
      int return_value(-1);

      if(m_free_slots_back >= 0)
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
         thread safe by using atomic operation on m_free_slots_back.
         The following code would make it thread safe.

         int S;
         S = m_free_slots_back.fetch_add(1, boost::memory_order_release);
         boost::atomic_thread_fence(boost::memory_order_acquire);
         assert(S < pool_size);
         m_free_slots[S] = v;
       */
      assert(v >= 0);
      assert(v < pool_size);

      ++m_free_slots_back;

      assert(m_free_slots_back < pool_size);
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
      m_raw_state(NULL),
      m_pool_slot(-1)
    {}

    void
    aquire(void)
    {
      assert(m_pool);
      assert(m_pool_slot >= 0);
      m_count.add_reference();
    }

    void
    release(void)
    {
      assert(m_pool);
      assert(m_pool_slot >= 0);
      if(m_count.remove_reference())
        {
          m_pool->release_slot(m_pool_slot);
          m_pool_slot = -1;
          m_pool = NULL;
        }
    }

    const void*
    raw_state(void)
    {
      return m_raw_state;
    }

    /* To what painter and where in data store buffer
       already packed into PainterDrawCommand::m_store
     */
    const fastuidraw::PainterPacker *m_painter;
    std::vector<fastuidraw::generic_data> m_data;
    int m_begin_id;
    unsigned int m_draw_command_id, m_offset;

    /* how m_data is aligned
     */
    unsigned int m_alignment;

  protected:
    /* pointer to raw state member held
       by derived class
     */
    const void *m_raw_state;

    /* location in pool and what pool
     */
    PoolBase::handle m_pool;
    int m_pool_slot;

  private:
    /* Entry reference count is not thread safe because
       the objects themselves are not.
    */
    fastuidraw::reference_count_non_concurrent m_count;
  };

  template<typename T>
  class Entry:public EntryBase
  {
  public:
    Entry(void)
    {
      m_raw_state = &m_state;
    }

    void
    set(const T &st, int alignment, PoolBase *p, int slot)
    {
      assert(p);
      assert(slot >= 0);

      m_pool = p;
      m_state = st;
      m_pool_slot = slot;

      this->m_begin_id = -1;
      this->m_draw_command_id = 0;
      this->m_offset = 0;
      this->m_painter = NULL;
      this->m_alignment = alignment;
      this->m_data.resize(m_state.data_size(alignment));
      m_state.pack_data(alignment, fastuidraw::make_c_array(this->m_data));
    }

    T m_state;
  };

  template<typename T>
  class Pool:public PoolBase
  {
  public:
    typedef fastuidraw::reference_counted_ptr<Pool> handle;
    typedef fastuidraw::reference_counted_ptr<Pool const> const_handle;

    /* Returning NULL indicates no free entries left in the pool
     */
    Entry<T>*
    allocate(const T &st, int alignment)
    {
      Entry<T> *return_value(NULL);
      int slot;

      slot = this->aquire_slot();
      if(slot >= 0)
        {
          return_value = &m_data[slot];
          return_value->set(st, alignment, this, slot);
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
    allocate(const T &st, int alignment)
    {
      Entry<T> *return_value;

      return_value = m_pools.back()->allocate(st, alignment);
      if(!return_value)
        {
          m_pools.push_back(FASTUIDRAWnew Pool<T>());
          return_value = m_pools.back()->allocate(st, alignment);
        }

      assert(return_value);
      return return_value;
    }

  private:
    std::vector<typename Pool<T>::handle> m_pools;
  };

  class PainterSaveStatePoolPrivate
  {
  public:
    explicit
    PainterSaveStatePoolPrivate(int d):
      m_alignment(d)
    {}

    int m_alignment;

    PoolSet<fastuidraw::PainterBrush> m_brush_pool;
    PoolSet<fastuidraw::PainterState::ClipEquations> m_clip_equations_pool;
    PoolSet<fastuidraw::PainterState::ItemMatrix> m_item_matrix_pool;
    PoolSet<fastuidraw::PainterState::VertexShaderData> m_vertex_shader_data_pool;
    PoolSet<fastuidraw::PainterState::FragmentShaderData> m_fragment_shader_data_pool;
  };

  template<typename T>
  class tracked_state
  {
  public:
    typedef T State;

    tracked_state(void):
      m_raw(NULL),
      m_changed_since_last_upload(true)
    {}

    void
    set(const State &v)
    {
      m_value = v;
      m_changed_since_last_upload = true;
      m_save_state = fastuidraw::PainterSaveState<T>();
      m_raw = NULL;
    }

    void
    set_state(void *hraw, const fastuidraw::PainterSaveState<State> &h)
    {
      assert(h);
      if(m_save_state != h)
        {
          m_save_state = h;
          m_raw = hraw;
          m_changed_since_last_upload = true;
        }
    }

    unsigned int
    room_needed_for_packing(unsigned int alignment) const
    {
      return m_changed_since_last_upload ? cvalue().data_size(alignment) : 0;
    }

    const State&
    cvalue(void) const
    {
      if(m_save_state)
        {
          return m_save_state.state();
        }
      else
        {
          return m_value;
        }
    }

    void*
    raw(void)
    {
      return m_raw;
    }

    const fastuidraw::PainterSaveState<State>&
    state(void) const
    {
      return m_save_state;
    }

    bool
    changed_since_last_upload(void) const
    {
      return m_changed_since_last_upload;
    }

    void
    mark_as_uploaded(void)
    {
      m_changed_since_last_upload = false;
    }

  private:
    fastuidraw::PainterSaveState<State> m_save_state;
    void *m_raw;
    State m_value;
    bool m_changed_since_last_upload;
  };

  /* brush required special implementation becuase of the
     ability to partially update a PainterBrush; we need
     the ability to return a reference to the brush

     How it works:
       when m_save_state is non-NULL, then m_value_clones_state
       is true when the value in m_value is the same as
       m_save_state->state().
   */
  template<>
  class tracked_state<fastuidraw::PainterBrush>
  {
  public:
    typedef fastuidraw::PainterBrush State;

    tracked_state(void):
      m_raw(NULL),
      m_changed_since_last_upload(true)
    {}

    void
    set(const State &v)
    {
      m_value = v;
      m_changed_since_last_upload = true;
      m_save_state = fastuidraw::PainterSaveState<State>();
      m_raw = NULL;
    }

    void
    set_state(void *hraw, const fastuidraw::PainterSaveState<State> &h,
              bool clones_state)
    {
      assert(h);
      if(h != m_save_state)
        {
          m_save_state = h;
          m_raw = hraw;
          m_changed_since_last_upload = true;
          m_value_clones_state = clones_state;
          m_value.clear_dirty();
        }
    }

    unsigned int
    room_needed_for_packing(unsigned int alignment) const
    {
      return changed_since_last_upload() ? cvalue().data_size(alignment) : 0;
    }

    const State&
    cvalue(void) const
    {
      if(m_save_state)
        {
          if(m_value.is_dirty())
            {
              /* m_value was modified since we last
                 had the state set, thus m_save_state
                 does not reflect the real value, clear
                 it
               */
              m_save_state = fastuidraw::PainterSaveState<State>();
              m_raw = NULL;
              m_changed_since_last_upload = true;
            }
          else
            {
              return m_save_state.state();
            }
        }
      return m_value;
    }

    State&
    value(void)
    {
      if(m_save_state && !m_value_clones_state)
        {
          m_value_clones_state = true;
          m_value = m_save_state.state();
          /* copying the value will set dirty bits up
             but the contents of m_save_state are for
             its value, so clear the bits.
           */
          m_value.clear_dirty();
        }
      return m_value;
    }

    const fastuidraw::PainterSaveState<State>&
    state(void) const
    {
      if(m_save_state && m_value.is_dirty())
        {
          m_save_state = fastuidraw::PainterSaveState<State>();
          m_raw = NULL;
          m_changed_since_last_upload = true;
        }
      return m_save_state;
    }

    void*
    raw(void)
    {
      return m_raw;
    }

    bool
    changed_since_last_upload(void) const
    {
      if(m_value.is_dirty())
        {
          m_changed_since_last_upload = true;
        }
      return m_changed_since_last_upload;
    }

    void
    mark_as_uploaded(void)
    {
      m_changed_since_last_upload = false;
    }

  private:
    mutable void *m_raw;
    mutable fastuidraw::PainterSaveState<State> m_save_state;
    mutable State m_value;
    mutable bool m_changed_since_last_upload;
    bool m_value_clones_state;
  };

  class painter_state
  {
  public:
    tracked_state<fastuidraw::PainterState::ClipEquations> m_clipping;
    tracked_state<fastuidraw::PainterState::ItemMatrix> m_item_matrix;
    tracked_state<fastuidraw::PainterBrush> m_brush;
    tracked_state<fastuidraw::PainterState::VertexShaderData> m_vert_shader_data;
    tracked_state<fastuidraw::PainterState::FragmentShaderData> m_frag_shader_data;

    fastuidraw::PainterShader::const_handle m_blend_shader;

    unsigned int
    room_needed_for_packing(unsigned int alignment) const
    {
      return m_clipping.room_needed_for_packing(alignment)
        + m_item_matrix.room_needed_for_packing(alignment)
        + m_brush.room_needed_for_packing(alignment)
        + m_vert_shader_data.room_needed_for_packing(alignment)
        + m_frag_shader_data.room_needed_for_packing(alignment);
    }

    bool
    changed_since_last_upload(void) const
    {
      return m_clipping.changed_since_last_upload()
        || m_item_matrix.changed_since_last_upload()
        || m_brush.changed_since_last_upload()
        || m_vert_shader_data.changed_since_last_upload()
        || m_frag_shader_data.changed_since_last_upload();
    }
  };

  class painter_state_location
  {
  public:
    uint32_t m_clipping_data_loc;
    uint32_t m_item_matrix_data_loc;
    uint32_t m_brush_shader_data_loc;
    uint32_t m_vert_shader_data_loc;
    uint32_t m_frag_shader_data_loc;
  };

  class PainterPackerPrivate;

  class per_draw_command
  {
  public:
    per_draw_command(const fastuidraw::PainterDrawCommand::const_handle &r,
                     const fastuidraw::PainterBackend::Configuration &config);

    unsigned int
    attribute_room(void)
    {
      assert(m_attributes_written <= m_draw_command->m_attributes.size());
      return m_draw_command->m_attributes.size() - m_attributes_written;
    }

    unsigned int
    index_room(void)
    {
      assert(m_indices_written <= m_draw_command->m_indices.size());
      return m_draw_command->m_indices.size() - m_indices_written;
    }

    unsigned int
    store_room(void)
    {
      unsigned int s;
      s = store_written();
      assert(s <= m_draw_command->m_store.size());
      return m_draw_command->m_store.size() - s;
    }

    unsigned int
    store_written(void)
    {
      return current_block() * m_alignment;
    }

    void
    unmap(void)
    {
      m_draw_command->unmap(m_attributes_written, m_indices_written, store_written());
    }

    void
    pack_painter_state(PainterPackerPrivate *p, bool force_upload,
                       painter_state_location &out_data);

    unsigned int
    pack_header(unsigned int header_size,
                const fastuidraw::PainterShader::const_handle &vert_shader,
                const fastuidraw::PainterShader::const_handle &frag_shader,
                unsigned int z,
                const painter_state &state,
                const painter_state_location &loc,
                const fastuidraw::PainterPacker::DataCallBack::handle &call_back);

    fastuidraw::PainterDrawCommand::const_handle m_draw_command;
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
      data_sz = st.data_size(m_alignment);
      dst = allocate_store(data_sz);
      st.pack_data(m_alignment, dst);
    }

    template<typename T>
    void
    pack_state_data(PainterPackerPrivate *p, tracked_state<T> &obj,
                    bool force_upload, uint32_t &location)
    {
      if(force_upload || obj.changed_since_last_upload())
        {
          if(obj.state())
            {
              EntryBase *e;
              e = reinterpret_cast<EntryBase*>(obj.raw());
              pack_state_data(p, e, location);
            }
          else
            {
              pack_state_data_from_value(obj.cvalue(), location);
            }
	  obj.mark_as_uploaded();
        }
    }

    unsigned int m_store_blocks_written;
    unsigned int m_alignment;
    fastuidraw::Image::const_handle m_last_image;
    fastuidraw::ColorStopSequenceOnAtlas::const_handle m_last_color_stop;
    std::list<fastuidraw::Image::const_handle> m_images_active;
    std::list<fastuidraw::ColorStopSequenceOnAtlas::const_handle> m_color_stops_active;

    uint32_t m_brush_shader_mask;
    PainterShaderGroupPrivate m_prev_state;
  };

  class PainterPackerPrivateWorkroom
  {
  public:
    std::vector<unsigned int> m_attribs_loaded;
  };

  class PainterPackerPrivate
  {
  public:
    explicit
    PainterPackerPrivate(fastuidraw::PainterBackend::handle backend,
                         fastuidraw::PainterPacker *p);

    void
    start_new_command(void);

    void
    upload_changed_state(void);

    fastuidraw::PainterBackend::handle m_backend;
    fastuidraw::PainterShaderSet m_default_shaders;
    unsigned int m_alignment;
    unsigned int m_header_size;

    painter_state m_state;
    painter_state_location m_painter_state_location;
    int m_number_begins;

    std::vector<per_draw_command> m_accumulated_draws;
    fastuidraw::PainterPacker *m_p;
    fastuidraw::PainterSaveStatePool m_pool;

    PainterPackerPrivateWorkroom m_work_room;
  };
}


//////////////////////////////////////////
// per_draw_command methods
per_draw_command::
per_draw_command(const fastuidraw::PainterDrawCommand::const_handle &r,
                 const fastuidraw::PainterBackend::Configuration &config):
  m_draw_command(r),
  m_attributes_written(0),
  m_indices_written(0),
  m_store_blocks_written(0),
  m_alignment(config.alignment()),
  m_brush_shader_mask(config.brush_shader_mask())
{
  m_prev_state.m_vert_group = 0;
  m_prev_state.m_frag_group = 0;
  m_prev_state.m_brush = 0;
  m_prev_state.m_blend_group = 0;
}


fastuidraw::c_array<fastuidraw::generic_data>
per_draw_command::
allocate_store(unsigned int num_elements)
{
  fastuidraw::c_array<fastuidraw::generic_data> return_value;
  assert(num_elements % m_alignment == 0);
  return_value = m_draw_command->m_store.sub_array(store_written(), num_elements);
  m_store_blocks_written += num_elements / m_alignment;
  return return_value;
}

void
per_draw_command::
pack_state_data(PainterPackerPrivate *p,
                EntryBase *d, uint32_t &location)
{
  if(d->m_painter == p->m_p && d->m_begin_id == p->m_number_begins
     && d->m_draw_command_id == p->m_accumulated_draws.size())
    {
      location = d->m_offset;
      return;
    }

  /* data not in current data store add
     it to the current store.
   */
  fastuidraw::const_c_array<fastuidraw::generic_data> src;
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
pack_painter_state(PainterPackerPrivate *p, bool force_upload,
                   painter_state_location &out_data)
{
  painter_state &state(p->m_state);

  pack_state_data(p, state.m_clipping, force_upload, out_data.m_clipping_data_loc);
  pack_state_data(p, state.m_item_matrix, force_upload, out_data.m_item_matrix_data_loc);
  pack_state_data(p, state.m_vert_shader_data, force_upload, out_data.m_vert_shader_data_loc);
  pack_state_data(p, state.m_frag_shader_data, force_upload, out_data.m_frag_shader_data_loc);
  pack_state_data(p, state.m_brush, force_upload, out_data.m_brush_shader_data_loc);

  /* We save a handle to the image and colorstop used by the brush,
     to make sure the Image and ColorStopSequenceOnAtlas objects are
     not deleted until the draw command built is sent down to the 3D
     API.
   */
  if(state.m_brush.cvalue().image() && m_last_image != state.m_brush.cvalue().image())
    {
      m_last_image = state.m_brush.cvalue().image();
      m_images_active.push_back(m_last_image);
    }

  if(state.m_brush.cvalue().color_stops() && m_last_color_stop != state.m_brush.cvalue().color_stops())
    {
      m_last_color_stop = state.m_brush.cvalue().color_stops();
      m_color_stops_active.push_back(m_last_color_stop);
    }
}

unsigned int
per_draw_command::
pack_header(unsigned int header_size,
            const fastuidraw::PainterShader::const_handle &vert_shader,
            const fastuidraw::PainterShader::const_handle &frag_shader,
            unsigned int z,
            const painter_state &state,
            const painter_state_location &loc,
            const fastuidraw::PainterPacker::DataCallBack::handle &call_back)
{
  unsigned int return_value;
  fastuidraw::c_array<fastuidraw::generic_data> dst, dst_write;
  fastuidraw::vecN<fastuidraw::generic_data, fastuidraw::PainterPacking::header_size> dst_read_write;

  return_value = current_block();
  dst_write = allocate_store(header_size);

  if(call_back)
    {
      call_back->current_draw_command(m_draw_command);
      dst = dst_read_write;
    }
  else
    {
      dst = dst_write;
    }

  dst[fastuidraw::PainterPacking::clip_equations_offset].u = loc.m_clipping_data_loc;
  dst[fastuidraw::PainterPacking::item_matrix_offset].u = loc.m_item_matrix_data_loc;

  dst[fastuidraw::PainterPacking::brush_shader_data_offset].u = loc.m_brush_shader_data_loc;
  dst[fastuidraw::PainterPacking::vert_shader_data_offset].u = loc.m_vert_shader_data_loc;
  dst[fastuidraw::PainterPacking::frag_shader_data_offset].u = loc.m_frag_shader_data_loc;

  PainterShaderGroupPrivate current;
  fastuidraw::PainterShader::Tag blend;

  if(state.m_blend_shader)
    {
      blend = state.m_blend_shader->tag();
    }
  current.m_vert_group = vert_shader->group();
  current.m_frag_group = frag_shader->group();
  current.m_brush = state.m_brush.cvalue().shader();
  current.m_blend_group = blend.m_group;


  dst[fastuidraw::PainterPacking::vert_frag_shader_offset].u =
    fastuidraw::pack_bits(fastuidraw::PainterPacking::vert_shader_bit0,
                         fastuidraw::PainterPacking::vert_shader_num_bits,
                         vert_shader->ID())
    | fastuidraw::pack_bits(fastuidraw::PainterPacking::frag_shader_bit0,
                           fastuidraw::PainterPacking::frag_shader_num_bits,
                           frag_shader->ID());

  dst[fastuidraw::PainterPacking::brush_shader_offset].u = current.m_brush;

  dst[fastuidraw::PainterPacking::z_blend_shader_offset].u =
    fastuidraw::pack_bits(fastuidraw::PainterPacking::z_bit0,
                         fastuidraw::PainterPacking::z_num_bits, z)
    | fastuidraw::pack_bits(fastuidraw::PainterPacking::blend_shader_bit0,
                           fastuidraw::PainterPacking::blend_shader_num_bits, blend.m_ID);

  if(current.m_vert_group != m_prev_state.m_vert_group
     || current.m_frag_group != m_prev_state.m_frag_group
     || current.m_blend_group != m_prev_state.m_blend_group
     || (m_brush_shader_mask & (current.m_brush ^ m_prev_state.m_brush)) != 0u)
    {
      m_draw_command->draw_break(m_prev_state, current, m_attributes_written, m_indices_written);
    }

  m_prev_state = current;

  if(call_back)
    {
      std::memcpy(dst_write.c_ptr(), dst_read_write.c_ptr(),
                  sizeof(fastuidraw::generic_data) * dst_read_write.size());
      call_back->header_added(dst_read_write, dst_write);
    }

  return return_value;
}

///////////////////////////////////////////
// PainterPackerPrivate methods
PainterPackerPrivate::
PainterPackerPrivate(fastuidraw::PainterBackend::handle backend,
                     fastuidraw::PainterPacker *p):
  m_backend(backend),
  m_p(p),
  m_pool(backend->configuration().alignment())
{
  m_alignment = m_backend->configuration().alignment();
  m_header_size = fastuidraw::round_up_to_multiple(fastuidraw::PainterPacking::header_size, m_alignment);
  m_default_shaders = m_backend->default_shaders();
  m_number_begins = 0;
}

void
PainterPackerPrivate::
start_new_command(void)
{
  if(!m_accumulated_draws.empty())
    {
      m_accumulated_draws.back().unmap();
    }

  fastuidraw::PainterDrawCommand::const_handle r;
  r = m_backend->map_draw_command();
  m_accumulated_draws.push_back(per_draw_command(r, m_backend->configuration()));
  m_accumulated_draws.back().pack_painter_state(this, true, m_painter_state_location);
}

void
PainterPackerPrivate::
upload_changed_state(void)
{
  assert(!m_accumulated_draws.empty());

  if(m_state.changed_since_last_upload())
    {
      unsigned int needed_room;

      needed_room = m_state.room_needed_for_packing(m_alignment);
      if(needed_room <= m_accumulated_draws.back().store_room())
        {
          m_accumulated_draws.back().pack_painter_state(this, false, m_painter_state_location);
        }
      else
        {
          start_new_command();
        }
    }
}

/////////////////////////////////////////
// fastuidraw::PainterShaderGroup methods
uint32_t
fastuidraw::PainterShaderGroup::
blend_group(void) const
{
  const PainterShaderGroupPrivate *d;
  d = static_cast<const PainterShaderGroupPrivate*>(this);
  return d->m_blend_group;
}

uint32_t
fastuidraw::PainterShaderGroup::
vert_group(void) const
{
  const PainterShaderGroupPrivate *d;
  d = static_cast<const PainterShaderGroupPrivate*>(this);
  return d->m_vert_group;
}

uint32_t
fastuidraw::PainterShaderGroup::
frag_group(void) const
{
  const PainterShaderGroupPrivate *d;
  d = static_cast<const PainterShaderGroupPrivate*>(this);
  return d->m_frag_group;
}

uint32_t
fastuidraw::PainterShaderGroup::
brush(void) const
{
  const PainterShaderGroupPrivate *d;
  d = static_cast<const PainterShaderGroupPrivate*>(this);
  return d->m_brush;
}

////////////////////////////////////////////
// fastuidraw::Painter methods
fastuidraw::PainterPacker::
PainterPacker(PainterBackend::handle backend)
{
  assert(backend);
  m_d = FASTUIDRAWnew PainterPackerPrivate(backend, this);
}

fastuidraw::PainterPacker::
~PainterPacker()
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::PainterPacker::
begin(void)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);

  assert(d->m_accumulated_draws.empty());
  d->m_backend->on_begin();
  d->start_new_command();
  ++d->m_number_begins;
}


void
fastuidraw::PainterPacker::
flush(void)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  if(!d->m_accumulated_draws.empty())
    {
      d->m_accumulated_draws.back().unmap();
    }

  d->m_backend->on_pre_draw();
  for(std::vector<per_draw_command>::iterator iter = d->m_accumulated_draws.begin(),
        end = d->m_accumulated_draws.end(); iter != end; ++iter)
    {
      assert(iter->m_draw_command->unmapped());
      iter->m_draw_command->draw();
    }
  d->m_accumulated_draws.clear();
}

void
fastuidraw::PainterPacker::
end(void)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  flush();
  d->m_backend->on_end();
}

void
fastuidraw::PainterPacker::
draw_generic(const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
             const_c_array<const_c_array<PainterIndex> > index_chunks,
             const PainterItemShader &shader, unsigned int z,
             const DataCallBack::handle &call_back)
{
  draw_generic(attrib_chunks, index_chunks, const_c_array<unsigned int>(),
               shader, z, call_back);
}

void
fastuidraw::PainterPacker::
draw_generic(const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
             const_c_array<const_c_array<PainterIndex> > index_chunks,
             const_c_array<unsigned int> attrib_chunk_selector,
             const PainterItemShader &shader, unsigned int z,
             const DataCallBack::handle &call_back)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);

  bool allocate_header;
  unsigned int header_loc;
  const unsigned int NOT_LOADED = ~0u;

  assert((attrib_chunk_selector.empty() && attrib_chunks.size() == index_chunks.size())
         || (attrib_chunk_selector.size() == index_chunks.size()) );

  if(attrib_chunks.empty() || !shader.vert_shader() || !shader.frag_shader())
    {
      /* should we emit a warning message that the PainterItemShader
         was missing the vertex or fragment shader value?
       */
      return;
    }

  d->m_work_room.m_attribs_loaded.clear();
  d->m_work_room.m_attribs_loaded.resize(attrib_chunk_selector.size(), NOT_LOADED);

  assert(shader.vert_shader());
  assert(shader.frag_shader());

  d->upload_changed_state();
  allocate_header = true;

  for(unsigned chunk = 0, num_chunks = index_chunks.size(); chunk < num_chunks; ++chunk)
    {
      unsigned int attrib_room, index_room, data_room;
      unsigned int attrib_src, needed_attrib_room;

      attrib_room = d->m_accumulated_draws.back().attribute_room();
      index_room = d->m_accumulated_draws.back().index_room();
      data_room = d->m_accumulated_draws.back().store_room();

      if(attrib_chunk_selector.empty())
        {
          attrib_src = chunk;
          needed_attrib_room = attrib_chunks[attrib_src].size();
        }
      else
        {
          attrib_src = attrib_chunk_selector[chunk];
          needed_attrib_room = (d->m_work_room.m_attribs_loaded[attrib_src] == NOT_LOADED) ?
            attrib_chunks[attrib_src].size() :
            0;
        }

      if(index_chunks[chunk].empty() || attrib_chunks[attrib_src].empty())
        {
          continue;
        }

      if(attrib_room < needed_attrib_room || index_room < index_chunks[chunk].size()
         || (allocate_header && data_room < d->m_header_size))
        {
          d->start_new_command();

          /* reset attribs_loaded[] and recompute needed_attrib_room
           */
          if(!attrib_chunk_selector.empty())
            {
              std::fill(d->m_work_room.m_attribs_loaded.begin(), d->m_work_room.m_attribs_loaded.end(), NOT_LOADED);
              needed_attrib_room = attrib_chunks[attrib_src].size();
            }

          attrib_room = d->m_accumulated_draws.back().attribute_room();
          index_room = d->m_accumulated_draws.back().index_room();
          data_room = d->m_accumulated_draws.back().store_room();
          allocate_header = true;

          if(attrib_room < needed_attrib_room || index_room < index_chunks[chunk].size())
            {
              assert(!"Unable to fit chunk into freshly allocated draw command, not good!");
              continue;
            }

          assert(data_room >= d->m_header_size);
        }

      per_draw_command &cmd(d->m_accumulated_draws.back());
      if(allocate_header)
        {
          allocate_header = false;
          header_loc = cmd.pack_header(d->m_header_size,
                                       shader.vert_shader(), shader.frag_shader(),
                                       z, d->m_state, d->m_painter_state_location,
                                       call_back);
        }

      /* copy attribute data and get offset into attribute buffer
         where attributes are copied
       */
      unsigned int attrib_offset;

      if(needed_attrib_room > 0)
        {
          c_array<PainterAttribute> attrib_dst_ptr;
          const_c_array<PainterAttribute> attrib_src_ptr;
          c_array<uint32_t> header_dst_ptr;

          attrib_src_ptr = attrib_chunks[attrib_src];
          attrib_dst_ptr = cmd.m_draw_command->m_attributes.sub_array(cmd.m_attributes_written, attrib_src_ptr.size());
          header_dst_ptr = cmd.m_draw_command->m_header_attributes.sub_array(cmd.m_attributes_written, attrib_src_ptr.size());

          std::memcpy(attrib_dst_ptr.c_ptr(), attrib_src_ptr.c_ptr(), sizeof(PainterAttribute) * attrib_dst_ptr.size());
          std::fill(header_dst_ptr.begin(), header_dst_ptr.end(), header_loc);

          if(!attrib_chunk_selector.empty())
            {
              assert(d->m_work_room.m_attribs_loaded[attrib_src] == NOT_LOADED);
              d->m_work_room.m_attribs_loaded[attrib_src] = cmd.m_attributes_written;
            }
          attrib_offset = cmd.m_attributes_written;
          cmd.m_attributes_written += attrib_dst_ptr.size();
        }
      else
        {
          assert(!attrib_chunk_selector.empty());
          assert(d->m_work_room.m_attribs_loaded[attrib_src] != NOT_LOADED);
          attrib_offset = d->m_work_room.m_attribs_loaded[attrib_src];
        }

      /* copy and adjust the index value by incrementing them by index_offset
       */
      c_array<PainterIndex> index_dst_ptr;
      const_c_array<PainterIndex> index_src_ptr;

      index_src_ptr = index_chunks[chunk];
      index_dst_ptr = cmd.m_draw_command->m_indices.sub_array(cmd.m_indices_written, index_src_ptr.size());
      for(unsigned int i = 0; i < index_dst_ptr.size(); ++i)
        {
          index_dst_ptr[i] = index_src_ptr[i] + attrib_offset;
        }
      cmd.m_indices_written += index_dst_ptr.size();
    }
}

const fastuidraw::GlyphAtlas::handle&
fastuidraw::PainterPacker::
glyph_atlas(void) const
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  return d->m_backend->glyph_atlas();
}

const fastuidraw::ImageAtlas::handle&
fastuidraw::PainterPacker::
image_atlas(void) const
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  return d->m_backend->image_atlas();
}

const fastuidraw::ColorStopAtlas::handle&
fastuidraw::PainterPacker::
colorstop_atlas(void) const
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  return d->m_backend->colorstop_atlas();
}

fastuidraw::PainterBrush&
fastuidraw::PainterPacker::
brush(void)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  return d->m_state.m_brush.value();
}

const fastuidraw::PainterBrush&
fastuidraw::PainterPacker::
cbrush(void) const
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  return d->m_state.m_brush.cvalue();
}

const fastuidraw::PainterPacker::PainterBrushState&
fastuidraw::PainterPacker::
brush_state(void)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  if(!d->m_state.m_brush.state())
    {
      PainterBrushState e;
      e = d->m_pool.create_state(d->m_state.m_brush.cvalue());
      d->m_state.m_brush.set_state(e.m_d, e, true);
    }
  return d->m_state.m_brush.state();
}

void
fastuidraw::PainterPacker::
brush_state(const PainterBrushState &h)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  assert(h && h.alignment_packing() == d->m_alignment);
  d->m_state.m_brush.set_state(h.m_d, h, false);
}

const fastuidraw::PainterShader::const_handle&
fastuidraw::PainterPacker::
blend_shader(void) const
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  return d->m_state.m_blend_shader;
}

void
fastuidraw::PainterPacker::
blend_shader(const fastuidraw::PainterShader::const_handle &h)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  assert(h);
  d->m_state.m_blend_shader = h;
}

const fastuidraw::PainterPacker::ItemMatrix&
fastuidraw::PainterPacker::
item_matrix(void) const
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  return d->m_state.m_item_matrix.cvalue();
}

void
fastuidraw::PainterPacker::
item_matrix(const fastuidraw::PainterPacker::ItemMatrix &v)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_state.m_item_matrix.set(v);
}

const fastuidraw::PainterPacker::ItemMatrixState&
fastuidraw::PainterPacker::
item_matrix_state(void)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  if(!d->m_state.m_item_matrix.state())
    {
      ItemMatrixState e;
      e = d->m_pool.create_state(d->m_state.m_item_matrix.cvalue());
      d->m_state.m_item_matrix.set_state(e.m_d, e);
    }
  return d->m_state.m_item_matrix.state();
}

void
fastuidraw::PainterPacker::
item_matrix_state(const fastuidraw::PainterPacker::ItemMatrixState &h)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  assert(h && h.alignment_packing() == d->m_alignment);
  d->m_state.m_item_matrix.set_state(h.m_d, h);
}

const fastuidraw::PainterPacker::ClipEquations&
fastuidraw::PainterPacker::
clip_equations(void) const
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  return d->m_state.m_clipping.cvalue();
}

void
fastuidraw::PainterPacker::
clip_equations(const fastuidraw::PainterPacker::ClipEquations &v)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_state.m_clipping.set(v);
}

const fastuidraw::PainterPacker::ClipEquationsState&
fastuidraw::PainterPacker::
clip_equations_state(void)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  if(!d->m_state.m_clipping.state())
    {
      ClipEquationsState e;
      e = d->m_pool.create_state(d->m_state.m_clipping.cvalue());
      d->m_state.m_clipping.set_state(e.m_d, e);
    }
  return d->m_state.m_clipping.state();
}

void
fastuidraw::PainterPacker::
clip_equations_state(const fastuidraw::PainterPacker::ClipEquationsState &h)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  assert(h && h.alignment_packing() == d->m_alignment);
  d->m_state.m_clipping.set_state(h.m_d, h);
}

void
fastuidraw::PainterPacker::
vertex_shader_data(const fastuidraw::PainterPacker::VertexShaderData &shader_data)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_state.m_vert_shader_data.set(shader_data);
}

const fastuidraw::PainterPacker::VertexShaderDataState&
fastuidraw::PainterPacker::
vertex_shader_data(void)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  if(!d->m_state.m_vert_shader_data.state())
    {
      VertexShaderDataState e;
      e = d->m_pool.create_state(d->m_state.m_vert_shader_data.cvalue());
      d->m_state.m_vert_shader_data.set_state(e.m_d, e);
    }
  return d->m_state.m_vert_shader_data.state();
}

void
fastuidraw::PainterPacker::
vertex_shader_data(const fastuidraw::PainterPacker::VertexShaderDataState &h)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  assert(h && h.alignment_packing() == d->m_alignment);
  d->m_state.m_vert_shader_data.set_state(h.m_d, h);
}

void
fastuidraw::PainterPacker::
fragment_shader_data(const fastuidraw::PainterPacker::FragmentShaderData &shader_data)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_state.m_frag_shader_data.set(shader_data);
}

const fastuidraw::PainterPacker::FragmentShaderDataState&
fastuidraw::PainterPacker::
fragment_shader_data(void)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  if(!d->m_state.m_frag_shader_data.state())
    {
      FragmentShaderDataState e;
      e = d->m_pool.create_state(d->m_state.m_frag_shader_data.cvalue());
      d->m_state.m_frag_shader_data.set_state(e.m_d, e);
    }
  return d->m_state.m_frag_shader_data.state();
}

void
fastuidraw::PainterPacker::
fragment_shader_data(const fastuidraw::PainterPacker::FragmentShaderDataState &h)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  assert(h && h.alignment_packing() == d->m_alignment);
  d->m_state.m_frag_shader_data.set_state(h.m_d, h);
}

const fastuidraw::PainterShaderSet&
fastuidraw::PainterPacker::
default_shaders(void) const
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  return d->m_default_shaders;
}

fastuidraw::PainterSaveStatePool&
fastuidraw::PainterPacker::
save_state_pool(void)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  return d->m_pool;
}

const fastuidraw::PainterBackend::PerformanceHints&
fastuidraw::PainterPacker::
hints(void)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  return d->m_backend->hints();
}

void
fastuidraw::PainterPacker::
register_vert_shader(const fastuidraw::PainterShader::handle &shader)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_backend->register_vert_shader(shader);
}

void
fastuidraw::PainterPacker::
register_frag_shader(const fastuidraw::PainterShader::handle &shader)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_backend->register_frag_shader(shader);
}

void
fastuidraw::PainterPacker::
register_blend_shader(const fastuidraw::PainterShader::handle &shader)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_backend->register_blend_shader(shader);
}

void
fastuidraw::PainterPacker::
register_shader(const fastuidraw::PainterItemShader &p)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_backend->register_shader(p);
}

void
fastuidraw::PainterPacker::
register_shader(const fastuidraw::PainterStrokeShader &p)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_backend->register_shader(p);
}

void
fastuidraw::PainterPacker::
register_shader(const fastuidraw::PainterGlyphShader &p)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_backend->register_shader(p);
}

void
fastuidraw::PainterPacker::
register_shader(const fastuidraw::PainterShaderSet &p)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_backend->register_shader(p);
}

void
fastuidraw::PainterPacker::
target_resolution(int w, int h)
{
  PainterPackerPrivate *d;
  d = reinterpret_cast<PainterPackerPrivate*>(m_d);
  d->m_backend->target_resolution(w, h);
}

//////////////////////////////////////////
// fastuidraw::PainterSaveStateBase methods
fastuidraw::PainterSaveStateBase::
PainterSaveStateBase(void)
{
  m_d = NULL;
}

fastuidraw::PainterSaveStateBase::
PainterSaveStateBase(void *p):
  m_d(p)
{
  EntryBase *d;
  d = reinterpret_cast<EntryBase*>(m_d);
  if(d)
    {
      d->aquire();
    }
}

fastuidraw::PainterSaveStateBase::
PainterSaveStateBase(const PainterSaveStateBase &obj)
{
  m_d = obj.m_d;

  if(obj.m_d)
    {
      EntryBase *obj_d;
      obj_d = reinterpret_cast<EntryBase*>(obj.m_d);
      obj_d->aquire();
    }
}


fastuidraw::PainterSaveStateBase::
~PainterSaveStateBase()
{
  if(m_d)
    {
      EntryBase *d;
      d = reinterpret_cast<EntryBase*>(m_d);
      d->release();
      m_d = NULL;
    }
}


void
fastuidraw::PainterSaveStateBase::
operator=(const PainterSaveStateBase &obj)
{
  if(m_d != obj.m_d)
    {
      if(m_d)
        {
          EntryBase *d;
          d = reinterpret_cast<EntryBase*>(m_d);
          d->release();
        }

      m_d = obj.m_d;

      if(obj.m_d)
        {
          EntryBase *obj_d;
          obj_d = reinterpret_cast<EntryBase*>(obj.m_d);
          obj_d->aquire();
        }
    }
}

unsigned int
fastuidraw::PainterSaveStateBase::
alignment_packing(void) const
{
  EntryBase *d;
  d = reinterpret_cast<EntryBase*>(m_d);
  return (d != NULL) ? d->m_alignment : 0;
}

const void*
fastuidraw::PainterSaveStateBase::
raw_state(void) const
{
  EntryBase *d;
  d = reinterpret_cast<EntryBase*>(m_d);
  assert(d);
  assert(d->raw_state());
  return d->raw_state();
}

/////////////////////////////////////////////////////
// PainterSaveStatePool methods
fastuidraw::PainterSaveStatePool::
PainterSaveStatePool(int alignment)
{
  m_d = FASTUIDRAWnew PainterSaveStatePoolPrivate(alignment);
}

fastuidraw::PainterSaveStatePool::
~PainterSaveStatePool()
{
  PainterSaveStatePoolPrivate *d;
  d = reinterpret_cast<PainterSaveStatePoolPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterSaveState<fastuidraw::PainterBrush>
fastuidraw::PainterSaveStatePool::
create_state(const PainterBrush &value)
{
  PainterSaveStatePoolPrivate *d;
  Entry<PainterBrush> *e;

  d = reinterpret_cast<PainterSaveStatePoolPrivate*>(m_d);
  e = d->m_brush_pool.allocate(value, d->m_alignment);
  return fastuidraw::PainterSaveState<PainterBrush>(e);
}

fastuidraw::PainterSaveState<fastuidraw::PainterState::ClipEquations>
fastuidraw::PainterSaveStatePool::
create_state(const PainterState::ClipEquations &value)
{
  PainterSaveStatePoolPrivate *d;
  Entry<PainterState::ClipEquations> *e;

  d = reinterpret_cast<PainterSaveStatePoolPrivate*>(m_d);
  e = d->m_clip_equations_pool.allocate(value, d->m_alignment);
  return fastuidraw::PainterSaveState<PainterState::ClipEquations>(e);
}

fastuidraw::PainterSaveState<fastuidraw::PainterState::ItemMatrix>
fastuidraw::PainterSaveStatePool::
create_state(const PainterState::ItemMatrix &value)
{
  PainterSaveStatePoolPrivate *d;
  Entry<PainterState::ItemMatrix> *e;

  d = reinterpret_cast<PainterSaveStatePoolPrivate*>(m_d);
  e = d->m_item_matrix_pool.allocate(value, d->m_alignment);
  return fastuidraw::PainterSaveState<PainterState::ItemMatrix>(e);
}

fastuidraw::PainterSaveState<fastuidraw::PainterState::VertexShaderData>
fastuidraw::PainterSaveStatePool::
create_state(const PainterState::VertexShaderData &value)
{
  PainterSaveStatePoolPrivate *d;
  Entry<PainterState::VertexShaderData> *e;

  d = reinterpret_cast<PainterSaveStatePoolPrivate*>(m_d);
  e = d->m_vertex_shader_data_pool.allocate(value, d->m_alignment);
  return fastuidraw::PainterSaveState<PainterState::VertexShaderData>(e);
}

fastuidraw::PainterSaveState<fastuidraw::PainterState::FragmentShaderData>
fastuidraw::PainterSaveStatePool::
create_state(const PainterState::FragmentShaderData &value)
{
  PainterSaveStatePoolPrivate *d;
  Entry<PainterState::FragmentShaderData> *e;

  d = reinterpret_cast<PainterSaveStatePoolPrivate*>(m_d);
  e = d->m_fragment_shader_data_pool.allocate(value, d->m_alignment);
  return fastuidraw::PainterSaveState<PainterState::FragmentShaderData>(e);
}
