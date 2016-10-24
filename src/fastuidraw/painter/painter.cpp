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
#include "../private/clip.hpp"

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
      FASTUIDRAWunused(header);
      m_mapped = &mapped_location[fastuidraw::PainterHeader::z_offset].u;
    }

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
          *m_dests[i].m_mapped = m_z_to_write;
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
    fastuidraw::range_type<unsigned int> m_clip_equation_series;

    clip_rect_state m_clip_rect_state;
    float m_curve_flatness;
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

  /* To avoid allocating memory all the time, we store the
     clip polygon data within the same std::vector<vec3>.
     The usage pattern is that the last element allocated
     is the first element to be freed.
   */
  class ClipEquationStore
  {
  public:
    ClipEquationStore(void)
    {}

    void
    push(void)
    {
      m_sz.push_back(m_store.size());
      m_store.resize(m_store.size() + m_current.size());
      std::copy(m_current.begin(), m_current.end(), m_store.begin() + m_sz.back());
    }

    void
    pop(void)
    {
      assert(!m_sz.empty());
      assert(m_sz.back() <= m_store.size());

      set_current(fastuidraw::make_c_array(m_store).sub_array(m_sz.back()));
      m_store.resize(m_sz.back());
      m_sz.pop_back();
    }

    void
    set_current(fastuidraw::const_c_array<fastuidraw::vec3> new_equations)
    {
      m_current.resize(new_equations.size());
      std::copy(new_equations.begin(), new_equations.end(), m_current.begin());
    }

    void
    add_to_current(const fastuidraw::vec3 &c)
    {
      m_current.push_back(c);
    }

    void
    clear_current(void)
    {
      m_current.clear();
    }

    void
    clear(void)
    {
      m_current.clear();
      m_store.clear();
      m_sz.clear();
    }

    fastuidraw::const_c_array<fastuidraw::vec3>
    current(void)
    {
      return fastuidraw::make_c_array(m_current);
    }

  private:
    std::vector<fastuidraw::vec3> m_store;
    std::vector<unsigned int> m_sz;
    std::vector<fastuidraw::vec3> m_current;
  };

  class PainterWorkRoom
  {
  public:
    std::vector<unsigned int> m_selector;
    std::vector<fastuidraw::const_c_array<fastuidraw::PainterIndex> > m_index_chunks;
    std::vector<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > m_attrib_chunks;
    std::vector<int> m_index_adjusts;
    std::vector<fastuidraw::vec2> m_pts_clip_against_planes;
    std::vector<fastuidraw::vec2> m_pts_draw_convex_polygon;
    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_pts_update_clip_series;
    std::vector<fastuidraw::vec3> m_update_clip_series_eqs;
    std::vector<float> m_clipper_floats;
    std::vector<fastuidraw::PainterIndex> m_indices;
    std::vector<fastuidraw::PainterAttribute> m_attribs;
    std::vector<unsigned int> m_edge_chunks;
    std::vector<unsigned int> m_stroke_dashed_join_chunks;
    std::vector<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > m_stroke_attrib_chunks;
    std::vector<fastuidraw::const_c_array<fastuidraw::PainterIndex> > m_stroke_index_chunks;
    std::vector<int> m_stroke_index_adjusts;
    fastuidraw::StrokedPath::ScratchSpace m_path_scratch;
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
                       fastuidraw::const_c_array<int> index_adjusts,
                       fastuidraw::const_c_array<unsigned int> attrib_chunk_selector,
                       unsigned int z,
                       const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back);

    void
    draw_generic(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                 const fastuidraw::PainterData &draw,
                 fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
                 fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                 fastuidraw::const_c_array<int> index_adjusts,
                 fastuidraw::const_c_array<unsigned int> attrib_chunk_selector,
                 unsigned int z,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back);

    void
    clip_against_planes(fastuidraw::const_c_array<fastuidraw::vec2> pts,
                        std::vector<fastuidraw::vec2> &out_pts);

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

    bool
    update_clip_equation_series(const fastuidraw::vec2 &pmin,
                                const fastuidraw::vec2 &pmax);

    float
    select_path_thresh(const fastuidraw::Path &path);

    void
    compute_edge_chunks(const fastuidraw::StrokedPath &stroked_path,
                        const fastuidraw::PainterShaderData::DataBase *raw_data,
                        const fastuidraw::StrokingDataSelectorBase &selector,
                        bool close_countours,
                        std::vector<unsigned int> &out_chunks);

    fastuidraw::vec2 m_resolution;
    fastuidraw::vec2 m_one_pixel_width;
    float m_curve_flatness;
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
    ClipEquationStore m_clip_store;
    PainterWorkRoom m_work_room;
  };

  inline
  unsigned int
  chunk_for_stroking(bool close_contours)
  {
    return close_contours ?
      fastuidraw::StrokedPath::join_chunk_with_closing_edge:
      fastuidraw::StrokedPath::join_chunk_without_closing_edge;
  }
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
  m_resolution(1.0f, 1.0f),
  m_one_pixel_width(1.0f, 1.0f),
  m_curve_flatness(4.0f),
  m_pool(backend->configuration_base().alignment())
{
  m_core = FASTUIDRAWnew fastuidraw::PainterPacker(backend);
  m_reset_brush = m_pool.create_packed_value(fastuidraw::PainterBrush());
  m_black_brush = m_pool.create_packed_value(fastuidraw::PainterBrush()
                                             .pen(0.0f, 0.0f, 0.0f, 0.0f));
  m_identiy_matrix = m_pool.create_packed_value(fastuidraw::PainterItemMatrix());
  m_current_z = 1;
}

bool
PainterPrivate::
update_clip_equation_series(const fastuidraw::vec2 &pmin,
                            const fastuidraw::vec2 &pmax)
{
  using namespace fastuidraw;
  using namespace fastuidraw::detail;

  const PainterItemMatrix &m(m_current_item_matrix);
  const_c_array<vec3> clips(m_clip_store.current());
  unsigned int src, dst, i;
  vec2 center(0.0f, 0.0f);

  m_work_room.m_pts_update_clip_series[0].resize(4);
  m_work_room.m_pts_update_clip_series[0][0] = pmin;
  m_work_room.m_pts_update_clip_series[0][1] = vec2(pmin.x(), pmax.y());
  m_work_room.m_pts_update_clip_series[0][2] = pmax;
  m_work_room.m_pts_update_clip_series[0][3] = vec2(pmax.x(), pmin.y());

  for(i = 0, src = 0, dst = 1; i < clips.size(); ++i, std::swap(src, dst))
    {
      vec3 nc;
      nc = clips[i] * m.m_item_matrix;

      clip_against_plane(nc, make_c_array(m_work_room.m_pts_update_clip_series[src]),
                         m_work_room.m_pts_update_clip_series[dst],
                         m_work_room.m_clipper_floats);
    }

  /* the input rectangle clipped to the previous clipping equation
     array is now stored in m_work_room.m_pts_update_clip_series[src]
   */
  const_c_array<vec2> poly;
  poly = make_c_array(m_work_room.m_pts_update_clip_series[src]);

  m_clip_store.clear_current();

  /* if the rectangle clipped is empty, then we are completely clipped.
   */
  if(poly.empty())
    {
      return true;
    }

  /* compute center of polygon so that we can correctly
     orient the normal vectors of the sides.
   */
  for(i = 0; i < poly.size(); ++i)
    {
      center += poly[i];
    }
  center /= static_cast<float>(poly.size());

  if(m_clip_rect_state.m_inverse_transpose_not_ready)
    {
      m_clip_rect_state.m_inverse_transpose_not_ready = false;
      m_current_item_matrix.m_item_matrix.inverse_transpose(m_clip_rect_state.m_item_matrix_inverse_transpose);
    }

  /* extract the normal vectors of the polygon sides with
     correct orientation.
   */
  for(unsigned int i = 0; i < poly.size(); ++i)
    {
      vec2 v, n;
      unsigned int next_i;

      next_i = i + 1;
      next_i = (next_i == poly.size()) ? 0 : next_i;
      v = poly[next_i] - poly[i];
      n = vec2(v.y(), -v.x());
      if(dot(center - poly[i], n) < 0.0f)
        {
          n = -n;
        }

      /* The clip equation we have in local coordinates
         is dot(n, p - poly[i]) >= 0. Algebra time:
           dot(n, p - poly[i]) = n.x * p.x + n.y * p.y + (-poly[i].x * n.x - poly[i].y * n.y)
                              = dot( (n, R), (p, 1))
         where
           R = -poly[i].x * n.x - poly[i].y * n.y = -dot(n, poly[i])
         We want the clip equations in clip coordinates though:
           dot( (n, R), (p, 1) ) = dot( (n, R), inverseM(M(p, 1)) )
                                 = dot( inverse_transpose_M(R,1), M(p, 1))
         thus the vector to use is inverse_transpose_M(R,1)
       */
      vec3 nn(n.x(), n.y(), -dot(n, poly[i]));
      m_clip_store.add_to_current(m_clip_rect_state.m_item_matrix_inverse_transpose * nn);
    }

  return false;
}

float
PainterPrivate::
select_path_thresh(const fastuidraw::Path &path)
{
  /* easy case: no projection
   */
  float return_value;
  bool no_perspective;

  const fastuidraw::float3x3 &m(m_current_item_matrix.m_item_matrix);

  no_perspective = (m(2, 0) == 0.0f && m(2, 1) == 0.0f);
  if(no_perspective || true)
    {
      float d0, d1, d;

      /* Poor man's approximation to operator norm coming from
         taking the supremum norm of matrix then multiplying
         by n * sqrt(n) where n = #dimensions = 2
       */
      d0 = m_resolution.x() * fastuidraw::t_max(fastuidraw::t_abs(m(0, 0)), fastuidraw::t_abs(m(0, 1)));
      d1 = m_resolution.y() * fastuidraw::t_max(fastuidraw::t_abs(m(1, 0)), fastuidraw::t_abs(m(1, 1)));
      d = fastuidraw::t_max(d0, d1) * m(2, 2);
      d *= 2.0f * static_cast<float>(M_SQRT2);

      return_value = m_curve_flatness / d;
    }
  else
    {
      /* TODO:
         Use path bounding box to determine how small w might
         be on the points of the path; if the bounding box
         goes through the near plane, then clip it against
         the view-port. If the bounding box still includes
         w = 0, then clamp it anyways.
       */
      FASTUIDRAWunused(path);
    }

  return return_value;
}

void
PainterPrivate::
compute_edge_chunks(const fastuidraw::StrokedPath &stroked_path,
                    const fastuidraw::PainterShaderData::DataBase *raw_data,
                    const fastuidraw::StrokingDataSelectorBase &selector,
                    bool close_countours,
                    std::vector<unsigned int> &out_chunks)
{
  float pixels_additional_room(0.0f), item_space_additional_room(0.0f);
  unsigned int sz;

  out_chunks.resize(stroked_path.maximum_edge_chunks());
  selector.stroking_distances(raw_data,
                              &pixels_additional_room,
                              &item_space_additional_room);

  sz = stroked_path.edge_chunks(m_work_room.m_path_scratch,
                                m_clip_store.current(),
                                m_current_item_matrix.m_item_matrix,
                                m_one_pixel_width,
                                pixels_additional_room,
                                item_space_additional_room,
                                close_countours,
                                fastuidraw::make_c_array(out_chunks));
  assert(sz <= out_chunks.size());
  out_chunks.resize(sz);
}

void
PainterPrivate::
clip_against_planes(fastuidraw::const_c_array<fastuidraw::vec2> pts,
                    std::vector<fastuidraw::vec2> &out_pts)
{
  using namespace fastuidraw;
  using namespace fastuidraw::detail;
  const PainterClipEquations &eqs(m_current_clip);
  const PainterItemMatrix &m(m_current_item_matrix);

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
draw_generic(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
             const fastuidraw::PainterData &draw,
             fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
             fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
             fastuidraw::const_c_array<int> index_adjusts,
             fastuidraw::const_c_array<unsigned int> attrib_chunk_selector,
             unsigned int z,
             const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back)
{
  fastuidraw::PainterPackerData p(draw);
  p.m_clip = current_clip_state();
  p.m_matrix = current_item_marix_state();
  m_core->draw_generic(shader, p, attrib_chunks, index_chunks, index_adjusts, attrib_chunk_selector, z, call_back);
}

void
PainterPrivate::
draw_generic_check(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                   const fastuidraw::PainterData &draw,
                   fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
                   fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                   fastuidraw::const_c_array<int> index_adjusts,
                   fastuidraw::const_c_array<unsigned int> attrib_chunk_selector,
                   unsigned int z,
                   const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back)
{
  if(!m_clip_rect_state.m_all_content_culled)
    {
      draw_generic(shader, draw, attrib_chunks, index_chunks, index_adjusts, attrib_chunk_selector, z, call_back);
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
  d->m_resolution.x() = static_cast<float>(w);
  d->m_resolution.y() = static_cast<float>(h);
  d->m_one_pixel_width = 1.0f / d->m_resolution;
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
    d->m_clip_store.set_current(clip_eq.m_clip_equations);
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
  d->m_clip_store.clear();
  d->m_state_stack.clear();
  d->m_core->end();
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
             const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
             const_c_array<const_c_array<PainterIndex> > index_chunks,
             const_c_array<int> index_adjusts,
             const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->draw_generic_check(shader, draw, attrib_chunks, index_chunks,
                        index_adjusts, const_c_array<unsigned int>(),
                        current_z(), call_back);
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
             const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
             const_c_array<const_c_array<PainterIndex> > index_chunks,
             const_c_array<int> index_adjusts,
             const_c_array<unsigned int> attrib_chunk_selector,
             const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->draw_generic_check(shader, draw, attrib_chunks, index_chunks,
                        index_adjusts, attrib_chunk_selector,
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
      d->m_work_room.m_attribs[i].m_attrib2 = uvec4(0u, 0u, 0u, 0u);
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
               0,
               call_back);
}

void
fastuidraw::Painter::
draw_convex_polygon(const PainterData &draw, const_c_array<vec2> pts,
                    const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  draw_convex_polygon(default_shaders().fill_shader().item_shader(), draw, pts, call_back);
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
  draw_quad(default_shaders().fill_shader().item_shader(), draw, p0, p1, p2, p3, call_back);
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
  draw_rect(default_shaders().fill_shader().item_shader(), draw, p, wh, call_back);
}

void
fastuidraw::Painter::
stroke_path(const PainterStrokeShader &shader, const PainterData &draw,
            const PainterAttributeData *edge_data, const_c_array<unsigned int> edge_chunks,
            unsigned int inc_edge,
            const PainterAttributeData *cap_data, unsigned int cap_chunk,
            const PainterAttributeData* join_data, const_c_array<unsigned int> join_chunks,
            unsigned int inc_join, bool with_anti_aliasing,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  if(d->m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  unsigned int startz, zinc_sum(0), inc_cap(0), num_joins(0), num_edges(0);
  bool modify_z;
  const reference_counted_ptr<PainterItemShader> *sh;
  c_array<const_c_array<PainterAttribute> > attrib_chunks;
  c_array<const_c_array<PainterIndex> > index_chunks;
  c_array<int> index_adjusts;

  if(join_data == NULL)
    {
      join_chunks = const_c_array<unsigned int>();
      inc_join = 0;
    }

  if(edge_data == NULL)
    {
      edge_chunks = const_c_array<unsigned int>();
      inc_edge = 0;
    }

  /* clear first to blank the values, std::vector::clear
     does not call deallocation on its backing store,
     thus there is no malloc/free noise
   */
  d->m_work_room.m_stroke_attrib_chunks.clear();
  d->m_work_room.m_stroke_index_chunks.clear();
  d->m_work_room.m_stroke_index_adjusts.resize(1 + edge_chunks.size() + join_chunks.size());
  d->m_work_room.m_stroke_attrib_chunks.resize(1 + edge_chunks.size() + join_chunks.size());
  d->m_work_room.m_stroke_index_chunks.resize(1 + edge_chunks.size() + join_chunks.size());

  attrib_chunks = make_c_array(d->m_work_room.m_stroke_attrib_chunks);
  index_chunks = make_c_array(d->m_work_room.m_stroke_index_chunks);
  index_adjusts = make_c_array(d->m_work_room.m_stroke_index_adjusts);

  num_joins = join_chunks.size();
  for(unsigned int J = 0; J < num_joins; ++J)
    {
      attrib_chunks[J] = join_data->attribute_data_chunk(join_chunks[J]);
      index_chunks[J] = join_data->index_data_chunk(join_chunks[J]);
      index_adjusts[J] = join_data->index_adjust_chunk(join_chunks[J]);
    }

  num_edges = edge_chunks.size();
  for(unsigned int E = 0; E < num_edges; ++E)
    {
      attrib_chunks[num_joins + E] = edge_data->attribute_data_chunk(edge_chunks[E]);
      index_chunks[num_joins + E] = edge_data->index_data_chunk(edge_chunks[E]);
      index_adjusts[num_joins + E] = edge_data->index_adjust_chunk(edge_chunks[E]);
    }

  if(cap_data != NULL)
    {
      attrib_chunks[num_joins + num_edges] = cap_data->attribute_data_chunk(cap_chunk);
      index_chunks[num_joins + num_edges] = cap_data->index_data_chunk(cap_chunk);
      index_adjusts[num_joins + num_edges] = cap_data->index_adjust_chunk(cap_chunk);
      inc_cap = cap_data->increment_z_value(cap_chunk);
    }
  else
    {
      attrib_chunks = attrib_chunks.sub_array(0, num_joins + num_edges);
      index_chunks = index_chunks.sub_array(0, num_joins + num_edges);
      index_adjusts = index_adjusts.sub_array(0, num_joins + num_edges);
    }

  startz = d->m_current_z;
  modify_z = !with_anti_aliasing || shader.aa_type() == PainterStrokeShader::draws_solid_then_fuzz;
  sh = (with_anti_aliasing) ? &shader.aa_shader_pass1(): &shader.non_aa_shader();

  if(modify_z)
    {
      unsigned int incr_z;
      zinc_sum = incr_z = inc_edge + inc_cap + inc_join;

      /*
        We want draw the passes so that the depth test prevents overlap drawing
        - For each set X, the raw depth value is from 0 to the increment_z_value()
        - We draw so that the X'th set is drawn with the set before it occluding it.
          (recall that larger z's occlude smaller z's).
       */
      if(join_data != NULL)
        {
          incr_z -= inc_join;
          d->draw_generic(*sh, draw,
                          attrib_chunks.sub_array(0, num_joins),
                          index_chunks.sub_array(0, num_joins),
                          index_adjusts.sub_array(0, num_joins),
                          fastuidraw::const_c_array<unsigned int>(),
                          startz + incr_z + 1, call_back);
        }

      if(edge_data != NULL)
        {
          incr_z -= inc_edge;
          d->draw_generic(*sh, draw,
                          attrib_chunks.sub_array(num_joins, num_edges),
                          index_chunks.sub_array(num_joins, num_edges),
                          index_adjusts.sub_array(num_joins, num_edges),
                          fastuidraw::const_c_array<unsigned int>(),
                          startz + incr_z + 1, call_back);
        }

      if(cap_data != NULL)
        {
          incr_z -= inc_cap;
          d->draw_generic(*sh, draw,
                          attrib_chunks.sub_array(num_joins + num_edges, 1),
                          index_chunks.sub_array(num_joins + num_edges, 1),
                          index_adjusts.sub_array(num_joins + num_edges, 1),
                          fastuidraw::const_c_array<unsigned int>(),
                          startz + incr_z + 1, call_back);
        }
    }
  else
    {
      d->draw_generic(*sh, draw, attrib_chunks,
                      index_chunks, index_adjusts,
                      fastuidraw::const_c_array<unsigned int>(),
                      d->m_current_z, call_back);
    }

  if(with_anti_aliasing)
    {
      /* the aa-pass does not add to depth from the
         stroke attribute data, thus the written
         depth is always startz.
       */
      d->draw_generic(shader.aa_shader_pass2(), draw, attrib_chunks,
                      index_chunks, index_adjusts,
                      fastuidraw::const_c_array<unsigned int>(),
                      startz, call_back);
    }

  if(modify_z)
    {
      d->m_current_z = startz + zinc_sum + 1;
    }
}

void
fastuidraw::Painter::
stroke_path(const PainterStrokeShader &shader, const PainterData &draw,
            const StrokedPath &path, float thresh,
            bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
            bool with_anti_aliasing,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  if(d->m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  const PainterAttributeData *edge_data(NULL), *cap_data(NULL), *join_data(NULL);
  unsigned int inc_edge, cap_chunk(0);
  unsigned int join_chunk(chunk_for_stroking(close_contours));
  unsigned int inc_join(0);
  float rounded_thresh;

  if(js == PainterEnums::rounded_joins
     || (cp == PainterEnums::rounded_caps && !close_contours))
    {
      const PainterShaderData::DataBase *raw_data;

      raw_data = draw.m_item_shader_data.data().data_base();
      rounded_thresh = shader.stroking_data_selector()->compute_rounded_thresh(raw_data, thresh);
    }

  edge_data = &path.edges(close_contours);
  inc_edge = path.z_increment_edge(close_contours);
  d->compute_edge_chunks(path,
                         draw.m_item_shader_data.data().data_base(),
                         *shader.stroking_data_selector(),
                         close_contours, d->m_work_room.m_edge_chunks);

  if(!close_contours)
    {
      switch(cp)
        {
        case PainterEnums::rounded_caps:
          cap_data = &path.rounded_caps(rounded_thresh);
          break;

        case PainterEnums::square_caps:
          cap_data = &path.square_caps();
          break;

        default:
          cap_data = NULL;
        }
    }

  switch(js)
    {
    case PainterEnums::bevel_joins:
      join_data = &path.bevel_joins();
      break;

    case PainterEnums::miter_joins:
      join_data = &path.miter_joins();
      break;

    case PainterEnums::rounded_joins:
      join_data = &path.rounded_joins(rounded_thresh);
      break;

    default:
      join_data = NULL;
    }

  if(join_data != NULL)
    {
      inc_join = join_data->increment_z_value(join_chunk);
    }

  stroke_path(shader, draw,
              edge_data, make_c_array(d->m_work_room.m_edge_chunks), inc_edge,
              cap_data, cap_chunk,
              join_data, const_c_array<unsigned int>(&join_chunk, 1),
              inc_join, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_path(const PainterStrokeShader &shader, const PainterData &draw, const Path &path,
            bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
            bool with_anti_aliasing,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  float thresh;

  d = reinterpret_cast<PainterPrivate*>(m_d);
  thresh = d->select_path_thresh(path);
  stroke_path(shader, draw, *path.tessellation(thresh)->stroked(), thresh,
              close_contours, cp, js, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_path(const PainterData &draw, const Path &path,
            bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
            bool with_anti_aliasing,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  stroke_path(default_shaders().stroke_shader(), draw, path,
              close_contours, cp, js, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_path_pixel_width(const PainterData &draw, const Path &path,
                        bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                        bool with_anti_aliasing,
                        const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  stroke_path(default_shaders().pixel_width_stroke_shader(), draw, path,
              close_contours, cp, js, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterStrokeShader &shader, const PainterData &draw,
                   const PainterAttributeData *edge_data, const_c_array<unsigned int> edge_chunks,
                   unsigned int inc_edge,
                   const PainterAttributeData *cap_data, unsigned int cap_chunk,
                   bool include_joins_from_closing_edge,
                   const DashEvaluatorBase *dash_evaluator, const PainterAttributeData *join_data,
                   bool with_anti_aliasing,
                   const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  /* dashed stroking has some trickiness with respect to how to handle joins.
       - we omit any join for which the dashing indicates to omit
         via its distance from the start of a contour
       - all other joins and edges are sent forward freely.
   */
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  if(d->m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  unsigned int inc_join(0);

  d->m_work_room.m_stroke_dashed_join_chunks.clear();
  if(dash_evaluator != NULL && join_data != NULL)
    {
      unsigned int num_joins;
      const PainterShaderData::DataBase *raw_data;

      raw_data = draw.m_item_shader_data.data().data_base();
      num_joins = dash_evaluator->number_joins(*join_data, include_joins_from_closing_edge);
      inc_join = num_joins;
      for(unsigned int J = 0; J < num_joins; ++J)
        {
          const_c_array<PainterIndex> idx;
          unsigned int chunk;

          chunk = dash_evaluator->named_join_chunk(J);
          idx = join_data->index_data_chunk(chunk);
          if(!idx.empty())
            {
              const_c_array<PainterAttribute> atr(join_data->attribute_data_chunk(chunk));
              assert(!atr.empty());
              if(dash_evaluator->covered_by_dash_pattern(raw_data, atr[0]))
                {
                  d->m_work_room.m_stroke_dashed_join_chunks.push_back(chunk);
                }
            }
        }
    }

  stroke_path(shader, draw, edge_data, edge_chunks, inc_edge,
              cap_data, cap_chunk,
              join_data, make_c_array(d->m_work_room.m_stroke_dashed_join_chunks),
              inc_join, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw,
                   const StrokedPath &path, float thresh,
                   bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                   bool with_anti_aliasing,
                   const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  if(d->m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  const PainterAttributeData *edge_data(NULL), *cap_data(NULL), *join_data(NULL);
  unsigned int inc_edge, cap_chunk(0);

  edge_data = &path.edges(close_contours);
  inc_edge = path.z_increment_edge(close_contours);
  d->compute_edge_chunks(path,
                         draw.m_item_shader_data.data().data_base(),
                         *shader.shader(cp).stroking_data_selector(),
                         close_contours, d->m_work_room.m_edge_chunks);
  if(!close_contours)
    {
      cap_data = &path.adjustable_caps();
    }

  switch(js)
    {
    case PainterEnums::bevel_joins:
      join_data = &path.bevel_joins();
      break;

    case PainterEnums::miter_joins:
      join_data = &path.miter_joins();
      break;

    case PainterEnums::rounded_joins:
      {
        const PainterShaderData::DataBase *raw_data;
        float rounded_thresh;

        raw_data = draw.m_item_shader_data.data().data_base();
        rounded_thresh = shader.shader(cp).stroking_data_selector()->compute_rounded_thresh(raw_data, thresh);
        join_data = &path.rounded_joins(rounded_thresh);
      }
      break;

    default:
      join_data = NULL;
    }

  stroke_dashed_path(shader.shader(cp), draw,
                     edge_data, make_c_array(d->m_work_room.m_edge_chunks), inc_edge,
                     cap_data, cap_chunk,
                     close_contours,
                     shader.dash_evaluator().get(), join_data,
                     with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw, const Path &path,
                   bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                   bool with_anti_aliasing,
                   const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  float thresh;

  d = reinterpret_cast<PainterPrivate*>(m_d);
  thresh = d->select_path_thresh(path);
  stroke_dashed_path(shader, draw, *path.tessellation(thresh)->stroked(), thresh,
                     close_contours, cp, js, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterData &draw, const Path &path,
                   bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                   bool with_anti_aliasing,
                   const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  stroke_dashed_path(default_shaders().dashed_stroke_shader(), draw, path,
                     close_contours, cp, js, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
stroke_dashed_path_pixel_width(const PainterData &draw, const Path &path,
                               bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                               bool with_anti_aliasing,
                               const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  stroke_dashed_path(default_shaders().pixel_width_dashed_stroke_shader(), draw, path,
                     close_contours, cp, js, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const PainterAttributeData &data, enum PainterEnums::fill_rule_t fill_rule,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  unsigned int idx_chunk, atr_chunk;
  idx_chunk = shader.chunk_selector()->chunk_from_fill_rule(fill_rule);
  atr_chunk = (shader.chunk_selector()->common_attribute_data()) ? 0 : idx_chunk;

  draw_generic(shader.item_shader(), draw,
               data.attribute_data_chunk(atr_chunk),
               data.index_data_chunk(idx_chunk),
               data.index_adjust_chunk(idx_chunk),
               call_back);
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const Path &path, enum PainterEnums::fill_rule_t fill_rule,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  float thresh;

  d = reinterpret_cast<PainterPrivate*>(m_d);
  thresh = d->select_path_thresh(path);
  fill_path(shader, draw, path.tessellation(thresh)->filled()->painter_data(), fill_rule, call_back);
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
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const PainterAttributeData &data, const CustomFillRuleBase &fill_rule,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  if(d->m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  bool common_attribs;
  common_attribs = shader.chunk_selector()->common_attribute_data();

  d->m_work_room.m_index_chunks.clear();
  d->m_work_room.m_index_adjusts.clear();
  d->m_work_room.m_attrib_chunks.clear();
  d->m_work_room.m_selector.clear();

  /* walk through what winding numbers are non-empty.
   */
  const_c_array<unsigned int> chks(data.non_empty_index_data_chunks());
  for(unsigned int i = 0; i < chks.size(); ++i)
    {
      unsigned int k;
      int winding_number;

      k = chks[i];
      if(shader.chunk_selector()->winding_number_from_chunk(k, winding_number))
        {
          if(fill_rule(winding_number))
            {
              const_c_array<PainterIndex> chunk;
              assert(!data.index_data_chunk(k).empty());
              chunk = data.index_data_chunk(k);
              d->m_work_room.m_index_chunks.push_back(chunk);
              d->m_work_room.m_index_adjusts.push_back(data.index_adjust_chunk(k));
              if(common_attribs)
                {
                  d->m_work_room.m_selector.push_back(0);
                }
              else
                {
                  const_c_array<PainterAttribute> attr_chunk;
                  attr_chunk = data.attribute_data_chunk(k);
                  d->m_work_room.m_attrib_chunks.push_back(attr_chunk);
                }
            }
        }
    }

  if(!d->m_work_room.m_index_chunks.empty())
    {
      if(common_attribs)
        {
          draw_generic(shader.item_shader(), draw,
                       data.attribute_data_chunks(),
                       make_c_array(d->m_work_room.m_index_chunks),
                       make_c_array(d->m_work_room.m_index_adjusts),
                       make_c_array(d->m_work_room.m_selector),
                       call_back);
        }
      else
        {
          draw_generic(shader.item_shader(), draw,
                       make_c_array(d->m_work_room.m_attrib_chunks),
                       make_c_array(d->m_work_room.m_index_chunks),
                       make_c_array(d->m_work_room.m_index_adjusts),
                       call_back);
        }
    }

}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader,
          const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  float thresh;

  d = reinterpret_cast<PainterPrivate*>(m_d);
  thresh = d->select_path_thresh(path);
  fill_path(shader, draw, path.tessellation(thresh)->filled()->painter_data(), fill_rule, call_back);
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
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);

  if(d->m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  const_c_array<unsigned int> chks(data.non_empty_index_data_chunks());
  for(unsigned int i = 0; i < chks.size(); ++i)
    {
      unsigned int k;

      k = chks[i];
      draw_generic(shader.shader(static_cast<enum glyph_type>(k)), draw,
                   data.attribute_data_chunk(k),
                   data.index_data_chunk(k),
                   data.index_adjust_chunk(k),
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

const fastuidraw::PainterItemMatrix&
fastuidraw::Painter::
transformation(void)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_current_item_matrix;
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
curveFlatness(float thresh)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  d->m_curve_flatness = thresh;
}

float
fastuidraw::Painter::
curveFlatness(void)
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_curve_flatness;
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
  st.m_curve_flatness = d->m_curve_flatness;

  d->m_state_stack.push_back(st);
  d->m_clip_store.push();
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
  d->m_curve_flatness = st.m_curve_flatness;
  while(d->m_occluder_stack.size() > st.m_occluder_stack_position)
    {
      d->m_occluder_stack.back().on_pop(this);
      d->m_occluder_stack.pop_back();
    }
  d->m_state_stack.pop_back();
  d->m_clip_store.pop();
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

  vec2 pmax(pmin + wh);

  d->m_clip_rect_state.m_all_content_culled =
    d->m_clip_rect_state.m_all_content_culled ||
    wh.x() <= 0.0f || wh.y() <= 0.0f ||
    d->rect_is_culled(pmin, wh) ||
    d->update_clip_equation_series(pmin, pmax);

  if(d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

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
query_stat(enum PainterPacker::stats_t st) const
{
  PainterPrivate *d;
  d = reinterpret_cast<PainterPrivate*>(m_d);
  return d->m_core->query_stat(st);
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
register_shader(const PainterFillShader &p)
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
