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
#include <fastuidraw/painter/painter_header.hpp>
#include <fastuidraw/painter/painter.hpp>

#include "../private/util_private.hpp"

namespace
{
  class ZDelayedAction;
  class ZDataCallBack;
  class PainterPrivate;

  class change_header_z
  {
  public:
    change_header_z(const fastuidraw::PainterHeader &header,
                    fastuidraw::c_array<fastuidraw::generic_data> mapped_location)
    {
      m_blend_shader = header.m_blend_shader;
      m_mapped = &mapped_location[fastuidraw::PainterHeader::z_blend_shader_offset].u;
    }

    //value of blend shader where z is written
    uint32_t m_blend_shader;

    //location to which to write to overwrite value.
    uint32_t *m_mapped;
  };

  class ZDelayedAction:public fastuidraw::PainterDraw::DelayedAction
  {
  public:
    void
    finalize_z(unsigned int z)
    {
      m_z_to_write = z;
      perform_action();
    }

  protected:

    virtual
    void
    action(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw> &)
    {
      for(unsigned int i = 0, endi = m_dests.size(); i < endi; ++i)
        {
          uint32_t zbits, blendbits;

          zbits = fastuidraw::pack_bits(fastuidraw::PainterHeader::z_bit0,
                                       fastuidraw::PainterHeader::z_num_bits,
                                       m_z_to_write);
          blendbits = fastuidraw::pack_bits(fastuidraw::PainterHeader::blend_shader_bit0,
                                           fastuidraw::PainterHeader::blend_shader_num_bits,
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
    virtual
    void
    current_draw(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw> &h)
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
    header_added(const fastuidraw::PainterHeader &original_value,
                 fastuidraw::c_array<fastuidraw::generic_data> mapped_location)
    {
      m_current->m_dests.push_back(change_header_z(original_value, mapped_location));
    }

    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw::DelayedAction> > m_actions;

  private:
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw> m_cmd;
    fastuidraw::reference_counted_ptr<ZDelayedAction> m_current;
  };

  bool
  all_pts_culled_by_one_half_plane(const fastuidraw::vecN<fastuidraw::vec3, 4> &pts,
                                   const fastuidraw::PainterClipEquations &eq)
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

  inline
  bool
  clip_equation_clips_everything(const fastuidraw::vec3 &cl)
  {
    return fastuidraw::t_abs(cl.x()) == 0.0f
      && fastuidraw::t_abs(cl.y()) == 0.0f
      && cl.z() <= 0.0f;
  }

  void
  draw_half_plane_complement(const fastuidraw::PainterData &draw,
                             fastuidraw::Painter *painter,
                             const fastuidraw::vec3 &plane,
                             const fastuidraw::reference_counted_ptr<ZDataCallBack> &callback)
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
        painter->draw_quad(draw,
                           fastuidraw::vec2(a, -1.0f),
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

        painter->draw_quad(draw,
                           fastuidraw::vec2(-1.0f, a),
                           fastuidraw::vec2(-1.0f, c),
                           fastuidraw::vec2(+1.0f, d),
                           fastuidraw::vec2(+1.0f, b),
                           callback);

      }
    else if(plane.z() <= 0.0f)
      {
        /* complement of half plane covers entire [-1,1]x[-1,1]
         */
        painter->draw_quad(draw,
                           fastuidraw::vec2(-1.0f, -1.0f),
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

    bool
    empty(void)
    {
      return m_enabled
        && (m_min.x() >= m_max.x() || m_min.y() >= m_max.y());
    }

    bool m_enabled;
    fastuidraw::vec2 m_min, m_max;
  };

  class clip_rect_state
  {
  public:
    clip_rect_state(void):
      m_item_matrix_tricky(false),
      m_inverse_transpose_not_ready(false),
      m_all_content_culled(false)
    {}

    void
    set_painter_core_clip(PainterPrivate *d);

    std::bitset<4>
    set_painter_core_clip(const fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> &cl,
                          PainterPrivate *d);

    clip_rect m_clip_rect;
    bool m_item_matrix_tricky;
    bool m_inverse_transpose_not_ready;
    bool m_all_content_culled;
    fastuidraw::float3x3 m_item_matrix_inverse_transpose;
  };

  class occluder_stack_entry
  {
  public:
    /* steals the data it does.
     */
    explicit
    occluder_stack_entry(std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw::DelayedAction> > &pz)
    {
      m_set_occluder_z.swap(pz);
    }

    void
    on_pop(fastuidraw::Painter *p);

  private:
    /* action to execute on popping.
     */
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw::DelayedAction> > m_set_occluder_z;
  };

  class state_stack_entry
  {
  public:
    unsigned int m_occluder_stack_position;
    fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix> m_matrix;
    fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> m_clip;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterBlendShader> m_blend;
    fastuidraw::BlendMode::packed_value m_blend_mode;

    clip_rect_state m_clip_rect_state;
  };

  class ComplementFillRule:public fastuidraw::Painter::CustomFillRuleBase
  {
  public:
    ComplementFillRule(const fastuidraw::Painter::CustomFillRuleBase *p):
      m_p(p)
    {
      assert(m_p);
    }

    bool
    operator()(int w) const
    {
      return !m_p->operator()(w);
    }

  private:
    const fastuidraw::Painter::CustomFillRuleBase *m_p;
  };

  class PainterWorkRoom
  {
  public:
    std::vector<unsigned int> m_selector;
    std::vector<fastuidraw::const_c_array<fastuidraw::PainterIndex> > m_index_chunks;
    std::vector<fastuidraw::vec2> m_pts_clip_against_planes;
    std::vector<fastuidraw::vec2> m_pts_draw_convex_polygon;
    std::vector<float> m_clipper_floats;
    std::vector<fastuidraw::PainterIndex> m_indices;
    std::vector<fastuidraw::PainterAttribute> m_attribs;
  };

  class AtrribIndex
  {
  public:
    fastuidraw::const_c_array<fastuidraw::PainterAttribute> m_attribs;
    fastuidraw::const_c_array<fastuidraw::PainterIndex> m_indices;
  };

  class StrokingData
  {
  public:
    AtrribIndex m_edges;
    unsigned int m_edge_zinc;

    std::vector<AtrribIndex> m_joins;
    unsigned int m_join_zinc;

    AtrribIndex m_caps;
    unsigned int m_cap_zinc;
  };

  class PainterPrivate
  {
  public:
    explicit
    PainterPrivate(fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend> backend);

    bool
    rect_is_culled(const fastuidraw::vec2 &pmin, const fastuidraw::vec2 &wh);

    void
    draw_generic_check(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                       const fastuidraw::PainterData &draw,
                       fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
                       fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                       fastuidraw::const_c_array<unsigned int> attrib_chunk_selector,
                       unsigned int z,
                       const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back);

    void
    clip_against_planes(fastuidraw::const_c_array<fastuidraw::vec2> pts,
                        std::vector<fastuidraw::vec2> &out_pts);

    static
    void
    clip_against_plane(const fastuidraw::vec3 &clip_eq,
                       fastuidraw::const_c_array<fastuidraw::vec2> pts,
                       std::vector<fastuidraw::vec2> &out_pts,
                       std::vector<float> &work_room);

    void
    set_current_item_matrix(const fastuidraw::PainterItemMatrix &v)
    {
      m_current_item_matrix = v;
      m_current_item_matrix_state = fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix>();
    }

    void
    set_current_clip(const fastuidraw::PainterClipEquations &v)
    {
      m_current_clip = v;
      m_current_clip_state = fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations>();
    }

    const fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix>&
    current_item_marix_state(void)
    {
      if(!m_current_item_matrix_state)
        {
          m_current_item_matrix_state = m_pool.create_packed_value(m_current_item_matrix);
        }
      return m_current_item_matrix_state;
    }

    void
    current_item_matrix_state(const fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix> &v)
    {
      m_current_item_matrix_state = v;
      m_current_item_matrix = v.value();
    }

    const fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations>&
    current_clip_state(void)
    {
      if(!m_current_clip_state)
        {
          m_current_clip_state = m_pool.create_packed_value(m_current_clip);
        }
      return m_current_clip_state;
    }

    void
    current_clip_state(const fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> &v)
    {
      m_current_clip_state = v;
      m_current_clip = v.value();
    }

    void
    stroke_path_helper(const StrokingData &str,
                       const fastuidraw::PainterStrokeShader &shader,
                       const fastuidraw::PainterData &draw,
                       bool with_anti_aliasing,
                       const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back);

    fastuidraw::vec2 m_one_pixel_width;
    unsigned int m_current_z;
    clip_rect_state m_clip_rect_state;
    std::vector<occluder_stack_entry> m_occluder_stack;
    std::vector<state_stack_entry> m_state_stack;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker> m_core;
    fastuidraw::PainterPackedValuePool m_pool;
    fastuidraw::PainterPackedValue<fastuidraw::PainterBrush> m_reset_brush, m_black_brush;
    fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix> m_identiy_matrix;
    fastuidraw::PainterItemMatrix m_current_item_matrix;
    fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix> m_current_item_matrix_state;
    fastuidraw::PainterClipEquations m_current_clip;
    fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> m_current_clip_state;
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
set_painter_core_clip(PainterPrivate *d)
{
  fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> null;
  set_painter_core_clip(null, d);
}


std::bitset<4>
clip_rect_state::
set_painter_core_clip(const fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> &pcl,
                      PainterPrivate *d)
{
  if(m_clip_rect.empty())
    {
      m_all_content_culled = true;
      return std::bitset<4>();
    }

  m_item_matrix_tricky = false;
  if(m_inverse_transpose_not_ready)
    {
      m_inverse_transpose_not_ready = false;
      d->m_current_item_matrix.m_item_matrix.inverse_transpose(m_item_matrix_inverse_transpose);
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
  fastuidraw::PainterClipEquations cl;
  cl.m_clip_equations[0] = m_item_matrix_inverse_transpose * fastuidraw::vec3( 1.0f,  0.0f, -m_clip_rect.m_min.x());
  cl.m_clip_equations[1] = m_item_matrix_inverse_transpose * fastuidraw::vec3(-1.0f,  0.0f,  m_clip_rect.m_max.x());
  cl.m_clip_equations[2] = m_item_matrix_inverse_transpose * fastuidraw::vec3( 0.0f,  1.0f, -m_clip_rect.m_min.y());
  cl.m_clip_equations[3] = m_item_matrix_inverse_transpose * fastuidraw::vec3( 0.0f, -1.0f,  m_clip_rect.m_max.y());
  d->set_current_clip(cl);

  for(int i = 0; i < 4; ++i)
    {
      if(clip_equation_clips_everything(cl.m_clip_equations[i]))
        {
          m_all_content_culled = true;
          return std::bitset<4>();
        }
    }

  if(!pcl)
    {
      return std::bitset<4>();
    }

  /* see if the vertices of the clipping rectangle (post m_item_matrix applied)
     are all within the passed clipped equations.
   */
  const fastuidraw::PainterClipEquations &eq(pcl.value());
  const fastuidraw::float3x3 &m(d->m_current_item_matrix.m_item_matrix);
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
PainterPrivate(fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend> backend):
  m_pool(backend->configuration_base().alignment())
{
  m_core = FASTUIDRAWnew fastuidraw::PainterPacker(backend);
  m_reset_brush = m_pool.create_packed_value(fastuidraw::PainterBrush());
  m_black_brush = m_pool.create_packed_value(fastuidraw::PainterBrush()
                                             .pen(0.0f, 0.0f, 0.0f, 0.0f));
  m_identiy_matrix = m_pool.create_packed_value(fastuidraw::PainterItemMatrix());
  m_current_z = 1;
  m_one_pixel_width = fastuidraw::vec2(0.0f, 0.0f);
}

void
PainterPrivate::
clip_against_planes(fastuidraw::const_c_array<fastuidraw::vec2> pts,
                    std::vector<fastuidraw::vec2> &out_pts)
{
  const fastuidraw::PainterClipEquations &eqs(m_current_clip);
  const fastuidraw::PainterItemMatrix &m(m_current_item_matrix);

  /* Clip planes are in clip coordinates, i.e.
       ClipDistance[i] = dot(M * p, clip_equation[i])
                       = dot(p, transpose(M)(clip_equation[i])
     To place them in local coordinates, then we need to apply
     the transpose of m_current_item_matrix to the clip planes
     which is the same as post-multiplying the matrix.
   */
  clip_against_plane(eqs.m_clip_equations[0] * m.m_item_matrix, pts,
                     m_work_room.m_pts_clip_against_planes,
                     m_work_room.m_clipper_floats);

  clip_against_plane(eqs.m_clip_equations[1] * m.m_item_matrix,
                     fastuidraw::make_c_array(m_work_room.m_pts_clip_against_planes),
                     out_pts,
                     m_work_room.m_clipper_floats);

  clip_against_plane(eqs.m_clip_equations[2] * m.m_item_matrix,
                     fastuidraw::make_c_array(out_pts),
                     m_work_room.m_pts_clip_against_planes,
                     m_work_room.m_clipper_floats);

  clip_against_plane(eqs.m_clip_equations[3] * m.m_item_matrix,
                     fastuidraw::make_c_array(m_work_room.m_pts_clip_against_planes),
                     out_pts,
                     m_work_room.m_clipper_floats);
}

void
PainterPrivate::
clip_against_plane(const fastuidraw::vec3 &clip_eq,
                   fastuidraw::const_c_array<fastuidraw::vec2> pts,
                   std::vector<fastuidraw::vec2> &out_pts,
                   std::vector<float> &work_room)
{
  /* clip the convex polygon of pts, placing the results
     into out_pts.
   */
  bool all_clipped, all_unclipped;
  unsigned int first_unclipped;

  if(pts.empty())
    {
      out_pts.resize(0);
      return;
    }

  work_room.resize(pts.size());
  all_clipped = true;
  all_unclipped = true;
  first_unclipped = pts.size();
  for(unsigned int i = 0; i < pts.size(); ++i)
    {
      work_room[i] = clip_eq.x() * pts[i].x() + clip_eq.y() * pts[i].y() + clip_eq.z();
      all_clipped = all_clipped && work_room[i] < 0.0f;
      all_unclipped = all_unclipped && work_room[i] >= 0.0f;
      if(first_unclipped == pts.size() && work_room[i] >= 0.0f)
        {
          first_unclipped = i;
        }
    }

  if(all_clipped)
    {
      /* all clipped, nothing to do!
       */
      out_pts.resize(0);
      return;
    }

  if(all_unclipped)
    {
      out_pts.resize(pts.size());
      std::copy(pts.begin(), pts.end(), out_pts.begin());
      return;
    }

  /* the polygon is convex, and atleast one point is clipped, thus
     the clip line goes through 2 edges.
   */
  fastuidraw::vecN<std::pair<unsigned int, unsigned int>, 2> edges;
  unsigned int num_edges(0);

  for(unsigned int i = 0, k = first_unclipped; i < pts.size() && num_edges < 2; ++i, ++k)
    {
      bool b0, b1;

      if(k == pts.size())
        {
          k = 0;
        }
      assert(k < pts.size());

      unsigned int next_k(k+1);
      if(next_k == pts.size())
        {
          next_k = 0;
        }
      assert(next_k < pts.size());

      b0 = work_room[k] >= 0.0f;
      b1 = work_room[next_k] >= 0.0f;
      if(b0 != b1)
        {
          edges[num_edges] = std::pair<unsigned int, unsigned int>(k, next_k);
          ++num_edges;
        }
    }

  assert(num_edges == 2);

  out_pts.reserve(pts.size() + 1);
  out_pts.resize(0);

  /* now add the points that are unclipped (in order)
     and the 2 new points representing the new points
     added by the clipping plane.
   */

  for(unsigned int i = first_unclipped; i <= edges[0].first; ++i)
    {
      out_pts.push_back(pts[i]);
    }

  /* now add the implicitely made vertex of the clip equation
     intersecting against the edge between pts[edges[0].first] and
     pts[edges[0].second]
   */
  {
    fastuidraw::vec2 pp;
    float t;

    t = -work_room[edges[0].first] / (work_room[edges[0].second] - work_room[edges[0].first]);
    pp = pts[edges[0].first] + t * (pts[edges[0].second] - pts[edges[0].first]);
    out_pts.push_back(pp);
  }

  /* the vertices from pts[edges[0].second] to pts[edges[1].first]
     are all on the clipped size of the plane, so they are skipped.
   */

  /* now add the implicitely made vertex of the clip equation
     intersecting against the edge between pts[edges[1].first] and
     pts[edges[1].second]
   */
  {
    fastuidraw::vec2 pp;
    float t;

    t = -work_room[edges[1].first] / (work_room[edges[1].second] - work_room[edges[1].first]);
    pp = pts[edges[1].first] + t * (pts[edges[1].second] - pts[edges[1].first]);
    out_pts.push_back(pp);
  }

  /* add all vertices starting from pts[edges[1].second] wrapping
     around until the points are clipped again.
   */
  for(unsigned int i = edges[1].second; i != first_unclipped && work_room[i] >= 0.0f;)
    {
      out_pts.push_back(pts[i]);
      ++i;
      if(i == pts.size())
        {
          i = 0;
        }
    }
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
  pts[0] = m_current_item_matrix.m_item_matrix * fastuidraw::vec3(pmin.x(), pmin.y(), 1.0f);
  pts[1] = m_current_item_matrix.m_item_matrix * fastuidraw::vec3(pmin.x(), pmax.y(), 1.0f);
  pts[2] = m_current_item_matrix.m_item_matrix * fastuidraw::vec3(pmax.x(), pmax.y(), 1.0f);
  pts[3] = m_current_item_matrix.m_item_matrix * fastuidraw::vec3(pmax.x(), pmin.y(), 1.0f);

  if(m_clip_rect_state.m_clip_rect.m_enabled)
    {
      /* use equations of from clip state
       */
      return all_pts_culled_by_one_half_plane(pts, m_current_clip);
    }
  else
    {
      fastuidraw::PainterClipEquations clip_eq;
      clip_eq.m_clip_equations[0] = fastuidraw::vec3( 1.0f,  0.0f, 1.0f);
      clip_eq.m_clip_equations[1] = fastuidraw::vec3(-1.0f,  0.0f, 1.0f);
      clip_eq.m_clip_equations[2] = fastuidraw::vec3( 0.0f,  1.0f, 1.0f);
      clip_eq.m_clip_equations[3] = fastuidraw::vec3( 0.0f, -1.0f, 1.0f);
      return all_pts_culled_by_one_half_plane(pts, clip_eq);
    }
}

void
PainterPrivate::
draw_generic_check(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                   const fastuidraw::PainterData &draw,
                   fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
                   fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                   fastuidraw::const_c_array<unsigned int> attrib_chunk_selector,
                   unsigned int z,
                   const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back)
{
  if(!m_clip_rect_state.m_all_content_culled)
    {
      fastuidraw::PainterPackerData p(draw);
      p.m_clip = current_clip_state();
      p.m_matrix = current_item_marix_state();
      m_core->draw_generic(shader, p, attrib_chunks, index_chunks, attrib_chunk_selector, z, call_back);
    }
}

void
PainterPrivate::
stroke_path_helper(const StrokingData &str,
                   const fastuidraw::PainterStrokeShader &shader,
                   const fastuidraw::PainterData &draw,
                   bool with_anti_aliasing,
                   const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back)
{
  using namespace fastuidraw;

  unsigned int startz, zinc_sum(0);
  bool modify_z;
  const reference_counted_ptr<PainterItemShader> *sh;
  std::vector<const_c_array<PainterAttribute> > vattrib_chunks(str.m_joins.size() + 2);
  std::vector<const_c_array<PainterIndex> > vindex_chunks(str.m_joins.size() + 2);
  c_array<const_c_array<PainterAttribute> > attrib_chunks;
  c_array<const_c_array<PainterIndex> > index_chunks;

  attrib_chunks = make_c_array(vattrib_chunks);
  index_chunks = make_c_array(vindex_chunks);

  attrib_chunks[0] = str.m_edges.m_attribs;
  index_chunks [0] = str.m_edges.m_indices;
  attrib_chunks[1] = str.m_caps.m_attribs;
  index_chunks [1] = str.m_caps.m_indices;
  for(unsigned int J = 0, endJ = str.m_joins.size(); J < endJ; ++J)
    {
      attrib_chunks[J + 2] = str.m_joins[J].m_attribs;
      index_chunks [J + 2] = str.m_joins[J].m_indices;
    }

  startz = m_current_z;
  modify_z = !with_anti_aliasing || shader.aa_type() == PainterStrokeShader::draws_solid_then_fuzz;
  sh = (with_anti_aliasing) ? &shader.aa_shader_pass1(): &shader.non_aa_shader();
  if(modify_z)
    {
      unsigned int incr_z;
      zinc_sum = incr_z = str.m_edge_zinc + str.m_join_zinc + str.m_cap_zinc;

      /*
        We want draw the passes so that the depth test prevents overlap drawing;
        - The K'th set has that the raw_depth value does from 0 to zincs[K] - 1.
        - We draw so that the K'th pass is drawn with the (K-1)-pass occluding it.
          (recall that larger z's occlude smaller z's).
       */
      incr_z -= str.m_edge_zinc;
      draw_generic_check(*sh, draw,
                         attrib_chunks.sub_array(0, 1),
                         index_chunks.sub_array(0, 1),
                         fastuidraw::const_c_array<unsigned int>(),
                         startz + incr_z + 1, call_back);

      incr_z -= str.m_join_zinc;
      draw_generic_check(*sh, draw,
                         attrib_chunks.sub_array(2),
                         index_chunks.sub_array(2),
                         fastuidraw::const_c_array<unsigned int>(),
                         startz + incr_z + 1, call_back);

      incr_z -= str.m_cap_zinc;
      draw_generic_check(*sh, draw,
                         attrib_chunks.sub_array(1, 1),
                         index_chunks.sub_array(1, 1),
                         fastuidraw::const_c_array<unsigned int>(),
                         startz + incr_z + 1, call_back);
    }
  else
    {
      draw_generic_check(*sh, draw, attrib_chunks, index_chunks,
                         fastuidraw::const_c_array<unsigned int>(),
                         m_current_z, call_back);
    }

  if(with_anti_aliasing)
    {
      /* the aa-pass does not add to depth from the
         stroke attribute data, thus the written
         depth is always startz.
       */
      draw_generic_check(shader.aa_shader_pass2(), draw, attrib_chunks, index_chunks,
                         fastuidraw::const_c_array<unsigned int>(),
                         startz, call_back);
    }

  if(modify_z)
    {
      m_current_z = startz + zinc_sum + 1;
    }
}

//////////////////////////////////
// fastuidraw::Painter methods
fastuidraw::Painter::
Painter(reference_counted_ptr<PainterBackend> backend)
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

fastuidraw::PainterPackedValuePool&
fastuidraw::Painter::
packed_value_pool(void)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_pool;
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
  d->m_clip_rect_state.m_inverse_transpose_not_ready = false;
  d->m_clip_rect_state.m_clip_rect.m_enabled = false;
  d->set_current_item_matrix(PainterItemMatrix());
  {
    PainterClipEquations clip_eq;
    clip_eq.m_clip_equations[0] = fastuidraw::vec3( 1.0f,  0.0f, 1.0f);
    clip_eq.m_clip_equations[1] = fastuidraw::vec3(-1.0f,  0.0f, 1.0f);
    clip_eq.m_clip_equations[2] = fastuidraw::vec3( 0.0f,  1.0f, 1.0f);
    clip_eq.m_clip_equations[3] = fastuidraw::vec3( 0.0f, -1.0f, 1.0f);
    d->set_current_clip(clip_eq);
  }
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
draw_generic(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
             const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
             const_c_array<const_c_array<PainterIndex> > index_chunks,
             const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->draw_generic_check(shader, draw, attrib_chunks, index_chunks,
                        const_c_array<unsigned int>(),
                        current_z(), call_back);
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
             const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
             const_c_array<const_c_array<PainterIndex> > index_chunks,
             const_c_array<unsigned int> attrib_chunk_selector,
             const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->draw_generic_check(shader, draw, attrib_chunks, index_chunks, attrib_chunk_selector,
                        current_z(), call_back);
}

void
fastuidraw::Painter::
draw_convex_polygon(const reference_counted_ptr<PainterItemShader> &shader,
                    const PainterData &draw, const_c_array<vec2> pts,
                    const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  if(pts.size() < 3)
    {
      return;
    }

  if(!d->m_core->hints().clipping_via_hw_clip_planes())
    {
      d->clip_against_planes(pts, d->m_work_room.m_pts_draw_convex_polygon);
      pts = make_c_array(d->m_work_room.m_pts_draw_convex_polygon);
      if(pts.size() < 3)
        {
          return;
        }
    }

  /* Draw a triangle fan centered at pts[0]
   */
  d->m_work_room.m_attribs.resize(pts.size());
  for(unsigned int i = 0; i < pts.size(); ++i)
    {
      d->m_work_room.m_attribs[i].m_attrib0 = fastuidraw::pack_vec4(pts[i].x(), pts[i].y(), 0.0f, 0.0f);
      d->m_work_room.m_attribs[i].m_attrib1 = uvec4(0u, 0u, 0u, 0u);
      d->m_work_room.m_attribs[i].m_attrib2 = uvec4(0, 0, 0, 0);
    }

  d->m_work_room.m_indices.clear();
  d->m_work_room.m_indices.reserve((pts.size() - 2) * 3);
  for(unsigned int i = 2; i < pts.size(); ++i)
    {
      d->m_work_room.m_indices.push_back(0);
      d->m_work_room.m_indices.push_back(i - 1);
      d->m_work_room.m_indices.push_back(i);
    }
  draw_generic(shader, draw,
               make_c_array(d->m_work_room.m_attribs),
               make_c_array(d->m_work_room.m_indices),
               call_back);
}

void
fastuidraw::Painter::
draw_convex_polygon(const PainterData &draw, const_c_array<vec2> pts,
                    const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  draw_convex_polygon(default_shaders().fill_shader(), draw, pts, call_back);
}

void
fastuidraw::Painter::
draw_quad(const reference_counted_ptr<PainterItemShader> &shader,
          const PainterData &draw, const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  vecN<vec2, 4> pts;
  pts[0] = p0;
  pts[1] = p1;
  pts[2] = p2;
  pts[3] = p3;
  draw_convex_polygon(shader, draw, const_c_array<vec2>(&pts[0], pts.size()), call_back);
}

void
fastuidraw::Painter::
draw_quad(const PainterData &draw, const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  draw_quad(default_shaders().fill_shader(), draw, p0, p1, p2, p3, call_back);
}

void
fastuidraw::Painter::
draw_rect(const reference_counted_ptr<PainterItemShader> &shader,
          const PainterData &draw, const vec2 &p, const vec2 &wh,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  draw_quad(shader, draw, p, p + vec2(0.0f, wh.y()),
            p + wh, p + vec2(wh.x(), 0.0f),
            call_back);
}

void
fastuidraw::Painter::
draw_rect(const PainterData &draw, const vec2 &p, const vec2 &wh,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  draw_rect(default_shaders().fill_shader(), draw, p, wh, call_back);
}

void
fastuidraw::Painter::
stroke_path(const PainterStrokeShader &shader, const PainterData &draw,
            const PainterAttributeData &pdata,
            enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
            bool with_anti_aliasing,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

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
  if(cp != close_contours)
    {
      join = PainterAttributeData::without_closing_edge(join);
      edge = PainterAttributeData::edge_no_closing_edge;
    }

  StrokingData str;

  str.m_edges.m_attribs = pdata.attribute_data_chunk(edge);
  str.m_edges.m_indices = pdata.index_data_chunk(edge);
  str.m_edge_zinc = pdata.increment_z_value(edge);

  str.m_caps.m_attribs = pdata.attribute_data_chunk(cap);
  str.m_caps.m_indices = pdata.index_data_chunk(cap);
  str.m_cap_zinc = pdata.increment_z_value(cap);

  str.m_joins.resize(1);
  str.m_joins[0].m_attribs = pdata.attribute_data_chunk(join);
  str.m_joins[0].m_indices = pdata.index_data_chunk(join);
  str.m_join_zinc = pdata.increment_z_value(join);

  d->stroke_path_helper(str, shader, draw, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_path(const PainterStrokeShader &shader, const PainterData &draw,
            const Path &path,
            enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
            bool with_anti_aliasing,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  stroke_path(shader, draw, path.tessellation()->stroked()->painter_data(),
              cp, js, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_path(const PainterData &draw, const Path &path,
            enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
            bool with_anti_aliasing,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  stroke_path(default_shaders().stroke_shader(), draw, path, cp, js, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_path_pixel_width(const PainterData &draw, const Path &path,
                        enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                        bool with_anti_aliasing,
                        const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  stroke_path(default_shaders().pixel_width_stroke_shader(), draw,
              path, cp, js, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw, const Path &path,
                   bool close_contour, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                   bool with_anti_aliasing,
                   const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  /* dashed stroking has some trickiness with respect to how to handle joins.
     Without caps:
       - we omit any join for which the dashing indicates to omit
         via its distance from the start of a contour
       - all other joins and edges are sent forward freely.
     With Caps:
       - we omit any join for which the dashing indicates to omit
         via its distance from the start of a contour
       - for those joins we omitted, we emit a cap-join at
         the join which covers the area (without overlap)
         to catch the region needed for caps made by dashing.
       - if the path is stroked unclosed, we use square cap
         points whose length is computed in CPU (sadly)
   */
  using namespace PainterEnums;

  enum PainterAttributeData::stroking_data_t edge, join;
  StrokingData str;

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

  if(close_contour)
    {
      edge = PainterAttributeData::edge_closing_edge;
    }
  else
    {
      join = PainterAttributeData::without_closing_edge(join);
      edge = PainterAttributeData::edge_no_closing_edge;
    }
  const PainterAttributeData &pdata(path.tessellation()->stroked()->painter_data());
  str.m_edges.m_attribs = pdata.attribute_data_chunk(edge);
  str.m_edges.m_indices = pdata.index_data_chunk(edge);
  str.m_edge_zinc = pdata.increment_z_value(edge);

  /* Those joins for which the distance value is inside the
     dash pattern, we include in str.m_joins with the join
     type, for those outside, we take the cap-join at the
     location. We know how many joins we need from the
     value of pdata.increment_z_value(join).
   */
  const PainterShaderData::DataBase *raw_data;
  raw_data = draw.m_item_shader_data.data().data_base();

  str.m_join_zinc = pdata.increment_z_value(join);
  for(unsigned int J = 0; J < str.m_join_zinc; ++J)
    {
      const_c_array<PainterIndex> idx;
      unsigned int chunk;

      chunk = PainterAttributeData::chunk_from_join(join, J);
      idx = pdata.index_data_chunk(chunk);
      if(!idx.empty())
        {
          float dist;
          range_type<float> dash_interval;
          const_c_array<PainterAttribute> atr(pdata.attribute_data_chunk(chunk));
          assert(!atr.empty());

          dist = unpack_float(atr[0].m_attrib1.y());
          if(shader.dash_evaluator()->compute_dash_interval(raw_data, dist, dash_interval))
            {
              str.m_joins.push_back(AtrribIndex());
              str.m_joins.back().m_attribs = atr;
              str.m_joins.back().m_indices = idx;
            }
        }
    }

  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->stroke_path_helper(str, shader.shader(cp), draw, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterData &draw, const Path &path,
                   bool close_contour, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                   bool with_anti_aliasing,
                   const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  stroke_dashed_path(default_shaders().dashed_stroke_shader(), draw, path,
                     close_contour, cp, js, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_dashed_path_pixel_width(const PainterData &draw, const Path &path,
                               bool close_contour, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                               bool with_anti_aliasing,
                               const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
   stroke_dashed_path(default_shaders().pixel_width_dashed_stroke_shader(), draw, path,
                      close_contour, cp, js, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
fill_path(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
          const PainterAttributeData &data, enum PainterEnums::fill_rule_t fill_rule,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  draw_generic(shader, draw, data.attribute_data_chunk(0),
               data.index_data_chunk(fill_rule),
               call_back);
}

void
fastuidraw::Painter::
fill_path(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
          const Path &path, enum PainterEnums::fill_rule_t fill_rule,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  fill_path(shader, draw, path.tessellation()->filled()->painter_data(), fill_rule, call_back);
}

void
fastuidraw::Painter::
fill_path(const PainterData &draw, const Path &path, enum PainterEnums::fill_rule_t fill_rule,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  fill_path(default_shaders().fill_shader(), draw, path, fill_rule, call_back);
}

void
fastuidraw::Painter::
fill_path(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
          const PainterAttributeData &data, const CustomFillRuleBase &fill_rule,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  d->m_work_room.m_index_chunks.clear();
  d->m_work_room.m_selector.clear();

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
              const_c_array<PainterIndex> chunk;
              assert(!data.index_data_chunk(k).empty());
              chunk = data.index_data_chunk(k);
              d->m_work_room.m_index_chunks.push_back(chunk);
              d->m_work_room.m_selector.push_back(0);
            }
        }
    }

  if(!d->m_work_room.m_selector.empty())
    {
      draw_generic(shader, draw, data.attribute_data_chunks(),
                   make_c_array(d->m_work_room.m_index_chunks),
                   make_c_array(d->m_work_room.m_selector), call_back);
    }

}

void
fastuidraw::Painter::
fill_path(const reference_counted_ptr<PainterItemShader> &shader,
          const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  fill_path(shader, draw, path.tessellation()->filled()->painter_data(), fill_rule, call_back);
}

void
fastuidraw::Painter::
fill_path(const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  fill_path(default_shaders().fill_shader(), draw, path, fill_rule, call_back);
}

void
fastuidraw::Painter::
draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
            const PainterAttributeData &data,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  const_c_array<unsigned int> chks(data.non_empty_index_data_chunks());
  for(unsigned int i = 0; i < chks.size(); ++i)
    {
      unsigned int k;

      k = chks[i];
      draw_generic(shader.shader(static_cast<enum glyph_type>(k)), draw,
                   data.attribute_data_chunk(k), data.index_data_chunk(k),
                   call_back);
      increment_z(data.increment_z_value(k));
    }
}

void
fastuidraw::Painter::
draw_glyphs(const PainterData &draw,
            const PainterAttributeData &data, bool use_anistopic_antialias,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  if(use_anistopic_antialias)
    {
      draw_glyphs(default_shaders().glyph_shader_anisotropic(), draw, data, call_back);
    }
  else
    {
      draw_glyphs(default_shaders().glyph_shader(), draw, data, call_back);
    }
}

void
fastuidraw::Painter::
concat(const float3x3 &tr)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  float3x3 m;
  m = d->m_current_item_matrix.m_item_matrix * tr;
  d->set_current_item_matrix(m);
  d->m_clip_rect_state.m_inverse_transpose_not_ready = true;

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

  d->set_current_item_matrix(m);
  d->m_clip_rect_state.m_inverse_transpose_not_ready = true;
  d->m_clip_rect_state.m_item_matrix_tricky = true;
}

const fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix>&
fastuidraw::Painter::
transformation_state(void)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->current_item_marix_state();
}

void
fastuidraw::Painter::
transformation_state(const PainterPackedValue<PainterItemMatrix> &h)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  d->current_item_matrix_state(h);
  d->m_clip_rect_state.m_item_matrix_tricky = true;
  d->m_clip_rect_state.m_inverse_transpose_not_ready = true;
}

void
fastuidraw::Painter::
translate(const vec2 &p)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  float3x3 m;
  m = d->m_current_item_matrix.m_item_matrix;
  m.translate(p.x(), p.y());
  d->set_current_item_matrix(m);
  d->m_clip_rect_state.m_inverse_transpose_not_ready = true;
  d->m_clip_rect_state.m_clip_rect.translate(-p);
}

void
fastuidraw::Painter::
scale(float s)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  float3x3 m;

  m = d->m_current_item_matrix.m_item_matrix;
  m.scale(s);
  d->set_current_item_matrix(m);
  d->m_clip_rect_state.m_inverse_transpose_not_ready = true;
  d->m_clip_rect_state.m_clip_rect.scale(1.0f / s);
}

void
fastuidraw::Painter::
shear(float sx, float sy)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  float3x3 m;

  m = d->m_current_item_matrix.m_item_matrix;
  m.shear(sx, sy);
  d->set_current_item_matrix(m);
  d->m_clip_rect_state.m_inverse_transpose_not_ready = true;
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

  m = d->m_current_item_matrix.m_item_matrix * tr;
  d->set_current_item_matrix(m);
  d->m_clip_rect_state.m_item_matrix_tricky = true;
  d->m_clip_rect_state.m_inverse_transpose_not_ready = true;
}

void
fastuidraw::Painter::
save(void)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  state_stack_entry st;
  st.m_occluder_stack_position = d->m_occluder_stack.size();
  st.m_matrix = d->current_item_marix_state();
  st.m_clip = d->current_clip_state();
  st.m_blend = d->m_core->blend_shader();
  st.m_blend_mode = d->m_core->blend_mode();
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
  d->current_item_matrix_state(st.m_matrix);
  d->current_clip_state(st.m_clip);
  d->m_core->blend_shader(st.m_blend, st.m_blend_mode);
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

  reference_counted_ptr<PainterBlendShader> old_blend;
  BlendMode::packed_value old_blend_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
     objects (held in m_actions) who's action is to write the correct
     z-value to occlude elements drawn after clipOut but not after
     the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = blend_shader();
  old_blend_mode = blend_mode();

  blend_shader(PainterEnums::blend_porter_duff_dst);
  fill_path(PainterData(d->m_black_brush), path, fill_rule, zdatacallback);
  blend_shader(old_blend, old_blend_mode);

  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));
}

void
fastuidraw::Painter::
clipOutPath(const Path &path, const CustomFillRuleBase &fill_rule)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  if(d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  fastuidraw::reference_counted_ptr<PainterBlendShader> old_blend;
  BlendMode::packed_value old_blend_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
     objects (held in m_actions) who's action is to write the correct
     z-value to occlude elements drawn after clipOut but not after
     the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = blend_shader();
  old_blend_mode = blend_mode();

  blend_shader(PainterEnums::blend_porter_duff_dst);
  fill_path(PainterData(d->m_black_brush), path, fill_rule, zdatacallback);
  blend_shader(old_blend, old_blend_mode);

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
  clipInRect(pmin, pmax - pmin);
  clipOutPath(path, PainterEnums::complement_fill_rule(fill_rule));
}

void
fastuidraw::Painter::
clipInPath(const Path &path, const CustomFillRuleBase &fill_rule)
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
  clipInRect(pmin, pmax - pmin);
  clipOutPath(path, ComplementFillRule(&fill_rule));
}

void
fastuidraw::Painter::
clipInRect(const vec2 &pmin, const vec2 &wh)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  d->m_clip_rect_state.m_all_content_culled =
    d->m_clip_rect_state.m_all_content_culled ||
    wh.x() <= 0.0f || wh.y() <= 0.0f ||
    d->rect_is_culled(pmin, wh);

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
      d->m_clip_rect_state.set_painter_core_clip(d);
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
      d->m_clip_rect_state.set_painter_core_clip(d);
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
  PainterPackedValue<PainterClipEquations> prev_clip, current_clip;

  prev_clip = d->current_clip_state();
  assert(prev_clip);

  d->m_clip_rect_state.m_clip_rect = clip_rect(pmin, pmax);

  std::bitset<4> skip_occluder;
  skip_occluder = d->m_clip_rect_state.set_painter_core_clip(prev_clip, d);
  current_clip = d->current_clip_state();

  if(d->m_clip_rect_state.m_all_content_culled)
    {
      /* The clip equations coming from the new clipping
         rectangle degenerate into an empty clipping region
         on the screen; immediately return.
       */
      return;
    }

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
  PainterPackedValue<PainterItemMatrix> matrix_state;
  matrix_state = d->current_item_marix_state();
  assert(matrix_state);
  d->current_item_matrix_state(d->m_identiy_matrix);

  reference_counted_ptr<ZDataCallBack> zdatacallback;
  zdatacallback = FASTUIDRAWnew ZDataCallBack();

  fastuidraw::reference_counted_ptr<PainterBlendShader> old_blend;
  BlendMode::packed_value old_blend_mode;
  old_blend = blend_shader();
  old_blend_mode = blend_mode();
  blend_shader(PainterEnums::blend_porter_duff_dst);

  /* we temporarily set the clipping to a slightly
     larger rectangle when drawing the occluders.
     We do this because round off error can have us
     miss a few pixels when drawing the occluder
   */
  PainterClipEquations slightly_bigger(current_clip.value());
  for(unsigned int i = 0; i < 4; ++i)
    {
      float f;
      vec3 &eq(slightly_bigger.m_clip_equations[i]);

      f = fastuidraw::t_abs(eq.x()) * d->m_one_pixel_width.x() + fastuidraw::t_abs(eq.y()) * d->m_one_pixel_width.y();
      eq.z() += f;
    }
  d->set_current_clip(slightly_bigger);

  /* draw the half plane occluders
   */
  for(unsigned int i = 0; i < 4; ++i)
    {
      if(!skip_occluder[i])
        {
          draw_half_plane_complement(PainterData(d->m_black_brush), this,
                                     prev_clip.value().m_clip_equations[i], zdatacallback);
        }
    }

  d->current_clip_state(current_clip);

  /* add to occluder stack.
   */
  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));

  d->current_item_matrix_state(matrix_state);
  blend_shader(old_blend, old_blend_mode);
}

const fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas>&
fastuidraw::Painter::
glyph_atlas(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->glyph_atlas();
}

const fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas>&
fastuidraw::Painter::
image_atlas(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->image_atlas();
}

const fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas>&
fastuidraw::Painter::
colorstop_atlas(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->colorstop_atlas();
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBlendShader>&
fastuidraw::Painter::
blend_shader(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->blend_shader();
}

fastuidraw::BlendMode::packed_value
fastuidraw::Painter::
blend_mode(void) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->blend_mode();
}

void
fastuidraw::Painter::
blend_shader(const fastuidraw::reference_counted_ptr<PainterBlendShader> &h,
             fastuidraw::BlendMode::packed_value mode)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->blend_shader(h, mode);
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
register_shader(const fastuidraw::reference_counted_ptr<PainterItemShader> &shader)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(shader);
}

void
fastuidraw::Painter::
register_shader(const fastuidraw::reference_counted_ptr<PainterBlendShader> &shader)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(shader);
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
register_shader(const PainterDashedStrokeShaderSet &p)
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
