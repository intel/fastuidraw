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

#include "painter_packer.hpp"
#include "painter_packed_value_pool_private.hpp"
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
}

class fastuidraw::PainterPacker::per_draw_command
{
public:
  per_draw_command(const reference_counted_ptr<PainterDraw> &r,
                   const PainterBackend::ConfigurationBase &config);

  unsigned int
  attribute_room(void) const
  {
    FASTUIDRAWassert(m_attributes_written <= m_draw_command->m_attributes.size());
    return m_draw_command->m_attributes.size() - m_attributes_written;
  }

  unsigned int
  index_room(void) const
  {
    FASTUIDRAWassert(m_indices_written <= m_draw_command->m_indices.size());
    return m_draw_command->m_indices.size() - m_indices_written;
  }

  unsigned int
  store_room(void) const
  {
    unsigned int s;
    s = store_written();
    FASTUIDRAWassert(s <= m_draw_command->m_store.size());
    return m_draw_command->m_store.size() - s;
  }

  unsigned int
  store_written(void) const
  {
    return current_block() * 4;
  }

  void
  unmap(void)
  {
    m_draw_command->unmap(m_attributes_written, m_indices_written, store_written());
  }

  void
  pack_painter_state(const PainterPackerData &state,
                     PainterPacker *p, painter_state_location &out_data);

  bool //returns true if a draw break was needed
  pack_header(unsigned int header_size,
              uint32_t brush_shader,
              PainterBlendShader *blend_shader,
              PainterCompositeShader *composite_shader,
              BlendMode mode,
              PainterItemShader *item_shader,
              int z,
              const painter_state_location &loc,
              const std::list<reference_counted_ptr<PainterPacker::DataCallBack> > &call_backs,
              unsigned int *header_location);

  bool
  draw_break(const reference_counted_ptr<const PainterDraw::Action> &action)
  {
    if (action)
      {
        return m_draw_command->draw_break(action, m_indices_written);
      }
    return false;
  }

  reference_counted_ptr<PainterDraw> m_draw_command;
  unsigned int m_attributes_written, m_indices_written;

private:
  c_array<generic_data>
  allocate_store(unsigned int num_elements);

  unsigned int
  current_block(void) const
  {
    return m_store_blocks_written;
  }

  void
  pack_state_data(PainterPacker *p,
                  detail::PackedValuePoolBase::ElementBase *st_d,
                  uint32_t &location);

  template<typename T>
  void
  pack_state_data_from_value(const T &st, uint32_t &location);

  template<typename T>
  void
  pack_state_data(PainterPacker *p,
                  const PainterData::value<T> &obj,
                  uint32_t &location);

  unsigned int m_store_blocks_written;
  uint32_t m_brush_shader_mask;
  PainterShaderGroupPrivate m_prev_state;
  BlendMode m_prev_composite_mode;
};

//////////////////////////////////////////
// fastuidraw::PainterPacker::per_draw_command methods
fastuidraw::PainterPacker::per_draw_command::
per_draw_command(const reference_counted_ptr<PainterDraw> &r,
                 const PainterBackend::ConfigurationBase &config):
  m_draw_command(r),
  m_attributes_written(0),
  m_indices_written(0),
  m_store_blocks_written(0),
  m_brush_shader_mask(config.brush_shader_mask())
{
  m_prev_state.m_item_group = 0;
  m_prev_state.m_brush = 0;
  m_prev_state.m_composite_group = 0;
  m_prev_state.m_blend_group = 0;
}

fastuidraw::c_array<fastuidraw::generic_data>
fastuidraw::PainterPacker::per_draw_command::
allocate_store(unsigned int num_elements)
{
  c_array<generic_data> return_value;
  FASTUIDRAWassert(num_elements % 4 == 0);
  return_value = m_draw_command->m_store.sub_array(store_written(), num_elements);
  m_store_blocks_written += num_elements >> 2;
  return return_value;
}

template<typename T>
void
fastuidraw::PainterPacker::per_draw_command::
pack_state_data_from_value(const T &st, uint32_t &location)
{
  c_array<generic_data> dst;
  unsigned int data_sz;

  location = current_block();
  data_sz = st.data_size();
  dst = allocate_store(data_sz);
  st.pack_data(dst);
}

template<typename T>
void
fastuidraw::PainterPacker::per_draw_command::
pack_state_data(PainterPacker *p,
                const PainterData::value<T> &obj,
                uint32_t &location)
{
  if (obj.m_packed_value)
    {
      detail::PackedValuePoolBase::ElementBase *e;
      e = static_cast<detail::PackedValuePoolBase::ElementBase*>(obj.m_packed_value.opaque_data());
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

void
fastuidraw::PainterPacker::per_draw_command::
pack_state_data(PainterPacker *p,
                detail::PackedValuePoolBase::ElementBase *d,
                uint32_t &location)
{
  if (d->m_painter == p && d->m_draw_command_id == p->m_number_commands)
    {
      location = d->m_offset;
      return;
    }

  /* data not in current data store but packed in d->m_data, place
   * it onto the current store.
   */
  c_array<const generic_data> src;
  c_array<generic_data> dst;

  location = current_block();
  src = make_c_array(d->m_data);
  dst = allocate_store(src.size());
  std::copy(src.begin(), src.end(), dst.begin());

  d->m_painter = p;
  d->m_draw_command_id = p->m_accumulated_draws.size();
  d->m_offset = location;
}

void
fastuidraw::PainterPacker::per_draw_command::
pack_painter_state(const fastuidraw::PainterPackerData &state,
                   PainterPacker *p, painter_state_location &out_data)
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
fastuidraw::PainterPacker::per_draw_command::
pack_header(unsigned int header_size,
            uint32_t brush_shader,
            PainterBlendShader *blend_shader,
            PainterCompositeShader *composite_shader,
            BlendMode composite_mode,
            PainterItemShader *item_shader,
            int z,
            const painter_state_location &loc,
            const std::list<reference_counted_ptr<PainterPacker::DataCallBack> > &call_backs,
            unsigned int *header_location)
{
  bool return_value(false);
  c_array<generic_data> dst;
  PainterHeader header;

  *header_location = current_block();
  dst = allocate_store(header_size);

  PainterShaderGroupPrivate current;
  PainterShader::Tag composite;
  PainterShader::Tag blend;

  FASTUIDRAWassert(item_shader);
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
              vecN<unsigned int, num_stats> &stats,
              reference_counted_ptr<PainterBackend> backend):
  m_backend(backend),
  m_blend_shader(nullptr),
  m_composite_shader(nullptr),
  m_number_commands(0),
  m_clear_color_buffer(false),
  m_stats(stats)
{
  m_header_size = PainterHeader::data_size();
  m_default_brush.make_packed(pool);
}

fastuidraw::PainterPacker::
~PainterPacker()
{
}

void
fastuidraw::PainterPacker::
start_new_command(void)
{
  if (!m_accumulated_draws.empty())
    {
      per_draw_command &c(m_accumulated_draws.back());

      m_stats[PainterEnums::num_attributes] += c.m_attributes_written;
      m_stats[PainterEnums::num_indices] += c.m_indices_written;
      m_stats[PainterEnums::num_generic_datas] += c.store_written();

      c.unmap();
    }

  reference_counted_ptr<PainterDraw> r;
  r = m_backend->map_draw();
  ++m_number_commands;
  m_accumulated_draws.push_back(per_draw_command(r, m_backend->configuration_base()));
}

template<typename T>
unsigned int
fastuidraw::PainterPacker::
compute_room_needed_for_packing(const PainterData::value<T> &obj)
{
  if (obj.m_packed_value)
    {
      detail::PackedValuePoolBase::ElementBase *d;
      d = static_cast<detail::PackedValuePoolBase::ElementBase*>(obj.m_packed_value.opaque_data());
      if (d->m_painter == this && d->m_draw_command_id == m_number_commands)
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
}

unsigned int
fastuidraw::PainterPacker::
compute_room_needed_for_packing(const PainterPackerData &draw_state)
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
fastuidraw::PainterPacker::
upload_draw_state(const PainterPackerData &draw_state)
{
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
              ++m_stats[PainterEnums::num_draws];
            }
        }
    }
}

template<typename T>
void
fastuidraw::PainterPacker::
draw_generic_implement(const reference_counted_ptr<PainterItemShader> &shader,
                       const PainterPackerData &draw,
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

          ++m_stats[PainterEnums::num_headers];
          allocate_header = false;
          draw_break_added = cmd.pack_header(m_header_size,
                                             fetch_value(draw.m_brush).shader(),
                                             m_blend_shader,
                                             m_composite_shader,
                                             m_composite_mode,
                                             shader.get(),
                                             z, m_painter_state_location,
                                             m_callback_list,
                                             &header_loc);
          if (draw_break_added)
            {
              ++m_stats[PainterEnums::num_draws];
            }
        }

      /* copy attribute data and get offset into attribute buffer
       * where attributes are copied
       */
      unsigned int attrib_offset;

      if (needed_attrib_room > 0)
        {
          c_array<PainterAttribute> attrib_dst_ptr;
          c_array<uint32_t> header_dst_ptr;

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
      c_array<PainterIndex> index_dst_ptr;

      index_dst_ptr = cmd.m_draw_command->m_indices.sub_array(cmd.m_indices_written, num_indices);
      src.write_indices(index_dst_ptr, attrib_offset, chunk);
      cmd.m_indices_written += index_dst_ptr.size();
    }
}

void
fastuidraw::PainterPacker::
add_callback(const reference_counted_ptr<DataCallBack> &callback)
{
  DataCallBackPrivate *cd;

  FASTUIDRAWassert(callback && !callback->active());
  if (!callback || callback->active())
    {
      return;
    }

  cd = static_cast<DataCallBackPrivate*>(callback->m_d);
  cd->m_list = &m_callback_list;
  cd->m_iterator = cd->m_list->insert(cd->m_list->begin(), callback);
}

void
fastuidraw::PainterPacker::
remove_callback(const reference_counted_ptr<DataCallBack> &callback)
{
  DataCallBackPrivate *cd;

  FASTUIDRAWassert(callback && callback->active());
  if (!callback || !callback->active())
    {
      return;
    }

  cd = static_cast<DataCallBackPrivate*>(callback->m_d);

  FASTUIDRAWassert(cd->m_list == &m_callback_list);
  if (cd->m_list != &m_callback_list)
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
  FASTUIDRAWassert(m_accumulated_draws.empty());
  m_surface = surface;
  m_clear_color_buffer = clear_color_buffer;
  m_begin_new_target = true;
  start_new_command();
  m_last_binded_image = nullptr;
}

void
fastuidraw::PainterPacker::
flush_implement(void)
{
  if (!m_accumulated_draws.empty())
    {
      per_draw_command &c(m_accumulated_draws.back());
      m_stats[PainterEnums::num_attributes] += c.m_attributes_written;
      m_stats[PainterEnums::num_indices] += c.m_indices_written;
      m_stats[PainterEnums::num_generic_datas] += c.store_written();
      c.unmap();
    }

  m_stats[PainterEnums::num_draws] += m_accumulated_draws.size();
  m_stats[PainterEnums::num_ends] += 1u;

  m_backend->on_pre_draw(m_surface, m_clear_color_buffer, m_begin_new_target);
  for(per_draw_command &cmd : m_accumulated_draws)
    {
      FASTUIDRAWassert(cmd.m_draw_command->unmapped());
      cmd.m_draw_command->draw();
    }
  m_accumulated_draws.clear();
  m_begin_new_target = false;
  m_clear_color_buffer = false;
  m_last_binded_image = nullptr;
}

void
fastuidraw::PainterPacker::
flush(void)
{
  if (m_accumulated_draws.size() > 1
      || m_accumulated_draws.back().m_attributes_written > 0
      || m_accumulated_draws.back().m_indices_written > 0)
    {
      flush_implement();
      start_new_command();
    }
}

void
fastuidraw::PainterPacker::
end(void)
{
  flush_implement();
  m_backend->on_post_draw();
  m_surface.clear();
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend::Surface>&
fastuidraw::PainterPacker::
surface(void) const
{
  return m_surface;
}

void
fastuidraw::PainterPacker::
draw_break(const reference_counted_ptr<const PainterDraw::Action> &action)
{
  if (m_accumulated_draws.back().draw_break(action))
    {
      ++m_stats[PainterEnums::num_draws];
    }
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
  AttributeIndexSrcFromArray src(attrib_chunks, index_chunks, index_adjusts, attrib_chunk_selector);
  draw_generic_implement(shader, draw, src, z);
}

void
fastuidraw::PainterPacker::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
             const PainterPackerData &data,
             const PainterAttributeWriter &src,
             int z)
{
  draw_generic_implement(shader, data, src, z);
}

unsigned int
fastuidraw::PainterPacker::
current_indices_written(void) { return m_accumulated_draws.back().m_indices_written; }

unsigned int
fastuidraw::PainterPacker::
current_draw(void) { return m_accumulated_draws.size(); }

///////////////////////////////
//
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
