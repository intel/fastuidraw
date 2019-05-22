/*!
 * \file stroking_attribute_writer.cpp
 * \brief file stroking_attribute_writer.cpp
 *
 * Copyright 2019 by Intel.
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
#include <fastuidraw/painter/attribute_data/stroking_attribute_writer.hpp>
#include <fastuidraw/painter/attribute_data/stroked_point.hpp>
#include <fastuidraw/painter/attribute_data/arc_stroked_point.hpp>

namespace
{
  enum drawing_type_t
    {
      drawing_edges = 0,
      drawing_joins,
      drawing_caps,

      number_drawing_types
    };

  enum offset_t
    {
      drawing_type_offset = 0,
      item_offset,
      z_offset,

      number_offsets,
    };

  typedef fastuidraw::StrokingAttributeWriter::StrokingMethod StrokingMethod;

  template<typename T>
  class DrawnItem
  {};

  template<>
  class DrawnItem<fastuidraw::PathEffect::segment_chain>
  {
  public:
    enum
      {
        value = drawing_edges
      };

    DrawnItem(void):
      m_depth_range_size(0),
      m_num_attribs(0),
      m_num_indices(0)
    {}

    DrawnItem(const fastuidraw::PathEffect::segment_chain &chain,
              enum fastuidraw::PainterEnums::stroking_method_t tp,
              const StrokingMethod&);

    void
    write_data(fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
               fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
               unsigned int attrib_location,
               fastuidraw::PainterAttributeWriter::WriteState *state,
               enum fastuidraw::PainterEnums::stroking_method_t tp,
               const StrokingMethod &method) const;

    unsigned int
    num_attribs(void) const
    {
      return m_num_attribs;
    }

    unsigned int
    num_indices(void) const
    {
      return m_num_indices;
    }

    unsigned int
    depth_range_size(void) const
    {
      return m_depth_range_size;
    }

  private:
    unsigned int m_depth_range_size;
    unsigned int m_num_attribs;
    unsigned int m_num_indices;
    fastuidraw::PathEffect::segment_chain m_chain;
  };

  template<>
  class DrawnItem<fastuidraw::PathEffect::join>
  {
  public:
    enum
      {
        value = drawing_joins
      };

    DrawnItem(void):
      m_num_attribs(0),
      m_num_indices(0)
    {}

    DrawnItem(const fastuidraw::PathEffect::join &join,
              enum fastuidraw::PainterEnums::stroking_method_t tp,
              const StrokingMethod &method);

    void
    write_data(fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
               fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
               unsigned int attrib_location,
               fastuidraw::PainterAttributeWriter::WriteState *state,
               enum fastuidraw::PainterEnums::stroking_method_t tp,
               const StrokingMethod &method) const;

    unsigned int
    num_attribs(void) const
    {
      return m_num_attribs;
    }

    unsigned int
    num_indices(void) const
    {
      return m_num_indices;
    }

    unsigned int
    depth_range_size(void) const
    {
      return 1;
    }

  private:
    unsigned int m_num_attribs;
    unsigned int m_num_indices;
    const fastuidraw::PathEffect::join *m_join;
  };

  template<>
  class DrawnItem<fastuidraw::PathEffect::cap>
  {
  public:
    enum
      {
        value = drawing_caps
      };

    DrawnItem(void):
      m_num_attribs(0),
      m_num_indices(0)
    {}

    DrawnItem(const fastuidraw::PathEffect::cap &cap,
              enum fastuidraw::PainterEnums::stroking_method_t tp,
              const StrokingMethod &method);

    void
    write_data(fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
               fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
               unsigned int attrib_location,
               fastuidraw::PainterAttributeWriter::WriteState *state,
               enum fastuidraw::PainterEnums::stroking_method_t tp,
               const StrokingMethod &method) const;

    unsigned int
    num_attribs(void) const
    {
      return m_num_attribs;
    }

    unsigned int
    num_indices(void) const
    {
      return m_num_indices;
    }

    unsigned int
    depth_range_size(void) const
    {
      return 1;
    }

  private:
    unsigned int m_num_attribs;
    unsigned int m_num_indices;
    const fastuidraw::PathEffect::cap *m_cap;
  };

  template<typename T>
  class DrawnItemSourceFetcher
  {};

  template<>
  class DrawnItemSourceFetcher<fastuidraw::PathEffect::segment_chain>
  {
  public:
    DrawnItemSourceFetcher(void):
      m_src(nullptr),
      m_size(0)
    {}

    void
    reset(void)
    {
      m_src = nullptr;
      m_size = 0;
    }

    void
    set(const fastuidraw::PathEffect::Storage &src)
    {
      m_src = &src;
      m_size = m_src->number_chains();
    }

    unsigned int
    size(void) const
    {
      return m_size;
    }

    fastuidraw::PathEffect::segment_chain
    operator[](unsigned int I) const
    {
      return m_src->chain(I);
    }

  private:
    const fastuidraw::PathEffect::Storage *m_src;
    unsigned int m_size;
  };

  template<>
  class DrawnItemSourceFetcher<fastuidraw::PathEffect::join>
  {
  public:
    void
    set(const fastuidraw::PathEffect::Storage &src)
    {
      m_src = src.joins();
    }

    void
    reset(void)
    {
      m_src = fastuidraw::c_array<const fastuidraw::PathEffect::join>();
    }

    unsigned int
    size(void) const
    {
      return m_src.size();
    }

    const fastuidraw::PathEffect::join&
    operator[](unsigned int I) const
    {
      return m_src[I];
    }

  private:
    fastuidraw::c_array<const fastuidraw::PathEffect::join> m_src;
  };

  template<>
  class DrawnItemSourceFetcher<fastuidraw::PathEffect::cap>
  {
  public:
    void
    set(const fastuidraw::PathEffect::Storage &src)
    {
      m_src = src.caps();
    }

    void
    reset(void)
    {
      m_src = fastuidraw::c_array<const fastuidraw::PathEffect::cap>();
    }

    unsigned int
    size(void) const
    {
      return m_src.size();
    }

    const fastuidraw::PathEffect::cap&
    operator[](unsigned int I) const
    {
      return m_src[I];
    }

  private:
    fastuidraw::c_array<const fastuidraw::PathEffect::cap> m_src;
  };

  template<typename T>
  class DrawnFetchedItems
  {
  public:
    enum
      {
        drawing_type = DrawnItem<T>::value
      };

    void
    reset(void)
    {
      m_z_size = 0;
      m_fetcher.reset();
    }

    unsigned int
    z_size(void) const
    {
      return m_z_size;
    }

    bool
    empty(void) const
    {
      return m_fetcher.size() == 0;
    }

    void
    set(const fastuidraw::PathEffect::Storage &src,
        enum fastuidraw::PainterEnums::stroking_method_t tp,
        const StrokingMethod &method);

    bool
    update_state(fastuidraw::PainterAttributeWriter::WriteState *state,
                 enum fastuidraw::PainterEnums::stroking_method_t tp,
                 const StrokingMethod &method, DrawnItem<T> &Q) const;

    bool
    update_state(fastuidraw::PainterAttributeWriter::WriteState *state,
                 enum fastuidraw::PainterEnums::stroking_method_t tp,
                 const StrokingMethod &method) const
    {
      DrawnItem<T> Q;
      return update_state(state, tp, method, Q);
    }

    bool
    write_data(fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
               fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
               unsigned int attrib_location,
               fastuidraw::PainterAttributeWriter::WriteState *state,
               unsigned int *num_attribs_written,
               unsigned int *num_indices_written,
               enum fastuidraw::PainterEnums::stroking_method_t tp,
               const StrokingMethod &method) const;

  private:
    unsigned int m_z_size;
    DrawnItemSourceFetcher<T> m_fetcher;
  };

  class StrokingAttributeWriterPrivate
  {
  public:
    bool
    update_state_values(fastuidraw::PainterAttributeWriter::WriteState *state) const;

    fastuidraw::vecN<fastuidraw::PainterItemShader*, number_drawing_types> m_shader;
    fastuidraw::vecN<enum fastuidraw::PainterEnums::stroking_method_t, number_drawing_types> m_stroking_type;
    StrokingMethod m_method;
    bool m_requires_coverage_buffer;
    DrawnFetchedItems<fastuidraw::PathEffect::segment_chain> m_segments;
    DrawnFetchedItems<fastuidraw::PathEffect::join> m_joins;
    DrawnFetchedItems<fastuidraw::PathEffect::cap> m_caps;
  };
}

/////////////////////////////////////////////////////////////
// DrawnItem<fastuidraw::TessellatedPath::segment_chain> methods
DrawnItem<fastuidraw::PathEffect::segment_chain>::
DrawnItem(const fastuidraw::PathEffect::segment_chain &chain,
          enum fastuidraw::PainterEnums::stroking_method_t tp,
          const StrokingMethod&):
  m_chain(chain)
{
  using namespace fastuidraw;
  if (tp == PainterEnums::stroking_method_linear)
    {
      StrokedPointPacking::pack_segment_chain_size(chain,
                                                   &m_depth_range_size,
                                                   &m_num_attribs,
                                                   &m_num_indices);
    }
  else
    {
      ArcStrokedPointPacking::pack_segment_chain_size(chain,
                                                      &m_depth_range_size,
                                                      &m_num_attribs,
                                                      &m_num_indices);
    }
}

void
DrawnItem<fastuidraw::PathEffect::segment_chain>::
write_data(fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
           fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
           unsigned int attrib_location,
           fastuidraw::PainterAttributeWriter::WriteState *state,
           enum fastuidraw::PainterEnums::stroking_method_t tp,
           const StrokingMethod &) const
{
  using namespace fastuidraw;

  FASTUIDRAWassert(dst_attribs.size() == num_attribs());
  FASTUIDRAWassert(dst_indices.size() == num_indices());

  if (tp == PainterEnums::stroking_method_linear)
    {
      StrokedPointPacking::pack_segment_chain(m_chain,
                                              state->m_state[z_offset],
                                              dst_attribs,
                                              dst_indices,
                                              attrib_location);
    }
  else
    {
      ArcStrokedPointPacking::pack_segment_chain(m_chain,
                                                 state->m_state[z_offset],
                                                 dst_attribs,
                                                 dst_indices,
                                                 attrib_location);
    }
}

/////////////////////////////////////////
// DrawnItem<fastuidraw::PathEffect::join> methods
DrawnItem<fastuidraw::PathEffect::join>::
DrawnItem(const fastuidraw::PathEffect::join &join,
          enum fastuidraw::PainterEnums::stroking_method_t tp,
          const StrokingMethod &method):
  m_join(&join)
{
  using namespace fastuidraw;
  if (tp == PainterEnums::stroking_method_linear)
    {
      #define CASE(X)                                                   \
        case X:                                                         \
          m_num_attribs = StrokedPointPacking::packing_size<enum PainterEnums::join_style, X>::number_attributes; \
          m_num_indices = StrokedPointPacking::packing_size<enum PainterEnums::join_style, X>::number_indices; \
          break

      switch (method.m_js)
        {
        default:
          FASTUIDRAWassert(!"Invalid join type passed to StrokingAttributeWriter");
          CASE(PainterEnums::no_joins);
          CASE(PainterEnums::bevel_joins);
          CASE(PainterEnums::miter_clip_joins);
          CASE(PainterEnums::miter_bevel_joins);
          CASE(PainterEnums::miter_joins);
        case PainterEnums::rounded_joins:
          StrokedPointPacking::pack_rounded_join_size(join, method.m_thresh, &m_num_attribs, &m_num_indices);
        }

      #undef CASE
    }
  else
    {
      FASTUIDRAWassert(method.m_js == PainterEnums::rounded_joins);
      ArcStrokedPointPacking::pack_join_size(join, &m_num_attribs, &m_num_indices);
    }
}

void
DrawnItem<fastuidraw::PathEffect::join>::
write_data(fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
           fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
           unsigned int attrib_location,
           fastuidraw::PainterAttributeWriter::WriteState *state,
           enum fastuidraw::PainterEnums::stroking_method_t tp,
           const StrokingMethod &method) const
{
  using namespace fastuidraw;
  if (tp == PainterEnums::stroking_method_linear)
    {
      StrokedPointPacking::pack_join(method.m_js,
                                     *m_join,
                                     state->m_state[z_offset],
                                     dst_attribs,
                                     dst_indices,
                                     attrib_location);
    }
  else
    {
      FASTUIDRAWassert(method.m_js == PainterEnums::rounded_joins);
      ArcStrokedPointPacking::pack_join(*m_join,
                                        state->m_state[z_offset],
                                        dst_attribs,
                                        dst_indices,
                                        attrib_location);
    }
}

/////////////////////////////////////////
// DrawnItem<fastuidraw::TessellatedPath::cap> methods
DrawnItem<fastuidraw::PathEffect::cap>::
DrawnItem(const fastuidraw::PathEffect::cap &cap,
          enum fastuidraw::PainterEnums::stroking_method_t tp,
          const StrokingMethod &method):
  m_cap(&cap)
{
  using namespace fastuidraw;
  if (tp == PainterEnums::stroking_method_linear)
    {
      #define CASE(X) case X:                                                 \
         m_num_attribs = StrokedPointPacking::packing_size<enum StrokedPointPacking::cap_type_t, X>::number_attributes; \
         m_num_indices = StrokedPointPacking::packing_size<enum StrokedPointPacking::cap_type_t, X>::number_indices; \
         break

      switch (method.m_cp)
        {
          CASE(StrokedPointPacking::square_cap);
          CASE(StrokedPointPacking::adjustable_cap);
          CASE(StrokedPointPacking::flat_cap);

        case StrokedPointPacking::rounded_cap:
          StrokedPointPacking::pack_rounded_cap_size(method.m_thresh, &m_num_attribs, &m_num_indices);
        }

       #undef CASE
    }
  else
    {
      m_num_attribs = ArcStrokedPointPacking::num_attributes_per_cap;
      m_num_indices = ArcStrokedPointPacking::num_indices_per_cap;
    }
}

void
DrawnItem<fastuidraw::PathEffect::cap>::
write_data(fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
           fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
           unsigned int attrib_location,
           fastuidraw::PainterAttributeWriter::WriteState *state,
           enum fastuidraw::PainterEnums::stroking_method_t tp,
           const StrokingMethod &method) const
{
  using namespace fastuidraw;
  if (tp == PainterEnums::stroking_method_linear)
    {
      StrokedPointPacking::pack_cap(method.m_cp,
                                    *m_cap,
                                    state->m_state[z_offset],
                                    dst_attribs,
                                    dst_indices,
                                    attrib_location);
    }
  else
    {
      ArcStrokedPointPacking::pack_cap(*m_cap,
                                       state->m_state[z_offset],
                                       dst_attribs,
                                       dst_indices,
                                       attrib_location);
    }
}

/////////////////////////////////////////////////
// DrawnFetchedItems methods
template<typename T>
void
DrawnFetchedItems<T>::
set(const fastuidraw::PathEffect::Storage &src,
    enum fastuidraw::PainterEnums::stroking_method_t tp,
    const StrokingMethod &method)
{
  m_z_size = 0;
  m_fetcher.set(src);
  for (unsigned int i = 0, endi = m_fetcher.size(); i < endi; ++i)
    {
      DrawnItem<T> q(m_fetcher[i], tp, method);
      m_z_size += q.depth_range_size();
    }
}

template<typename T>
bool
DrawnFetchedItems<T>::
update_state(fastuidraw::PainterAttributeWriter::WriteState *state,
             enum fastuidraw::PainterEnums::stroking_method_t tp,
             const StrokingMethod &method,
             DrawnItem<T> &Q) const
{
  unsigned int &item(state->m_state[item_offset]);

  FASTUIDRAWassert(drawing_type == state->m_state[drawing_type_offset]);
  if (item >= m_fetcher.size())
    {
      return false;
    }

  Q = DrawnItem<T>(m_fetcher[item], tp, method);
  state->m_min_attributes_for_next = Q.num_attribs();
  state->m_min_indices_for_next = Q.num_indices();

  return true;
}

template<typename T>
bool
DrawnFetchedItems<T>::
write_data(fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
           fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
           unsigned int attrib_location,
           fastuidraw::PainterAttributeWriter::WriteState *state,
           unsigned int *num_attribs_written,
           unsigned int *num_indices_written,
           enum fastuidraw::PainterEnums::stroking_method_t tp,
           const StrokingMethod &method) const
{
  unsigned int &item(state->m_state[item_offset]);
  unsigned int &z(state->m_state[z_offset]);
  unsigned int vertex_offset(0), index_offset(0);
  DrawnItem<T> q(m_fetcher[item], tp, method);
  bool more_to_draw(true);

  while (dst_attribs.size() >= vertex_offset + q.num_attribs()
         && dst_indices.size() >= index_offset + q.num_indices()
         && more_to_draw)
    {
      fastuidraw::c_array<fastuidraw::PainterAttribute> tmp_attribs;
      fastuidraw::c_array<fastuidraw::PainterIndex> tmp_indices;

      /* The class T relies on ArcStrokedPointPacking and
       * StrokedPointPacking to do the actual attribute
       * packing. Those methods require that the size of the
       * arrays passed to them are exactly the sizes they
       * request.
       */
      tmp_attribs = dst_attribs.sub_array(vertex_offset, q.num_attribs());
      tmp_indices = dst_indices.sub_array(index_offset, q.num_indices());

      /* The z-value present is the z-value after the
       * last item completed. We want the z-value to
       * START at z - q.depth_range_size(), this is why
       * we decrement z -before- drawing.
       */
      z -= q.depth_range_size();
      q.write_data(tmp_attribs, tmp_indices,
                   attrib_location + vertex_offset,
                   state, tp, method);

      vertex_offset += tmp_attribs.size();
      index_offset += tmp_indices.size();

      ++item;
      more_to_draw = update_state(state, tp, method, q);
    }

  *num_attribs_written = vertex_offset;
  *num_indices_written = index_offset;

  return more_to_draw;
}

//////////////////////////////////////////
// StrokingAttributeWriterPrivate methods
bool
StrokingAttributeWriterPrivate::
update_state_values(fastuidraw::PainterAttributeWriter::WriteState *state) const
{
  unsigned int &drawing_type(state->m_state[drawing_type_offset]);
  unsigned int &item(state->m_state[item_offset]);

  if (drawing_type == drawing_edges)
    {
      if (m_segments.update_state(state, m_stroking_type[drawing_type], m_method))
        {
          state->m_item_shader_override = m_shader[drawing_type];
          state->m_item_coverage_shader_override = m_shader[drawing_type]->coverage_shader().get();
          return true;
        }
      ++drawing_type;
      item = 0;
    }

  if (drawing_type == drawing_joins)
    {
      if (m_joins.update_state(state, m_stroking_type[drawing_type], m_method))
        {
          state->m_item_shader_override = m_shader[drawing_type];
          state->m_item_coverage_shader_override = m_shader[drawing_type]->coverage_shader().get();
          return true;
        }
      ++drawing_type;
      item = 0;
    }

  if (drawing_type == drawing_caps)
    {
      if (m_caps.update_state(state, m_stroking_type[drawing_type], m_method))
        {
          state->m_item_shader_override = m_shader[drawing_type];
          state->m_item_coverage_shader_override = m_shader[drawing_type]->coverage_shader().get();
          return true;
        }
      ++drawing_type;
    }

  return false;
}

/////////////////////////////////////////////
// fastuidraw::StrokingAttributeWriter methods
fastuidraw::StrokingAttributeWriter::
StrokingAttributeWriter(void)
{
  m_d = FASTUIDRAWnew StrokingAttributeWriterPrivate();
}

fastuidraw::StrokingAttributeWriter::
~StrokingAttributeWriter()
{
  StrokingAttributeWriterPrivate *d;
  d = static_cast<StrokingAttributeWriterPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::StrokingAttributeWriter::
set_source(const PathEffect::Storage &src,
           const PainterStrokeShader &shader,
           const StrokingMethod &method,
           enum PainterEnums::stroking_method_t tp,
           enum PainterStrokeShader::shader_type_t aa,
           bool draw_edges)
{
  StrokingAttributeWriterPrivate *d;
  d = static_cast<StrokingAttributeWriterPrivate*>(m_d);

  /* clear previous state */
  d->m_segments.reset();
  d->m_joins.reset();
  d->m_caps.reset();
  d->m_shader[drawing_edges] = nullptr;
  d->m_shader[drawing_joins] = nullptr;
  d->m_shader[drawing_caps] = nullptr;

  d->m_method = method;
  if (d->m_method.m_cp != StrokedPointPacking::flat_cap)
    {
      enum PainterEnums::stroking_method_t ctp;

      /* packing caps as ArcStrokedPoint is only allowed for rounded-caps */
      ctp = (d->m_method.m_cp == StrokedPointPacking::rounded_cap) ? tp : PainterEnums::stroking_method_linear;
      d->m_caps.set(src, ctp, d->m_method);
      if (!d->m_caps.empty())
        {
          d->m_shader[drawing_caps] = shader.shader(ctp, aa).get();
          d->m_stroking_type[drawing_caps] = ctp;
        }
    }

  if (d->m_method.m_js != PainterEnums::no_joins)
    {
      enum PainterEnums::stroking_method_t jtp;

      /* packing caps as ArcStrokedPoint is only allowed for rounded-joins */
      jtp = (d->m_method.m_js == PainterEnums::rounded_joins) ? tp : PainterEnums::stroking_method_linear;
      d->m_joins.set(src, jtp, d->m_method);
      if (!d->m_joins.empty())
        {
          d->m_shader[drawing_joins] = shader.shader(jtp, aa).get();
          d->m_stroking_type[drawing_joins] = jtp;
        }
    }

  if (draw_edges)
    {
      d->m_segments.set(src, tp, d->m_method);
      if (!d->m_segments.empty())
        {
          d->m_shader[drawing_edges] = shader.shader(tp, aa).get();
          d->m_stroking_type[drawing_edges] = tp;
        }
    }

  d->m_requires_coverage_buffer =
    (d->m_shader[drawing_edges] && d->m_shader[drawing_edges]->coverage_shader())
    || (d->m_shader[drawing_joins] && d->m_shader[drawing_joins]->coverage_shader())
    || (d->m_shader[drawing_caps] && d->m_shader[drawing_caps]->coverage_shader());
}

bool
fastuidraw::StrokingAttributeWriter::
requires_coverage_buffer(void) const
{
  StrokingAttributeWriterPrivate *d;
  d = static_cast<StrokingAttributeWriterPrivate*>(m_d);
  return d->m_requires_coverage_buffer;
}

unsigned int
fastuidraw::StrokingAttributeWriter::
state_length(void) const
{
  return number_offsets;
}

bool
fastuidraw::StrokingAttributeWriter::
initialize_state(WriteState *state) const
{
  unsigned int z_total;
  StrokingAttributeWriterPrivate *d;

  d = static_cast<StrokingAttributeWriterPrivate*>(m_d);
  z_total = d->m_segments.z_size()
    + d->m_joins.z_size() + d->m_caps.z_size();

  state->m_state[drawing_type_offset] = drawing_edges;
  state->m_state[item_offset] = 0;
  state->m_state[z_offset] = z_total;
  state->m_z_range.m_begin = 0;
  state->m_z_range.m_end = z_total;

  return d->update_state_values(state);
}

void
fastuidraw::StrokingAttributeWriter::
on_new_store(WriteState *state) const
{
  FASTUIDRAWunused(state);
}

bool
fastuidraw::StrokingAttributeWriter::
write_data(c_array<PainterAttribute> dst_attribs,
           c_array<PainterIndex> dst_indices,
           unsigned int attrib_location,
           WriteState *state,
           unsigned int *num_attribs_written,
           unsigned int *num_indices_written) const
{
  unsigned int drawing_type(state->m_state[drawing_type_offset]);
  bool more;
  StrokingAttributeWriterPrivate *d;

  d = static_cast<StrokingAttributeWriterPrivate*>(m_d);
  switch (drawing_type)
    {
    case drawing_edges:
      more = d->m_segments.write_data(dst_attribs, dst_indices,
                                      attrib_location, state,
                                      num_attribs_written,
                                      num_indices_written,
                                      d->m_stroking_type[drawing_type],
                                      d->m_method);
      break;

    case drawing_joins:
      more = d->m_joins.write_data(dst_attribs, dst_indices,
                                   attrib_location, state,
                                   num_attribs_written,
                                   num_indices_written,
                                   d->m_stroking_type[drawing_type],
                                   d->m_method);
      break;

    case drawing_caps:
      more = d->m_caps.write_data(dst_attribs, dst_indices,
                                  attrib_location, state,
                                  num_attribs_written,
                                  num_indices_written,
                                  d->m_stroking_type[drawing_type],
                                  d->m_method);
      break;

    default:
      return false;
    }

  if (!more)
    {
      more = d->update_state_values(state);
    }

  return more;
}
