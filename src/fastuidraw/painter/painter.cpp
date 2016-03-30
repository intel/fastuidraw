/*!
 * \file painter.cpp
 * \brief file painter.cpp
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
#include <bitset>

#include <fastuidraw/util/math.hpp>
#include <fastuidraw/painter/painter.hpp>

#include "../private/util_private.hpp"

namespace
{
  class ZDelayedAction;
  class ZDataCallBack;

  class change_header_z
  {
  public:
    change_header_z(fastuidraw::const_c_array<fastuidraw::generic_data> original_value,
                    fastuidraw::c_array<fastuidraw::generic_data> mapped_location)
    {
      m_blend_shader = fastuidraw::unpack_bits(fastuidraw::PainterPacking::blend_shader_bit0,
                                              fastuidraw::PainterPacking::blend_shader_num_bits,
                                              original_value[fastuidraw::PainterPacking::z_blend_shader_offset].u);
      m_mapped = &mapped_location[fastuidraw::PainterPacking::z_blend_shader_offset].u;
    }

    //value of blend shader where z is written
    uint32_t m_blend_shader;

    //location to which to write to overwrite value.
    uint32_t *m_mapped;
  };

  class ZDelayedAction:public fastuidraw::PainterDrawCommand::DelayedAction
  {
  public:
    typedef fastuidraw::reference_counted_ptr<ZDelayedAction> handle;
    typedef fastuidraw::reference_counted_ptr<const ZDelayedAction> const_handle;

    void
    finalize_z(unsigned int z)
    {
      m_z_to_write = z;
      perform_action();
    }

  protected:

    virtual
    void
    action(const fastuidraw::PainterDrawCommand::const_handle &)
    {
      for(unsigned int i = 0, endi = m_dests.size(); i < endi; ++i)
        {
          uint32_t zbits, blendbits;

          zbits = fastuidraw::pack_bits(fastuidraw::PainterPacking::z_bit0,
                                       fastuidraw::PainterPacking::z_num_bits,
                                       m_z_to_write);
          blendbits = fastuidraw::pack_bits(fastuidraw::PainterPacking::blend_shader_bit0,
                                           fastuidraw::PainterPacking::blend_shader_num_bits,
                                           m_dests[i].m_blend_shader);
          *m_dests[i].m_mapped = zbits | blendbits;
        }
    }

  private:
    friend class ZDataCallBack;
    uint32_t m_z_to_write;
    std::vector<change_header_z> m_dests;
  };

  class ZDataCallBack:public fastuidraw::PainterPacker::DataCallBack
  {
  public:
    typedef fastuidraw::reference_counted_ptr<ZDataCallBack> handle;
    typedef fastuidraw::reference_counted_ptr<const ZDataCallBack> const_handle;

    virtual
    void
    current_draw_command(const fastuidraw::PainterDrawCommand::const_handle &h)
    {
      if(h != m_cmd)
        {
          m_cmd = h;
          m_current = FASTUIDRAWnew ZDelayedAction();
          m_actions.push_back(m_current);
          m_cmd->add_action(m_current);
        }
    }

    virtual
    void
    header_added(fastuidraw::const_c_array<fastuidraw::generic_data> original_value,
                 fastuidraw::c_array<fastuidraw::generic_data> mapped_location)
    {
      m_current->m_dests.push_back(change_header_z(original_value, mapped_location));
    }

    std::vector<fastuidraw::PainterDrawCommand::DelayedAction::handle> m_actions;

  private:
    fastuidraw::PainterDrawCommand::const_handle m_cmd;
    ZDelayedAction::handle m_current;
  };

  bool
  all_pts_culled_by_one_half_plane(const fastuidraw::vecN<fastuidraw::vec3, 4> &pts,
                                   const fastuidraw::PainterState::ClipEquations &eq)
  {
    for(int i = 0; i < 4; ++i)
      {
        if(fastuidraw::dot(pts[0], eq.m_clip_equations[i]) < 0.0f
           && fastuidraw::dot(pts[1], eq.m_clip_equations[i]) < 0.0f
           && fastuidraw::dot(pts[2], eq.m_clip_equations[i]) < 0.0f
           && fastuidraw::dot(pts[3], eq.m_clip_equations[i]) < 0.0f)
          {
            return true;
          }
      }
    return false;
  }

  void
  draw_half_plane_complement(fastuidraw::Painter *painter,
                             const fastuidraw::vec3 &plane,
                             const ZDataCallBack::handle &callback)
  {
    if(fastuidraw::t_abs(plane.x()) > fastuidraw::t_abs(plane.y()))
      {
        float a, b, c, d;
        /* a so that A * a + B * -1 + C = 0 -> a = (+B - C) / A
           b so that A * a + B * +1 + C = 0 -> b = (-B - C) / A
         */
        a = (+plane.y() - plane.z()) / plane.x();
        b = (-plane.y() - plane.z()) / plane.x();

        /* the two points are then (a, -1) and (b, 1). Grab
           (c, -1) and (d, 1) so that they are on the correct side
           of the half plane
         */
        if(plane.x() > 0.0f)
          {
            /* increasing x then make the plane more positive,
               and we want the negative side, so take c and
               d to left of a and b.
             */
            c = fastuidraw::t_min(-1.0f, a);
            d = fastuidraw::t_min(-1.0f, b);
          }
        else
          {
            c = fastuidraw::t_max(1.0f, a);
            d = fastuidraw::t_max(1.0f, b);
          }
        /* the 4 points of the polygon are then
           (a, -1), (c, -1), (d, 1), (b, 1).
         */
        painter->draw_quad(fastuidraw::vec2(a, -1.0f),
                           fastuidraw::vec2(c, -1.0f),
                           fastuidraw::vec2(d, +1.0f),
                           fastuidraw::vec2(b, +1.0f),
                           callback);
      }
    else if(fastuidraw::t_abs(plane.y()) > 0.0f)
      {
        float a, b, c, d;
        a = (+plane.x() - plane.z()) / plane.y();
        b = (-plane.x() - plane.z()) / plane.y();

        if(plane.y() > 0.0f)
          {
            c = fastuidraw::t_min(-1.0f, a);
            d = fastuidraw::t_min(-1.0f, b);
          }
        else
          {
            c = fastuidraw::t_max(1.0f, a);
            d = fastuidraw::t_max(1.0f, b);
          }

        painter->draw_quad(fastuidraw::vec2(-1.0f, a),
                           fastuidraw::vec2(-1.0f, c),
                           fastuidraw::vec2(+1.0f, d),
                           fastuidraw::vec2(+1.0f, b),
                           callback);

      }
    else if(plane.z() <= 0.0)
      {
        /* complement of half plane covers entire [-1,1]x[-1,1]
         */
        painter->draw_quad(fastuidraw::vec2(-1.0f, -1.0f),
                           fastuidraw::vec2(-1.0f, +1.0f),
                           fastuidraw::vec2(+1.0f, +1.0f),
                           fastuidraw::vec2(+1.0f, -1.0f),
                           callback);
      }
  }


  class clip_rect
  {
  public:
    clip_rect(void):
      m_enabled(false),
      m_min(0.0f, 0.0f),
      m_max(0.0f, 0.0f)
    {}

    clip_rect(const fastuidraw::vec2 &pmin, const fastuidraw::vec2 &pmax):
      m_enabled(true),
      m_min(pmin),
      m_max(pmax)
    {}

    void
    intersect(const clip_rect &rect);

    void
    translate(const fastuidraw::vec2 &pt);

    void
    shear(float sx, float sy);

    void
    scale(float s);

    bool m_enabled;
    fastuidraw::vec2 m_min, m_max;
  };

  class clip_rect_state
  {
  public:
    clip_rect_state(void):
      m_item_matrix_tricky(false),
      m_item_matrix_changed(false),
      m_all_content_culled(false)
    {}

    void
    set_painter_core_clip(const fastuidraw::PainterPacker::handle &core);

    std::bitset<4>
    set_painter_core_clip(const fastuidraw::PainterState::ClipEquationsState &cl,
                          const fastuidraw::PainterPacker::handle &core);

    clip_rect m_clip_rect;
    bool m_item_matrix_tricky;
    bool m_item_matrix_changed;
    bool m_all_content_culled;
    fastuidraw::float3x3 m_item_matrix_inverse_transpose;
  };

  class occluder_stack_entry
  {
  public:
    /* steals the data it does.
     */
    explicit
    occluder_stack_entry(std::vector<fastuidraw::PainterDrawCommand::DelayedAction::handle> &pz)
    {
      m_set_occluder_z.swap(pz);
    }

    void
    on_pop(fastuidraw::Painter *p);

  private:
    /* action to execute on popping.
     */
    std::vector<fastuidraw::PainterDrawCommand::DelayedAction::handle> m_set_occluder_z;
  };

  class state_stack_entry
  {
  public:
    unsigned int m_occluder_stack_position;
    fastuidraw::Painter::ItemMatrixState m_matrix;
    fastuidraw::PainterState::ClipEquationsState m_clip;
    fastuidraw::Painter::PainterBrushState m_brush;
    fastuidraw::Painter::VertexShaderDataState m_vertex_shader_data;
    fastuidraw::Painter::FragmentShaderDataState m_fragment_shader_data;

    clip_rect_state m_clip_rect_state;
  };

  class PainterWorkRoom
  {
  public:
    std::vector<unsigned int> m_selector;
    std::vector<fastuidraw::const_c_array<fastuidraw::PainterIndex> > m_index_chunks;
  };

  class PainterPrivate
  {
  public:
    explicit
    PainterPrivate(fastuidraw::PainterBackend::handle backend);

    bool
    rect_is_culled(const fastuidraw::vec2 &pmin, const fastuidraw::vec2 &wh);

    void
    draw_generic_check(fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
                       fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                       fastuidraw::const_c_array<unsigned int> attrib_chunk_selector,
                       const fastuidraw::PainterItemShader &shader, unsigned int z,
                       const fastuidraw::PainterPacker::DataCallBack::handle &call_back);

    fastuidraw::vec2 m_one_pixel_width;
    unsigned int m_current_z;
    clip_rect_state m_clip_rect_state;
    std::vector<occluder_stack_entry> m_occluder_stack;
    std::vector<state_stack_entry> m_state_stack;
    fastuidraw::PainterPacker::handle m_core;
    fastuidraw::Painter::PainterBrushState m_reset_brush, m_black_brush;
    fastuidraw::Painter::ItemMatrixState m_identiy_matrix;
    clip_rect m_clip_rect_in_item_coordinates;
    PainterWorkRoom m_work_room;
  };

}

//////////////////////////////////////////
// clip_rect methods
void
clip_rect::
intersect(const clip_rect &rect)
{
  if(!rect.m_enabled)
    {
      return;
    }

  if(m_enabled)
    {
      m_min.x() = fastuidraw::t_max(rect.m_min.x(), m_min.x());
      m_min.y() = fastuidraw::t_max(rect.m_min.y(), m_min.y());

      m_max.x() = fastuidraw::t_min(rect.m_max.x(), m_max.x());
      m_max.y() = fastuidraw::t_min(rect.m_max.y(), m_max.y());
    }
  else
    {
      *this = rect;
    }
}

void
clip_rect::
translate(const fastuidraw::vec2 &pt)
{
  m_min += pt;
  m_max += pt;
}

void
clip_rect::
shear(float sx, float sy)
{
  fastuidraw::vec2 s(sx, sy);
  m_min *= s;
  m_max *= s;
}

void
clip_rect::
scale(float s)
{
  m_min *= s;
  m_max *= s;
}



////////////////////////////////////////
// occluder_stack_entry methods
void
occluder_stack_entry::
on_pop(fastuidraw::Painter *p)
{
  /* depth test is GL_GEQUAL, so we need to increment the Z
     before hand so that the occluders block all that
     is drawn below them.
   */
  p->increment_z();
  for(unsigned int i = 0, endi = m_set_occluder_z.size(); i < endi; ++i)
    {
      ZDelayedAction *ptr;
      assert(dynamic_cast<ZDelayedAction*>(m_set_occluder_z[i].get()) != NULL);
      ptr = static_cast<ZDelayedAction*>(m_set_occluder_z[i].get());
      ptr->finalize_z(p->current_z());
    }
}

///////////////////////////////////////////////
// clip_rect_stat methods
void
clip_rect_state::
set_painter_core_clip(const fastuidraw::PainterPacker::handle &core)
{
  fastuidraw::PainterState::ClipEquationsState null;
  set_painter_core_clip(null, core);
}


std::bitset<4>
clip_rect_state::
set_painter_core_clip(const fastuidraw::PainterState::ClipEquationsState &pcl,
                      const fastuidraw::PainterPacker::handle &core)
{
  m_item_matrix_tricky = false;
  if(m_item_matrix_changed)
    {
      m_item_matrix_changed = false;
      core->item_matrix().m_item_matrix.inverse_transpose(m_item_matrix_inverse_transpose);
    }
  /* The clipping window is given by:
       w * min_x <= x <= w * max_x
       w * min_y <= y <= w * max_y
     which expands to
         x + w * min_x >= 0  --> ( 1,  0, -min_x)
        -x - w * max_x >= 0  --> (-1,  0, max_x)
         y + w * min_y >= 0  --> ( 0,  1, -min_y)
        -y - w * max_y >= 0  --> ( 0, -1, max_y)
       However, the clip equations are in clip coordinates
       so we need to apply the inverse transpose of the
       transformation matrix to the 4 vectors
   */
  fastuidraw::PainterPacker::ClipEquations cl;
  cl.m_clip_equations[0] = m_item_matrix_inverse_transpose * fastuidraw::vec3( 1.0f,  0.0f, -m_clip_rect.m_min.x());
  cl.m_clip_equations[1] = m_item_matrix_inverse_transpose * fastuidraw::vec3(-1.0f,  0.0f,  m_clip_rect.m_max.x());
  cl.m_clip_equations[2] = m_item_matrix_inverse_transpose * fastuidraw::vec3( 0.0f,  1.0f, -m_clip_rect.m_min.y());
  cl.m_clip_equations[3] = m_item_matrix_inverse_transpose * fastuidraw::vec3( 0.0f, -1.0f,  m_clip_rect.m_max.y());
  core->clip_equations(cl);

  if(!pcl)
    {
      return std::bitset<4>();
    }

  /* see if the vertices of the clipping rectangle (post m_item_matrix applied)
     are all within the passed clipped equations.
   */
  const fastuidraw::PainterState::ClipEquations &eq(pcl.state());
  const fastuidraw::float3x3 &m(core->item_matrix().m_item_matrix);
  std::bitset<4> return_value;
  fastuidraw::vecN<fastuidraw::vec3, 4> q;

  /* return_value[i] is true exactly when each point of the rectangle is inside
                     the i'th clip equation.
   */
  q[0] = m * fastuidraw::vec3(m_clip_rect.m_min.x(), m_clip_rect.m_min.y(), 1.0f);
  q[1] = m * fastuidraw::vec3(m_clip_rect.m_max.x(), m_clip_rect.m_min.y(), 1.0f);
  q[2] = m * fastuidraw::vec3(m_clip_rect.m_min.x(), m_clip_rect.m_max.y(), 1.0f);
  q[3] = m * fastuidraw::vec3(m_clip_rect.m_max.x(), m_clip_rect.m_max.y(), 1.0f);

  for(int i = 0; i < 4; ++i)
    {
      return_value[i] = dot(q[0], eq.m_clip_equations[i]) >= 0.0f
        && dot(q[1], eq.m_clip_equations[i]) >= 0.0f
        && dot(q[2], eq.m_clip_equations[i]) >= 0.0f
        && dot(q[3], eq.m_clip_equations[i]) >= 0.0f;
    }

  return return_value;
}

//////////////////////////////////
// PainterPrivate methods
PainterPrivate::
PainterPrivate(fastuidraw::PainterBackend::handle backend)
{
  m_core = FASTUIDRAWnew fastuidraw::PainterPacker(backend);
  m_reset_brush = m_core->brush_state();
  m_core->brush().pen(0.0, 0.0, 0.0, 0.0);
  m_black_brush = m_core->brush_state();
  m_identiy_matrix = m_core->item_matrix_state();
  m_current_z = 1;
  m_one_pixel_width = fastuidraw::vec2(0.0f, 0.0f);
}

bool
PainterPrivate::
rect_is_culled(const fastuidraw::vec2 &pmin, const fastuidraw::vec2 &wh)
{
  /* apply the current transformation matrix to
     the corners of the clipping rectangle and check
     if there is a clipping plane for which all
     those points are on the wrong size.
   */
  fastuidraw::vec2 pmax(wh + pmin);
  fastuidraw::vecN<fastuidraw::vec3, 4> pts;
  pts[0] = m_core->item_matrix().m_item_matrix * fastuidraw::vec3(pmin.x(), pmin.y(), 1.0f);
  pts[1] = m_core->item_matrix().m_item_matrix * fastuidraw::vec3(pmin.x(), pmax.y(), 1.0f);
  pts[2] = m_core->item_matrix().m_item_matrix * fastuidraw::vec3(pmax.x(), pmax.y(), 1.0f);
  pts[3] = m_core->item_matrix().m_item_matrix * fastuidraw::vec3(pmax.x(), pmin.y(), 1.0f);

  if(m_clip_rect_state.m_clip_rect.m_enabled)
    {
      /* use equations of from clip state
       */
      return all_pts_culled_by_one_half_plane(pts, m_core->clip_equations());
    }
  else
    {
      fastuidraw::PainterState::ClipEquations clip_eq;
      clip_eq.m_clip_equations[0] = fastuidraw::vec3(1.0f, 0.0f, 1.0f);
      clip_eq.m_clip_equations[1] = fastuidraw::vec3(-1.0f, 0.0f, 1.0f);
      clip_eq.m_clip_equations[2] = fastuidraw::vec3(0.0f, 1.0f, 1.0f);
      clip_eq.m_clip_equations[3] = fastuidraw::vec3(0.0f, -1.0f, 1.0f);
      return all_pts_culled_by_one_half_plane(pts, clip_eq);
    }
}

void
PainterPrivate::
draw_generic_check(fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
                   fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                   fastuidraw::const_c_array<unsigned int> attrib_chunk_selector,
                   const fastuidraw::PainterItemShader &shader, unsigned int z,
                   const fastuidraw::PainterPacker::DataCallBack::handle &call_back)
{
  if(!m_clip_rect_state.m_all_content_culled)
    {
      m_core->draw_generic(attrib_chunks, index_chunks, attrib_chunk_selector, shader, z, call_back);
    }
}


//////////////////////////////////
// fastuidraw::Painter methods
fastuidraw::Painter::
Painter(PainterBackend::handle backend)
{
  m_d = FASTUIDRAWnew PainterPrivate(backend);
}

fastuidraw::Painter::
~Painter()
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::Painter::
target_resolution(int w, int h)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  w = t_max(w, 1);
  h = t_max(h, 1);
  d->m_one_pixel_width.x() = 1.0f / static_cast<float>(w);
  d->m_one_pixel_width.y() = 1.0f / static_cast<float>(h);
  d->m_core->target_resolution(w, h);
}

void
fastuidraw::Painter::
begin(bool reset_z)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  d->m_core->begin();

  if(reset_z)
    {
      d->m_current_z = 1;
    }

  d->m_clip_rect_state.m_item_matrix_tricky = false;
  d->m_clip_rect_state.m_item_matrix_changed = false;
  d->m_clip_rect_state.m_clip_rect.m_enabled = false;
  d->m_core->item_matrix(PainterState::ItemMatrix());
  d->m_core->clip_equations(PainterState::ClipEquations());
  d->m_core->brush_state(d->m_reset_brush);
  vertex_shader_data(VertexShaderData());
  fragment_shader_data(FragmentShaderData());
  blend_shader(PainterEnums::blend_porter_duff_src_over);
}

void
fastuidraw::Painter::
end(void)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  /* pop m_clip_stack to perform necessary writes
   */
  while(!d->m_occluder_stack.empty())
    {
      d->m_occluder_stack.back().on_pop(this);
      d->m_occluder_stack.pop_back();
    }
  /* clear state stack as well.
   */
  d->m_state_stack.clear();
  d->m_core->end();
}

void
fastuidraw::Painter::
draw_generic(const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
             const_c_array<const_c_array<PainterIndex> > index_chunks,
             const PainterItemShader &shader,
             const PainterPacker::DataCallBack::handle &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->draw_generic_check(attrib_chunks, index_chunks, const_c_array<unsigned int>(),
                        shader, current_z(), call_back);
}

void
fastuidraw::Painter::
draw_generic(const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
             const_c_array<const_c_array<PainterIndex> > index_chunks,
             const_c_array<unsigned int> attrib_chunk_selector,
             const PainterItemShader &shader,
             const PainterPacker::DataCallBack::handle &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->draw_generic_check(attrib_chunks, index_chunks, attrib_chunk_selector,
                        shader, current_z(), call_back);
}

void
fastuidraw::Painter::
draw_quad(const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3,
          const PainterPacker::DataCallBack::handle &call_back)
{
  vecN<PainterAttribute, 4> attribs;
  vecN<PainterIndex, 6> indices;

  indices[0] = 0;
  indices[1] = 1;
  indices[2] = 2;
  indices[3] = 0;
  indices[4] = 2;
  indices[5] = 3;

  attribs[0].m_primary_attrib = vec4(p0.x(), p0.y(), 0.0, 0.0);
  attribs[1].m_primary_attrib = vec4(p1.x(), p1.y(), 0.0, 0.0);
  attribs[2].m_primary_attrib = vec4(p2.x(), p2.y(), 0.0, 0.0);
  attribs[3].m_primary_attrib = vec4(p3.x(), p3.y(), 0.0, 0.0);

  for(int i = 0; i < 4; ++i)
    {
      attribs[i].m_secondary_attrib = vec4(0.0, 0.0, 0.0, 0.0);
      attribs[i].m_uint_attrib = uvec4(0, 0, 0, 0);
    }
  draw_generic(attribs, indices, default_shaders().fill_shader(), call_back);
}

void
fastuidraw::Painter::
draw_rect(const vec2 &p, const vec2 &wh,
          const PainterPacker::DataCallBack::handle &call_back)
{
  draw_quad(p, p + vec2(0.0f, wh.y()),
            p + wh, p + vec2(wh.x(), 0.0f),
            call_back);
}

void
fastuidraw::Painter::
stroke_path(const PainterAttributeData &pdata,
            enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
            bool with_anti_aliasing, const PainterStrokeShader &shader)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  PainterItemShader sh;
  using namespace PainterEnums;
  enum PainterAttributeData::stroking_data_t cap, join, edge;

  switch(js)
    {
    case rounded_joins:
      join = PainterAttributeData::rounded_joins_closing_edge;
      break;
    case bevel_joins:
      join = PainterAttributeData::bevel_joins_closing_edge;
      break;
    case miter_joins:
      join = PainterAttributeData::miter_joins_closing_edge;
      break;
    default:
      join = PainterAttributeData::stroking_data_count;
    }

  switch(cp)
    {
    case rounded_caps:
      cap = PainterAttributeData::rounded_cap;
      break;
    case square_caps:
      cap = PainterAttributeData::square_cap;
      break;
    default:
      cap = PainterAttributeData::stroking_data_count;
    }

  edge = PainterAttributeData::edge_closing_edge;
  if(cp != close_outlines)
    {
      join = PainterAttributeData::without_closing_edge(join);
      edge = PainterAttributeData::edge_no_closing_edge;
    }

  vecN<const_c_array<PainterAttribute>, 3> attrib_chunks;
  vecN<const_c_array<PainterIndex>, 3> index_chunks;
  vecN<unsigned int, 3> zincs;
  unsigned int startz;

  attrib_chunks[0] = pdata.attribute_data_chunk(edge);
  attrib_chunks[1] = pdata.attribute_data_chunk(join);
  attrib_chunks[2] = pdata.attribute_data_chunk(cap);
  index_chunks[0] = pdata.index_data_chunk(edge);
  index_chunks[1] = pdata.index_data_chunk(join);
  index_chunks[2] = pdata.index_data_chunk(cap);
  zincs[0] = pdata.increment_z_value(edge);
  zincs[1] = pdata.increment_z_value(join);
  zincs[2] = pdata.increment_z_value(cap);

  startz = d->m_current_z;
  if(with_anti_aliasing)
    {
      sh = shader.aa_shader_pass1();
    }
  else
    {
      sh = shader.non_aa_shader();
    }

  /* raw depth value goes from 0 to zincs[0] - 1
     written depth goes from (startz + zincs[1] + zincs[2] + 1) to (startz + zincs[0] + zincs[1] + zincs[2])
   */
  d->m_current_z = startz + zincs[1] + zincs[2] + 1;
  draw_generic(attrib_chunks[0], index_chunks[0], sh);

  /* raw depth value goes from 0 to zincs[1] - 1
     written depth goes from (startz + zincs[2] + 1) to (startz + zincs[1] + zincs[2])
   */
  d->m_current_z = startz + zincs[2] + 1;
  draw_generic(attrib_chunks[1], index_chunks[1], sh);

  /* raw depth value goes from 0 to zincs[2] - 1
     written depth goes from (startz + 1) to (startz + zincs[2])
   */
  d->m_current_z = startz + 1;
  draw_generic(attrib_chunks[2], index_chunks[2], sh);

  if(with_anti_aliasing)
    {
      /* the aa-pass does not add to depth from the
         stroke attribute data, thus the written
         depth is always startz.
       */
      d->m_current_z = startz;
      sh = shader.aa_shader_pass2();
      draw_generic(attrib_chunks, index_chunks, sh);
    }

  d->m_current_z = startz + zincs[0] + zincs[1] + zincs[2] + 1;
}

void
fastuidraw::Painter::
stroke_path(const Path &path,
            enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
            bool with_anti_aliasing, const PainterStrokeShader &shader)
{
  stroke_path(path.tessellation()->stroked()->painter_data(),
              cp, js, with_anti_aliasing, shader);
}

void
fastuidraw::Painter::
stroke_path(const Path &path,
            enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
            bool with_anti_aliasing)
{
  stroke_path(path, cp, js, with_anti_aliasing, default_shaders().stroke_shader());
}

void
fastuidraw::Painter::
stroke_path_pixel_width(const Path &path,
                        enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                        bool with_anti_aliasing)
{
  stroke_path(path, cp, js, with_anti_aliasing,
              default_shaders().pixel_width_stroke_shader());
}

void
fastuidraw::Painter::
fill_path(const PainterAttributeData &data,
          enum PainterEnums::fill_rule_t fill_rule,
          const PainterItemShader &shader)
{
  draw_generic(data.attribute_data_chunk(0),
               data.index_data_chunk(fill_rule),
               shader);
}

void
fastuidraw::Painter::
fill_path(const Path &path, enum PainterEnums::fill_rule_t fill_rule,
          const PainterItemShader &shader)
{
  fill_path(path.tessellation()->filled()->painter_data(), fill_rule, shader);
}

void
fastuidraw::Painter::
fill_path(const Path &path, enum PainterEnums::fill_rule_t fill_rule)
{
  fill_path(path, fill_rule, default_shaders().fill_shader());
}

void
fastuidraw::Painter::
fill_path(const PainterAttributeData &data,
          const CustomFillRuleBase &fill_rule,
          const PainterItemShader &shader)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  d->m_work_room.m_selector.clear();
  d->m_work_room.m_index_chunks.clear();

  /* walk through what winding numbers are non-empty.
   */
  const_c_array<unsigned int> chks(data.non_empty_index_data_chunks());
  for(unsigned int i = 0; i < chks.size(); ++i)
    {
      unsigned int k;
      int winding_number;

      k = chks[i];
      if(k == PainterEnums::complement_nonzero_fill_rule || k >= PainterEnums::fill_rule_data_count)
        {
          winding_number = PainterAttributeData::winding_number_from_index_chunk(k);
          if(fill_rule(winding_number))
            {
              assert(!data.index_data_chunk(k).empty());
              d->m_work_room.m_index_chunks.push_back(data.index_data_chunk(k));
              d->m_work_room.m_selector.push_back(0);
            }
        }
    }

  if(!d->m_work_room.m_selector.empty())
    {
      draw_generic(data.attribute_data_chunks(),
                   make_c_array(d->m_work_room.m_index_chunks),
                   make_c_array(d->m_work_room.m_selector), shader);
    }

}

void
fastuidraw::Painter::
fill_path(const Path &path, const CustomFillRuleBase &fill_rule,
          const PainterItemShader &shader)
{
  fill_path(path.tessellation()->filled()->painter_data(), fill_rule, shader);
}

void
fastuidraw::Painter::
fill_path(const Path &path, const CustomFillRuleBase &fill_rule)
{
  fill_path(path, fill_rule, default_shaders().fill_shader());
}

void
fastuidraw::Painter::
draw_glyphs(const PainterAttributeData &data, const PainterGlyphShader &shader)
{
  const_c_array<unsigned int> chks(data.non_empty_index_data_chunks());
  for(unsigned int i = 0; i < chks.size(); ++i)
    {
      unsigned int k;

      k = chks[i];
      draw_generic(data.attribute_data_chunk(k), data.index_data_chunk(k),
                   shader.shader(static_cast<enum glyph_type>(k)));
    }
}

void
fastuidraw::Painter::
draw_glyphs(const PainterAttributeData &data, bool use_anistopic_antialias)
{
  if(use_anistopic_antialias)
    {
      draw_glyphs(data, default_shaders().glyph_shader_anisotropic());
    }
  else
    {
      draw_glyphs(data, default_shaders().glyph_shader());
    }
}

void
fastuidraw::Painter::
concat(const float3x3 &tr)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  float3x3 m;
  m = d->m_core->item_matrix().m_item_matrix * tr;
  d->m_core->item_matrix(m);
  d->m_clip_rect_state.m_item_matrix_changed = true;

  if(d->m_clip_rect_state.m_item_matrix_tricky
     || tr(0, 1) != 0.0f || tr(1, 0) != 0.0f
     || tr(2, 0) != 0.0f || tr(2, 1) != 0.0f
     || tr(2, 2) != 1.0f)
    {
      d->m_clip_rect_state.m_item_matrix_tricky = true;
    }
  else
    {
      d->m_clip_rect_state.m_clip_rect.translate(vec2(-tr(0, 2), -tr(1, 2)));
      d->m_clip_rect_state.m_clip_rect.shear(1.0f / tr(0,0), 1.0f / tr(1,1));
    }
}

void
fastuidraw::Painter::
transformation(const float3x3 &m)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  d->m_core->item_matrix(m);
  d->m_clip_rect_state.m_item_matrix_changed = true;
  d->m_clip_rect_state.m_item_matrix_tricky = true;
}


void
fastuidraw::Painter::
transformation_state(const ItemMatrixState &h)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  d->m_core->item_matrix_state(h);
  d->m_clip_rect_state.m_item_matrix_tricky = true;
  d->m_clip_rect_state.m_item_matrix_changed = true;
}

void
fastuidraw::Painter::
translate(const vec2 &p)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  float3x3 m;
  m = d->m_core->item_matrix().m_item_matrix;
  m.translate(p.x(), p.y());
  d->m_core->item_matrix(m);
  d->m_clip_rect_state.m_item_matrix_changed = true;
  d->m_clip_rect_state.m_clip_rect.translate(-p);
}

void
fastuidraw::Painter::
scale(float s)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  float3x3 m;

  m = d->m_core->item_matrix().m_item_matrix;
  m.scale(s);
  d->m_core->item_matrix(m);
  d->m_clip_rect_state.m_item_matrix_changed = true;
  d->m_clip_rect_state.m_clip_rect.scale(1.0f / s);
}

void
fastuidraw::Painter::
shear(float sx, float sy)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  float3x3 m;

  m = d->m_core->item_matrix().m_item_matrix;
  m.shear(sx, sy);
  d->m_core->item_matrix(m);
  d->m_clip_rect_state.m_item_matrix_changed = true;
  d->m_clip_rect_state.m_clip_rect.shear(1.0f / sx, 1.0f / sy);
}

void
fastuidraw::Painter::
rotate(float angle)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  float3x3 m, tr;
  float s, c;

  s = t_sin(angle);
  c = t_cos(angle);

  tr(0, 0) = c;
  tr(1, 0) = s;

  tr(0, 1) = -s;
  tr(1, 1) = c;

  m = d->m_core->item_matrix().m_item_matrix * tr;
  d->m_core->item_matrix(m);
  d->m_clip_rect_state.m_item_matrix_tricky = true;
  d->m_clip_rect_state.m_item_matrix_changed = true;
}

void
fastuidraw::Painter::
save(void)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  state_stack_entry st;
  st.m_occluder_stack_position = d->m_occluder_stack.size();
  st.m_matrix = d->m_core->item_matrix_state();
  st.m_clip = d->m_core->clip_equations_state();
  st.m_brush = brush_state();
  st.m_vertex_shader_data = vertex_shader_data();
  st.m_fragment_shader_data = fragment_shader_data();
  st.m_clip_rect_state = d->m_clip_rect_state;

  d->m_state_stack.push_back(st);
}

void
fastuidraw::Painter::
restore(void)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  assert(!d->m_state_stack.empty());
  const state_stack_entry &st(d->m_state_stack.back());

  d->m_clip_rect_state = st.m_clip_rect_state;
  d->m_core->item_matrix_state(st.m_matrix);
  d->m_core->clip_equations_state(st.m_clip);
  brush_state(st.m_brush);
  vertex_shader_data(st.m_vertex_shader_data);
  fragment_shader_data(st.m_fragment_shader_data);
  while(d->m_occluder_stack.size() > st.m_occluder_stack_position)
    {
      d->m_occluder_stack.back().on_pop(this);
      d->m_occluder_stack.pop_back();
    }
  d->m_state_stack.pop_back();
}

/* How we handle clipping.
        - clipOut by path P
           1. add "draw" the path P filled, but with call back for
              the data indicating where in the attribute or data
              store buffer to write the new z-value
           2. on doing clipPop, we know the z-value to use for
              all the elements that are occluded by the fill
              path, so we write that value.

        - clipIn by rect R
            * easy case A: No changes to tranformation matrix since last clipIn by rect
               1. intersect current clipping rectangle with R, set clip equations
            * easy case B: Trasnformation matrix change is "easy" (i.e maps coordiante aligned rect to coordiante aligned rects)
               1. map old clip rect to new coordinates, interset rect, set clip equations.
            * hard case: Transformation matrix change does not map coordiante aligned rect to coordiante aligned rects
               1. set clip equations
               2. temporarily set transformation matrix to identity
               3. draw 4 half-planes: for each OLD clipping equation draw that half plane
               4. restore transformation matrix

        - clipIn by path P
            1. clipIn by R, R = bounding box of P
            2. clipOut by R\P.
*/

void
fastuidraw::Painter::
clipOutPath(const Path &path, enum PainterEnums::fill_rule_t fill_rule)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  if(d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  PainterBrushState old_brush;
  PainterShader::const_handle old_blend;
  ZDataCallBack::handle zdatacallback;

  /* zdatacallback generates a list of PainterDrawCommand::DelayedAction
     objects (held in m_actions) who's action is to write the correct
     z-value to occlude elements drawn after clipOut but not after
     the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_brush = brush_state();
  old_blend = blend_shader();

  brush_state(d->m_black_brush);
  blend_shader(PainterEnums::blend_porter_duff_dst);
  draw_generic(path.tessellation()->filled()->painter_data().attribute_data_chunk(0),
               path.tessellation()->filled()->painter_data().index_data_chunk(fill_rule),
               default_shaders().fill_shader(), zdatacallback);
  brush_state(old_brush);
  blend_shader(old_blend);

  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));
}

void
fastuidraw::Painter::
clipInPath(const Path &path, enum PainterEnums::fill_rule_t fill_rule)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  if(d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  vec2 pmin, pmax;
  pmin = path.tessellation()->bounding_box_min();
  pmax = path.tessellation()->bounding_box_max();
  clipInRect(pmin, pmax);
  clipOutPath(path, PainterEnums::complement_fill_rule(fill_rule));
}

void
fastuidraw::Painter::
clipInRect(const vec2 &pmin, const vec2 &wh)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  d->m_clip_rect_state.m_all_content_culled =
    d->m_clip_rect_state.m_all_content_culled || d->rect_is_culled(pmin, wh);

  if(d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  vec2 pmax(pmin + wh);

  if(!d->m_clip_rect_state.m_clip_rect.m_enabled)
    {
      /* no clipped rect defined yet, just take the arguments
         as the clipping window
       */
      d->m_clip_rect_state.m_clip_rect = clip_rect(pmin, pmax);
      d->m_clip_rect_state.set_painter_core_clip(d->m_core);
      return;
    }
  else if(!d->m_clip_rect_state.m_item_matrix_tricky)
    {
      /* a previous clipping window (defined in m_clip_rect_state),
         but transformation takes screen aligned rectangles to
         screen aligned rectangles, thus the current value of
         m_clip_rect_state.m_clip_rect is the clipping rect
         in local coordinates, so we can intersect it with
         the passed rectangle.
       */
      d->m_clip_rect_state.m_clip_rect.intersect(clip_rect(pmin, pmax));
      d->m_clip_rect_state.set_painter_core_clip(d->m_core);
      return;
    }


  /* the transformation is tricky, thus the current value of
     m_clip_rect_state.m_clip_rect does NOT reflect the actual
     clipping rectangle.

     The clipping is done as follows:
      1. we set the clip equations to come from pmin, pmax
      2. we draw the -complement- of the half planes of each
         of the old clip equations as occluders
   */
  PainterState::ClipEquationsState prev_clip, current_clip;

  prev_clip = d->m_core->clip_equations_state();
  assert(prev_clip);

  d->m_clip_rect_state.m_clip_rect = clip_rect(pmin, pmax);

  std::bitset<4> skip_occluder;
  skip_occluder = d->m_clip_rect_state.set_painter_core_clip(prev_clip, d->m_core);
  current_clip = d->m_core->clip_equations_state();

  /* if the new clipping rectangle is completely contained
     in the older clipping region, then we can skip drawing
     the complement of the old clipping rectangle as occluders
   */
  if(skip_occluder.all())
    {
      return;
    }

  /* draw the complement of the half planes. The half planes
     are in 3D api coordinates, so set the matrix temporarily
     to identity. Note that we use the interface directly
     in m_core because this->transformation_state() sets
     m_clip_rect_state.m_item_matrix_tricky to true.
   */
  ItemMatrixState matrix_state;
  matrix_state = d->m_core->item_matrix_state();
  assert(matrix_state);
  d->m_core->item_matrix_state(d->m_identiy_matrix);

  ZDataCallBack::handle zdatacallback;
  zdatacallback = FASTUIDRAWnew ZDataCallBack();

  PainterBrushState old_brush;
  PainterShader::const_handle old_blend;
  old_brush = brush_state();
  old_blend = blend_shader();
  brush_state(d->m_black_brush);
  blend_shader(PainterEnums::blend_porter_duff_dst);

  /* we temporarily set the clipping to a slightly
     larger rectangle when drawing the occluders.
     We do this because round off error can have us
     miss a few pixels when drawing the occluder
   */
  PainterState::ClipEquations slightly_bigger(current_clip.state());
  for(unsigned int i = 0; i < 4; ++i)
    {
      float f;
      vec3 &eq(slightly_bigger.m_clip_equations[i]);

      f = fastuidraw::t_abs(eq.x()) * d->m_one_pixel_width.x() + fastuidraw::t_abs(eq.y()) * d->m_one_pixel_width.y();
      eq.z() += f;
    }
  d->m_core->clip_equations(slightly_bigger);

  /* draw the half plane occluders
   */
  for(unsigned int i = 0; i < 4; ++i)
    {
      if(!skip_occluder[i])
        {
          draw_half_plane_complement(this, prev_clip.state().m_clip_equations[i], zdatacallback);
        }
    }

  d->m_core->clip_equations_state(current_clip);

  /* add to occluder stack.
   */
  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));

  d->m_core->item_matrix_state(matrix_state);
  brush_state(old_brush);
  blend_shader(old_blend);
}

const fastuidraw::GlyphAtlas::handle&
fastuidraw::Painter::
glyph_atlas(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->glyph_atlas();
}

const fastuidraw::ImageAtlas::handle&
fastuidraw::Painter::
image_atlas(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->image_atlas();
}

const fastuidraw::ColorStopAtlas::handle&
fastuidraw::Painter::
colorstop_atlas(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->colorstop_atlas();
}

const fastuidraw::Painter::ItemMatrixState&
fastuidraw::Painter::
transformation_state(void)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->item_matrix_state();
}

fastuidraw::PainterBrush&
fastuidraw::Painter::
brush(void)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->brush();
}

const fastuidraw::PainterBrush&
fastuidraw::Painter::
cbrush(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->cbrush();
}

const fastuidraw::Painter::PainterBrushState&
fastuidraw::Painter::
brush_state(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->brush_state();
}

void
fastuidraw::Painter::
brush_state(const PainterBrushState &h)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->brush_state(h);
}

const fastuidraw::PainterShader::const_handle&
fastuidraw::Painter::
blend_shader(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->blend_shader();
}

void
fastuidraw::Painter::
blend_shader(const PainterShader::const_handle &h)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->blend_shader(h);
}

void
fastuidraw::Painter::
vertex_shader_data(const VertexShaderData &shader_data)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->vertex_shader_data(shader_data);
}

void
fastuidraw::Painter::
vertex_shader_data(const VertexShaderDataState &h)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->vertex_shader_data(h);
}

const fastuidraw::Painter::VertexShaderDataState&
fastuidraw::Painter::
vertex_shader_data(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->vertex_shader_data();
}

void
fastuidraw::Painter::
fragment_shader_data(const FragmentShaderData &shader_data)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->fragment_shader_data(shader_data);
}

void
fastuidraw::Painter::
fragment_shader_data(const FragmentShaderDataState &h)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->fragment_shader_data(h);
}

const fastuidraw::Painter::FragmentShaderDataState&
fastuidraw::Painter::
fragment_shader_data(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->fragment_shader_data();
}

const fastuidraw::PainterShaderSet&
fastuidraw::Painter::
default_shaders(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->default_shaders();
}

unsigned int
fastuidraw::Painter::
current_z(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_current_z;
}

void
fastuidraw::Painter::
increment_z(int amount)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_current_z += amount;
}

void
fastuidraw::Painter::
register_vert_shader(const PainterShader::handle &shader)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->register_vert_shader(shader);
}

void
fastuidraw::Painter::
register_frag_shader(const PainterShader::handle &shader)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->register_frag_shader(shader);
}

void
fastuidraw::Painter::
register_blend_shader(const PainterShader::handle &shader)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->register_blend_shader(shader);
}

void
fastuidraw::Painter::
register_shader(const PainterItemShader &p)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(p);
}

void
fastuidraw::Painter::
register_shader(const PainterStrokeShader &p)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(p);
}

void
fastuidraw::Painter::
register_shader(const PainterGlyphShader &p)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(p);
}

void
fastuidraw::Painter::
register_shader(const PainterShaderSet &p)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(p);
}
