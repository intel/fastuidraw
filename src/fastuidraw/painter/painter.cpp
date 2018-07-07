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
#include <algorithm>

#include <fastuidraw/util/math.hpp>
#include <fastuidraw/painter/painter_header.hpp>
#include <fastuidraw/painter/painter.hpp>

#include "../private/util_private.hpp"
#include "../private/util_private_ostream.hpp"
#include "../private/clip.hpp"

namespace
{
  class ZDelayedAction;
  class ZDataCallBack;
  class PainterPrivate;

  /* A WindingSet is way to cache values from a
   * fastuidraw::CustomFillRuleBase.
   */
  class WindingSet:fastuidraw::noncopyable
  {
  public:
    WindingSet(void):
      m_min_value(1),
      m_max_value(0)
    {}

    bool
    operator()(int V) const
    {
      return m_values[compute_index(V)] != 0;
    }

    void
    clear(void)
    {
      m_values.clear();
    }

    void
    set(int min_value, int max_value,
        const fastuidraw::CustomFillRuleBase &fill_rule)
    {
      m_values.clear();
      m_min_value = min_value;
      m_max_value = max_value;
      m_values.resize(fastuidraw::t_max(1 + m_max_value - m_min_value, 0), 0);
      for(int w = min_value; w <= max_value; ++w)
        {
          m_values[compute_index(w)] = fill_rule(w) ? 1u : 0u;
        }
    }

    void
    set(const fastuidraw::FilledPath &filled_path,
        fastuidraw::c_array<const unsigned int> subsets,
        const fastuidraw::CustomFillRuleBase &fill_rule)
    {
      int max_winding(0), min_winding(0);
      bool first_entry(true);

      for(unsigned int s : subsets)
        {
          fastuidraw::FilledPath::Subset subset(filled_path.subset(s));
          int m, M;

          if (!subset.winding_numbers().empty())
            {
              m = subset.winding_numbers().front();
              M = subset.winding_numbers().back();
              if (first_entry)
                {
                  min_winding = m;
                  max_winding = M;
                  first_entry = false;
                }
              else
                {
                  min_winding = fastuidraw::t_min(min_winding, m);
                  max_winding = fastuidraw::t_max(max_winding, M);
                }
            }
        }
      set(min_winding, max_winding, fill_rule);
    }

  private:
    unsigned int
    compute_index(int v) const
    {
      FASTUIDRAWassert(v >= m_min_value && v <= m_max_value);
      return v - m_min_value;
    }

    /* NOTE:
     *  we use an array of uint32_t's that although takes
     *  more storage, has faster element access (because
     *  to access an element does not require any bit-logic)
     *  and a winding_set is used as a cache for output to
     *  a custom fill rule.
     */
    int m_min_value, m_max_value;
    std::vector<uint32_t> m_values;
  };

  class change_header_z
  {
  public:
    change_header_z(const fastuidraw::PainterHeader &header,
                    fastuidraw::c_array<fastuidraw::generic_data> mapped_location)
    {
      FASTUIDRAWunused(header);
      m_mapped = &mapped_location[fastuidraw::PainterHeader::z_offset].i;
    }

    //location to which to write to overwrite value.
    int32_t *m_mapped;
  };

  class ZDelayedAction:public fastuidraw::PainterDraw::DelayedAction
  {
  public:
    void
    finalize_z(int z)
    {
      m_z_to_write = z;
      perform_action();
    }

  protected:

    virtual
    void
    action(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw> &)
    {
      for(change_header_z &d : m_dests)
        {
          *d.m_mapped = m_z_to_write;
        }
    }

  private:
    friend class ZDataCallBack;
    int32_t m_z_to_write;
    std::vector<change_header_z> m_dests;
  };

  class ZDataCallBack:public fastuidraw::PainterPacker::DataCallBack
  {
  public:
    virtual
    void
    current_draw(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw> &h)
    {
      if (h != m_cmd)
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
        if (fastuidraw::dot(pts[0], eq.m_clip_equations[i]) < 0.0f
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
    if (fastuidraw::t_abs(plane.x()) > fastuidraw::t_abs(plane.y()))
      {
        float a, b, c, d;
        /* a so that A * a + B * -1 + C = 0 -> a = (+B - C) / A
         * b so that A * a + B * +1 + C = 0 -> b = (-B - C) / A
         */
        a = (+plane.y() - plane.z()) / plane.x();
        b = (-plane.y() - plane.z()) / plane.x();

        /* the two points are then (a, -1) and (b, 1). Grab
         * (c, -1) and (d, 1) so that they are on the correct side
         * of the half plane
         */
        if (plane.x() > 0.0f)
          {
            /* increasing x then make the plane more positive,
             * and we want the negative side, so take c and
             * d to left of a and b.
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
         * (a, -1), (c, -1), (d, 1), (b, 1).
         */
        painter->draw_quad(draw,
                           fastuidraw::vec2(a, -1.0f),
                           fastuidraw::vec2(c, -1.0f),
                           fastuidraw::vec2(d, +1.0f),
                           fastuidraw::vec2(b, +1.0f),
                           callback);
      }
    else if (fastuidraw::t_abs(plane.y()) > 0.0f)
      {
        float a, b, c, d;
        a = (+plane.x() - plane.z()) / plane.y();
        b = (-plane.x() - plane.z()) / plane.y();

        if (plane.y() > 0.0f)
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
    else if (plane.z() <= 0.0f)
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

  /* Tracks the most recent clipping rect:
   * - the 4 clip equations in clip-coordinates
   * - the current transformation from item coordinates
   *   to clip-coordinates
   * - the clippin rectangle in local coordinates; this value
   *   only "makes sense" if m_item_matrix_transition_tricky
   *   is false
   */
  class clip_rect_state
  {
  public:
    clip_rect_state(void):
      m_all_content_culled(false),
      m_item_matrix_transition_tricky(false),
      m_inverse_transpose_not_ready(false)
    {}

    void
    reset(void)
    {
      m_all_content_culled = false;
      m_item_matrix_transition_tricky = false;
      m_inverse_transpose_not_ready = false;
      m_clip_rect.m_enabled = false;
      item_matrix(fastuidraw::float3x3(), false);

      fastuidraw::PainterClipEquations clip_eq;
      clip_eq.m_clip_equations[0] = fastuidraw::vec3( 1.0f,  0.0f, 1.0f);
      clip_eq.m_clip_equations[1] = fastuidraw::vec3(-1.0f,  0.0f, 1.0f);
      clip_eq.m_clip_equations[2] = fastuidraw::vec3( 0.0f,  1.0f, 1.0f);
      clip_eq.m_clip_equations[3] = fastuidraw::vec3( 0.0f, -1.0f, 1.0f);
      clip_equations(clip_eq);
    }

    void
    set_clip_equations_to_clip_rect(void);

    std::bitset<4>
    set_clip_equations_to_clip_rect(const fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> &prev_clip);

    const fastuidraw::float3x3&
    item_matrix_inverse_transpose(void);

    const fastuidraw::PainterItemMatrix&
    current_painter_item_matrix(void)
    {
      return m_item_matrix;
    }

    const fastuidraw::float3x3&
    item_matrix(void)
    {
      return m_item_matrix.m_item_matrix;
    }

    void
    item_matrix(const fastuidraw::float3x3 &v, bool trick_transition)
    {
      m_item_matrix_transition_tricky = m_item_matrix_transition_tricky || trick_transition;
      m_inverse_transpose_not_ready = true;
      m_item_matrix.m_item_matrix = v;
      m_item_matrix_state = fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix>();
    }

    const fastuidraw::PainterClipEquations&
    clip_equations(void)
    {
      return m_clip_equations;
    }

    void
    clip_equations(const fastuidraw::PainterClipEquations &v)
    {
      m_clip_equations = v;
      m_clip_equations_state = fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations>();
    }

    const fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix>&
    current_item_marix_state(fastuidraw::PainterPackedValuePool &pool)
    {
      if (!m_item_matrix_state)
        {
          m_item_matrix_state = pool.create_packed_value(m_item_matrix);
        }
      return m_item_matrix_state;
    }

    void
    item_matrix_state(const fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix> &v,
                              bool mark_dirty)
    {
      m_item_matrix_transition_tricky = m_item_matrix_transition_tricky || mark_dirty;
      m_inverse_transpose_not_ready = m_inverse_transpose_not_ready || mark_dirty;
      m_item_matrix_state = v;
      m_item_matrix = v.value();
    }

    const fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations>&
    clip_equations_state(fastuidraw::PainterPackedValuePool &pool)
    {
      if (!m_clip_equations_state)
        {
          m_clip_equations_state = pool.create_packed_value(m_clip_equations);
        }
      return m_clip_equations_state;
    }

    void
    clip_equations_state(const fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> &v)
    {
      m_clip_equations_state = v;
      m_clip_equations = v.value();
    }

    bool
    item_matrix_transition_tricky(void)
    {
      return m_item_matrix_transition_tricky;
    }

    void
    clip_polygon(fastuidraw::c_array<const fastuidraw::vec2> pts,
                 std::vector<fastuidraw::vec2> &out_pts,
                 std::vector<fastuidraw::vec2> &work_vec2s);

    bool
    rect_is_culled(const fastuidraw::vec2 &pmin, const fastuidraw::vec2 &wh);

    clip_rect m_clip_rect;
    bool m_all_content_culled;

  private:

    bool m_item_matrix_transition_tricky;
    fastuidraw::PainterItemMatrix m_item_matrix;
    fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix> m_item_matrix_state;
    fastuidraw::PainterClipEquations m_clip_equations;
    fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> m_clip_equations_state;
    bool m_inverse_transpose_not_ready;
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
    fastuidraw::reference_counted_ptr<fastuidraw::PainterBlendShader> m_blend;
    fastuidraw::BlendMode::packed_value m_blend_mode;
    fastuidraw::range_type<unsigned int> m_clip_equation_series;

    clip_rect_state m_clip_rect_state;
    float m_curve_flatness;
  };

  class ComplementFillRule:public fastuidraw::CustomFillRuleBase
  {
  public:
    ComplementFillRule(const fastuidraw::CustomFillRuleBase *p):
      m_p(p)
    {
      FASTUIDRAWassert(m_p);
    }

    bool
    operator()(int w) const
    {
      return !m_p->operator()(w);
    }

  private:
    const fastuidraw::CustomFillRuleBase *m_p;
  };

  /* To avoid allocating memory all the time, we store the
   * clip polygon data within the same std::vector<vec3>.
   * The usage pattern is that the last element allocated
   * is the first element to be freed.
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
      FASTUIDRAWassert(!m_sz.empty());
      FASTUIDRAWassert(m_sz.back() <= m_store.size());

      set_current(fastuidraw::make_c_array(m_store).sub_array(m_sz.back()));
      m_store.resize(m_sz.back());
      m_sz.pop_back();
    }

    void
    set_current(fastuidraw::c_array<const fastuidraw::vec3> new_equations)
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

    fastuidraw::c_array<const fastuidraw::vec3>
    current(void)
    {
      return fastuidraw::make_c_array(m_current);
    }

    /* @param (input) clip_matrix_local transformation from local to clip coordinates
     * @param (input) in_out_pts[0] convex polygon to clip
     * @return what index into in_out_pts that holds polygon clipped
     */
    unsigned int
    clip_against_current(const fastuidraw::float3x3 &clip_matrix_local,
                         fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> &in_out_pts);

  private:
    std::vector<fastuidraw::vec3> m_store;
    std::vector<unsigned int> m_sz;
    std::vector<fastuidraw::vec3> m_current;
  };

  class StrokingItem
  {
  public:
    StrokingItem(void)
    {}

    StrokingItem(bool with_aa,
                 const fastuidraw::PainterStrokeShader &shader,
                 bool use_arc_shading,
                 unsigned int start, unsigned int size):
      m_range(start, start + size)
    {
      if (!use_arc_shading)
        {
          m_shader_pass1 = (with_aa) ? &shader.aa_shader_pass1() : &shader.non_aa_shader();
          m_shader_pass2 = (with_aa) ? &shader.aa_shader_pass2() : nullptr;
        }
      else
        {
          m_shader_pass1 = (with_aa) ? &shader.arc_aa_shader_pass1() : &shader.arc_non_aa_shader();
          m_shader_pass2 = (with_aa) ? &shader.arc_aa_shader_pass2() : nullptr;
        }
    }

    fastuidraw::range_type<unsigned int> m_range;
    const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> *m_shader_pass1;
    const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> *m_shader_pass2;
  };

  class StrokingItems
  {
  public:
    explicit
    StrokingItems(bool with_aa):
      m_with_aa(with_aa),
      m_count(0),
      m_last_end(0)
    {}

    void
    add_element(bool use_arc_shading, const fastuidraw::PainterStrokeShader &shader, unsigned int size)
    {
      m_data[m_count] = StrokingItem(m_with_aa, shader, use_arc_shading, m_last_end, size);
      if (m_count == 0
          || m_data[m_count].m_shader_pass1 != m_data[m_count - 1].m_shader_pass1
          || m_data[m_count].m_shader_pass2 != m_data[m_count - 1].m_shader_pass2)
        {
          ++m_count;
        }
      else
        {
          m_data[m_count - 1].m_range.m_end += size;
        }
      m_last_end = m_data[m_count - 1].m_range.m_end;
    }

    fastuidraw::c_array<const StrokingItem>
    data(void) const
    {
      return fastuidraw::c_array<const StrokingItem>(m_data).sub_array(0, m_count);
    }

    const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>*
    current_shader_pass1(void) const
    {
      FASTUIDRAWassert(m_count > 0);
      return m_data[m_count - 1].m_shader_pass1;
    }

    const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>*
    current_shader_pass2(void) const
    {
      FASTUIDRAWassert(m_count > 0);
      return m_data[m_count - 1].m_shader_pass2;
    }

  private:
    fastuidraw::vecN<StrokingItem, 3> m_data;
    bool m_with_aa;
    unsigned int m_count, m_last_end;
  };

  class PainterWorkRoom
  {
  public:
    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_pts_update_clip_series;
    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_clipper_vec2s;

    // work room for drawing polygons
    std::vector<fastuidraw::vec2> m_pts_draw_convex_polygon;
    std::vector<fastuidraw::PainterIndex> m_polygon_indices;
    std::vector<fastuidraw::PainterAttribute> m_polygon_attribs;

    // work room for stroking
    std::vector<fastuidraw::c_array<const fastuidraw::PainterAttribute> > m_stroke_attrib_chunks;
    std::vector<fastuidraw::c_array<const fastuidraw::PainterIndex> > m_stroke_index_chunks;
    std::vector<int> m_stroke_increment_zs;
    std::vector<int> m_stroke_start_zs;
    std::vector<int> m_stroke_index_adjusts;
    std::vector<const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>* > m_stroke_shaders_pass1;
    std::vector<const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>* > m_stroke_shaders_pass2;
    fastuidraw::StrokedPath::ChunkSet m_stroke_chunk_set;
    fastuidraw::StrokedPath::ScratchSpace m_stroked_path_scratch;
    fastuidraw::StrokedCapsJoins::ChunkSet m_stroke_caps_joins_chunk_set;
    fastuidraw::StrokedCapsJoins::ScratchSpace m_stroked_caps_joins_scratch;

    // common filling work room
    WindingSet m_fill_ws;
    fastuidraw::FilledPath::ScratchSpace m_filled_path_scratch;

    // work room for fill
    std::vector<fastuidraw::c_array<const fastuidraw::PainterAttribute> > m_fill_attrib_chunks;
    std::vector<fastuidraw::c_array<const fastuidraw::PainterIndex> > m_fill_index_chunks;
    std::vector<int> m_fill_index_adjusts;
    std::vector<unsigned int> m_fill_selector, m_fill_subset_selector;

    // work room for anti-alias fuzz of fill
    std::vector<fastuidraw::c_array<const fastuidraw::PainterAttribute> > m_fill_aa_fuzz_attrib_chunks;
    std::vector<fastuidraw::c_array<const fastuidraw::PainterIndex> > m_fill_aa_fuzz_index_chunks;
    std::vector<int> m_fill_aa_fuzz_index_adjusts;
    std::vector<int> m_fill_aa_fuzz_start_zs;
    std::vector<int> m_fill_aa_fuzz_z_increments;
  };

  class PainterPrivate
  {
  public:
    explicit
    PainterPrivate(fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend> backend);

    void
    draw_generic(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                 const fastuidraw::PainterData &draw,
                 fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
                 fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                 fastuidraw::c_array<const int> index_adjusts,
                 fastuidraw::c_array<const unsigned int> attrib_chunk_selector,
                 int z,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back);

    void
    draw_generic(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                 const fastuidraw::PainterData &draw,
                 const fastuidraw::PainterPacker::DataWriter &src,
                 int z,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back);

    int
    pre_draw_anti_alias_fuzz(const fastuidraw::FilledPath &filled_path, fastuidraw::c_array<const unsigned int> subsets,
                             const WindingSet &wset,
                             std::vector<int> &z_increments,
                             std::vector<fastuidraw::c_array<const fastuidraw::PainterAttribute> > &attrib_chunks,
                             std::vector<fastuidraw::c_array<const fastuidraw::PainterIndex> > &index_chunks,
                             std::vector<int> &index_adjusts,
                             std::vector<int> &start_zs);

    void
    draw_generic_z_layered(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                           const fastuidraw::PainterData &draw,
                           fastuidraw::c_array<const int> z_increments, int zinc_sum,
                           fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
                           fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                           fastuidraw::c_array<const int> index_adjusts,
                           fastuidraw::c_array<const int> start_zs, int startz,
                           const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back);

    typedef fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> ItemShaderRef;
    typedef const ItemShaderRef ConstItemShaderRef;
    typedef ConstItemShaderRef *ConstItemShaderRefPtr;
    void
    draw_generic_z_layered(fastuidraw::c_array<const ConstItemShaderRefPtr> shaders,
                           const fastuidraw::PainterData &draw,
                           fastuidraw::c_array<const int> z_increments, int zinc_sum,
                           fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
                           fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                           fastuidraw::c_array<const int> index_adjusts,
                           fastuidraw::c_array<const int> start_zs, int startz,
                           const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back);

    void
    stroke_path_raw(const fastuidraw::PainterStrokeShader &edge_shader,
                    bool edge_use_arc_shaders,
                    bool join_use_arc_shaders,
                    bool caps_use_arc_shaders,
                    const fastuidraw::PainterData &pdraw,
                    const fastuidraw::PainterAttributeData *edge_data,
                    fastuidraw::c_array<const unsigned int> edge_chunks,
                    const fastuidraw::PainterAttributeData *cap_data,
                    fastuidraw::c_array<const unsigned int> cap_chunks,
                    const fastuidraw::PainterAttributeData* join_data,
                    fastuidraw::c_array<const unsigned int> join_chunks,
                    bool with_anti_aliasing,
                    const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back);

    void
    stroke_path_common(const fastuidraw::PainterStrokeShader &shader,
                       const fastuidraw::PainterData &draw,
                       const fastuidraw::StrokedPath &path, float thresh,
                       bool close_contours,
                       enum fastuidraw::PainterEnums::cap_style cp,
                       enum fastuidraw::PainterEnums::join_style js,
                       bool with_anti_aliasing,
                       const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back);

    bool
    update_clip_equation_series(const fastuidraw::vec2 &pmin,
                                const fastuidraw::vec2 &pmax);

    float
    compute_path_magnification(const fastuidraw::Path &path);

    float
    compute_path_magnification_non_perspective(void);

    float
    compute_path_magnification_perspective(const fastuidraw::Path &path);

    const fastuidraw::StrokedPath*
    select_stroked_path(const fastuidraw::Path &path,
                        const fastuidraw::PainterStrokeShader &shader,
                        const fastuidraw::PainterData &draw,
                        float &out_thresh);

    const fastuidraw::FilledPath&
    select_filled_path(const fastuidraw::Path &path);

    fastuidraw::vec2 m_resolution;
    fastuidraw::vec2 m_one_pixel_width;
    float m_curve_flatness;
    bool m_stroke_arc_path;
    int m_current_z;
    clip_rect_state m_clip_rect_state;
    std::vector<occluder_stack_entry> m_occluder_stack;
    std::vector<state_stack_entry> m_state_stack;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker> m_core;
    fastuidraw::PainterPackedValuePool m_pool;
    fastuidraw::PainterPackedValue<fastuidraw::PainterBrush> m_reset_brush, m_black_brush;
    fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix> m_identiy_matrix;
    ClipEquationStore m_clip_store;
    PainterWorkRoom m_work_room;
    unsigned int m_max_attribs_per_block, m_max_indices_per_block;
  };
}

//////////////////////////////////////////
// clip_rect methods
void
clip_rect::
intersect(const clip_rect &rect)
{
  if (!rect.m_enabled)
    {
      return;
    }

  if (m_enabled)
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
   * before hand so that the occluders block all that
   * is drawn below them.
   */
  p->increment_z();
  for(const auto &s : m_set_occluder_z)
    {
      ZDelayedAction *ptr;
      FASTUIDRAWassert(dynamic_cast<ZDelayedAction*>(s.get()) != nullptr);
      ptr = static_cast<ZDelayedAction*>(s.get());
      ptr->finalize_z(p->current_z());
    }
}

///////////////////////////////////////////////
// clip_rect_stat methods
const fastuidraw::float3x3&
clip_rect_state::
item_matrix_inverse_transpose(void)
{
  if (m_inverse_transpose_not_ready)
    {
      m_inverse_transpose_not_ready = false;
      m_item_matrix.m_item_matrix.inverse_transpose(m_item_matrix_inverse_transpose);
    }
  return m_item_matrix_inverse_transpose;
}

void
clip_rect_state::
set_clip_equations_to_clip_rect(void)
{
  fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> null;
  set_clip_equations_to_clip_rect(null);
}


std::bitset<4>
clip_rect_state::
set_clip_equations_to_clip_rect(const fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> &pcl)
{
  if (m_clip_rect.empty())
    {
      m_all_content_culled = true;
      return std::bitset<4>();
    }

  m_item_matrix_transition_tricky = false;
  const fastuidraw::float3x3 &inverse_transpose(item_matrix_inverse_transpose());
  /* The clipping window is given by:
   *   w * min_x <= x <= w * max_x
   *   w * min_y <= y <= w * max_y
   * which expands to
   *     x + w * min_x >= 0  --> ( 1,  0, -min_x)
   *    -x - w * max_x >= 0  --> (-1,  0, max_x)
   *     y + w * min_y >= 0  --> ( 0,  1, -min_y)
   *    -y - w * max_y >= 0  --> ( 0, -1, max_y)
   *   However, the clip equations are in clip coordinates
   *   so we need to apply the inverse transpose of the
   *   transformation matrix to the 4 vectors
   */
  fastuidraw::PainterClipEquations cl;
  cl.m_clip_equations[0] = inverse_transpose * fastuidraw::vec3( 1.0f,  0.0f, -m_clip_rect.m_min.x());
  cl.m_clip_equations[1] = inverse_transpose * fastuidraw::vec3(-1.0f,  0.0f,  m_clip_rect.m_max.x());
  cl.m_clip_equations[2] = inverse_transpose * fastuidraw::vec3( 0.0f,  1.0f, -m_clip_rect.m_min.y());
  cl.m_clip_equations[3] = inverse_transpose * fastuidraw::vec3( 0.0f, -1.0f,  m_clip_rect.m_max.y());
  clip_equations(cl);

  for(int i = 0; i < 4; ++i)
    {
      if (clip_equation_clips_everything(cl.m_clip_equations[i]))
        {
          m_all_content_culled = true;
          return std::bitset<4>();
        }
    }

  if (!pcl)
    {
      return std::bitset<4>();
    }

  /* see if the vertices of the clipping rectangle (post m_item_matrix applied)
   * are all within the passed clipped equations.
   */
  const fastuidraw::PainterClipEquations &eq(pcl.value());
  const fastuidraw::float3x3 &m(m_item_matrix.m_item_matrix);
  std::bitset<4> return_value;
  fastuidraw::vecN<fastuidraw::vec3, 4> q;

  /* return_value[i] is true exactly when each point of the rectangle is inside
   *                 the i'th clip equation.
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

void
clip_rect_state::
clip_polygon(fastuidraw::c_array<const fastuidraw::vec2> pts,
             std::vector<fastuidraw::vec2> &out_pts,
             std::vector<fastuidraw::vec2> &work_vec2s)
{
  const fastuidraw::PainterClipEquations &eqs(m_clip_equations);
  const fastuidraw::float3x3 &m(item_matrix());

  /* Clip planes are in clip coordinates, i.e.
   *   ClipDistance[i] = dot(M * p, clip_equation[i])
   *                   = dot(p, transpose(M)(clip_equation[i])
   * To place them in local coordinates, then we need to apply
   * the transpose of m_item_matrix to the clip planes
   * which is the same as post-multiplying the matrix.
   */
  fastuidraw::detail::clip_against_plane(eqs.m_clip_equations[0] * m, pts,
                                         work_vec2s);

  fastuidraw::detail::clip_against_plane(eqs.m_clip_equations[1] * m,
                                         fastuidraw::make_c_array(work_vec2s),
                                         out_pts);

  fastuidraw::detail::clip_against_plane(eqs.m_clip_equations[2] * m,
                                         fastuidraw::make_c_array(out_pts),
                                         work_vec2s);

  fastuidraw::detail::clip_against_plane(eqs.m_clip_equations[3] * m,
                                         fastuidraw::make_c_array(work_vec2s),
                                         out_pts);
}

bool
clip_rect_state::
rect_is_culled(const fastuidraw::vec2 &pmin, const fastuidraw::vec2 &wh)
{
  /* apply the current transformation matrix to
   * the corners of the clipping rectangle and check
   * if there is a clipping plane for which all
   * those points are on the wrong size.
   */
  fastuidraw::vec2 pmax(wh + pmin);
  fastuidraw::vecN<fastuidraw::vec3, 4> pts;
  pts[0] = m_item_matrix.m_item_matrix * fastuidraw::vec3(pmin.x(), pmin.y(), 1.0f);
  pts[1] = m_item_matrix.m_item_matrix * fastuidraw::vec3(pmin.x(), pmax.y(), 1.0f);
  pts[2] = m_item_matrix.m_item_matrix * fastuidraw::vec3(pmax.x(), pmax.y(), 1.0f);
  pts[3] = m_item_matrix.m_item_matrix * fastuidraw::vec3(pmax.x(), pmin.y(), 1.0f);

  if (m_clip_rect.m_enabled)
    {
      /* use equations from clip state
       */
      return all_pts_culled_by_one_half_plane(pts, m_clip_equations);
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

/////////////////////////////////
//ClipEquationStore methods
unsigned int
ClipEquationStore::
clip_against_current(const fastuidraw::float3x3 &clip_matrix_local,
                     fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> &in_out_pts)
{
  fastuidraw::c_array<const fastuidraw::vec3> clips(current());
  unsigned int src, dst, i;
  for(i = 0, src = 0, dst = 1; i < clips.size(); ++i, std::swap(src, dst))
    {
      fastuidraw::vec3 nc;
      fastuidraw::c_array<const fastuidraw::vec2> in;

      nc = clips[i] * clip_matrix_local;
      in = fastuidraw::make_c_array(in_out_pts[src]);
      fastuidraw::detail::clip_against_plane(nc, in, in_out_pts[dst]);
    }
  return src;
}

//////////////////////////////////
// PainterPrivate methods
PainterPrivate::
PainterPrivate(fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend> backend):
  m_resolution(1.0f, 1.0f),
  m_one_pixel_width(1.0f, 1.0f),
  m_curve_flatness(1.0f),
  m_stroke_arc_path(false),
  m_pool(backend->configuration_base().alignment())
{
  m_core = FASTUIDRAWnew fastuidraw::PainterPacker(backend);
  m_reset_brush = m_pool.create_packed_value(fastuidraw::PainterBrush());
  m_black_brush = m_pool.create_packed_value(fastuidraw::PainterBrush()
                                             .pen(0.0f, 0.0f, 0.0f, 0.0f));
  m_identiy_matrix = m_pool.create_packed_value(fastuidraw::PainterItemMatrix());
  m_current_z = 1;
  m_max_attribs_per_block = backend->attribs_per_mapping();
  m_max_indices_per_block = backend->indices_per_mapping();
}

bool
PainterPrivate::
update_clip_equation_series(const fastuidraw::vec2 &pmin,
                            const fastuidraw::vec2 &pmax)
{
  fastuidraw::vec2 center(0.0f, 0.0f);
  unsigned int src;

  m_work_room.m_pts_update_clip_series[0].resize(4);
  m_work_room.m_pts_update_clip_series[0][0] = pmin;
  m_work_room.m_pts_update_clip_series[0][1] = fastuidraw::vec2(pmin.x(), pmax.y());
  m_work_room.m_pts_update_clip_series[0][2] = pmax;
  m_work_room.m_pts_update_clip_series[0][3] = fastuidraw::vec2(pmax.x(), pmin.y());
  src = m_clip_store.clip_against_current(m_clip_rect_state.item_matrix(),
                                          m_work_room.m_pts_update_clip_series);

  /* the input rectangle clipped to the previous clipping equation
   * array is now stored in m_work_room.m_pts_update_clip_series[src]
   */
  fastuidraw::c_array<const fastuidraw::vec2> poly;
  poly = fastuidraw::make_c_array(m_work_room.m_pts_update_clip_series[src]);

  m_clip_store.clear_current();

  /* if the rectangle clipped is empty, then we are completely clipped.
   */
  if (poly.empty())
    {
      return true;
    }

  /* compute center of polygon so that we can correctly
   * orient the normal vectors of the sides.
   */
  for(const auto &pt : poly)
    {
      center += pt;
    }
  center /= static_cast<float>(poly.size());

  const fastuidraw::float3x3 &inverse_transpose(m_clip_rect_state.item_matrix_inverse_transpose());
  /* extract the normal vectors of the polygon sides with
   * correct orientation.
   */
  for(unsigned int i = 0; i < poly.size(); ++i)
    {
      fastuidraw::vec2 v, n;
      unsigned int next_i;

      next_i = i + 1;
      next_i = (next_i == poly.size()) ? 0 : next_i;
      v = poly[next_i] - poly[i];
      n = fastuidraw::vec2(v.y(), -v.x());
      if (dot(center - poly[i], n) < 0.0f)
        {
          n = -n;
        }

      /* The clip equation we have in local coordinates
       * is dot(n, p - poly[i]) >= 0. Algebra time:
       *   dot(n, p - poly[i]) = n.x * p.x + n.y * p.y + (-poly[i].x * n.x - poly[i].y * n.y)
       *                      = dot( (n, R), (p, 1))
       * where
       *   R = -poly[i].x * n.x - poly[i].y * n.y = -dot(n, poly[i])
       * We want the clip equations in clip coordinates though:
       *   dot( (n, R), (p, 1) ) = dot( (n, R), inverseM(M(p, 1)) )
       *                         = dot( inverse_transpose_M(R,1), M(p, 1))
       * thus the vector to use is inverse_transpose_M(R,1)
       */
      fastuidraw::vec3 nn(n.x(), n.y(), -dot(n, poly[i]));
      m_clip_store.add_to_current(inverse_transpose * nn);
    }

  return false;
}

float
PainterPrivate::
compute_path_magnification_non_perspective(void)
{
  float d;
  const fastuidraw::float3x3 &m(m_clip_rect_state.item_matrix());

  /* Use the sqrt of the area distortion to determine the dividing factor,
   * for matrices with a great deal of skew, this will choose a lower a
   * level of detail that taking the operator norm of the matrix. For
   * reference, the sqrt of the area distortion is the geometric mean
   * of the 2 singular values of a 2x2 matrix.
   *
   * The multiplier 0.25 comes from that normalized device
   * coordinates are [-1, 1]x[-1, 1] and thus the scaling
   * factor to pixel coordinates is half of m_resolution
   * for each dimension.
   *
   * QUESTION: should we instead take the maxiumum of the two
   * singular values instead?
   */
  d = fastuidraw::t_abs(m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0));
  d *= 0.25f * m_resolution.x() * m_resolution.y() / fastuidraw::t_abs(m(2, 2));
  d = fastuidraw::t_sqrt(d);

  return d;
}

float
PainterPrivate::
compute_path_magnification_perspective(const fastuidraw::Path &path)
{
  /* Clip the path bounding box against all the clip
   * equations and compute the area of the polygon
   * clipped.
   */
  fastuidraw::vec2 bb_min, bb_max;
  bool r;
  unsigned int src;

  r = path.approximate_bounding_box(&bb_min, &bb_max);
  if (!r)
    {
      /* it does not matter, since the path is essentially
       * empty. By using a negative value, we get the
       * default tessellation of the path (which is based
       * off of curvature).
       */
      return -1.0f;
    }
  m_work_room.m_clipper_vec2s[0].resize(4);
  m_work_room.m_clipper_vec2s[0][0] = bb_min;
  m_work_room.m_clipper_vec2s[0][1] = fastuidraw::vec2(bb_min.x(), bb_max.y());
  m_work_room.m_clipper_vec2s[0][2] = bb_max;
  m_work_room.m_clipper_vec2s[0][3] = fastuidraw::vec2(bb_max.x(), bb_min.y());

  /* TODO: for stroking, it might be that although the
   * original path is completely clipped, the stroke of
   * is not. It might be wise to inflate the geometry
   * of the path by how much slack the stroking parameters
   * require.
   */
  const fastuidraw::float3x3 &m(m_clip_rect_state.item_matrix());
  src = m_clip_store.clip_against_current(m,
                                          m_work_room.m_clipper_vec2s);

  fastuidraw::c_array<const fastuidraw::vec2> poly;
  poly = make_c_array(m_work_room.m_clipper_vec2s[src]);

  if (poly.empty())
    {
      /* bounding box of path is clipped, just take default
       * tessellation and call it a day (!).
       */
      return -1.0f;
    }

  /* Get the area of the polygon in item coordinates
   * and in pixel coodinates. The square root of that
   * ratio of the area is what we are going to use as
   * our "d". Bad things happen if the clipped polygon
   * still has points where w == 0.0.
   *
   * TODO: is using area wise? With perpsective, different
   * portions of the path will be zoomed in more than
   * others. The area represents a kind of average. Perhaps
   * we should take at each point the distortion of the
   * transformation at the point and take the worse of
   * the bunch.
   */
  float area_local_coords(0.0f), area_pixel_coords(0.0f), ratio;
  for(unsigned int i = 0, endi = poly.size(); i < endi; ++i)
    {
      unsigned int next_i;
      next_i = (i == endi - 1) ? 0 : i + 1u;

      fastuidraw::vec2 p(poly[i]);
      fastuidraw::vec2 q(poly[next_i]);
      area_local_coords += p.x() * q.y() - q.x() * p.y();

      fastuidraw::vec3 c_p, c_q;
      c_p = m * fastuidraw::vec3(p.x(), p.y(), 1.0f);
      c_q = m * fastuidraw::vec3(q.x(), q.y(), 1.0f);

      p = m_resolution * fastuidraw::vec2(c_p.x(), c_p.y()) / c_p.z();
      q = m_resolution * fastuidraw::vec2(c_q.x(), c_q.y()) / c_q.z();
      area_pixel_coords += p.x() * q.y() - q.x() * p.y();
    }

  area_local_coords = fastuidraw::t_abs(area_local_coords);
  area_pixel_coords = fastuidraw::t_abs(area_pixel_coords);
  if (area_local_coords <= 0.0f || area_pixel_coords <= 0.0f)
    {
      return -1.0f;
    }
  ratio = area_pixel_coords / area_local_coords;

  /* in the loop above, the pixel coordinates were NOT scaled
   * by 0.5 (needed because the normalized coordinates are in
   * range [-1.0, 1.0], so we scale the final ratio now.
   */
  return 0.5f * fastuidraw::t_sqrt(ratio);
}

float
PainterPrivate::
compute_path_magnification(const fastuidraw::Path &path)
{
  bool no_perspective;
  const fastuidraw::float3x3 &m(m_clip_rect_state.item_matrix());

  no_perspective = (m(2, 0) == 0.0f && m(2, 1) == 0.0f);
  if (no_perspective)
    {
      return compute_path_magnification_non_perspective();
    }
  else
    {
      return compute_path_magnification_perspective(path);
    }
}

const fastuidraw::StrokedPath*
PainterPrivate::
select_stroked_path(const fastuidraw::Path &path,
                    const fastuidraw::PainterStrokeShader &shader,
                    const fastuidraw::PainterData &draw,
                    float &thresh)
{
  using namespace fastuidraw;
  float mag, t;

  mag = compute_path_magnification(path);
  thresh = shader.stroking_data_selector()->compute_thresh(draw.m_item_shader_data.data().data_base(),
                                                           mag, m_curve_flatness);
  t = fastuidraw::t_min(thresh, m_curve_flatness / mag);

  const TessellatedPath *tess;
  if (m_stroke_arc_path)
    {
      tess = path.arc_tessellation(t).get();
    }
  else
    {
      tess = path.tessellation(t).get();
    }
  return tess->stroked().get();
}

const fastuidraw::FilledPath&
PainterPrivate::
select_filled_path(const fastuidraw::Path &path)
{
  using namespace fastuidraw;
  float mag, thresh;

  mag = compute_path_magnification(path);
  thresh = m_curve_flatness / mag;
  return *path.tessellation(thresh)->filled();
}

void
PainterPrivate::
draw_generic(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
             const fastuidraw::PainterData &draw,
             fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
             fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
             fastuidraw::c_array<const int> index_adjusts,
             fastuidraw::c_array<const unsigned int> attrib_chunk_selector,
             int z,
             const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back)
{
  fastuidraw::PainterPackerData p(draw);

  p.m_clip = m_clip_rect_state.clip_equations_state(m_pool);
  p.m_matrix = m_clip_rect_state.current_item_marix_state(m_pool);
  m_core->draw_generic(shader, p, attrib_chunks, index_chunks, index_adjusts, attrib_chunk_selector, z, call_back);
}

void
PainterPrivate::
draw_generic(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
             const fastuidraw::PainterData &draw,
             const fastuidraw::PainterPacker::DataWriter &src,
             int z,
             const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back)
{
  fastuidraw::PainterPackerData p(draw);
  p.m_clip = m_clip_rect_state.clip_equations_state(m_pool);
  p.m_matrix = m_clip_rect_state.current_item_marix_state(m_pool);
  m_core->draw_generic(shader, p, src, z, call_back);
}

int
PainterPrivate::
pre_draw_anti_alias_fuzz(const fastuidraw::FilledPath &filled_path,
                         fastuidraw::c_array<const unsigned int> subsets,
                         const WindingSet &wset,
                         std::vector<int> &z_increments,
                         std::vector<fastuidraw::c_array<const fastuidraw::PainterAttribute> > &attrib_chunks,
                         std::vector<fastuidraw::c_array<const fastuidraw::PainterIndex> > &index_chunks,
                         std::vector<int> &index_adjusts,
                         std::vector<int> &start_zs)
{
  int return_value(0);

  z_increments.clear();
  attrib_chunks.clear();
  index_chunks.clear();
  index_adjusts.clear();
  start_zs.clear();
  for(unsigned int s : subsets)
    {
      fastuidraw::FilledPath::Subset subset(filled_path.subset(s));
      const fastuidraw::PainterAttributeData &data(subset.aa_fuzz_painter_data());
      fastuidraw::c_array<const int> winding_numbers(subset.winding_numbers());

      for(int w : winding_numbers)
        {
          if (wset(w))
            {
              unsigned int ch;
              fastuidraw::range_type<int> R;

              ch = fastuidraw::FilledPath::Subset::aa_fuzz_chunk_from_winding_number(w);
              R = data.z_range(ch);

              attrib_chunks.push_back(data.attribute_data_chunk(ch));
              index_chunks.push_back(data.index_data_chunk(ch));
              index_adjusts.push_back(data.index_adjust_chunk(ch));

              z_increments.push_back(R.difference());
              start_zs.push_back(R.m_begin);

              return_value += z_increments.back();
            }
        }
    }

  return return_value;
}

void
PainterPrivate::
draw_generic_z_layered(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                       const fastuidraw::PainterData &draw,
                       fastuidraw::c_array<const int> z_increments, int zinc_sum,
                       fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
                       fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                       fastuidraw::c_array<const int> index_adjusts,
                       fastuidraw::c_array<const int> start_zs, int startz,
                       const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back)
{
  for(unsigned int i = 0, incr_z = zinc_sum; i < start_zs.size(); ++i)
    {
      int z;

      incr_z -= z_increments[i];
      z = startz + incr_z - start_zs[i];

      draw_generic(shader, draw,
                   attrib_chunks.sub_array(i, 1),
                   index_chunks.sub_array(i, 1),
                   index_adjusts.sub_array(i, 1),
                   fastuidraw::c_array<const unsigned int>(),
                   z, call_back);
    }
}



void
PainterPrivate::
draw_generic_z_layered(fastuidraw::c_array<const ConstItemShaderRefPtr> shaders,
                       const fastuidraw::PainterData &draw,
                       fastuidraw::c_array<const int> z_increments, int zinc_sum,
                       fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
                       fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                       fastuidraw::c_array<const int> index_adjusts,
                       fastuidraw::c_array<const int> start_zs, int startz,
                       const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back)
{
  for(unsigned int i = 0, incr_z = zinc_sum; i < start_zs.size(); ++i)
    {
      int z;

      incr_z -= z_increments[i];
      z = startz + incr_z - start_zs[i];

      draw_generic(*shaders[i], draw,
                   attrib_chunks.sub_array(i, 1),
                   index_chunks.sub_array(i, 1),
                   index_adjusts.sub_array(i, 1),
                   fastuidraw::c_array<const unsigned int>(),
                   z, call_back);
    }
}

void
PainterPrivate::
stroke_path_common(const fastuidraw::PainterStrokeShader &shader,
                   const fastuidraw::PainterData &draw,
                   const fastuidraw::StrokedPath &path, float thresh,
                   bool close_contours,
                   enum fastuidraw::PainterEnums::cap_style cp,
                   enum fastuidraw::PainterEnums::join_style js,
                   bool with_anti_aliasing,
                   const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back)
{
  using namespace fastuidraw;

  if (m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  const PainterAttributeData *edge_data(nullptr), *cap_data(nullptr), *join_data(nullptr);
  bool is_miter_join;
  const PainterShaderData::DataBase *raw_data;
  const StrokedCapsJoins &caps_joins(path.caps_joins());
  bool edge_arc_shader(path.has_arcs()), cap_arc_shader(false), join_arc_shader(false);

  raw_data = draw.m_item_shader_data.data().data_base();
  edge_data = &path.edges();
  if (!close_contours)
    {
      switch(cp)
        {
        case PainterEnums::rounded_caps:
          if (edge_arc_shader)
            {
              cap_data = &caps_joins.arc_rounded_caps();
              cap_arc_shader = true;
            }
          else
            {
              cap_data = &caps_joins.rounded_caps(thresh);
            }
          break;

        case PainterEnums::square_caps:
          cap_data = &caps_joins.square_caps();
          break;

        case PainterEnums::flat_caps:
          cap_data = nullptr;
          break;

        case PainterEnums::number_cap_styles:
          cap_data = &caps_joins.adjustable_caps();
          break;
        }
    }

  switch(js)
    {
    case PainterEnums::miter_clip_joins:
      is_miter_join = true;
      join_data = &caps_joins.miter_clip_joins();
      break;

    case PainterEnums::miter_bevel_joins:
      is_miter_join = true;
      join_data = &caps_joins.miter_bevel_joins();
      break;

    case PainterEnums::miter_joins:
      is_miter_join = true;
      join_data = &caps_joins.miter_joins();
      break;

    case PainterEnums::bevel_joins:
      is_miter_join = false;
      join_data = &caps_joins.bevel_joins();
      break;

    case PainterEnums::rounded_joins:
      is_miter_join = false;
      if (edge_arc_shader)
        {
          join_data = &caps_joins.arc_rounded_joins();
          join_arc_shader = true;
        }
      else
        {
          join_data = &caps_joins.rounded_joins(thresh);
        }
      break;

    default:
      join_data = nullptr;
      is_miter_join = false;
    }

  float pixels_additional_room(0.0f), item_space_additional_room(0.0f);
  shader.stroking_data_selector()->stroking_distances(raw_data, &pixels_additional_room, &item_space_additional_room);

  path.compute_chunks(m_work_room.m_stroked_path_scratch,
                      m_clip_store.current(),
                      m_clip_rect_state.item_matrix(),
                      m_one_pixel_width,
                      pixels_additional_room,
                      item_space_additional_room,
                      close_contours,
                      m_max_attribs_per_block,
                      m_max_indices_per_block,
                      m_work_room.m_stroke_chunk_set);

  caps_joins.compute_chunks(m_work_room.m_stroked_caps_joins_scratch,
                            m_clip_store.current(),
                            m_clip_rect_state.item_matrix(),
                            m_one_pixel_width,
                            pixels_additional_room,
                            item_space_additional_room,
                            close_contours,
                            m_max_attribs_per_block,
                            m_max_indices_per_block,
                            is_miter_join,
                            m_work_room.m_stroke_caps_joins_chunk_set);

  stroke_path_raw(shader, edge_arc_shader, join_arc_shader, cap_arc_shader, draw,
                  edge_data, m_work_room.m_stroke_chunk_set.edge_chunks(),
                  cap_data, m_work_room.m_stroke_caps_joins_chunk_set.cap_chunks(),
                  join_data, m_work_room.m_stroke_caps_joins_chunk_set.join_chunks(),
                  with_anti_aliasing, call_back);
}

void
PainterPrivate::
stroke_path_raw(const fastuidraw::PainterStrokeShader &shader,
                bool edge_use_arc_shaders,
                bool join_use_arc_shaders,
                bool cap_use_arc_shaders,
                const fastuidraw::PainterData &pdraw,
                const fastuidraw::PainterAttributeData *edge_data,
                fastuidraw::c_array<const unsigned int> edge_chunks,
                const fastuidraw::PainterAttributeData *cap_data,
                fastuidraw::c_array<const unsigned int> cap_chunks,
                const fastuidraw::PainterAttributeData* join_data,
                fastuidraw::c_array<const unsigned int> join_chunks,
                bool with_anti_aliasing,
                const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker::DataCallBack> &call_back)
{
  using namespace fastuidraw;

  if (m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  if (join_data == nullptr)
    {
      join_chunks = c_array<const unsigned int>();
    }

  if (edge_data == nullptr)
    {
      edge_chunks = c_array<const unsigned int>();
    }

  if (cap_data == nullptr)
    {
      cap_chunks = c_array<const unsigned int>();
    }

  /* clear first to blank the values, std::vector::clear
   * does not call deallocation on its backing store,
   * thus there is no malloc/free noise
   */
  unsigned int total_chunks;

  total_chunks = cap_chunks.size() + edge_chunks.size() + join_chunks.size();
  if (total_chunks == 0)
    {
      return;
    }

  unsigned int zinc_sum(0), current(0), modify_z_coeff;
  PainterStrokeShader::type_t aa_type;
  bool modify_z;
  c_array<c_array<const PainterAttribute> > attrib_chunks;
  c_array<c_array<const PainterIndex> > index_chunks;
  c_array<int> index_adjusts;
  c_array<int> z_increments;
  c_array<int> start_zs;
  c_array<const reference_counted_ptr<PainterItemShader>* > shaders_pass1;
  c_array<const reference_counted_ptr<PainterItemShader>* > shaders_pass2;
  StrokingItems stroking_items(with_anti_aliasing);
  reference_counted_ptr<PainterBlendShader> old_blend;
  BlendMode::packed_value old_blend_mode;
  PainterData draw(pdraw);

  m_work_room.m_stroke_attrib_chunks.resize(total_chunks);
  m_work_room.m_stroke_index_chunks.resize(total_chunks);
  m_work_room.m_stroke_increment_zs.resize(total_chunks);
  m_work_room.m_stroke_index_adjusts.resize(total_chunks);
  m_work_room.m_stroke_start_zs.resize(total_chunks);
  m_work_room.m_stroke_shaders_pass1.resize(total_chunks);
  m_work_room.m_stroke_shaders_pass2.resize(total_chunks);

  attrib_chunks = make_c_array(m_work_room.m_stroke_attrib_chunks);
  index_chunks = make_c_array(m_work_room.m_stroke_index_chunks);
  z_increments = make_c_array(m_work_room.m_stroke_increment_zs);
  index_adjusts = make_c_array(m_work_room.m_stroke_index_adjusts);
  start_zs = make_c_array(m_work_room.m_stroke_start_zs);
  shaders_pass1 = make_c_array(m_work_room.m_stroke_shaders_pass1);
  shaders_pass2 = make_c_array(m_work_room.m_stroke_shaders_pass2);
  current = 0;

  if (!edge_chunks.empty())
    {
      stroking_items.add_element(edge_use_arc_shaders, shader, edge_chunks.size());
      for(unsigned int E = 0; E < edge_chunks.size(); ++E, ++current)
        {
          attrib_chunks[current] = edge_data->attribute_data_chunk(edge_chunks[E]);
          index_chunks[current] = edge_data->index_data_chunk(edge_chunks[E]);
          index_adjusts[current] = edge_data->index_adjust_chunk(edge_chunks[E]);
          z_increments[current] = edge_data->z_range(edge_chunks[E]).difference();
          start_zs[current] = edge_data->z_range(edge_chunks[E]).m_begin;
          shaders_pass1[current] = stroking_items.current_shader_pass1();
          shaders_pass2[current] = stroking_items.current_shader_pass2();
          zinc_sum += z_increments[current];
        }
    }

  if (!join_chunks.empty())
    {
      stroking_items.add_element(join_use_arc_shaders, shader, join_chunks.size());
      for(unsigned int J = 0; J < join_chunks.size(); ++J, ++current)
        {
          attrib_chunks[current] = join_data->attribute_data_chunk(join_chunks[J]);
          index_chunks[current] = join_data->index_data_chunk(join_chunks[J]);
          index_adjusts[current] = join_data->index_adjust_chunk(join_chunks[J]);
          z_increments[current] = join_data->z_range(join_chunks[J]).difference();
          start_zs[current] = join_data->z_range(join_chunks[J]).m_begin;
          shaders_pass1[current] = stroking_items.current_shader_pass1();
          shaders_pass2[current] = stroking_items.current_shader_pass2();
          zinc_sum += z_increments[current];
        }
    }

  if (!cap_chunks.empty())
    {
      stroking_items.add_element(cap_use_arc_shaders, shader, cap_chunks.size());
      for(unsigned int C = 0; C < cap_chunks.size(); ++C, ++current)
        {
          attrib_chunks[current] = cap_data->attribute_data_chunk(cap_chunks[C]);
          index_chunks[current] = cap_data->index_data_chunk(cap_chunks[C]);
          index_adjusts[current] = cap_data->index_adjust_chunk(cap_chunks[C]);
          z_increments[current] = cap_data->z_range(cap_chunks[C]).difference();
          start_zs[current] = cap_data->z_range(cap_chunks[C]).m_begin;
          shaders_pass1[current] = stroking_items.current_shader_pass1();
          shaders_pass2[current] = stroking_items.current_shader_pass2();
          zinc_sum += z_increments[current];
        }
    }

  aa_type = shader.aa_type();
  modify_z = !with_anti_aliasing || aa_type == PainterStrokeShader::draws_solid_then_fuzz;
  modify_z_coeff = (with_anti_aliasing) ? 2 : 1;

  if (modify_z || with_anti_aliasing)
    {
      /* if any of the data elements of draw are NOT packed state,
       * make them as packed state so that they are reused
       * to prevent filling up the data buffer with repeated
       * state data.
       */
      draw.make_packed(m_pool);
    }

  if (with_anti_aliasing)
    {
      if (aa_type == PainterStrokeShader::cover_then_draw)
        {
          const PainterBlendShaderSet &shader_set(m_core->default_shaders().blend_shaders());
          enum PainterEnums::blend_mode_t m(PainterEnums::blend_porter_duff_dst);

          old_blend = m_core->blend_shader();
          old_blend_mode = m_core->blend_mode();
          m_core->blend_shader(shader_set.shader(m), shader_set.blend_mode(m));
          m_core->draw_break(shader.aa_action_pass1());
        }
    }

  if (modify_z)
    {
      draw_generic_z_layered(shaders_pass1, draw, z_increments, zinc_sum,
                             attrib_chunks, index_chunks, index_adjusts,
                             start_zs,
                             m_current_z + (modify_z_coeff - 1) * zinc_sum,
                             call_back);
    }
  else
    {
      for(const StrokingItem &S : stroking_items.data())
        {
          draw_generic(*S.m_shader_pass1, draw,
                       attrib_chunks.sub_array(S.m_range),
                       index_chunks.sub_array(S.m_range),
                       index_adjusts.sub_array(S.m_range),
                       fastuidraw::c_array<const unsigned int>(),
                       m_current_z, call_back);
        }
    }

  if (with_anti_aliasing)
    {
      if (aa_type == PainterStrokeShader::cover_then_draw)
        {
          m_core->blend_shader(old_blend, old_blend_mode);
          m_core->draw_break(shader.aa_action_pass2());
        }

      if (modify_z)
        {
          draw_generic_z_layered(shaders_pass2, draw, z_increments, zinc_sum,
                                 attrib_chunks, index_chunks, index_adjusts,
                                 start_zs, m_current_z, call_back);
        }
      else
        {
          for(const StrokingItem &S : stroking_items.data())
            {
              draw_generic(*S.m_shader_pass2, draw,
                           attrib_chunks.sub_array(S.m_range),
                           index_chunks.sub_array(S.m_range),
                           index_adjusts.sub_array(S.m_range),
                           fastuidraw::c_array<const unsigned int>(),
                           m_current_z, call_back);
            }
        }
    }

  if (modify_z)
    {
      m_current_z += modify_z_coeff * zinc_sum;
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
  d = static_cast<PainterPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::PainterPackedValuePool&
fastuidraw::Painter::
packed_value_pool(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_pool;
}

void
fastuidraw::Painter::
begin(const reference_counted_ptr<PainterBackend::Surface> &surface,
      bool clear_color_buffer)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  d->m_core->begin(surface, clear_color_buffer);
  d->m_resolution = vec2(surface->viewport().m_dimensions);
  d->m_resolution.x() = std::max(1.0f, d->m_resolution.x());
  d->m_resolution.y() = std::max(1.0f, d->m_resolution.y());
  d->m_one_pixel_width = 1.0f / d->m_resolution;

  d->m_current_z = 1;
  d->m_clip_rect_state.reset();
  d->m_clip_store.set_current(d->m_clip_rect_state.clip_equations().m_clip_equations);
  blend_shader(PainterEnums::blend_porter_duff_src_over);
}

void
fastuidraw::Painter::
end(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  /* pop m_clip_stack to perform necessary writes */
  while(!d->m_occluder_stack.empty())
    {
      d->m_occluder_stack.back().on_pop(this);
      d->m_occluder_stack.pop_back();
    }
  /* clear state stack as well. */
  d->m_clip_store.clear();
  d->m_state_stack.clear();
  d->m_core->end();
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
             c_array<const c_array<const PainterAttribute> > attrib_chunks,
             c_array<const c_array<const PainterIndex> > index_chunks,
             c_array<const int> index_adjusts,
             const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  if (!d->m_clip_rect_state.m_all_content_culled)
    {
      d->draw_generic(shader, draw, attrib_chunks, index_chunks,
                      index_adjusts, c_array<const unsigned int>(),
                      current_z(), call_back);
    }
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
             c_array<const c_array<const PainterAttribute> > attrib_chunks,
             c_array<const c_array<const PainterIndex> > index_chunks,
             c_array<const int> index_adjusts,
             c_array<const unsigned int> attrib_chunk_selector,
             const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  if (!d->m_clip_rect_state.m_all_content_culled)
    {
      d->draw_generic(shader, draw, attrib_chunks, index_chunks,
                      index_adjusts, attrib_chunk_selector,
                      current_z(), call_back);
    }
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
             const PainterPacker::DataWriter &src,
             const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  if (!d->m_clip_rect_state.m_all_content_culled)
    {
      d->draw_generic(shader, draw, src, current_z(), call_back);
    }
}

void
fastuidraw::Painter::
queue_action(const reference_counted_ptr<const PainterDraw::Action> &action)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_core->draw_break(action);
}

void
fastuidraw::Painter::
draw_convex_polygon(const PainterFillShader &shader,
                    const PainterData &draw, c_array<const vec2> pts,
                    const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (pts.size() < 3 || d->m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  if (!d->m_core->hints().clipping_via_hw_clip_planes())
    {
      d->m_clip_rect_state.clip_polygon(pts, d->m_work_room.m_pts_draw_convex_polygon,
                                        d->m_work_room.m_clipper_vec2s[0]);
      pts = make_c_array(d->m_work_room.m_pts_draw_convex_polygon);
      if (pts.size() < 3)
        {
          return;
        }
    }

  d->m_work_room.m_polygon_attribs.resize(pts.size());
  for(unsigned int i = 0; i < pts.size(); ++i)
    {
      d->m_work_room.m_polygon_attribs[i].m_attrib0 = fastuidraw::pack_vec4(pts[i].x(), pts[i].y(), 0.0f, 0.0f);
      d->m_work_room.m_polygon_attribs[i].m_attrib1 = uvec4(0u, 0u, 0u, 0u);
      d->m_work_room.m_polygon_attribs[i].m_attrib2 = uvec4(0u, 0u, 0u, 0u);
    }

  d->m_work_room.m_polygon_indices.clear();
  d->m_work_room.m_polygon_indices.reserve((pts.size() - 2) * 3);
  for(unsigned int i = 2; i < pts.size(); ++i)
    {
      d->m_work_room.m_polygon_indices.push_back(0);
      d->m_work_room.m_polygon_indices.push_back(i - 1);
      d->m_work_room.m_polygon_indices.push_back(i);
    }

  draw_generic(shader.item_shader(), draw,
               make_c_array(d->m_work_room.m_polygon_attribs),
               make_c_array(d->m_work_room.m_polygon_indices),
               0,
               call_back);
}

void
fastuidraw::Painter::
draw_convex_polygon(const PainterData &draw, c_array<const vec2> pts,
                    const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  draw_convex_polygon(default_shaders().fill_shader(), draw, pts, call_back);
}

void
fastuidraw::Painter::
draw_quad(const PainterFillShader &shader,
          const PainterData &draw, const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  vecN<vec2, 4> pts;
  pts[0] = p0;
  pts[1] = p1;
  pts[2] = p2;
  pts[3] = p3;
  draw_convex_polygon(shader, draw, pts, call_back);
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
draw_rect(const PainterFillShader &shader,
          const PainterData &draw, const vec2 &p, const vec2 &wh,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  draw_quad(shader, draw, p, p + vec2(0.0f, wh.y()),
            p + wh, p + vec2(wh.x(), 0.0f), call_back);
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
            const StrokedPath &path, float thresh,
            bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
            bool with_anti_aliasing,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  FASTUIDRAWassert(0 <= cp && cp < PainterEnums::number_cap_styles);
  FASTUIDRAWassert(0 <= js && js < PainterEnums::number_join_styles);
  d->stroke_path_common(shader, draw, path, thresh,
                        close_contours, cp, js, with_anti_aliasing,
                        call_back);
}

void
fastuidraw::Painter::
stroke_path(const PainterStrokeShader &shader, const PainterData &draw, const Path &path,
            bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
            bool with_anti_aliasing,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  const StrokedPath *stroked_path;
  float thresh;

  d = static_cast<PainterPrivate*>(m_d);
  stroked_path = d->select_stroked_path(path, shader, draw, thresh);
  stroke_path(shader, draw, *stroked_path, thresh,
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
stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw,
                   const StrokedPath &path, float thresh,
                   bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                   bool with_anti_aliasing,
                   const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  FASTUIDRAWassert(0 <= cp && cp < PainterEnums::number_cap_styles);
  FASTUIDRAWassert(0 <= js && js < PainterEnums::number_join_styles);
  d->stroke_path_common(shader.shader(cp), draw,
                        path, thresh, close_contours,
                        PainterEnums::number_cap_styles, js,
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
  const StrokedPath *stroked_path;
  float thresh;

  d = static_cast<PainterPrivate*>(m_d);
  stroked_path = d->select_stroked_path(path, shader.shader(cp), draw, thresh);
  stroke_dashed_path(shader, draw, *stroked_path, thresh,
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
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const FilledPath &filled_path, enum PainterEnums::fill_rule_t fill_rule,
          bool with_anti_aliasing,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  unsigned int idx_chunk, atr_chunk, num_subsets, incr_z;

  d = static_cast<PainterPrivate*>(m_d);
  if (d->m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  idx_chunk = FilledPath::Subset::fill_chunk_from_fill_rule(fill_rule);
  atr_chunk = 0;

  d->m_work_room.m_fill_subset_selector.resize(filled_path.number_subsets());
  num_subsets = filled_path.select_subsets(d->m_work_room.m_filled_path_scratch,
                                           d->m_clip_store.current(),
                                           d->m_clip_rect_state.item_matrix(),
                                           d->m_max_attribs_per_block,
                                           d->m_max_indices_per_block,
                                           make_c_array(d->m_work_room.m_fill_subset_selector));

  if (num_subsets == 0)
    {
      return;
    }

  fastuidraw::c_array<const unsigned int> subset_list;
  subset_list = make_c_array(d->m_work_room.m_fill_subset_selector).sub_array(0, num_subsets);

  d->m_work_room.m_fill_attrib_chunks.clear();
  d->m_work_room.m_fill_index_chunks.clear();
  d->m_work_room.m_fill_index_adjusts.clear();
  for(unsigned int s : subset_list)
    {
      FilledPath::Subset subset(filled_path.subset(s));
      const PainterAttributeData &data(subset.painter_data());

      d->m_work_room.m_fill_attrib_chunks.push_back(data.attribute_data_chunk(atr_chunk));
      d->m_work_room.m_fill_index_chunks.push_back(data.index_data_chunk(idx_chunk));
      d->m_work_room.m_fill_index_adjusts.push_back(data.index_adjust_chunk(idx_chunk));
    }

  if (with_anti_aliasing)
    {
      d->m_work_room.m_fill_ws.set(filled_path, subset_list,
                                   CustomFillRuleFunction(fill_rule));
      incr_z = d->pre_draw_anti_alias_fuzz(filled_path, subset_list,
                                           d->m_work_room.m_fill_ws,
                                           d->m_work_room.m_fill_aa_fuzz_z_increments,
                                           d->m_work_room.m_fill_aa_fuzz_attrib_chunks,
                                           d->m_work_room.m_fill_aa_fuzz_index_chunks,
                                           d->m_work_room.m_fill_aa_fuzz_index_adjusts,
                                           d->m_work_room.m_fill_aa_fuzz_start_zs);
    }
  else
    {
      incr_z = 0;
    }

  d->draw_generic(shader.item_shader(), draw,
                  fastuidraw::make_c_array(d->m_work_room.m_fill_attrib_chunks),
                  fastuidraw::make_c_array(d->m_work_room.m_fill_index_chunks),
                  fastuidraw::make_c_array(d->m_work_room.m_fill_index_adjusts),
                  c_array<const unsigned int>(), //chunk selector
                  d->m_current_z + incr_z, call_back);

  if (with_anti_aliasing)
    {
      d->draw_generic_z_layered(shader.aa_fuzz_shader(), draw,
                                fastuidraw::make_c_array(d->m_work_room.m_fill_aa_fuzz_z_increments), incr_z,
                                fastuidraw::make_c_array(d->m_work_room.m_fill_aa_fuzz_attrib_chunks),
                                fastuidraw::make_c_array(d->m_work_room.m_fill_aa_fuzz_index_chunks),
                                fastuidraw::make_c_array(d->m_work_room.m_fill_aa_fuzz_index_adjusts),
                                fastuidraw::make_c_array(d->m_work_room.m_fill_aa_fuzz_start_zs),
                                d->m_current_z, call_back);
      d->m_current_z += incr_z;
    }
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const Path &path, enum PainterEnums::fill_rule_t fill_rule,
          bool with_anti_aliasing,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;

  d = static_cast<PainterPrivate*>(m_d);
  fill_path(shader, draw, d->select_filled_path(path),
            fill_rule, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
fill_path(const PainterData &draw, const Path &path, enum PainterEnums::fill_rule_t fill_rule,
          bool with_anti_aliasing,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  fill_path(default_shaders().fill_shader(), draw, path, fill_rule,
            with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const FilledPath &filled_path, const CustomFillRuleBase &fill_rule,
          bool with_anti_aliasing,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  unsigned int num_subsets;
  PainterPrivate *d;

  d = static_cast<PainterPrivate*>(m_d);
  if (d->m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  d->m_work_room.m_fill_subset_selector.resize(filled_path.number_subsets());
  num_subsets = filled_path.select_subsets(d->m_work_room.m_filled_path_scratch,
                                           d->m_clip_store.current(),
                                           d->m_clip_rect_state.item_matrix(),
                                           d->m_max_attribs_per_block,
                                           d->m_max_indices_per_block,
                                           make_c_array(d->m_work_room.m_fill_subset_selector));

  if (num_subsets == 0)
    {
      return;
    }

  fastuidraw::c_array<const unsigned int> subset_list;
  int incr_z;

  subset_list = make_c_array(d->m_work_room.m_fill_subset_selector).sub_array(0, num_subsets);
  d->m_work_room.m_fill_ws.set(filled_path, subset_list, fill_rule);

  d->m_work_room.m_fill_attrib_chunks.clear();
  d->m_work_room.m_fill_index_chunks.clear();
  d->m_work_room.m_fill_index_adjusts.clear();
  d->m_work_room.m_fill_selector.clear();

  for(unsigned int s : subset_list)
    {
      FilledPath::Subset subset(filled_path.subset(s));
      const PainterAttributeData &data(subset.painter_data());
      c_array<const fastuidraw::PainterAttribute> attrib_chunk;
      c_array<const int> winding_numbers(subset.winding_numbers());
      unsigned int attrib_selector_value;
      bool added_chunk;

      added_chunk = false;
      attrib_selector_value = d->m_work_room.m_fill_attrib_chunks.size();

      for(int winding_number : winding_numbers)
        {
          int chunk;
          c_array<const PainterIndex> index_chunk;

          chunk = FilledPath::Subset::fill_chunk_from_winding_number(winding_number);
          index_chunk = data.index_data_chunk(chunk);
          if (!index_chunk.empty() && d->m_work_room.m_fill_ws(winding_number))
            {
              d->m_work_room.m_fill_selector.push_back(attrib_selector_value);
              d->m_work_room.m_fill_index_chunks.push_back(index_chunk);
              d->m_work_room.m_fill_index_adjusts.push_back(data.index_adjust_chunk(chunk));
              added_chunk = true;
            }
        }

      if (added_chunk)
        {
          attrib_chunk = data.attribute_data_chunk(0);
          d->m_work_room.m_fill_attrib_chunks.push_back(attrib_chunk);
        }
    }

  if (d->m_work_room.m_fill_index_chunks.empty())
    {
      return;
    }

  if (with_anti_aliasing)
    {
      incr_z = d->pre_draw_anti_alias_fuzz(filled_path, subset_list,
                                           d->m_work_room.m_fill_ws,
                                           d->m_work_room.m_fill_aa_fuzz_z_increments,
                                           d->m_work_room.m_fill_aa_fuzz_attrib_chunks,
                                           d->m_work_room.m_fill_aa_fuzz_index_chunks,
                                           d->m_work_room.m_fill_aa_fuzz_index_adjusts,
                                           d->m_work_room.m_fill_aa_fuzz_start_zs);
    }
  else
    {
      incr_z = 0;
    }

  d->draw_generic(shader.item_shader(), draw,
                  make_c_array(d->m_work_room.m_fill_attrib_chunks),
                  make_c_array(d->m_work_room.m_fill_index_chunks),
                  make_c_array(d->m_work_room.m_fill_index_adjusts),
                  make_c_array(d->m_work_room.m_fill_selector),
                  d->m_current_z + incr_z, call_back);

  if (with_anti_aliasing)
    {
      d->draw_generic_z_layered(shader.aa_fuzz_shader(), draw,
                                fastuidraw::make_c_array(d->m_work_room.m_fill_aa_fuzz_z_increments), incr_z,
                                fastuidraw::make_c_array(d->m_work_room.m_fill_aa_fuzz_attrib_chunks),
                                fastuidraw::make_c_array(d->m_work_room.m_fill_aa_fuzz_index_chunks),
                                fastuidraw::make_c_array(d->m_work_room.m_fill_aa_fuzz_index_adjusts),
                                fastuidraw::make_c_array(d->m_work_room.m_fill_aa_fuzz_start_zs),
                                d->m_current_z, call_back);
      d->m_current_z += incr_z;
    }
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader,
          const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
          bool with_anti_aliasing,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;

  d = static_cast<PainterPrivate*>(m_d);
  fill_path(shader, draw, d->select_filled_path(path),
            fill_rule, with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
fill_path(const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
          bool with_anti_aliasing,
          const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  fill_path(default_shaders().fill_shader(), draw, path, fill_rule,
            with_anti_aliasing, call_back);
}

void
fastuidraw::Painter::
draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
            const PainterAttributeData &data,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  c_array<const unsigned int> chks(data.non_empty_index_data_chunks());
  for(unsigned int i = 0; i < chks.size(); ++i)
    {
      unsigned int k;

      k = chks[i];
      draw_generic(shader.shader(static_cast<enum glyph_type>(k)), draw,
                   data.attribute_data_chunk(k),
                   data.index_data_chunk(k),
                   data.index_adjust_chunk(k),
                   call_back);
    }
}

void
fastuidraw::Painter::
draw_glyphs(const PainterData &draw,
            const PainterAttributeData &data, bool use_anistopic_antialias,
            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back)
{
  if (use_anistopic_antialias)
    {
      draw_glyphs(default_shaders().glyph_shader_anisotropic(), draw, data, call_back);
    }
  else
    {
      draw_glyphs(default_shaders().glyph_shader(), draw, data, call_back);
    }
}

const fastuidraw::PainterItemMatrix&
fastuidraw::Painter::
transformation(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_clip_rect_state.current_painter_item_matrix();
}

void
fastuidraw::Painter::
transformation(const float3x3 &m)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_clip_rect_state.item_matrix(m, true);
}

const fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix>&
fastuidraw::Painter::
transformation_state(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_clip_rect_state.current_item_marix_state(d->m_pool);
}

void
fastuidraw::Painter::
transformation_state(const PainterPackedValue<PainterItemMatrix> &h)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_clip_rect_state.item_matrix_state(h, true);
}

void
fastuidraw::Painter::
concat(const float3x3 &tr)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  float3x3 m;
  bool tricky;

  tricky = (tr(0, 1) != 0.0f || tr(1, 0) != 0.0f
            || tr(2, 0) != 0.0f || tr(2, 1) != 0.0f
            || tr(2, 2) != 1.0f);

  m = d->m_clip_rect_state.item_matrix() * tr;
  d->m_clip_rect_state.item_matrix(m, tricky);

  if (!tricky)
    {
      d->m_clip_rect_state.m_clip_rect.translate(vec2(-tr(0, 2), -tr(1, 2)));
      d->m_clip_rect_state.m_clip_rect.shear(1.0f / tr(0,0), 1.0f / tr(1,1));
    }
}

void
fastuidraw::Painter::
translate(const vec2 &p)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  float3x3 m(d->m_clip_rect_state.item_matrix());
  m.translate(p.x(), p.y());
  d->m_clip_rect_state.item_matrix(m, false);
  d->m_clip_rect_state.m_clip_rect.translate(-p);
}

void
fastuidraw::Painter::
scale(float s)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  float3x3 m(d->m_clip_rect_state.item_matrix());
  m.scale(s);
  d->m_clip_rect_state.item_matrix(m, false);
  d->m_clip_rect_state.m_clip_rect.scale(1.0f / s);
}

void
fastuidraw::Painter::
shear(float sx, float sy)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  float3x3 m(d->m_clip_rect_state.item_matrix());
  m.shear(sx, sy);
  d->m_clip_rect_state.item_matrix(m, false);
  d->m_clip_rect_state.m_clip_rect.shear(1.0f / sx, 1.0f / sy);
}

void
fastuidraw::Painter::
rotate(float angle)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  float3x3 tr;
  float s, c;

  s = t_sin(angle);
  c = t_cos(angle);

  tr(0, 0) = c;
  tr(1, 0) = s;

  tr(0, 1) = -s;
  tr(1, 1) = c;

  float3x3 m(d->m_clip_rect_state.item_matrix());
  m = m * tr;
  d->m_clip_rect_state.item_matrix(m, true);
}

void
fastuidraw::Painter::
stroke_arc_path(bool b)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_stroke_arc_path = b;
}

bool
fastuidraw::Painter::
stroke_arc_path(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_stroke_arc_path;
}

void
fastuidraw::Painter::
curveFlatness(float thresh)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_curve_flatness = thresh;
}

float
fastuidraw::Painter::
curveFlatness(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_curve_flatness;
}

void
fastuidraw::Painter::
save(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  state_stack_entry st;
  st.m_occluder_stack_position = d->m_occluder_stack.size();
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
  d = static_cast<PainterPrivate*>(m_d);

  FASTUIDRAWassert(!d->m_state_stack.empty());
  const state_stack_entry &st(d->m_state_stack.back());

  d->m_clip_rect_state = st.m_clip_rect_state;
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
 *      - clipOut by path P
 *         1. add "draw" the path P filled, but with call back for
 *            the data indicating where in the attribute or data
 *            store buffer to write the new z-value
 *         2. on doing clipPop, we know the z-value to use for
 *            all the elements that are occluded by the fill
 *            path, so we write that value.
 *
 *      - clipIn by rect R
 * easy case A: No changes to tranformation matrix since last clipIn by rect
 *             1. intersect current clipping rectangle with R, set clip equations
 * easy case B: Trasnformation matrix change is "easy" (i.e maps coordiante aligned rect to coordiante aligned rects)
 *             1. map old clip rect to new coordinates, interset rect, set clip equations.
 * hard case: Transformation matrix change does not map coordiante aligned rect to coordiante aligned rects
 *             1. set clip equations
 *             2. temporarily set transformation matrix to identity
 *             3. draw 4 half-planes: for each OLD clipping equation draw that half plane
 *             4. restore transformation matrix
 *
 *      - clipIn by path P
 *          1. clipIn by R, R = bounding box of P
 *          2. clipOut by R\P.
 */

void
fastuidraw::Painter::
clipOutPath(const Path &path, enum PainterEnums::fill_rule_t fill_rule)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  reference_counted_ptr<PainterBlendShader> old_blend;
  BlendMode::packed_value old_blend_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = blend_shader();
  old_blend_mode = blend_mode();

  blend_shader(PainterEnums::blend_porter_duff_dst);
  fill_path(PainterData(d->m_black_brush), path, fill_rule, false, zdatacallback);
  blend_shader(old_blend, old_blend_mode);

  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));
}

void
fastuidraw::Painter::
clipOutPath(const Path &path, const CustomFillRuleBase &fill_rule)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  fastuidraw::reference_counted_ptr<PainterBlendShader> old_blend;
  BlendMode::packed_value old_blend_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = blend_shader();
  old_blend_mode = blend_mode();

  blend_shader(PainterEnums::blend_porter_duff_dst);
  fill_path(PainterData(d->m_black_brush), path, fill_rule, false, zdatacallback);
  blend_shader(old_blend, old_blend_mode);

  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));
}

void
fastuidraw::Painter::
clipInPath(const Path &path, enum PainterEnums::fill_rule_t fill_rule)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
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
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
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
  d = static_cast<PainterPrivate*>(m_d);

  vec2 pmax(pmin + wh);

  d->m_clip_rect_state.m_all_content_culled =
    d->m_clip_rect_state.m_all_content_culled ||
    wh.x() <= 0.0f || wh.y() <= 0.0f ||
    d->m_clip_rect_state.rect_is_culled(pmin, wh) ||
    d->update_clip_equation_series(pmin, pmax);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  if (!d->m_clip_rect_state.m_clip_rect.m_enabled)
    {
      /* no clipped rect defined yet, just take the arguments
       * as the clipping window
       */
      d->m_clip_rect_state.m_clip_rect = clip_rect(pmin, pmax);
      d->m_clip_rect_state.set_clip_equations_to_clip_rect();
      return;
    }
  else if (!d->m_clip_rect_state.item_matrix_transition_tricky())
    {
      /* a previous clipping window (defined in m_clip_rect_state),
       * but transformation takes screen aligned rectangles to
       * screen aligned rectangles, thus the current value of
       * m_clip_rect_state.m_clip_rect is the clipping rect
       * in local coordinates, so we can intersect it with
       * the passed rectangle.
       */
      d->m_clip_rect_state.m_clip_rect.intersect(clip_rect(pmin, pmax));
      d->m_clip_rect_state.set_clip_equations_to_clip_rect();
      return;
    }


  /* the transformation is tricky, thus the current value of
   * m_clip_rect_state.m_clip_rect does NOT reflect the actual
   * clipping rectangle.
   *
   * The clipping is done as follows:
   *  1. we set the clip equations to come from pmin, pmax
   *  2. we draw the -complement- of the half planes of each
   *     of the old clip equations as occluders
   */
  PainterPackedValue<PainterClipEquations> prev_clip, current_clip;

  prev_clip = d->m_clip_rect_state.clip_equations_state(d->m_pool);
  FASTUIDRAWassert(prev_clip);

  d->m_clip_rect_state.m_clip_rect = clip_rect(pmin, pmax);

  std::bitset<4> skip_occluder;
  skip_occluder = d->m_clip_rect_state.set_clip_equations_to_clip_rect(prev_clip);
  current_clip = d->m_clip_rect_state.clip_equations_state(d->m_pool);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* The clip equations coming from the new clipping
       * rectangle degenerate into an empty clipping region
       * on the screen; immediately return.
       */
      return;
    }

  /* if the new clipping rectangle is completely contained
   * in the older clipping region, then we can skip drawing
   * the complement of the old clipping rectangle as occluders
   */
  if (skip_occluder.all())
    {
      return;
    }

  /* draw the complement of the half planes. The half planes
   * are in 3D api coordinates, so set the matrix temporarily
   * to identity. Note that we pass false to item_matrix_state()
   * to prevent marking the derived values from the matrix
   * state from being marked as dirty.
   */
  PainterPackedValue<PainterItemMatrix> matrix_state;
  matrix_state = d->m_clip_rect_state.current_item_marix_state(d->m_pool);
  FASTUIDRAWassert(matrix_state);
  d->m_clip_rect_state.item_matrix_state(d->m_identiy_matrix, false);

  reference_counted_ptr<ZDataCallBack> zdatacallback;
  zdatacallback = FASTUIDRAWnew ZDataCallBack();

  fastuidraw::reference_counted_ptr<PainterBlendShader> old_blend;
  BlendMode::packed_value old_blend_mode;
  old_blend = blend_shader();
  old_blend_mode = blend_mode();
  blend_shader(PainterEnums::blend_porter_duff_dst);

  /* we temporarily set the clipping to a slightly
   * larger rectangle when drawing the occluders.
   * We do this because round off error can have us
   * miss a few pixels when drawing the occluder
   */
  PainterClipEquations slightly_bigger(current_clip.value());
  for(unsigned int i = 0; i < 4; ++i)
    {
      float f;
      vec3 &eq(slightly_bigger.m_clip_equations[i]);

      f = fastuidraw::t_abs(eq.x()) * d->m_one_pixel_width.x() + fastuidraw::t_abs(eq.y()) * d->m_one_pixel_width.y();
      eq.z() += f;
    }
  d->m_clip_rect_state.clip_equations(slightly_bigger);

  /* draw the half plane occluders
   */
  for(unsigned int i = 0; i < 4; ++i)
    {
      if (!skip_occluder[i])
        {
          draw_half_plane_complement(PainterData(d->m_black_brush), this,
                                     prev_clip.value().m_clip_equations[i], zdatacallback);
        }
    }

  d->m_clip_rect_state.clip_equations_state(current_clip);

  /* add to occluder stack.
   */
  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));

  d->m_clip_rect_state.item_matrix_state(matrix_state, false);
  blend_shader(old_blend, old_blend_mode);
}

const fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas>&
fastuidraw::Painter::
glyph_atlas(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_core->glyph_atlas();
}

const fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas>&
fastuidraw::Painter::
image_atlas(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_core->image_atlas();
}

const fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas>&
fastuidraw::Painter::
colorstop_atlas(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_core->colorstop_atlas();
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBlendShader>&
fastuidraw::Painter::
blend_shader(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_core->blend_shader();
}

fastuidraw::BlendMode::packed_value
fastuidraw::Painter::
blend_mode(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_core->blend_mode();
}

void
fastuidraw::Painter::
blend_shader(const fastuidraw::reference_counted_ptr<PainterBlendShader> &h,
             fastuidraw::BlendMode::packed_value mode)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_core->blend_shader(h, mode);
}

const fastuidraw::PainterShaderSet&
fastuidraw::Painter::
default_shaders(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_core->default_shaders();
}

unsigned int
fastuidraw::Painter::
query_stat(enum PainterPacker::stats_t st) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_core->query_stat(st);
}

int
fastuidraw::Painter::
current_z(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_current_z;
}

void
fastuidraw::Painter::
increment_z(int amount)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_current_z += amount;
}

void
fastuidraw::Painter::
register_shader(const fastuidraw::reference_counted_ptr<PainterItemShader> &shader)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(shader);
}

void
fastuidraw::Painter::
register_shader(const fastuidraw::reference_counted_ptr<PainterBlendShader> &shader)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(shader);
}

void
fastuidraw::Painter::
register_shader(const PainterStrokeShader &p)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(p);
}

void
fastuidraw::Painter::
register_shader(const PainterFillShader &p)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(p);
}

void
fastuidraw::Painter::
register_shader(const PainterDashedStrokeShaderSet &p)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(p);
}

void
fastuidraw::Painter::
register_shader(const PainterGlyphShader &p)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(p);
}

void
fastuidraw::Painter::
register_shader(const PainterShaderSet &p)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_core->register_shader(p);
}
