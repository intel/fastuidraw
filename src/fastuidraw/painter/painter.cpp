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
#include <fastuidraw/text/glyph_generate_params.hpp>
#include <fastuidraw/painter/backend/painter_header.hpp>
#include <fastuidraw/painter/painter.hpp>

#include "../private/util_private.hpp"
#include "../private/util_private_math.hpp"
#include "../private/util_private_ostream.hpp"
#include "../private/clip.hpp"
#include "../private/bounding_box.hpp"
#include "../private/rect_atlas.hpp"
#include "backend/private/painter_packer.hpp"

namespace
{
  class ZDelayedAction;
  class ZDataCallBack;
  class PainterPrivate;

  bool
  adjust_brush_for_internal_shearing(const fastuidraw::PainterData &in_value,
                                     fastuidraw::vec2 translate,
                                     fastuidraw::vec2 shear,
                                     fastuidraw::PainterBrush &tmp_brush,
                                     fastuidraw::PainterData &out_value)
  {
    using namespace fastuidraw;
    if (in_value.m_brush.data().shader() & (PainterBrush::image_mask | PainterBrush::gradient_type_mask))
      {
        out_value = PainterData(in_value);
        tmp_brush = in_value.m_brush.data();

        /* apply the same transformation to the brush
         * as we apply to the Painter.
         */
        tmp_brush.apply_translate(translate);
        tmp_brush.apply_shear(shear.x(), shear.y());

        out_value.set(&tmp_brush);
        return true;
      }
    return false;
  }

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
      for(const fastuidraw::c_array<fastuidraw::generic_data> &d : m_dests)
        {
          d[fastuidraw::PainterHeader::z_offset].i = m_z_to_write;
          d[fastuidraw::PainterHeader::flags_offset].u = fastuidraw::PainterHeader::drawing_occluder;
        }
    }

  private:
    friend class ZDataCallBack;
    int32_t m_z_to_write;
    std::vector<fastuidraw::c_array<fastuidraw::generic_data> > m_dests;
  };

  class ZDataCallBack:public fastuidraw::PainterPacker::DataCallBack
  {
  public:
    virtual
    void
    header_added(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw> &h,
                 const fastuidraw::PainterHeader &original_value,
                 fastuidraw::c_array<fastuidraw::generic_data> mapped_location)
    {
      if (h != m_cmd)
        {
          m_cmd = h;
          m_current = FASTUIDRAWnew ZDelayedAction();
          m_actions.push_back(m_current);
          m_cmd->add_action(m_current);
        }

      FASTUIDRAWunused(original_value);
      m_current->m_dests.push_back(mapped_location);
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

  inline
  enum fastuidraw::Painter::shader_anti_alias_t
  select_anti_alias_from_auto(enum fastuidraw::Painter::hq_anti_alias_support_t support)
  {
    return (support == fastuidraw::Painter::hq_anti_alias_fast) ?
      fastuidraw::Painter::shader_anti_alias_high_quality :
      fastuidraw::Painter::shader_anti_alias_simple;
  }

  inline
  enum fastuidraw::Painter::shader_anti_alias_t
  compute_shader_anti_alias(enum fastuidraw::Painter::shader_anti_alias_t v,
                            enum fastuidraw::Painter::hq_anti_alias_support_t support,
                            enum fastuidraw::Painter::shader_anti_alias_t fastest)
  {
    v = (v == fastuidraw::Painter::shader_anti_alias_fastest) ?
      fastest :
      v;

    v = (v == fastuidraw::Painter::shader_anti_alias_auto) ?
      select_anti_alias_from_auto(support):
      v;

    v = (v == fastuidraw::Painter::shader_anti_alias_high_quality
         && support == fastuidraw::Painter::hq_anti_alias_no_support) ?
      fastuidraw::Painter::shader_anti_alias_simple :
      v;

    return v;
  }

  class clip_rect
  {
  public:
    clip_rect(void):
      m_enabled(false),
      m_min(0.0f, 0.0f),
      m_max(0.0f, 0.0f)
    {}

    clip_rect(const fastuidraw::Rect &rect):
      m_enabled(true),
      m_min(rect.m_min_point),
      m_max(rect.m_max_point)
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

  /* Enumeration to describe the matrix type
   */
  enum matrix_type_t
    {
      non_scaling_matrix,
      scaling_matrix,
      shearing_matrix,
      perspective_matrix,

      unclassified_matrix,
    };

  bool
  matrix_has_perspective(const fastuidraw::float3x3 &matrix)
  {
    const float tol(1e-5);
    return fastuidraw::t_abs(matrix(2, 0)) > tol
      || fastuidraw::t_abs(matrix(2, 1)) > tol;
  }

  inline
  fastuidraw::vec2
  coords_from_normalized_coords(fastuidraw::vec2 ndc,
                                fastuidraw::vec2 dims)
  {
    ndc += fastuidraw::vec2(1.0f, 1.0f);
    ndc *= 0.5f;
    ndc *= fastuidraw::vec2(dims);
    return ndc;
  }

  inline
  fastuidraw::vec2
  coords_from_normalized_coords(fastuidraw::vec2 ndc,
                                fastuidraw::ivec2 dims)
  {
    return coords_from_normalized_coords(ndc, fastuidraw::vec2(dims));
  }

  inline
  fastuidraw::vec2
  normalized_coords_from_coords(fastuidraw::ivec2 c,
                                fastuidraw::ivec2 dims)
  {
    fastuidraw::vec2 pc(c), pdims(dims);

    pc /= pdims;
    pc *= 2.0f;
    pc -= fastuidraw::vec2(1.0f, 1.0f);
    return pc;
  }

  class DefaultGlyphRendererChooser:public fastuidraw::Painter::GlyphRendererChooser
  {
  public:
    DefaultGlyphRendererChooser(void);

    virtual
    fastuidraw::GlyphRenderer
    choose_glyph_render(float logical_pixel_size,
                        const fastuidraw::float3x3 &transformation,
                        float max_singular_value,
                        float min_singular_value) const;

    virtual
    fastuidraw::GlyphRenderer
    choose_glyph_render(float logical_pixel_size,
                        const fastuidraw::float3x3 &transformation) const;

  private:
    static
    int
    choose_pixel_size(float pixel_size);

    float m_coverage_text_cut_off;
    float m_distance_text_cut_off;
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
    enum rect_coordinate_type_t
      {
        rect_in_local_coordinates,
        rect_in_normalized_device_coordinates,
      };

    clip_rect_state(void):
      m_all_content_culled(false),
      m_item_matrix_transition_tricky(false),
      m_inverse_transpose_not_ready(false),
      m_item_matrix_singular_values_ready(false)
    {}

    void
    reset(const fastuidraw::PainterBackend::Surface::Viewport &vwp,
          fastuidraw::ivec2 surface_dims);

    void
    set_clip_equations_to_clip_rect(enum rect_coordinate_type_t c);

    std::bitset<4>
    set_clip_equations_to_clip_rect(const fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> &prev_clip,
                                    enum rect_coordinate_type_t c);

    const fastuidraw::float3x3&
    item_matrix_inverse_transpose(void);

    const fastuidraw::float3x3&
    item_matrix(void)
    {
      return m_item_matrix.m_item_matrix;
    }

    const fastuidraw::vec2&
    item_matrix_singular_values(void);

    float
    item_matrix_operator_norm(void)
    {
      return item_matrix_singular_values()[0];
    }

    enum matrix_type_t
    matrix_type(void);

    void //a negative value for singular_value_scaled indicates to recompute singular values
    item_matrix(const fastuidraw::float3x3 &v,
                bool trick_transition, enum matrix_type_t M,
		float singular_value_scaled = -1.0f);

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
    current_item_matrix_state(fastuidraw::PainterPackedValuePool &pool)
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
    rect_is_culled(const fastuidraw::Rect &rect);

    void
    set_normalized_device_translate(const fastuidraw::vec2 &v)
    {
      m_item_matrix_state = fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix>();
      m_item_matrix.m_normalized_translate = v;
    }

    void
    update_rect_to_transformation(void)
    {
      using namespace fastuidraw;
      const float3x3 &tr(m_item_matrix.m_item_matrix);

      m_item_matrix_transition_tricky = m_item_matrix_transition_tricky
        || tr(0, 1) != 0.0f || tr(1, 0) != 0.0f
        || tr(2, 0) != 0.0f || tr(2, 1) != 0.0f
        || tr(2, 2) != 1.0f;

      if (!m_item_matrix_transition_tricky)
        {
          m_clip_rect.translate(vec2(-tr(0, 2), -tr(1, 2)));
          m_clip_rect.shear(1.0f / tr(0, 0), 1.0f / tr(1, 1));
        }
    }

    clip_rect m_clip_rect;
    bool m_all_content_culled;

  private:
    enum matrix_type_t m_matrix_type;
    bool m_item_matrix_transition_tricky;
    fastuidraw::PainterItemMatrix m_item_matrix;
    fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix> m_item_matrix_state;
    fastuidraw::PainterClipEquations m_clip_equations;
    fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> m_clip_equations_state;
    bool m_inverse_transpose_not_ready;
    fastuidraw::vec2 m_viewport_dimensions;
    fastuidraw::float3x3 m_item_matrix_inverse_transpose;
    bool m_item_matrix_singular_values_ready;
    fastuidraw::vec2 m_item_matrix_singular_values;
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
    on_pop(PainterPrivate *p);

  private:
    /* action to execute on popping.
     */
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw::DelayedAction> > m_set_occluder_z;
  };

  class state_stack_entry
  {
  public:
    state_stack_entry(void):
      m_composite(nullptr),
      m_blend(nullptr)
    {}

    unsigned int m_occluder_stack_position;
    fastuidraw::PainterCompositeShader *m_composite;
    fastuidraw::PainterBlendShader *m_blend;
    fastuidraw::BlendMode m_composite_mode;
    fastuidraw::range_type<unsigned int> m_clip_equation_series;

    clip_rect_state m_clip_rect_state;
    float m_curve_flatness;
  };

  class TransparencyBuffer:
    public fastuidraw::reference_counted<TransparencyBuffer>::non_concurrent
  {
  public:
    TransparencyBuffer(const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker> &packer,
                       const fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend::Surface> &surface,
                       const fastuidraw::reference_counted_ptr<const fastuidraw::Image> &image):
      m_packer(packer),
      m_surface(surface),
      m_image(image),
      m_rect_atlas(surface->dimensions())
    {}

    /* the PainterPacker used */
    fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker> m_packer;

    /* the depth of transparency buffer */
    unsigned int m_depth;

    /* the surface used by the TransparencyBuffer */
    fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend::Surface> m_surface;

    /* the surface realized as an image */
    fastuidraw::reference_counted_ptr<const fastuidraw::Image> m_image;

    /* the atlas to track what regions are free */
    fastuidraw::detail::RectAtlas m_rect_atlas;
  };

  class TransparencyStackEntry
  {
  public:
    /* the state stack size when the TransparencyStackEntry
     * became active
     */
    unsigned int m_state_stack_size;

    /* The region of the transparency in normalized coords */
    fastuidraw::Rect m_normalized_rect;

    /* the image to blit, together with what pixels of the image to blit */
    const fastuidraw::Image *m_image;
    fastuidraw::vec2 m_brush_translate;
    fastuidraw::vec4 m_modulate_color;

    /* The translation is from the root surface of Painter::begin()
     * to the location within TransparencyBuffer::m_surface.
     */
    fastuidraw::PainterPacker *m_packer;
    fastuidraw::vec2 m_normalized_translate;

    /* identity matrix state; this is needed because PainterItemMatrix
     * also stores the normalized translation.
     */
    fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix> m_identity_matrix;
  };

  class TransparencyStackEntryFactory
  {
  public:
    TransparencyStackEntryFactory(void):
      m_current_backing_size(0, 0),
      m_current_backing_useable_size(0, 0)
    {}

    /* clears each of the m_rect_atlas within the pool;
     * if the surface_sz changes then also clears the
     * pool entirely.
     */
    void
    begin(fastuidraw::ivec2 surface_dimensions,
          const fastuidraw::PainterBackend::Surface::Viewport &vwp);

    /* creates a TransparencyStackEntry value using the
     * available pools.
     */
    TransparencyStackEntry
    fetch(unsigned int depth,
          const fastuidraw::Rect &normalized_rect,
          PainterPrivate *d);

    /* issues PainterPacker::end() in the correct order
     * on all elements that were fetched within the
     * begin/end pair.
     */
    void
    end(void);

    /* list of active offscreen buffers */
    const std::vector<const fastuidraw::PainterBackend::Surface*>&
    active_surfaces(void) const
    {
      return m_active_surfaces;
    }

  private:
    typedef std::vector<fastuidraw::reference_counted_ptr<TransparencyBuffer> > PerActiveDepth;

    fastuidraw::PainterBackend::Surface::Viewport m_viewport;
    fastuidraw::ivec2 m_current_backing_size, m_current_backing_useable_size;
    std::vector<fastuidraw::reference_counted_ptr<TransparencyBuffer> > m_unused_buffers;
    std::vector<PerActiveDepth> m_per_active_depth;
    std::vector<const fastuidraw::PainterBackend::Surface*> m_active_surfaces;
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
      m_clip.push();
      m_poly.push();

      m_bb_stack.push_back(m_current_bb);
    }

    void
    pop(void)
    {
      m_clip.pop();
      m_poly.pop();

      m_current_bb = m_bb_stack.back();
      m_bb_stack.pop_back();
    }

    void
    reset(fastuidraw::c_array<const fastuidraw::vec3> clip);

    void
    set_current(const fastuidraw::float3x3 &transform,
                const fastuidraw::float3x3 &inverse_transpose,
                fastuidraw::c_array<const fastuidraw::vec2> new_poly);

    void
    reset_current_to_rect(const fastuidraw::Rect &rect_normalized_device_coords);

    void
    clear(void)
    {
      m_clip.clear();
      m_poly.clear();

      m_bb_stack.clear();
      m_current_bb = fastuidraw::BoundingBox<float>();
    }

    fastuidraw::c_array<const fastuidraw::vec3>
    current(void) const
    {
      return fastuidraw::make_c_array(m_clip.current());
    }

    fastuidraw::c_array<const fastuidraw::vec3>
    current_poly(void) const
    {
      return fastuidraw::make_c_array(m_poly.current());
    }

    const fastuidraw::BoundingBox<float>&
    current_bb(void) const
    {
      return m_current_bb;
    }

    /* @param (input) clip_matrix_local transformation from local to clip coordinates
     * @param (input) in_out_pts[0] convex polygon to clip
     * @return what index into in_out_pts that holds polygon clipped
     */
    unsigned int
    clip_against_current(const fastuidraw::float3x3 &clip_matrix_local,
                         fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> &in_out_pts);

  private:
    class Vec3Stack
    {
    public:
      std::vector<fastuidraw::vec3>&
      current(void)
      {
        return m_current;
      }

      const std::vector<fastuidraw::vec3>&
      current(void) const
      {
        return m_current;
      }

      void
      set_current(fastuidraw::c_array<const fastuidraw::vec3> new_values)
      {
        m_current.resize(new_values.size());
        std::copy(new_values.begin(), new_values.end(), m_current.begin());
      }

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
      clear(void)
      {
        m_current.clear();
        m_store.clear();
        m_sz.clear();
      }

    private:
      std::vector<fastuidraw::vec3> m_store;
      std::vector<unsigned int> m_sz;
      std::vector<fastuidraw::vec3> m_current;
    };

    Vec3Stack m_clip, m_poly;

    fastuidraw::BoundingBox<float> m_current_bb;
    std::vector<fastuidraw::BoundingBox<float> > m_bb_stack;
  };

  class StrokingItem
  {
  public:
    StrokingItem(void)
    {}

    StrokingItem(enum fastuidraw::Painter::shader_anti_alias_t with_aa,
                 const fastuidraw::PainterStrokeShader &shader,
                 bool use_arc_shading,
                 unsigned int start, unsigned int size):
      m_range(start, start + size)
    {
      enum fastuidraw::PainterStrokeShader::shader_type_t pass1, pass2;
      enum fastuidraw::PainterStrokeShader::stroke_type_t tp;

      tp = (use_arc_shading) ?
        fastuidraw::PainterStrokeShader::arc_stroke_type:
        fastuidraw::PainterStrokeShader::linear_stroke_type;

      pass1 = (with_aa == fastuidraw::Painter::shader_anti_alias_high_quality) ?
        fastuidraw::PainterStrokeShader::hq_aa_shader_pass1:
        fastuidraw::PainterStrokeShader::aa_shader_pass1;

      pass2 = (with_aa == fastuidraw::Painter::shader_anti_alias_high_quality) ?
        fastuidraw::PainterStrokeShader::hq_aa_shader_pass2:
        fastuidraw::PainterStrokeShader::aa_shader_pass2;

      m_shader_pass1 = (with_aa == fastuidraw::Painter::shader_anti_alias_none) ?
        &shader.shader(tp, fastuidraw::PainterStrokeShader::non_aa_shader) :
        &shader.shader(tp, pass1);

      m_shader_pass2 = (with_aa == fastuidraw::Painter::shader_anti_alias_none) ?
        nullptr :
        &shader.shader(tp, pass2);
    }

    fastuidraw::range_type<unsigned int> m_range;
    const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> *m_shader_pass1;
    const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> *m_shader_pass2;
  };

  class StrokingItems
  {
  public:
    explicit
    StrokingItems(enum fastuidraw::Painter::shader_anti_alias_t with_aa):
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
    enum fastuidraw::Painter::shader_anti_alias_t m_with_aa;
    unsigned int m_count, m_last_end;
  };

  class RoundedRectTransformations
  {
  public:
    RoundedRectTransformations(const fastuidraw::PainterData &in_value,
                               const fastuidraw::RoundedRect &R)
    {
      ready_transforms(R);
      ready_painter_data(in_value);
    }

    explicit
    RoundedRectTransformations(const fastuidraw::RoundedRect &R):
      m_use(nullptr, nullptr, nullptr, nullptr)
    {
      ready_transforms(R);
    }

    void
    ready_transforms(const fastuidraw::RoundedRect &R)
    {
      using namespace fastuidraw;

      /* min-x/max-y */
      m_translates[0] = vec2(R.m_min_point.x(), R.m_max_point.y() - R.m_corner_radii[Rect::minx_maxy_corner].y());
      m_shears[0] = R.m_corner_radii[Rect::minx_maxy_corner];

      /* max-x/max-y */
      m_translates[1] = vec2(R.m_max_point.x(), R.m_max_point.y() - R.m_corner_radii[Rect::maxx_maxy_corner].y());
      m_shears[1] = vec2(-1.0f, 1.0f) * R.m_corner_radii[Rect::maxx_maxy_corner];

      /* min-x/min-y */
      m_translates[2] = vec2(R.m_min_point.x(), R.m_min_point.y() + R.m_corner_radii[Rect::minx_miny_corner].y());
      m_shears[2] = vec2(1.0f, -1.0f) * R.m_corner_radii[RoundedRect::minx_miny_corner];

      /* max-x/min-y */
      m_translates[3] = vec2(R.m_max_point.x(), R.m_min_point.y() + R.m_corner_radii[Rect::maxx_miny_corner].y());
      m_shears[3] = -R.m_corner_radii[Rect::maxx_miny_corner];
    }

    void
    ready_painter_data(const fastuidraw::PainterData &in_value)
    {
      for (int i = 0; i < 4; ++i)
        {
          if (adjust_brush_for_internal_shearing(in_value,
                                                 m_translates[i], m_shears[i],
                                                 m_tmp_brushes[i], m_tmp_datas[i]))
            {
              m_use[i] = &m_tmp_datas[i];
            }
          else
            {
              m_use[i] = &in_value;
            }
        }
    }

    fastuidraw::vecN<fastuidraw::vec2, 4> m_translates;
    fastuidraw::vecN<fastuidraw::vec2, 4> m_shears;

    /* needed pain for the brushes */
    fastuidraw::vecN<fastuidraw::PainterBrush, 4> m_tmp_brushes;
    fastuidraw::vecN<fastuidraw::PainterData, 4> m_tmp_datas;
    fastuidraw::vecN<const fastuidraw::PainterData*, 4> m_use;
  };

  fastuidraw::PainterAttribute
  anti_alias_edge_attribute(const fastuidraw::vec2 &position, uint32_t type,
                            const fastuidraw::vec2 &normal, int z)
  {
    fastuidraw::PainterAttribute A;
    FASTUIDRAWassert(z >= 0);

    A.m_attrib0.x() = fastuidraw::pack_float(position.x());
    A.m_attrib0.y() = fastuidraw::pack_float(position.y());
    A.m_attrib0.z() = type;
    A.m_attrib0.w() = z;

    A.m_attrib1.x() = fastuidraw::pack_float(normal.x());
    A.m_attrib1.y() = fastuidraw::pack_float(normal.y());

    return A;
  }

  void
  pack_anti_alias_edge(const fastuidraw::vec2 &start, const fastuidraw::vec2 &end,
                       int z,
                       std::vector<fastuidraw::PainterAttribute> &dst_attribs,
                       std::vector<fastuidraw::PainterIndex> &dst_idx)
  {
    using namespace fastuidraw;

    const unsigned int tris[12] =
      {
        0, 3, 4,
        0, 4, 1,
        1, 4, 5,
        1, 5, 2
      };

    const double sgn[6] =
      {
        -1.0,
        0.0,
        +1.0,
        -1.0,
        0.0,
        +1.0
      };

    const uint32_t types[6] =
      {
        FilledPath::Subset::aa_fuzz_type_on_boundary,
        FilledPath::Subset::aa_fuzz_type_on_path,
        FilledPath::Subset::aa_fuzz_type_on_boundary,
        FilledPath::Subset::aa_fuzz_type_on_boundary,
        FilledPath::Subset::aa_fuzz_type_on_path,
        FilledPath::Subset::aa_fuzz_type_on_boundary,
      };

    for (unsigned int k = 0; k < 12; ++k)
      {
        dst_idx.push_back(tris[k] + dst_attribs.size());
      }

    vec2 tangent, normal;

    tangent = end - start;
    tangent /= tangent.magnitude();
    normal = vec2(-tangent.y(), tangent.x());
    for (unsigned int k = 0; k < 6; ++k)
      {
        const vec2 *p;
        PainterAttribute A;

        p = (k < 3) ? &start : &end;
        A = anti_alias_edge_attribute(*p, types[k], sgn[k] * normal, z);
        dst_attribs.push_back(A);
      }
  }

  class ClipperWorkRoom:fastuidraw::noncopyable
  {
  public:
    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_pts_update_series;
    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_vec2s;
  };

  class PolygonWorkRoom:fastuidraw::noncopyable
  {
  public:
    std::vector<fastuidraw::vec2> m_pts;
    std::vector<fastuidraw::PainterIndex> m_indices;
    std::vector<fastuidraw::PainterAttribute> m_attribs;
    std::vector<fastuidraw::PainterIndex> m_aa_fuzz_indices;
    std::vector<fastuidraw::PainterAttribute> m_aa_fuzz_attribs;
    int m_fuzz_increment_z;
  };

  class StrokingWorkRoom:fastuidraw::noncopyable
  {
  public:
    std::vector<fastuidraw::c_array<const fastuidraw::PainterAttribute> > m_attrib_chunks;
    std::vector<fastuidraw::c_array<const fastuidraw::PainterIndex> > m_index_chunks;
    std::vector<int> m_increment_zs;
    std::vector<int> m_start_zs;
    std::vector<int> m_index_adjusts;
    std::vector<const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>* > m_shaders_pass1;
    std::vector<const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>* > m_shaders_pass2;
    std::vector<unsigned int> m_subsets;
    fastuidraw::StrokedPath::ScratchSpace m_path_scratch;
    fastuidraw::StrokedCapsJoins::ChunkSet m_caps_joins_chunk_set;
    fastuidraw::StrokedCapsJoins::ScratchSpace m_caps_joins_scratch;
  };

  class AntiAliasFillWorkRoom:fastuidraw::noncopyable
  {
  public:
    std::vector<fastuidraw::c_array<const fastuidraw::PainterAttribute> > m_attrib_chunks;
    std::vector<fastuidraw::c_array<const fastuidraw::PainterIndex> > m_index_chunks;
    std::vector<int> m_index_adjusts;
    std::vector<int> m_start_zs;
    std::vector<int> m_z_increments;
    int m_total_increment_z;
  };

  class OpaqueFillWorkRoom:fastuidraw::noncopyable
  {
  public:
    std::vector<fastuidraw::c_array<const fastuidraw::PainterAttribute> > m_attrib_chunks;
    std::vector<fastuidraw::c_array<const fastuidraw::PainterIndex> > m_index_chunks;
    std::vector<int> m_index_adjusts;
    std::vector<unsigned int> m_chunk_selector;
  };

  class FillSubsetWorkRoom:fastuidraw::noncopyable
  {
  public:
    WindingSet m_ws;
    fastuidraw::FilledPath::ScratchSpace m_scratch;
    std::vector<unsigned int> m_subsets;
  };

  class GlyphSequenceWorkRoom:fastuidraw::noncopyable
  {
  public:
    fastuidraw::GlyphSequence::ScratchSpace m_scratch;
    std::vector<unsigned int> m_subsets;
    std::vector<fastuidraw::c_array<const fastuidraw::PainterAttribute> > m_attribs;
    std::vector<fastuidraw::c_array<const fastuidraw::PainterIndex> > m_indices;
  };

  class RoundedRectWorkRoom:fastuidraw::noncopyable
  {
  public:
    fastuidraw::vecN<OpaqueFillWorkRoom, 4> m_opaque_fill;
    fastuidraw::vecN<AntiAliasFillWorkRoom, 4> m_aa_fuzz;
    fastuidraw::vecN<FillSubsetWorkRoom, 4> m_subsets;
    fastuidraw::vecN<const fastuidraw::FilledPath*, 4> m_filled_paths;
    std::vector<fastuidraw::PainterAttribute> m_rect_fuzz_attributes;
    std::vector<fastuidraw::PainterIndex> m_rect_fuzz_indices;
  };

  class GenericLayeredWorkRoom:fastuidraw::noncopyable
  {
  public:
    std::vector<int> m_z_increments;
    std::vector<int> m_start_zs;
    std::vector<int> m_empty_adjusts;
  };

  class PainterWorkRoom:fastuidraw::noncopyable
  {
  public:
    ClipperWorkRoom m_clipper;
    PolygonWorkRoom m_polygon;
    StrokingWorkRoom m_stroke;
    FillSubsetWorkRoom m_fill_subset;
    OpaqueFillWorkRoom m_fill_opaque;
    AntiAliasFillWorkRoom m_fill_aa_fuzz;
    GlyphSequenceWorkRoom m_glyph;
    RoundedRectWorkRoom m_rounded_rect;
    GenericLayeredWorkRoom m_generic_layered;
  };

  class PainterPrivate
  {
  public:
    typedef fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> ItemShaderRef;
    typedef const ItemShaderRef ConstItemShaderRef;
    typedef ConstItemShaderRef *ConstItemShaderRefPtr;

    explicit
    PainterPrivate(fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend> backend);

    void
    concat(const fastuidraw::float3x3 &tr);

    void
    translate(const fastuidraw::vec2 &p);

    void
    scale(float s);

    void
    shear(float sx, float sy);

    void
    rotate(float angle);

    void
    draw_generic(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                 const fastuidraw::PainterData &draw,
                 fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
                 fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                 fastuidraw::c_array<const int> index_adjusts,
                 fastuidraw::c_array<const unsigned int> attrib_chunk_selector,
                 int z);

    void
    draw_generic(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                 const fastuidraw::PainterData &draw,
                 fastuidraw::c_array<const fastuidraw::PainterAttribute> attrib_chunks,
                 fastuidraw::c_array<const fastuidraw::PainterIndex> index_chunks,
                 int index_adjust,
                 int z)
    {
      using namespace fastuidraw;
      c_array<const c_array<const PainterAttribute> > aa(&attrib_chunks, 1);
      c_array<const c_array<const PainterIndex> > ii(&index_chunks, 1);
      c_array<const int> ia(&index_adjust, 1);
      draw_generic(shader, draw, aa, ii, ia, c_array<const unsigned int>(), z);
    }

    void
    draw_generic(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                 const fastuidraw::PainterData &draw,
                 const fastuidraw::PainterAttributeWriter &src,
                 int z);

    void
    draw_generic_z_layered(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                           const fastuidraw::PainterData &draw,
                           fastuidraw::c_array<const int> z_increments, int zinc_sum,
                           fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
                           fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                           fastuidraw::c_array<const int> index_adjusts,
                           fastuidraw::c_array<const int> start_zs, int startz);

    void
    draw_generic_z_layered(fastuidraw::c_array<const ConstItemShaderRefPtr> shaders,
                           const fastuidraw::PainterData &draw,
                           fastuidraw::c_array<const int> z_increments, int zinc_sum,
                           fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
                           fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                           fastuidraw::c_array<const int> index_adjusts,
                           fastuidraw::c_array<const int> start_zs, int startz);

    void
    stroke_path_raw(const fastuidraw::PainterStrokeShader &edge_shader,
                    bool edge_use_arc_shaders,
                    bool join_use_arc_shaders,
                    bool caps_use_arc_shaders,
                    const fastuidraw::PainterData &pdraw,
                    const fastuidraw::StrokedPath *stroked_path,
                    fastuidraw::c_array<const unsigned int> stroked_subset_ids,
                    const fastuidraw::PainterAttributeData *cap_data,
                    fastuidraw::c_array<const unsigned int> cap_chunks,
                    const fastuidraw::PainterAttributeData* join_data,
                    fastuidraw::c_array<const unsigned int> join_chunks,
                    enum fastuidraw::Painter::shader_anti_alias_t anti_aliasing);

    void
    stroke_path_common(const fastuidraw::PainterStrokeShader &shader,
                       const fastuidraw::PainterData &draw,
                       const fastuidraw::StrokedPath &path, float thresh,
                       enum fastuidraw::Painter::cap_style cp,
                       enum fastuidraw::Painter::join_style js,
                       enum fastuidraw::Painter::shader_anti_alias_t anti_alias);

    void
    stroke_path_common(const fastuidraw::PainterStrokeShader &shader,
                       const fastuidraw::PainterData &draw,
                       fastuidraw::c_array<const fastuidraw::vec2> pts,
                       enum fastuidraw::Painter::cap_style cp,
                       enum fastuidraw::Painter::join_style js,
                       enum fastuidraw::Painter::shader_anti_alias_t anti_alias);

    void
    pre_draw_anti_alias_fuzz(const fastuidraw::FilledPath &filled_path,
                             enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality,
                             const FillSubsetWorkRoom &fill_subset,
                             AntiAliasFillWorkRoom *output);

    void
    draw_anti_alias_fuzz(const fastuidraw::PainterFillShader &shader,
                         const fastuidraw::PainterData &draw,
                         enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality,
                         const AntiAliasFillWorkRoom &data,
                         int z);

    void
    draw_anti_alias_fuzz(const fastuidraw::PainterFillShader &shader,
                         const fastuidraw::PainterData &draw,
                         enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality,
                         const AntiAliasFillWorkRoom &data)
    {
      draw_anti_alias_fuzz(shader, draw, anti_alias_quality,
                           data, m_current_z);
    }

    template<typename T>
    void
    fill_path(const fastuidraw::PainterFillShader &shader,
              const fastuidraw::PainterData &draw,
              const fastuidraw::FilledPath &filled_path,
              const T &fill_rule,
              enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality);

    void
    fill_path_compute_opaque_chunks(const fastuidraw::FilledPath &filled_path,
                                    enum fastuidraw::Painter::fill_rule_t fill_rule,
                                    FillSubsetWorkRoom &workroom,
                                    OpaqueFillWorkRoom *output);

    void
    fill_path_compute_opaque_chunks(const fastuidraw::FilledPath &filled_path,
                                    const fastuidraw::CustomFillRuleBase &fill_rule,
                                    FillSubsetWorkRoom &workroom,
                                    OpaqueFillWorkRoom *output);

    void
    fill_rounded_rect(const fastuidraw::PainterFillShader &shader,
                      const fastuidraw::PainterData &draw,
                      const fastuidraw::RoundedRect &R,
                      enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality);

    void
    ready_non_aa_polygon_attribs(fastuidraw::c_array<const fastuidraw::vec2> pts);

    void
    ready_aa_polygon_attribs(fastuidraw::c_array<const fastuidraw::vec2> pts,
                             enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality);

    int
    fill_convex_polygon(bool allow_sw_clipping,
                        const fastuidraw::PainterFillShader &shader,
                        const fastuidraw::PainterData &draw,
                        fastuidraw::c_array<const fastuidraw::vec2> pts,
                        enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality,
                        int z);

    int
    fill_convex_polygon(const fastuidraw::PainterFillShader &shader,
                        const fastuidraw::PainterData &draw,
                        fastuidraw::c_array<const fastuidraw::vec2> pts,
                        enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality,
                        int z)
    {
      return fill_convex_polygon(true, shader, draw, pts, anti_alias_quality, z);
    }

    int
    fill_convex_polygon(bool allow_sw_clipping,
                        const fastuidraw::PainterFillShader &shader,
                        const fastuidraw::PainterData &draw,
                        fastuidraw::c_array<const fastuidraw::vec2> pts,
                        enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality)
    {
      return fill_convex_polygon(allow_sw_clipping, shader, draw, pts, anti_alias_quality, m_current_z);
    }

    int
    fill_convex_polygon(const fastuidraw::PainterFillShader &shader,
                        const fastuidraw::PainterData &draw,
                        fastuidraw::c_array<const fastuidraw::vec2> pts,
                        enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality)
    {
      return fill_convex_polygon(true, shader, draw, pts, anti_alias_quality, m_current_z);
    }

    int
    fill_rect(const fastuidraw::PainterFillShader &shader,
              const fastuidraw::PainterData &draw,
              const fastuidraw::Rect &rect,
              enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality,
              int z)
    {
      using namespace fastuidraw;

      vecN<vec2, 4> pts;
      pts[0] = vec2(rect.m_min_point.x(), rect.m_min_point.y());
      pts[1] = vec2(rect.m_min_point.x(), rect.m_max_point.y());
      pts[2] = vec2(rect.m_max_point.x(), rect.m_max_point.y());
      pts[3] = vec2(rect.m_max_point.x(), rect.m_min_point.y());
      return fill_convex_polygon(shader, draw, pts, anti_alias_quality, z);
    }

    void
    draw_half_plane_complement(const fastuidraw::PainterFillShader &shader,
                               const fastuidraw::PainterData &draw,
                               const fastuidraw::vec3 &plane);

    bool
    update_clip_equation_series(const fastuidraw::Rect &rect);

    float
    compute_magnification(const fastuidraw::Rect &rect);

    float
    compute_max_magnification_at_points(fastuidraw::c_array<const fastuidraw::vec2> poly);

    float
    compute_magnification_at_point(fastuidraw::vec2 p)
    {
      fastuidraw::c_array<const fastuidraw::vec2> poly(&p, 1);
      return compute_max_magnification_at_points(poly);
    }

    float
    compute_magnification(const fastuidraw::Path &path);

    float
    compute_path_thresh(const fastuidraw::Path &path);

    float
    compute_path_thresh(const fastuidraw::Path &path,
                        const fastuidraw::PainterShaderData::DataBase *shader_data,
                        const fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase> &selector,
                        float &thresh);

    unsigned int
    select_subsets(const fastuidraw::FilledPath &path,
                   fastuidraw::c_array<unsigned int> dst);

    unsigned int
    select_subsets(const fastuidraw::StrokedPath &path,
                   float pixels_additional_room,
                   float item_space_additional_room,
                   fastuidraw::c_array<unsigned int> dst);

    void
    select_chunks(const fastuidraw::StrokedCapsJoins &caps_joins,
                  float pixels_additional_room,
                  float item_space_additional_room,
                  bool select_joins_for_miter_style,
                  fastuidraw::StrokedCapsJoins::ChunkSet *dst);

    const fastuidraw::StrokedPath*
    select_stroked_path(const fastuidraw::Path &path,
                        const fastuidraw::PainterStrokeShader &shader,
                        const fastuidraw::PainterData &draw,
                        enum fastuidraw::Painter::shader_anti_alias_t aa_mode,
                        enum fastuidraw::Painter::stroking_method_t stroking_method,
                        float &out_thresh);

    const fastuidraw::FilledPath&
    select_filled_path(const fastuidraw::Path &path);

    fastuidraw::PainterPacker*
    packer(void)
    {
      return m_transparency_stack.empty() ?
        m_root_packer.get() :
        m_transparency_stack.back().m_packer;
    }

    const fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix>&
    identity_matrix(void)
    {
      return m_transparency_stack.empty() ?
        m_root_identity_matrix :
        m_transparency_stack.back().m_identity_matrix;
    }

    fastuidraw::GlyphRenderer
    compute_glyph_renderer(float pixel_size,
                           const fastuidraw::Painter::GlyphRendererChooser &chooser);

    DefaultGlyphRendererChooser m_default_glyph_renderer_chooser;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker> m_root_packer;
    fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix> m_root_identity_matrix;
    fastuidraw::vecN<unsigned int, fastuidraw::PainterPacker::num_stats> m_stats;
    fastuidraw::PainterBackend::Surface::Viewport m_viewport;
    fastuidraw::vec2 m_viewport_dimensions;
    fastuidraw::vec2 m_one_pixel_width;
    float m_curve_flatness;
    int m_current_z, m_draw_data_added_count;
    clip_rect_state m_clip_rect_state;
    std::vector<occluder_stack_entry> m_occluder_stack;
    std::vector<state_stack_entry> m_state_stack;
    TransparencyStackEntryFactory m_transparency_stack_entry_factory;
    std::vector<TransparencyStackEntry> m_transparency_stack;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend> m_backend;
    fastuidraw::PainterShaderSet m_default_shaders;
    fastuidraw::PainterPackedValuePool m_pool;
    fastuidraw::PainterPackedValue<fastuidraw::PainterBrush> m_reset_brush, m_black_brush;
    ClipEquationStore m_clip_store;
    PainterWorkRoom m_work_room;
    unsigned int m_max_attribs_per_block, m_max_indices_per_block;
    float m_coverage_text_cut_off, m_distance_text_cut_off;
    fastuidraw::Path m_rounded_corner_path;
    fastuidraw::Path m_rounded_corner_path_complement;
    fastuidraw::Path m_square_path;
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

  if (sx < 0.0f)
    {
      std::swap(m_min.x(), m_max.x());
    }

  if (sy < 0.0f)
    {
      std::swap(m_min.y(), m_max.y());
    }
}

void
clip_rect::
scale(float s)
{
  m_min *= s;
  m_max *= s;
  if (s < 0.0f)
    {
      std::swap(m_min, m_max);
    }
}

////////////////////////////////////////
// DefaultGlyphRendererChooser methods
DefaultGlyphRendererChooser::
DefaultGlyphRendererChooser(void)
{
  /* Somewhat guessing:
   *  - using distance field up to a factor 3/8 zoom out still looks ok
   *  - using distance field up to a factor 2.0 zoom in still looks ok
   */
  float distance_field_size;
  distance_field_size = fastuidraw::GlyphGenerateParams::distance_field_pixel_size();
  m_coverage_text_cut_off = (distance_field_size * 3.0f) / 8.0f;
  m_distance_text_cut_off = 2.0f * distance_field_size;
}


fastuidraw::GlyphRenderer
DefaultGlyphRendererChooser::
choose_glyph_render(float logical_pixel_size,
                    const fastuidraw::float3x3 &transformation,
                    float max_singular_value,
                    float min_singular_value) const
{
  float effective_pixel_width;

  FASTUIDRAWunused(transformation);
  FASTUIDRAWunused(min_singular_value);
  effective_pixel_width = logical_pixel_size * max_singular_value;
  if (effective_pixel_width <= m_coverage_text_cut_off)
    {
      int pixel_size;

      pixel_size = choose_pixel_size(effective_pixel_width);
      return fastuidraw::GlyphRenderer(pixel_size);
    }
  else if (effective_pixel_width <= m_distance_text_cut_off)
    {
      return fastuidraw::GlyphRenderer(fastuidraw::distance_field_glyph);
    }
  else
    {
      return fastuidraw::GlyphRenderer(fastuidraw::restricted_rays_glyph);
    }
}

fastuidraw::GlyphRenderer
DefaultGlyphRendererChooser::
choose_glyph_render(float logical_pixel_size,
                    const fastuidraw::float3x3 &transformation) const
{
  FASTUIDRAWunused(logical_pixel_size);
  FASTUIDRAWunused(transformation);
  return fastuidraw::GlyphRenderer(fastuidraw::restricted_rays_glyph);
}

int
DefaultGlyphRendererChooser::
choose_pixel_size(float pixel_size)
{
  using namespace fastuidraw;

  uint32_t return_value;

  /* We enforce a minimal glyph size of 4.0 */
  return_value = static_cast<uint32_t>(t_max(4.0f, pixel_size));

  /* larger glyph sizes can be stretched more */
  if (return_value & 1u && return_value >= 16u)
    {
      --return_value;
    }

  return return_value;
}

////////////////////////////////////////
// occluder_stack_entry methods
void
occluder_stack_entry::
on_pop(PainterPrivate *p)
{
  /* depth test is GL_GEQUAL, so we need to increment the Z
   * before hand so that the occluders block all that
   * is drawn below them.
   */
  p->m_current_z += 1;
  for(const auto &s : m_set_occluder_z)
    {
      ZDelayedAction *ptr;
      FASTUIDRAWassert(dynamic_cast<ZDelayedAction*>(s.get()) != nullptr);
      ptr = static_cast<ZDelayedAction*>(s.get());
      ptr->finalize_z(p->m_current_z);
    }
}

///////////////////////////////////////////////
// clip_rect_stat methods
void
clip_rect_state::
reset(const fastuidraw::PainterBackend::Surface::Viewport &vwp,
      fastuidraw::ivec2 surface_dims)
{
  using namespace fastuidraw;

  PainterClipEquations clip_eq;

  m_all_content_culled = false;
  m_item_matrix_transition_tricky = false;
  m_inverse_transpose_not_ready = false;
  m_matrix_type = non_scaling_matrix;
  m_viewport_dimensions = vec2(vwp.m_dimensions);

  vwp.compute_clip_equations(surface_dims, &clip_eq.m_clip_equations);
  m_clip_rect.m_enabled = false;
  clip_equations(clip_eq);

  item_matrix(float3x3(), false, non_scaling_matrix);
}

const fastuidraw::vec2&
clip_rect_state::
item_matrix_singular_values(void)
{
  if (!m_item_matrix_singular_values_ready)
    {
      fastuidraw::float2x2 M;

      M(0, 0) = 0.5f * m_viewport_dimensions.x() * m_item_matrix.m_item_matrix(0, 0);
      M(0, 1) = 0.5f * m_viewport_dimensions.x() * m_item_matrix.m_item_matrix(0, 1);
      M(1, 0) = 0.5f * m_viewport_dimensions.y() * m_item_matrix.m_item_matrix(1, 0);
      M(1, 1) = 0.5f * m_viewport_dimensions.y() * m_item_matrix.m_item_matrix(1, 1);
      m_item_matrix_singular_values = fastuidraw::detail::compute_singular_values(M);
      m_item_matrix_singular_values_ready = true;
    }
  return m_item_matrix_singular_values;
}

void
clip_rect_state::
item_matrix(const fastuidraw::float3x3 &v,
	    bool trick_transition, enum matrix_type_t M,
	    float singular_value_scaled)
{
  m_item_matrix_transition_tricky = m_item_matrix_transition_tricky || trick_transition;
  m_inverse_transpose_not_ready = true;
  m_item_matrix.m_item_matrix = v;
  m_item_matrix_state = fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix>();
  if (singular_value_scaled < 0.0f)
    {
      m_item_matrix_singular_values_ready = false;
    }
  else if (m_item_matrix_singular_values_ready)
    {
      m_item_matrix_singular_values *= singular_value_scaled;
    }

  if (M > m_matrix_type)
    {
      /* TODO: make this tighter.
       *  It is possible that concatenation of matrices
       *  can reduce the matrix type value (for example
       *  first scaling by 4 then by 0.25). The most
       *  severe example is the concatenation of two
       *  perpsective matrices yielding a matrix
       *  without perspective. However, recomputing the
       *  matrix type at every change seems excessive.
       */
      m_matrix_type = M;
    }
}

enum matrix_type_t
clip_rect_state::
matrix_type(void)
{
  if (m_matrix_type == unclassified_matrix)
    {
      if (matrix_has_perspective(m_item_matrix.m_item_matrix))
	{
	  m_matrix_type = perspective_matrix;
	}
      else
	{
	  const fastuidraw::vec2 &svd(item_matrix_singular_values());
	  float diff, sc;
	  const float tol(1e-5), one_plus_tol(1.0f + tol), one_minus_tol(1.0f - tol);

	  diff = fastuidraw::t_abs(svd[0] - svd[1]);
	  sc = fastuidraw::t_abs(svd[0] * m_item_matrix.m_item_matrix(2, 2));
	  if (diff > tol)
	    {
	      m_matrix_type = shearing_matrix;
	    }
	  else if (sc > one_plus_tol || sc < one_minus_tol)
	    {
	      m_matrix_type = scaling_matrix;
	    }
	  else
	    {
	      m_matrix_type = non_scaling_matrix;
	    }
	}
    }
  return m_matrix_type;
}

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
set_clip_equations_to_clip_rect(enum rect_coordinate_type_t c)
{
  fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> null;
  set_clip_equations_to_clip_rect(null, c);
}

std::bitset<4>
clip_rect_state::
set_clip_equations_to_clip_rect(const fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations> &pcl,
                                enum rect_coordinate_type_t c)
{
  if (m_clip_rect.empty())
    {
      m_all_content_culled = true;
      return std::bitset<4>();
    }

  m_item_matrix_transition_tricky = false;

  fastuidraw::PainterClipEquations cl;
  cl.m_clip_equations[0] = fastuidraw::vec3( 1.0f,  0.0f, -m_clip_rect.m_min.x());
  cl.m_clip_equations[1] = fastuidraw::vec3(-1.0f,  0.0f,  m_clip_rect.m_max.x());
  cl.m_clip_equations[2] = fastuidraw::vec3( 0.0f,  1.0f, -m_clip_rect.m_min.y());
  cl.m_clip_equations[3] = fastuidraw::vec3( 0.0f, -1.0f,  m_clip_rect.m_max.y());

  if (c == rect_in_local_coordinates)
    {
      /* The clipping window is given by:
       *   w * min_x <= x <= w * max_x
       *   w * min_y <= y <= w * max_y
       * which expands to
       *     x + w * min_x >= 0  --> ( 1,  0, -min_x)
       *    -x - w * max_x >= 0  --> (-1,  0, max_x)
       *     y + w * min_y >= 0  --> ( 0,  1, -min_y)
       *    -y - w * max_y >= 0  --> ( 0, -1, max_y)
       * However, the clip equations are in clip coordinates
       * so we need to apply the inverse transpose of the
       * transformation matrix to the 4 vectors
       */
      const fastuidraw::float3x3 &inverse_transpose(item_matrix_inverse_transpose());
      for (int i = 0; i < 4; ++i)
        {
          cl.m_clip_equations[i] =  inverse_transpose * cl.m_clip_equations[i];
        }
    }
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
rect_is_culled(const fastuidraw::Rect &rect)
{
  /* apply the current transformation matrix to
   * the corners of the clipping rectangle and check
   * if there is a clipping plane for which all
   * those points are on the wrong size.
   */
  const fastuidraw::vec2 &pmin(rect.m_min_point);
  const fastuidraw::vec2 &pmax(rect.m_max_point);

  if (pmin.x() >= pmax.x() || pmin.y() >= pmax.y())
    {
      return true;
    }

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
void
ClipEquationStore::
reset(fastuidraw::c_array<const fastuidraw::vec3> clip)
{
  clear();

  m_poly.current().push_back(fastuidraw::vec3(-1.0f, -1.0f, +1.0f));
  m_poly.current().push_back(fastuidraw::vec3(+1.0f, -1.0f, +1.0f));
  m_poly.current().push_back(fastuidraw::vec3(+1.0f, +1.0f, +1.0f));
  m_poly.current().push_back(fastuidraw::vec3(-1.0f, +1.0f, +1.0f));

  m_clip.set_current(clip);

  m_current_bb.union_point(fastuidraw::vec2(-1.0f, -1.0f));
  m_current_bb.union_point(fastuidraw::vec2(+1.0f, +1.0f));
}

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

void
ClipEquationStore::
reset_current_to_rect(const fastuidraw::Rect &R)
{
  using namespace fastuidraw;
  vecN<vec2, 4> pts;
  float3x3 id;

  /* the Rect R is in normalized device coordinates, so
   * the transformation to apply is just the identity
   */
  pts[0] = vec2(R.m_min_point.x(), R.m_min_point.y());
  pts[1] = vec2(R.m_min_point.x(), R.m_max_point.y());
  pts[2] = vec2(R.m_max_point.x(), R.m_max_point.y());
  pts[3] = vec2(R.m_max_point.x(), R.m_min_point.y());
  set_current(id, id, pts);
}

void
ClipEquationStore::
set_current(const fastuidraw::float3x3 &transform,
            const fastuidraw::float3x3 &inverse_transpose,
            fastuidraw::c_array<const fastuidraw::vec2> poly)
{
  using namespace fastuidraw;

  m_poly.current().clear();
  m_clip.current().clear();
  m_current_bb = BoundingBox<float>();

  if (poly.empty())
    {
      return;
    }

  /* compute center of polygon so that we can correctly
   * orient the normal vectors of the sides.
   */
  fastuidraw::vec2 center(0.0f, 0.0f);
  for(const auto &pt : poly)
    {
      center += pt;
    }
  center /= static_cast<float>(poly.size());

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
      n = vec2(v.y(), -v.x());
      if (dot(center - poly[i], n) < 0.0f)
        {
          n = -n;
        }

      /* The clip equation we have in local coordinates
       * is dot(n, p - poly[i]) >= 0. Algebra time:
       *   dot(n, p - poly[i]) = n.x * p.x + n.y * p.y + (-poly[i].x * n.x - poly[i].y * n.y)
       *                       = dot( (n, R), (p, 1))
       * where
       *   R = -poly[i].x * n.x - poly[i].y * n.y = -dot(n, poly[i])
       * We want the clip equations in clip coordinates though:
       *   dot( (n, R), (p, 1) ) = dot( (n, R), inverseM(M(p, 1)) )
       *                         = dot( inverse_transpose_M(R, 1), M(p, 1))
       * thus the vector to use is inverse_transpose_M(R, 1)
       */
      vec3 nn(n.x(), n.y(), -dot(n, poly[i]));
      vec3 clip_pt(transform * vec3(poly[i].x(), poly[i].y(), 1.0f));
      float recip_z(1.0f / clip_pt.z());
      vec2 normalized_pt(clip_pt * recip_z);

      m_clip.current().push_back(inverse_transpose * nn);
      m_poly.current().push_back(clip_pt);
      m_current_bb.union_point(normalized_pt);
    }
}

////////////////////////////////////////
// TransparencyStackEntryFactory methods
void
TransparencyStackEntryFactory::
begin(fastuidraw::ivec2 surface_sz,
      const fastuidraw::PainterBackend::Surface::Viewport &vwp)
{
  bool clear_buffers;

  /* We can freely translate normalized device coords, but we
   * cannot scale them. Thus set our viewport to the same size
   * as the passed viewport but the origin at (0, 0).
   */
  m_viewport.m_origin = fastuidraw::ivec2(0, 0);
  m_viewport.m_dimensions = vwp.m_dimensions;

  /* We can only use the portions of the backing store
   * that is within m_viewport.
   */
  m_current_backing_useable_size = vwp.compute_visible_dimensions(surface_sz);

  clear_buffers = (m_current_backing_useable_size.x() > m_current_backing_size.x())
    || (m_current_backing_useable_size.y() > m_current_backing_size.y())
    || (m_current_backing_size.x() > 2 * m_current_backing_useable_size.x())
    || (m_current_backing_size.y() > 2 * m_current_backing_useable_size.y());

  if (clear_buffers)
    {
      m_per_active_depth.clear();
      m_unused_buffers.clear();
      m_current_backing_size = m_current_backing_useable_size;
    }
  else
    {
      for (PerActiveDepth &v : m_per_active_depth)
        {
          for (const auto &r : v)
            {
              r->m_rect_atlas.clear(m_current_backing_useable_size);
              m_unused_buffers.push_back(r);
            }
          v.clear();
        }
    }
  m_active_surfaces.clear();
}

TransparencyStackEntry
TransparencyStackEntryFactory::
fetch(unsigned int transparency_depth,
      const fastuidraw::Rect &normalized_rect,
      PainterPrivate *d)
{
  using namespace fastuidraw;

  TransparencyStackEntry return_value;
  ivec2 rect(-1, -1);
  ivec2 dims, bl, tr;
  vec2 fbl, ftr;

  /* we need to discretize the rectangle, so that
   * the size is correct. Using the normalized_rect.size()
   * scaled is actually WRONG. Rather we need to look
   * at the range of -sample- points the rect hits.
   */
  fbl = coords_from_normalized_coords(normalized_rect.m_min_point, d->m_viewport.m_dimensions);
  ftr = coords_from_normalized_coords(normalized_rect.m_max_point, d->m_viewport.m_dimensions);
  bl = ivec2(fbl);
  tr = ivec2(ftr);
  dims = tr - bl;
  if (ftr.x() > tr.x() && dims.x() < m_current_backing_useable_size.x())
    {
      ++dims.x();
      ++tr.x();
    }
  if (ftr.y() > tr.y() && dims.y() < m_current_backing_useable_size.y())
    {
      ++dims.y();
      ++tr.y();
    }

  fbl = vec2(bl);
  ftr = vec2(tr);

  if (transparency_depth >= m_per_active_depth.size())
    {
      m_per_active_depth.resize(transparency_depth + 1);
    }

  for (unsigned int i = 0, endi = m_per_active_depth[transparency_depth].size();
       (rect.x() < 0 || rect.y() < 0) && i < endi; ++i)
    {
      rect = m_per_active_depth[transparency_depth][i]->m_rect_atlas.add_rectangle(dims);
      if (rect.x() >= 0 && rect.y() >= 0)
        {
          return_value.m_image = m_per_active_depth[transparency_depth][i]->m_image.get();
          return_value.m_packer = m_per_active_depth[transparency_depth][i]->m_packer.get();
        }
    }

  if (rect.x() < 0 || rect.y() < 0)
    {
      reference_counted_ptr<TransparencyBuffer> TB;

      if (m_unused_buffers.empty())
        {
          reference_counted_ptr<PainterPacker> packer;
          reference_counted_ptr<PainterBackend::Surface> surface;
          reference_counted_ptr<const Image> image;

          packer = FASTUIDRAWnew PainterPacker(d->m_pool, d->m_stats, d->m_backend);
          surface = d->m_backend->create_surface(m_current_backing_size);
          surface->clear_color(vec4(0.0f, 0.0f, 0.0f, 0.0f));
          image = surface->image(d->m_backend->image_atlas());
          TB = FASTUIDRAWnew TransparencyBuffer(packer, surface, image);
        }
      else
        {
          TB = m_unused_buffers.back();
          m_unused_buffers.pop_back();
        }

      ++d->m_stats[Painter::num_render_targets];
      TB->m_depth = transparency_depth;
      TB->m_surface->viewport(m_viewport);
      m_per_active_depth[transparency_depth].push_back(TB);
      m_active_surfaces.push_back(TB->m_surface.get());

      rect = TB->m_rect_atlas.add_rectangle(dims);
      return_value.m_image = TB->m_image.get();
      return_value.m_packer = TB->m_packer.get();
      return_value.m_packer->begin(TB->m_surface, true);
    }

  FASTUIDRAWassert(return_value.m_image);
  FASTUIDRAWassert(return_value.m_packer);

  /* we are NOT taking the original normalized rect,
   * instead we are taking the normalized rect made
   * from bl, tr which are the discretizations of
   * the original rect to pixels.
   */
  return_value.m_normalized_rect
    .min_point(normalized_coords_from_coords(bl, d->m_viewport.m_dimensions))
    .max_point(normalized_coords_from_coords(tr, d->m_viewport.m_dimensions));

  FASTUIDRAWassert(rect.x() >= 0 && rect.y() >= 0);

  /* we need to have that:
   *   return_value.m_normalized_translate + normalized_rect = R
   * where R is rect in normalized device coordinates.
   */
  vec2 Rndc;
  Rndc = m_viewport.compute_normalized_device_coords(vec2(rect));
  return_value.m_normalized_translate = Rndc - return_value.m_normalized_rect.m_min_point;

  /* the drawing of the fill_rect in end_layer() draws normalized_rect
   * scaled by m_viewport.m_dimensions. We need to have that
   *   return_value.m_brush_translate + S = rect
   * where S is normalized_rect in -pixel- coordinates
   */
  vec2 Spc;
  Spc = m_viewport.compute_pixel_coordinates(return_value.m_normalized_rect.m_min_point);
  return_value.m_brush_translate = vec2(rect) - Spc ;

  PainterItemMatrix M;
  M.m_normalized_translate = return_value.m_normalized_translate;
  return_value.m_identity_matrix = d->m_pool.create_packed_value(M);

  return return_value;
}

void
TransparencyStackEntryFactory::
end(void)
{
  /* issue end() in reverse depth order */
  for (auto iter = m_per_active_depth.rbegin(); iter != m_per_active_depth.rend(); ++iter)
    {
      for (const auto &r : *iter)
        {
          r->m_packer->end();
        }
    }
}

//////////////////////////////////
// PainterPrivate methods
PainterPrivate::
PainterPrivate(fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend> backend):
  m_viewport_dimensions(1.0f, 1.0f),
  m_one_pixel_width(1.0f, 1.0f),
  m_curve_flatness(0.5f),
  m_backend(backend)
{
  // By calling PainterBackend::default_shaders(), we make the shaders
  // registered. By setting m_default_shaders to its return value,
  // and using that for the return value of Painter::default_shaders(),
  // we skip the check in PainterBackend::default_shaders() to register
  // the shaders as well.
  m_default_shaders = m_backend->default_shaders();
  m_root_packer = FASTUIDRAWnew fastuidraw::PainterPacker(m_pool, m_stats, m_backend);
  m_reset_brush = m_pool.create_packed_value(fastuidraw::PainterBrush());
  m_black_brush = m_pool.create_packed_value(fastuidraw::PainterBrush()
                                             .color(0.0f, 0.0f, 0.0f, 0.0f));
  m_root_identity_matrix = m_pool.create_packed_value(fastuidraw::PainterItemMatrix());
  m_current_z = 1;
  m_draw_data_added_count = 0;
  m_max_attribs_per_block = backend->attribs_per_mapping();
  m_max_indices_per_block = backend->indices_per_mapping();

  /* Create a path representing a corner of rounded rectangle.
   * This path will be used/resued to clip against rounded
   * rectangles
   */
  m_rounded_corner_path << fastuidraw::vec2(0.0f, 0.0f)
                        << fastuidraw::Path::arc(-FASTUIDRAW_PI / 2.0f, fastuidraw::vec2(1.0f, 1.0f))
                        << fastuidraw::vec2(1.0f, 0.0f)
                        << fastuidraw::Path::contour_close();

  /* we use the complement path to draw the occluders;
   * we need to rely on the complement instead of using
   * the original path with the zero-fill rule because
   * FilledPath adds fuzz along the boundary and sometimes
   * that fuzz is present in drawing.
   */
  m_rounded_corner_path_complement << fastuidraw::vec2(0.0f, 0.0f)
                                   << fastuidraw::Path::arc(-FASTUIDRAW_PI / 2.0f, fastuidraw::vec2(1.0f, 1.0f))
                                   << fastuidraw::vec2(0.0f, 1.0f)
                                   << fastuidraw::Path::contour_close();

  /* a saved path for stroking and filling rects */
  m_square_path << fastuidraw::vec2(0.0f, 0.0f)
                << fastuidraw::vec2(0.0f, 1.0f)
                << fastuidraw::vec2(1.0f, 1.0f)
                << fastuidraw::vec2(1.0f, 0.0f)
                << fastuidraw::Path::contour_close();
}

void
PainterPrivate::
concat(const fastuidraw::float3x3 &tr)
{
  using namespace fastuidraw;

  float3x3 m;
  bool tricky;

  tricky = (tr(0, 1) != 0.0f || tr(1, 0) != 0.0f
            || tr(2, 0) != 0.0f || tr(2, 1) != 0.0f
            || tr(2, 2) != 1.0f);

  m = m_clip_rect_state.item_matrix() * tr;
  m_clip_rect_state.item_matrix(m, tricky, unclassified_matrix);

  if (!tricky)
    {
      m_clip_rect_state.m_clip_rect.translate(vec2(-tr(0, 2), -tr(1, 2)));
      m_clip_rect_state.m_clip_rect.shear(1.0f / tr(0, 0), 1.0f / tr(1, 1));
    }
}

void
PainterPrivate::
translate(const fastuidraw::vec2 &p)
{
  using namespace fastuidraw;

  float3x3 m(m_clip_rect_state.item_matrix());
  m.translate(p.x(), p.y());
  m_clip_rect_state.item_matrix(m, false, non_scaling_matrix, 1.0f);
  m_clip_rect_state.m_clip_rect.translate(-p);
}

void
PainterPrivate::
scale(float s)
{
  using namespace fastuidraw;

  float3x3 m(m_clip_rect_state.item_matrix());
  m.scale(s);
  m_clip_rect_state.item_matrix(m, false, scaling_matrix, t_abs(s));
  m_clip_rect_state.m_clip_rect.scale(1.0f / s);
}

void
PainterPrivate::
shear(float sx, float sy)
{
  using namespace fastuidraw;

  float3x3 m(m_clip_rect_state.item_matrix());
  enum matrix_type_t tp;
  float sf;

  tp = (t_abs(sx) != t_abs(sy)) ?
    shearing_matrix : scaling_matrix;

  sf = (t_abs(sx) != t_abs(sy)) ?
    -1.0f : t_abs(sx);

  m.shear(sx, sy);
  m_clip_rect_state.item_matrix(m, false, tp, sf);
  m_clip_rect_state.m_clip_rect.shear(1.0f / sx, 1.0f / sy);
}

void
PainterPrivate::
rotate(float angle)
{
  using namespace fastuidraw;

  float3x3 tr;
  float s, c;

  s = t_sin(angle);
  c = t_cos(angle);

  tr(0, 0) = c;
  tr(1, 0) = s;

  tr(0, 1) = -s;
  tr(1, 1) = c;

  float3x3 m(m_clip_rect_state.item_matrix());
  m = m * tr;
  m_clip_rect_state.item_matrix(m, true, non_scaling_matrix, 1.0f);
}

bool
PainterPrivate::
update_clip_equation_series(const fastuidraw::Rect &rect)
{
  /* We compute the rectangle clipped against the current
   * clipping planes which gives us a convex polygon.
   * That convex polygon then satsifies all of the
   * previous clip equations. We then generate the
   * clip equations coming from that polygon and REPLACE
   * the old clip equations with the new ones.
   */
  unsigned int src;

  m_work_room.m_clipper.m_pts_update_series[0].resize(4);
  m_work_room.m_clipper.m_pts_update_series[0][0] = rect.m_min_point;
  m_work_room.m_clipper.m_pts_update_series[0][1] = fastuidraw::vec2(rect.m_min_point.x(), rect.m_max_point.y());
  m_work_room.m_clipper.m_pts_update_series[0][2] = rect.m_max_point;
  m_work_room.m_clipper.m_pts_update_series[0][3] = fastuidraw::vec2(rect.m_max_point.x(), rect.m_min_point.y());
  src = m_clip_store.clip_against_current(m_clip_rect_state.item_matrix(),
                                          m_work_room.m_clipper.m_pts_update_series);

  /* the input rectangle clipped to the previous clipping equation
   * array is now stored in m_work_room.m_clipper.m_pts_update_series[src]
   */
  fastuidraw::c_array<const fastuidraw::vec2> poly;
  poly = fastuidraw::make_c_array(m_work_room.m_clipper.m_pts_update_series[src]);
  m_clip_store.set_current(m_clip_rect_state.item_matrix(),
                           m_clip_rect_state.item_matrix_inverse_transpose(),
                           poly);
  return poly.empty();
}

float
PainterPrivate::
compute_magnification(const fastuidraw::Path &path)
{
  fastuidraw::Rect R;

  /* TODO: for stroking, it might be that although the
   * original path is completely clipped, the stroke of
   * is not. It might be wise to inflate the geometry
   * of the path by how much slack the stroking parameters
   * require.
   */
  if (!path.approximate_bounding_box(&R))
    {
      /* Path is empty, does not matter what tessellation
       * is taken.
       */
      return -1.0f;
    }
  return compute_magnification(R);
}

float
PainterPrivate::
compute_magnification(const fastuidraw::Rect &rect)
{
  unsigned int src;

  /* clip the bounding box given by rect */
  m_work_room.m_clipper.m_vec2s[0].resize(4);
  m_work_room.m_clipper.m_vec2s[0][0] = rect.m_min_point;
  m_work_room.m_clipper.m_vec2s[0][1] = fastuidraw::vec2(rect.m_min_point.x(), rect.m_max_point.y());
  m_work_room.m_clipper.m_vec2s[0][2] = rect.m_max_point;
  m_work_room.m_clipper.m_vec2s[0][3] = fastuidraw::vec2(rect.m_max_point.x(), rect.m_min_point.y());

  src = m_clip_store.clip_against_current(m_clip_rect_state.item_matrix(),
                                          m_work_room.m_clipper.m_vec2s);

  fastuidraw::c_array<const fastuidraw::vec2> poly;
  poly = make_c_array(m_work_room.m_clipper.m_vec2s[src]);

  return compute_max_magnification_at_points(poly);
}

float
PainterPrivate::
compute_max_magnification_at_points(fastuidraw::c_array<const fastuidraw::vec2> poly)
{
  using namespace fastuidraw;

  if (poly.empty())
    {
      /* bounding box is completely clipped */
      return -1.0f;
    }

  float op_norm;
  const float3x3 &m(m_clip_rect_state.item_matrix());

  op_norm = m_clip_rect_state.item_matrix_operator_norm();
  if (!matrix_has_perspective(m))
    {
      return op_norm / t_abs(m(2, 2));
    }

  /* To figure out the magnification, we need to maximize
   *
   *   || DF ||
   *
   * where
   *
   *  F(x, y) = S * P( M(x, y, 1) )
   *
   * where
   *
   *  P(x, y, w) = (x/w, y/w)
   *
   * and M is our 3x3 matrix and S is the scaling factor from
   * normalized device coordinate to pixel coordinates.
   *
   *  DF = DS * DP * T
   *
   * where T is given by
   *
   *   | R    |
   *   | A  B |
   *
   * where R is the upper 2x2 matrix of M and (A, B, C)
   * is the bottom row of M. DS is given by:
   *
   *  | sx  0 |
   *  |  0 sy |
   *
   * where sx and sy are the normalized device to pixel
   * coordinate scaling factors. DP is given by
   *
   *    1     |    1         0    -x_c/w_c |
   *  ----- * |    0         1    -y_c/w_c |
   *   w_c
   *
   * where x_c, y_c, w_c are the clip-coordinates.
   * A small abount of linear algebra gives that
   *
   *   DS * DP = DP * DSS
   *
   * where
   *        | sx  0 |
   *   SS = |  0 sy |
   *        | sx sy |
   *
   * then DF can be simplified to
   *
   *   DF = DP * | S * R           |
   *             | sx * A + sx * B |
   *
   *         1   /          A * sx * x_c / w_c   \
   *      = ---  | S * R +  B + sy * y_c / w_c   |
   *        w_c  \                               /
   *
   *         1    /           \
   *      = --- * | S * R + V |
   *        w_c   \           /
   *
   * where
   *
   *         | A * sx * x_c / w_c          0         |
   *    V =  | 0                  B + sy * y_c / w_c |
   *
   * Thus,
   *              1
   *  ||DF|| <= ----- * ( ||S * R || + ||V||)
   *            |w_c|
   *
   *  ||S * R|| is given by m_clip_rect_state.item_matrix_operator_norm().
   *
   * and
   *
   *  ||V|| = max( |A * sx * x_c / w_c|, |B * sy * y_c / w_c| )
   *
   * now, |x_c| <= |w_c| and |y_c| <= |w_c|, this simplifies ||V|| to:
   *
   *  ||V|| <= max( |A * sx|, |B * sy|)
   */

  float min_w;

  /* initalize min_w to some obsecenely large value */
  min_w = 1e+6;

  /* find the smallest w of all the clipped points */
  for(unsigned int i = 0, endi = poly.size(); i < endi; ++i)
    {
      vec3 q;
      const float tol_w = 1e-6;

      q = m * vec3(poly[i].x(), poly[i].y(), 1.0f);
      /* the clip equations from the start guarnatee that
       * q.z() >= 0. If it is zero, then the edge of q satsifies
       * |x| = w and |y| = w which means we should probably
       * ignore q.
       */
      if (q.z() > tol_w)
	{
	  min_w = t_min(min_w, q.z());
	}
    }

  float v_norm;
  v_norm = 0.5f * t_max(t_abs(m(2, 0) * m_viewport_dimensions.x()),
                        t_abs(m(2, 1) * m_viewport_dimensions.y()));

  return (op_norm + v_norm) / min_w;
}

float
PainterPrivate::
compute_path_thresh(const fastuidraw::Path &path,
                    const fastuidraw::PainterShaderData::DataBase *shader_data,
                    const fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase> &selector,
                    float &thresh)
{
  float mag, t;

  mag = compute_magnification(path);
  thresh = selector->compute_thresh(shader_data, mag, m_curve_flatness);
  t = fastuidraw::t_min(thresh, m_curve_flatness / mag);

  return t;
}

const fastuidraw::StrokedPath*
PainterPrivate::
select_stroked_path(const fastuidraw::Path &path,
                    const fastuidraw::PainterStrokeShader &shader,
                    const fastuidraw::PainterData &draw,
                    enum fastuidraw::Painter::shader_anti_alias_t aa_mode,
                    enum fastuidraw::Painter::stroking_method_t stroking_method,
                    float &thresh)
{
  using namespace fastuidraw;

  float t;
  const PainterShaderData::DataBase *data(draw.m_item_shader_data.data().data_base());
  const reference_counted_ptr<const StrokingDataSelectorBase> &selector(shader.stroking_data_selector());

  if (selector->data_compatible(data))
    {
      t = compute_path_thresh(path, data, selector, thresh);
    }
  else
    {
      FASTUIDRAWwarning("Passed incompatible data to select path for stroking");
      return nullptr;
    }

  if (stroking_method == Painter::stroking_method_auto)
    {
      stroking_method = shader.arc_stroking_is_fast(aa_mode) ?
        Painter::stroking_method_arc:
        Painter::stroking_method_linear;
    }

  const TessellatedPath *tess;
  tess = path.tessellation(t).get();

  if (stroking_method != Painter::stroking_method_arc
      || !shader.stroking_data_selector()->arc_stroking_possible(data))
    {
      tess = tess->linearization(t);
    }
  return tess->stroked().get();
}

float
PainterPrivate::
compute_path_thresh(const fastuidraw::Path &path)
{
  float mag, thresh;

  mag = compute_magnification(path);
  thresh = m_curve_flatness / mag;
  return thresh;
}

unsigned int
PainterPrivate::
select_subsets(const fastuidraw::FilledPath &path,
               fastuidraw::c_array<unsigned int> dst)
{
  FASTUIDRAWassert(dst.size() >= path.number_subsets());
  if (m_clip_rect_state.m_all_content_culled)
    {
      return 0u;
    }

  return path.select_subsets(m_work_room.m_fill_subset.m_scratch,
                             m_clip_store.current(),
                             m_clip_rect_state.item_matrix(),
                             m_max_attribs_per_block,
                             m_max_indices_per_block,
                             dst);
}

unsigned int
PainterPrivate::
select_subsets(const fastuidraw::StrokedPath &path,
               float pixels_additional_room,
               float item_space_additional_room,
               fastuidraw::c_array<unsigned int> dst)
{
  FASTUIDRAWassert(dst.size() >= path.number_subsets());
  if (m_clip_rect_state.m_all_content_culled)
    {
      return 0u;
    }

  FASTUIDRAWassert(dst.size() >= path.number_subsets());
  return path.select_subsets(m_work_room.m_stroke.m_path_scratch,
                             m_clip_store.current(),
                             m_clip_rect_state.item_matrix(),
                             m_one_pixel_width,
                             pixels_additional_room,
                             item_space_additional_room,
                             m_max_attribs_per_block,
                             m_max_indices_per_block,
                             dst);
}

void
PainterPrivate::
select_chunks(const fastuidraw::StrokedCapsJoins &caps_joins,
              float pixels_additional_room,
              float item_space_additional_room,
              bool select_joins_for_miter_style,
              fastuidraw::StrokedCapsJoins::ChunkSet *dst)
{
  FASTUIDRAWassert(dst);
  if (m_clip_rect_state.m_all_content_culled)
    {
      dst->reset();
      return;
    }

  caps_joins.compute_chunks(m_work_room.m_stroke.m_caps_joins_scratch,
                            m_clip_store.current(),
                            m_clip_rect_state.item_matrix(),
                            m_one_pixel_width,
                            pixels_additional_room,
                            item_space_additional_room,
                            m_max_attribs_per_block,
                            m_max_indices_per_block,
                            select_joins_for_miter_style,
                            *dst);
}

const fastuidraw::FilledPath&
PainterPrivate::
select_filled_path(const fastuidraw::Path &path)
{
  using namespace fastuidraw;
  float thresh;

  thresh = compute_path_thresh(path);
  return *path.tessellation(thresh)->filled(thresh);
}

void
PainterPrivate::
draw_generic(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
             const fastuidraw::PainterData &draw,
             fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
             fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
             fastuidraw::c_array<const int> index_adjusts,
             fastuidraw::c_array<const unsigned int> attrib_chunk_selector,
             int z)
{
  fastuidraw::PainterPackerData p(draw);

  p.m_clip = m_clip_rect_state.clip_equations_state(m_pool);
  p.m_matrix = m_clip_rect_state.current_item_matrix_state(m_pool);
  packer()->draw_generic(shader, p, attrib_chunks, index_chunks, index_adjusts, attrib_chunk_selector, z);
  ++m_draw_data_added_count;
}

void
PainterPrivate::
draw_generic(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
             const fastuidraw::PainterData &draw,
             const fastuidraw::PainterAttributeWriter &src,
             int z)
{
  fastuidraw::PainterPackerData p(draw);
  p.m_clip = m_clip_rect_state.clip_equations_state(m_pool);
  p.m_matrix = m_clip_rect_state.current_item_matrix_state(m_pool);
  packer()->draw_generic(shader, p, src, z);
  ++m_draw_data_added_count;
}

void
PainterPrivate::
pre_draw_anti_alias_fuzz(const fastuidraw::FilledPath &filled_path,
                         enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality,
                         const FillSubsetWorkRoom &fill_subset,
                         AntiAliasFillWorkRoom *output)
{
  output->m_total_increment_z = 0;
  output->m_z_increments.clear();
  output->m_attrib_chunks.clear();
  output->m_index_chunks.clear();
  output->m_index_adjusts.clear();
  output->m_start_zs.clear();
  for(unsigned int s : fill_subset.m_subsets)
    {
      fastuidraw::FilledPath::Subset subset(filled_path.subset(s));
      const fastuidraw::PainterAttributeData &data(subset.aa_fuzz_painter_data());
      fastuidraw::c_array<const int> winding_numbers(subset.winding_numbers());

      for(int w : winding_numbers)
        {
          if (fill_subset.m_ws(w))
            {
              unsigned int ch;
              fastuidraw::range_type<int> R;

              ch = fastuidraw::FilledPath::Subset::aa_fuzz_chunk_from_winding_number(w);
              R = data.z_range(ch);

              output->m_attrib_chunks.push_back(data.attribute_data_chunk(ch));
              output->m_index_chunks.push_back(data.index_data_chunk(ch));
              output->m_index_adjusts.push_back(data.index_adjust_chunk(ch));

              if (anti_alias_quality == fastuidraw::Painter::shader_anti_alias_simple)
                {
                  output->m_z_increments.push_back(R.difference());
                  output->m_start_zs.push_back(R.m_begin);
                  output->m_total_increment_z += output->m_z_increments.back();
                }
              else
                {
                  output->m_z_increments.push_back(0);
                  output->m_start_zs.push_back(0);
                }
            }
        }
    }

  if (anti_alias_quality == fastuidraw::Painter::shader_anti_alias_high_quality
      && !output->m_attrib_chunks.empty())
    {
      ++output->m_total_increment_z;
    }
}

void
PainterPrivate::
draw_anti_alias_fuzz(const fastuidraw::PainterFillShader &shader,
                     const fastuidraw::PainterData &draw,
                     enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality,
                     const AntiAliasFillWorkRoom &data,
                     int z)
{
  using namespace fastuidraw;
  if (anti_alias_quality == Painter::shader_anti_alias_simple)
    {
      draw_generic_z_layered(shader.aa_fuzz_shader(), draw,
                             make_c_array(data.m_z_increments),
                             data.m_total_increment_z,
                             make_c_array(data.m_attrib_chunks),
                             make_c_array(data.m_index_chunks),
                             make_c_array(data.m_index_adjusts),
                             make_c_array(data.m_start_zs),
                             z);

    }
  else if (anti_alias_quality == fastuidraw::Painter::shader_anti_alias_high_quality)
    {
      draw_generic(shader.aa_fuzz_hq_shader_pass1(), draw,
                   make_c_array(data.m_attrib_chunks),
                   make_c_array(data.m_index_chunks),
                   make_c_array(data.m_index_adjusts),
                   fastuidraw::c_array<const unsigned int>(),
                   z);
      packer()->draw_break(shader.aa_fuzz_hq_action_pass1());

      draw_generic(shader.aa_fuzz_hq_shader_pass2(), draw,
                   make_c_array(data.m_attrib_chunks),
                   make_c_array(data.m_index_chunks),
                   make_c_array(data.m_index_adjusts),
                   fastuidraw::c_array<const unsigned int>(),
                   z);
      packer()->draw_break(shader.aa_fuzz_hq_action_pass2());
    }
}

void
PainterPrivate::
draw_generic_z_layered(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> &shader,
                       const fastuidraw::PainterData &draw,
                       fastuidraw::c_array<const int> z_increments, int zinc_sum,
                       fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
                       fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                       fastuidraw::c_array<const int> index_adjusts,
                       fastuidraw::c_array<const int> start_zs, int startz)
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
                   z);
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
                       fastuidraw::c_array<const int> start_zs, int startz)
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
                   z);
    }
}

void
PainterPrivate::
stroke_path_common(const fastuidraw::PainterStrokeShader &shader,
                   const fastuidraw::PainterData &draw,
                   fastuidraw::c_array<const fastuidraw::vec2> line_strip,
                   enum fastuidraw::Painter::cap_style cp,
                   enum fastuidraw::Painter::join_style js,
                   enum fastuidraw::Painter::shader_anti_alias_t anti_alias)
{
  using namespace fastuidraw;

  if (m_clip_rect_state.m_all_content_culled || line_strip.empty())
    {
      return;
    }

  Path path;
  float thresh(-1.0f);

  if (line_strip.size() < 2)
    {
      js = fastuidraw::Painter::no_joins;
    }

  /* TODO: creating a temporary path makes unnecessary memory
   * allocation/deallocation noise; instead we should compute
   * the attribute data directly and feed it directly to
   * stroke_path_raw().
   */
  for (const vec2 &pt : line_strip)
    {
      path << pt;
    }

  if (cp == Painter::rounded_caps || js == Painter::bevel_joins)
    {
      const PainterShaderData::DataBase *shader_data;
      float mag;

      if (js == Painter::bevel_joins)
        {
          c_array<const vec2> tmp(line_strip);
          tmp.pop_front();
          tmp.pop_back();
          mag = compute_max_magnification_at_points(tmp);
        }
      else
        {
          mag = -1.0f;
        }

      if (cp == Painter::rounded_caps)
        {
          mag = t_max(mag, compute_magnification_at_point(line_strip.front()));
          mag = t_max(mag, compute_magnification_at_point(line_strip.back()));
        }

      if (mag > 0.0f)
        {
          shader_data = draw.m_item_shader_data.data().data_base();
          thresh = shader.stroking_data_selector()->compute_thresh(shader_data, mag, m_curve_flatness);
        }
    }

  stroke_path_common(shader, draw,
                     *path.tessellation(-1.0f)->stroked(), thresh,
                     cp, js, anti_alias);
}

void
PainterPrivate::
stroke_path_common(const fastuidraw::PainterStrokeShader &shader,
                   const fastuidraw::PainterData &draw,
                   const fastuidraw::StrokedPath &path, float thresh,
                   enum fastuidraw::Painter::cap_style cp,
                   enum fastuidraw::Painter::join_style js,
                   enum fastuidraw::Painter::shader_anti_alias_t anti_aliasing)
{
  using namespace fastuidraw;

  if (m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  const PainterAttributeData *cap_data(nullptr), *join_data(nullptr);
  bool is_miter_join;
  const PainterShaderData::DataBase *raw_data;
  const StrokedCapsJoins &caps_joins(path.caps_joins());
  bool edge_arc_shader(path.has_arcs()), cap_arc_shader(false), join_arc_shader(false);

  raw_data = draw.m_item_shader_data.data().data_base();

  switch(cp)
    {
    case Painter::rounded_caps:
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

    case Painter::square_caps:
      cap_data = &caps_joins.square_caps();
      break;

    case Painter::flat_caps:
      cap_data = nullptr;
      break;

    case Painter::number_cap_styles:
      cap_data = &caps_joins.adjustable_caps();
      break;
    }

  switch(js)
    {
    case Painter::miter_clip_joins:
      is_miter_join = true;
      join_data = &caps_joins.miter_clip_joins();
      break;

    case Painter::miter_bevel_joins:
      is_miter_join = true;
      join_data = &caps_joins.miter_bevel_joins();
      break;

    case Painter::miter_joins:
      is_miter_join = true;
      join_data = &caps_joins.miter_joins();
      break;

    case Painter::bevel_joins:
      is_miter_join = false;
      join_data = &caps_joins.bevel_joins();
      break;

    case Painter::rounded_joins:
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

  unsigned int subset_count;
  m_work_room.m_stroke.m_subsets.resize(path.number_subsets());

  subset_count = path.select_subsets(m_work_room.m_stroke.m_path_scratch,
                                     m_clip_store.current(),
                                     m_clip_rect_state.item_matrix(),
                                     m_one_pixel_width,
                                     pixels_additional_room,
                                     item_space_additional_room,
                                     m_max_attribs_per_block,
                                     m_max_indices_per_block,
                                     make_c_array(m_work_room.m_stroke.m_subsets));
  FASTUIDRAWassert(subset_count <= m_work_room.m_stroke.m_subsets.size());
  m_work_room.m_stroke.m_subsets.resize(subset_count);

  caps_joins.compute_chunks(m_work_room.m_stroke.m_caps_joins_scratch,
                            m_clip_store.current(),
                            m_clip_rect_state.item_matrix(),
                            m_one_pixel_width,
                            pixels_additional_room,
                            item_space_additional_room,
                            m_max_attribs_per_block,
                            m_max_indices_per_block,
                            is_miter_join,
                            m_work_room.m_stroke.m_caps_joins_chunk_set);

  stroke_path_raw(shader, edge_arc_shader, join_arc_shader, cap_arc_shader, draw,
                  &path, make_c_array(m_work_room.m_stroke.m_subsets),
                  cap_data, m_work_room.m_stroke.m_caps_joins_chunk_set.cap_chunks(),
                  join_data, m_work_room.m_stroke.m_caps_joins_chunk_set.join_chunks(),
                  anti_aliasing);
}

void
PainterPrivate::
stroke_path_raw(const fastuidraw::PainterStrokeShader &shader,
                bool edge_use_arc_shaders,
                bool join_use_arc_shaders,
                bool cap_use_arc_shaders,
                const fastuidraw::PainterData &pdraw,
                const fastuidraw::StrokedPath *stroked_path,
                fastuidraw::c_array<const unsigned int> stroked_subset_ids,
                const fastuidraw::PainterAttributeData *cap_data,
                fastuidraw::c_array<const unsigned int> cap_chunks,
                const fastuidraw::PainterAttributeData* join_data,
                fastuidraw::c_array<const unsigned int> join_chunks,
                enum fastuidraw::Painter::shader_anti_alias_t anti_aliasing)
{
  using namespace fastuidraw;
  const unsigned int stroked_path_chunk = 0;

  if (m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  if (join_data == nullptr)
    {
      join_chunks = c_array<const unsigned int>();
    }

  if (stroked_path == nullptr)
    {
      stroked_subset_ids = c_array<const unsigned int>();
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

  total_chunks = cap_chunks.size() + stroked_subset_ids.size() + join_chunks.size();
  if (total_chunks == 0)
    {
      return;
    }

  enum Painter::shader_anti_alias_t fastest;

  fastest = (edge_use_arc_shaders) ?
    shader.fastest_anti_alias_mode(PainterStrokeShader::arc_stroke_type) :
    shader.fastest_anti_alias_mode(PainterStrokeShader::linear_stroke_type);

  anti_aliasing = compute_shader_anti_alias(anti_aliasing,
                                            shader.hq_anti_alias_support(),
                                            fastest);

  unsigned int zinc_sum(0), current(0), modify_z_coeff;
  bool modify_z;
  c_array<c_array<const PainterAttribute> > attrib_chunks;
  c_array<c_array<const PainterIndex> > index_chunks;
  c_array<int> index_adjusts;
  c_array<int> z_increments;
  c_array<int> start_zs;
  c_array<const reference_counted_ptr<PainterItemShader>* > shaders_pass1;
  c_array<const reference_counted_ptr<PainterItemShader>* > shaders_pass2;
  StrokingItems stroking_items(anti_aliasing);
  PainterBlendShader* old_blend;
  PainterCompositeShader* old_composite;
  BlendMode old_composite_mode;
  PainterData draw(pdraw);

  m_work_room.m_stroke.m_attrib_chunks.resize(total_chunks);
  m_work_room.m_stroke.m_index_chunks.resize(total_chunks);
  m_work_room.m_stroke.m_increment_zs.resize(total_chunks);
  m_work_room.m_stroke.m_index_adjusts.resize(total_chunks);
  m_work_room.m_stroke.m_start_zs.resize(total_chunks);
  m_work_room.m_stroke.m_shaders_pass1.resize(total_chunks);
  m_work_room.m_stroke.m_shaders_pass2.resize(total_chunks);

  attrib_chunks = make_c_array(m_work_room.m_stroke.m_attrib_chunks);
  index_chunks = make_c_array(m_work_room.m_stroke.m_index_chunks);
  z_increments = make_c_array(m_work_room.m_stroke.m_increment_zs);
  index_adjusts = make_c_array(m_work_room.m_stroke.m_index_adjusts);
  start_zs = make_c_array(m_work_room.m_stroke.m_start_zs);
  shaders_pass1 = make_c_array(m_work_room.m_stroke.m_shaders_pass1);
  shaders_pass2 = make_c_array(m_work_room.m_stroke.m_shaders_pass2);
  current = 0;

  if (!stroked_subset_ids.empty())
    {
      stroking_items.add_element(edge_use_arc_shaders, shader, stroked_subset_ids.size());
      for(unsigned int E = 0; E < stroked_subset_ids.size(); ++E, ++current)
        {
          StrokedPath::Subset S(stroked_path->subset(stroked_subset_ids[E]));

          attrib_chunks[current] = S.painter_data().attribute_data_chunk(stroked_path_chunk);
          index_chunks[current] = S.painter_data().index_data_chunk(stroked_path_chunk);
          index_adjusts[current] = S.painter_data().index_adjust_chunk(stroked_path_chunk);
          z_increments[current] = S.painter_data().z_range(stroked_path_chunk).difference();
          start_zs[current] = S.painter_data().z_range(stroked_path_chunk).m_begin;
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

  bool with_anti_aliasing;

  with_anti_aliasing = (anti_aliasing != Painter::shader_anti_alias_none);
  modify_z = (anti_aliasing != Painter::shader_anti_alias_high_quality);
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

  if (anti_aliasing == Painter::shader_anti_alias_high_quality)
    {
      /* prevent the draw from changing color values so that it
       * only hits auxiliar buffer values.
       */
      const PainterCompositeShaderSet &shader_set(m_default_shaders.composite_shaders());
      enum Painter::composite_mode_t m(Painter::composite_porter_duff_dst);

      old_blend = packer()->blend_shader();
      old_composite = packer()->composite_shader();
      old_composite_mode = packer()->composite_mode();
      packer()->composite_shader(shader_set.shader(m).get(), shader_set.composite_mode(m));
      packer()->blend_shader(m_default_shaders.blend_shaders().shader(Painter::blend_w3c_normal).get());
      packer()->draw_break(shader.hq_aa_action_pass1());
    }

  if (modify_z)
    {
      draw_generic_z_layered(shaders_pass1, draw, z_increments, zinc_sum,
                             attrib_chunks, index_chunks, index_adjusts,
                             start_zs,
                             m_current_z + (modify_z_coeff - 1) * zinc_sum);
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
                       m_current_z);
        }
    }

  if (with_anti_aliasing)
    {
      if (anti_aliasing == Painter::shader_anti_alias_high_quality)
        {
          packer()->blend_shader(old_blend);
          packer()->composite_shader(old_composite, old_composite_mode);
          packer()->draw_break(shader.hq_aa_action_pass2());
        }

      if (modify_z)
        {
          draw_generic_z_layered(shaders_pass2, draw, z_increments, zinc_sum,
                                 attrib_chunks, index_chunks, index_adjusts,
                                 start_zs, m_current_z);
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
                           m_current_z);
            }
        }
    }

  if (modify_z)
    {
      m_current_z += modify_z_coeff * zinc_sum;
    }
}

void
PainterPrivate::
fill_path_compute_opaque_chunks(const fastuidraw::FilledPath &filled_path,
                                enum fastuidraw::Painter::fill_rule_t fill_rule,
                                FillSubsetWorkRoom &workroom,
                                OpaqueFillWorkRoom *output)
{
  using namespace fastuidraw;

  unsigned int idx_chunk, atr_chunk, num_subsets;

  workroom.m_subsets.resize(filled_path.number_subsets());
  num_subsets = select_subsets(filled_path, make_c_array(workroom.m_subsets));
  workroom.m_subsets.resize(num_subsets);

  if (num_subsets == 0)
    {
      return;
    }

  workroom.m_ws.set(filled_path, make_c_array(workroom.m_subsets),
                    CustomFillRuleFunction(fill_rule));

  output->m_attrib_chunks.clear();
  output->m_index_chunks.clear();
  output->m_index_adjusts.clear();
  output->m_chunk_selector.clear();

  idx_chunk = FilledPath::Subset::fill_chunk_from_fill_rule(fill_rule);
  atr_chunk = 0;
  for(unsigned int s : workroom.m_subsets)
    {
      FilledPath::Subset subset(filled_path.subset(s));
      const PainterAttributeData &data(subset.painter_data());

      output->m_attrib_chunks.push_back(data.attribute_data_chunk(atr_chunk));
      output->m_index_chunks.push_back(data.index_data_chunk(idx_chunk));
      output->m_index_adjusts.push_back(data.index_adjust_chunk(idx_chunk));
    }
}

void
PainterPrivate::
fill_path_compute_opaque_chunks(const fastuidraw::FilledPath &filled_path,
                                const fastuidraw::CustomFillRuleBase &fill_rule,
                                FillSubsetWorkRoom &workroom,
                                OpaqueFillWorkRoom *output)
{
  using namespace fastuidraw;

  unsigned int num_subsets;

  workroom.m_subsets.resize(filled_path.number_subsets());
  num_subsets = select_subsets(filled_path, make_c_array(workroom.m_subsets));
  workroom.m_subsets.resize(num_subsets);

  if (num_subsets == 0)
    {
      return;
    }

  workroom.m_ws.set(filled_path, make_c_array(workroom.m_subsets), fill_rule);

  output->m_attrib_chunks.clear();
  output->m_index_chunks.clear();
  output->m_index_adjusts.clear();
  output->m_chunk_selector.clear();
  for(unsigned int s : workroom.m_subsets)
    {
      FilledPath::Subset subset(filled_path.subset(s));
      const PainterAttributeData &data(subset.painter_data());
      c_array<const fastuidraw::PainterAttribute> attrib_chunk;
      c_array<const int> winding_numbers(subset.winding_numbers());
      unsigned int attrib_selector_value;
      bool added_chunk;

      added_chunk = false;
      attrib_selector_value = output->m_attrib_chunks.size();

      for(int winding_number : winding_numbers)
        {
          int chunk;
          c_array<const PainterIndex> index_chunk;

          chunk = FilledPath::Subset::fill_chunk_from_winding_number(winding_number);
          index_chunk = data.index_data_chunk(chunk);
          if (!index_chunk.empty() && workroom.m_ws(winding_number))
            {
              output->m_chunk_selector.push_back(attrib_selector_value);
              output->m_index_chunks.push_back(index_chunk);
              output->m_index_adjusts.push_back(data.index_adjust_chunk(chunk));
              added_chunk = true;
            }
        }

      if (added_chunk)
        {
          attrib_chunk = data.attribute_data_chunk(0);
          output->m_attrib_chunks.push_back(attrib_chunk);
        }
    }
}

template<typename T>
void
PainterPrivate::
fill_path(const fastuidraw::PainterFillShader &shader,
          const fastuidraw::PainterData &draw,
          const fastuidraw::FilledPath &filled_path,
          const T &fill_rule,
          enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality)
{
  using namespace fastuidraw;

  fill_path_compute_opaque_chunks(filled_path, fill_rule,
                                  m_work_room.m_fill_subset,
                                  &m_work_room.m_fill_opaque);

  if (m_work_room.m_fill_opaque.m_index_chunks.empty()
      || m_work_room.m_fill_subset.m_subsets.empty())
    {
      return;
    }

  anti_alias_quality = compute_shader_anti_alias(anti_alias_quality,
                                                 shader.hq_anti_alias_support(),
                                                 shader.fastest_anti_alias_mode());

  if (anti_alias_quality != Painter::shader_anti_alias_none)
    {
      pre_draw_anti_alias_fuzz(filled_path, anti_alias_quality,
                               m_work_room.m_fill_subset,
                               &m_work_room.m_fill_aa_fuzz);
    }
  else
    {
      m_work_room.m_fill_aa_fuzz.m_total_increment_z = 0;
    }

  draw_generic(shader.item_shader(), draw,
               make_c_array(m_work_room.m_fill_opaque.m_attrib_chunks),
               make_c_array(m_work_room.m_fill_opaque.m_index_chunks),
               make_c_array(m_work_room.m_fill_opaque.m_index_adjusts),
               make_c_array(m_work_room.m_fill_opaque.m_chunk_selector),
               m_current_z + m_work_room.m_fill_aa_fuzz.m_total_increment_z);

  if (anti_alias_quality != Painter::shader_anti_alias_none)
    {
      draw_anti_alias_fuzz(shader, draw, anti_alias_quality,
                           m_work_room.m_fill_aa_fuzz);
    }

  m_current_z += m_work_room.m_fill_aa_fuzz.m_total_increment_z;
}

void
PainterPrivate::
fill_rounded_rect(const fastuidraw::PainterFillShader &shader,
                  const fastuidraw::PainterData &draw,
                  const fastuidraw::RoundedRect &R,
                  enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality)
{
  using namespace fastuidraw;

  /* Save our transformation and clipping state */
  clip_rect_state m(m_clip_rect_state);
  RoundedRectTransformations rect_transforms(draw, R);
  int total_incr_z(0);
  Rect interior_rect, r_min, r_max, r_min_extra, r_max_extra;
  float wedge_miny, wedge_maxy;

  wedge_miny = t_max(R.m_corner_radii[Rect::minx_miny_corner].y(), R.m_corner_radii[Rect::maxx_miny_corner].y());
  wedge_maxy = t_max(R.m_corner_radii[Rect::minx_maxy_corner].y(), R.m_corner_radii[Rect::maxx_maxy_corner].y());

  interior_rect.m_min_point = vec2(R.m_min_point.x(), R.m_min_point.y() + wedge_miny);
  interior_rect.m_max_point = vec2(R.m_max_point.x(), R.m_max_point.y() - wedge_maxy);

  r_min.m_min_point = vec2(R.m_min_point.x() + R.m_corner_radii[Rect::minx_miny_corner].x(),
                           R.m_min_point.y());
  r_min.m_max_point = vec2(R.m_max_point.x() - R.m_corner_radii[Rect::maxx_miny_corner].x(),
                           R.m_min_point.y() + wedge_miny);

  r_max.m_min_point = vec2(R.m_min_point.x() + R.m_corner_radii[Rect::minx_maxy_corner].x(),
                           R.m_max_point.y() - wedge_maxy);
  r_max.m_max_point = vec2(R.m_max_point.x() - R.m_corner_radii[Rect::maxx_maxy_corner].x(),
                           R.m_max_point.y());

  anti_alias_quality = compute_shader_anti_alias(anti_alias_quality,
                                                 shader.hq_anti_alias_support(),
                                                 shader.fastest_anti_alias_mode());

  if (anti_alias_quality != Painter::shader_anti_alias_none)
    {
      int incr;

      incr = (anti_alias_quality == Painter::shader_anti_alias_simple) ? 1 : 0;

      m_work_room.m_rounded_rect.m_rect_fuzz_attributes.clear();
      m_work_room.m_rounded_rect.m_rect_fuzz_indices.clear();
      pack_anti_alias_edge(r_min.m_min_point,
                           vec2(r_min.m_max_point.x(), r_min.m_min_point.y()),
                           total_incr_z,
                           m_work_room.m_rounded_rect.m_rect_fuzz_attributes,
                           m_work_room.m_rounded_rect.m_rect_fuzz_indices);
      total_incr_z += incr;

      pack_anti_alias_edge(vec2(r_max.m_min_point.x(), r_max.m_max_point.y()),
                           r_max.m_max_point,
                           total_incr_z,
                           m_work_room.m_rounded_rect.m_rect_fuzz_attributes,
                           m_work_room.m_rounded_rect.m_rect_fuzz_indices);
      total_incr_z += incr;

      pack_anti_alias_edge(vec2(R.m_min_point.x(), R.m_min_point.y() + R.m_corner_radii[Rect::minx_miny_corner].y()),
                           vec2(R.m_min_point.x(), R.m_max_point.y() - R.m_corner_radii[Rect::minx_maxy_corner].y()),
                           total_incr_z,
                           m_work_room.m_rounded_rect.m_rect_fuzz_attributes,
                           m_work_room.m_rounded_rect.m_rect_fuzz_indices);
      total_incr_z += incr;

      pack_anti_alias_edge(vec2(R.m_max_point.x(), R.m_min_point.y() + R.m_corner_radii[Rect::maxx_miny_corner].y()),
                           vec2(R.m_max_point.x(), R.m_max_point.y() - R.m_corner_radii[Rect::maxx_maxy_corner].y()),
                           total_incr_z,
                           m_work_room.m_rounded_rect.m_rect_fuzz_attributes,
                           m_work_room.m_rounded_rect.m_rect_fuzz_indices);
      total_incr_z += incr;

      if (anti_alias_quality != Painter::shader_anti_alias_simple)
        {
          /* one for the above edges */
          total_incr_z = 1;
        }
    }

  for (int i = 0; i < 4; ++i)
    {
      translate(rect_transforms.m_translates[i]);
      shear(rect_transforms.m_shears[i].x(), rect_transforms.m_shears[i].y());
      m_work_room.m_rounded_rect.m_filled_paths[i] = &select_filled_path(m_rounded_corner_path);
      fill_path_compute_opaque_chunks(*m_work_room.m_rounded_rect.m_filled_paths[i],
                                      Painter::nonzero_fill_rule,
                                      m_work_room.m_rounded_rect.m_subsets[i],
                                      &m_work_room.m_rounded_rect.m_opaque_fill[i]);

      if (anti_alias_quality != Painter::shader_anti_alias_none)
        {
          pre_draw_anti_alias_fuzz(*m_work_room.m_rounded_rect.m_filled_paths[i],
                                   anti_alias_quality,
                                   m_work_room.m_rounded_rect.m_subsets[i],
                                   &m_work_room.m_rounded_rect.m_aa_fuzz[i]);
          total_incr_z += m_work_room.m_rounded_rect.m_aa_fuzz[i].m_total_increment_z;
        }
      else
        {
          m_work_room.m_rounded_rect.m_aa_fuzz[i].m_total_increment_z = 0;
        }
      m_clip_rect_state = m;
    }

  fill_rect(shader, draw, interior_rect, Painter::shader_anti_alias_none, m_current_z + total_incr_z);
  fill_rect(shader, draw, r_min, Painter::shader_anti_alias_none, m_current_z + total_incr_z);
  fill_rect(shader, draw, r_max, Painter::shader_anti_alias_none, m_current_z + total_incr_z);

  if (R.m_corner_radii[Rect::minx_miny_corner].y() > R.m_corner_radii[Rect::maxx_miny_corner].y())
    {
      Rect r;

      r.m_min_point.x() = r_min.m_max_point.x();
      r.m_min_point.y() = R.m_min_point.y() + R.m_corner_radii[Rect::maxx_miny_corner].y();
      r.m_max_point.x() = R.m_max_point.x();
      r.m_max_point.y() = r_min.m_max_point.y();
      fill_rect(shader, draw, r, Painter::shader_anti_alias_none, m_current_z + total_incr_z);
    }
  else if (R.m_corner_radii[Rect::minx_miny_corner].y() < R.m_corner_radii[Rect::maxx_miny_corner].y())
    {
      Rect r;

      r.m_min_point.x() = R.m_min_point.x();
      r.m_min_point.y() = R.m_min_point.y() + R.m_corner_radii[Rect::minx_miny_corner].y();
      r.m_max_point.x() = R.m_min_point.x() + R.m_corner_radii[Rect::minx_miny_corner].x();
      r.m_max_point.y() = r_min.m_max_point.y();
      fill_rect(shader, draw, r, Painter::shader_anti_alias_none, m_current_z + total_incr_z);
    }

  if (R.m_corner_radii[Rect::minx_maxy_corner].y() > R.m_corner_radii[Rect::maxx_maxy_corner].y())
    {
      Rect r;

      r.m_min_point.x() = r_max.m_max_point.x();
      r.m_min_point.y() = r_max.m_min_point.y();
      r.m_max_point.x() = R.m_max_point.x();
      r.m_max_point.y() = R.m_max_point.y() - R.m_corner_radii[Rect::maxx_maxy_corner].y();
      fill_rect(shader, draw, r, Painter::shader_anti_alias_none, m_current_z + total_incr_z);
    }
  else if (R.m_corner_radii[Rect::minx_maxy_corner].y() < R.m_corner_radii[Rect::maxx_maxy_corner].y())
    {
      Rect r;

      r.m_min_point.x() = R.m_min_point.x();
      r.m_min_point.y() = r_max.m_min_point.y();
      r.m_max_point.x() = R.m_min_point.x() + R.m_corner_radii[Rect::minx_maxy_corner].x();
      r.m_max_point.y() = R.m_max_point.y() - R.m_corner_radii[Rect::minx_maxy_corner].y();
      fill_rect(shader, draw, r, Painter::shader_anti_alias_none, m_current_z + total_incr_z);
    }

  for (int i = 0; i < 4; ++i)
    {
      translate(rect_transforms.m_translates[i]);
      shear(rect_transforms.m_shears[i].x(), rect_transforms.m_shears[i].y());
      draw_generic(shader.item_shader(), *rect_transforms.m_use[i],
                   make_c_array(m_work_room.m_rounded_rect.m_opaque_fill[i].m_attrib_chunks),
                   make_c_array(m_work_room.m_rounded_rect.m_opaque_fill[i].m_index_chunks),
                   make_c_array(m_work_room.m_rounded_rect.m_opaque_fill[i].m_index_adjusts),
                   make_c_array(m_work_room.m_rounded_rect.m_opaque_fill[i].m_chunk_selector),
                   m_current_z + total_incr_z);
      m_clip_rect_state = m;
    }

  if (anti_alias_quality != Painter::shader_anti_alias_none)
    {
      int incr_z(total_incr_z);

      if (anti_alias_quality == Painter::shader_anti_alias_simple)
        {
          incr_z -= 4;
          draw_generic(shader.aa_fuzz_shader(), draw,
                       make_c_array(m_work_room.m_rounded_rect.m_rect_fuzz_attributes),
                       make_c_array(m_work_room.m_rounded_rect.m_rect_fuzz_indices),
                       0, m_current_z + incr_z);
        }
      else
        {
          incr_z -= 1;
          draw_generic(shader.aa_fuzz_hq_shader_pass1(), draw,
                       make_c_array(m_work_room.m_rounded_rect.m_rect_fuzz_attributes),
                       make_c_array(m_work_room.m_rounded_rect.m_rect_fuzz_indices),
                       0, m_current_z + incr_z);
          packer()->draw_break(shader.aa_fuzz_hq_action_pass1());

          draw_generic(shader.aa_fuzz_hq_shader_pass2(), draw,
                       make_c_array(m_work_room.m_rounded_rect.m_rect_fuzz_attributes),
                       make_c_array(m_work_room.m_rounded_rect.m_rect_fuzz_indices),
                       0, m_current_z + incr_z);
          packer()->draw_break(shader.aa_fuzz_hq_action_pass2());
        }

      for (int i = 0; i < 4; ++i)
        {
          incr_z -= m_work_room.m_rounded_rect.m_aa_fuzz[i].m_total_increment_z;
          translate(rect_transforms.m_translates[i]);
          shear(rect_transforms.m_shears[i].x(), rect_transforms.m_shears[i].y());
          draw_anti_alias_fuzz(shader, *rect_transforms.m_use[i], anti_alias_quality,
                               m_work_room.m_rounded_rect.m_aa_fuzz[i],
                               m_current_z + incr_z);
          m_clip_rect_state = m;
        }
    }
  m_current_z += total_incr_z;
}

void
PainterPrivate::
ready_non_aa_polygon_attribs(fastuidraw::c_array<const fastuidraw::vec2> pts)
{
  using namespace fastuidraw;

  m_work_room.m_polygon.m_attribs.resize(pts.size());
  for(unsigned int i = 0; i < pts.size(); ++i)
    {
      m_work_room.m_polygon.m_attribs[i].m_attrib0 = pack_vec4(pts[i].x(), pts[i].y(), 0.0f, 0.0f);
      m_work_room.m_polygon.m_attribs[i].m_attrib1 = uvec4(0u, 0u, 0u, 0u);
      m_work_room.m_polygon.m_attribs[i].m_attrib2 = uvec4(0u, 0u, 0u, 0u);
    }

  m_work_room.m_polygon.m_indices.clear();
  m_work_room.m_polygon.m_indices.reserve((pts.size() - 2) * 3);
  for(unsigned int i = 2; i < pts.size(); ++i)
    {
      m_work_room.m_polygon.m_indices.push_back(0);
      m_work_room.m_polygon.m_indices.push_back(i - 1);
      m_work_room.m_polygon.m_indices.push_back(i);
    }
}

void
PainterPrivate::
ready_aa_polygon_attribs(fastuidraw::c_array<const fastuidraw::vec2> pts,
                         enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality)
{
  using namespace fastuidraw;

  m_work_room.m_polygon.m_fuzz_increment_z = 0;
  if (anti_alias_quality != Painter::shader_anti_alias_none)
    {
      int mult;

      mult = (anti_alias_quality == Painter::shader_anti_alias_simple) ? 1 : 0;
      m_work_room.m_polygon.m_aa_fuzz_attribs.clear();
      m_work_room.m_polygon.m_aa_fuzz_indices.clear();

      for (unsigned int i = 0; i < pts.size(); ++i, m_work_room.m_polygon.m_fuzz_increment_z += mult)
        {
          unsigned int next_i, next_next_i;
          unsigned int center, current_start, current_outer;
          unsigned int next_start, next_outer;
          bool is_closing_edge;
          vec2 t, n;
          float d, sd;

          next_i = (i + 1u == pts.size()) ? 0 : i + 1u;
          next_next_i = (next_i + 1u == pts.size()) ? 0 : next_i + 1u;
          is_closing_edge = (i + 1u == pts.size());

          current_start = m_work_room.m_polygon.m_aa_fuzz_attribs.size();
          pack_anti_alias_edge(pts[i], pts[next_i],
                               m_work_room.m_polygon.m_fuzz_increment_z,
                               m_work_room.m_polygon.m_aa_fuzz_attribs,
                               m_work_room.m_polygon.m_aa_fuzz_indices);
          next_start = m_work_room.m_polygon.m_aa_fuzz_attribs.size();

          t = pts[next_next_i] - pts[next_i];
          t /= t.magnitude();
          n = vec2(-t.y(), t.x());
          d = dot(pts[next_i] - pts[i], -n);
          sd = t_sign(d);

          center = current_start + 4;
          if (is_closing_edge)
            {
              PainterAttribute A;

              next_outer = m_work_room.m_polygon.m_aa_fuzz_attribs.size();
              A = anti_alias_edge_attribute(pts[next_i],
                                            FilledPath::Subset::aa_fuzz_type_on_boundary,
                                            -sd * n, m_work_room.m_polygon.m_fuzz_increment_z);
              m_work_room.m_polygon.m_aa_fuzz_attribs.push_back(A);
            }

          if (d < 0.0)
            {
              current_outer = current_start + 5u;
              if (!is_closing_edge)
                {
                  next_outer = next_start + 2u;
                }
            }
          else
            {
              current_outer = current_start + 3u;
              if (!is_closing_edge)
                {
                  next_outer = next_start + 0u;
                }
            }

          m_work_room.m_polygon.m_aa_fuzz_indices.push_back(current_outer);
          m_work_room.m_polygon.m_aa_fuzz_indices.push_back(next_outer);
          m_work_room.m_polygon.m_aa_fuzz_indices.push_back(center);
        }

      if (anti_alias_quality == Painter::shader_anti_alias_high_quality)
        {
          m_work_room.m_polygon.m_fuzz_increment_z = 1u;
        }
    }
}

int
PainterPrivate::
fill_convex_polygon(bool allow_sw_clipping,
                    const fastuidraw::PainterFillShader &shader,
                    const fastuidraw::PainterData &draw,
                    fastuidraw::c_array<const fastuidraw::vec2> pts,
                    enum fastuidraw::Painter::shader_anti_alias_t anti_alias_quality,
                    int z)
{
  using namespace fastuidraw;

  if (pts.size() < 3 || m_clip_rect_state.m_all_content_culled)
    {
      return 0;
    }

  if (allow_sw_clipping && !packer()->hints().clipping_via_hw_clip_planes())
    {
      m_clip_rect_state.clip_polygon(pts, m_work_room.m_polygon.m_pts,
                                     m_work_room.m_clipper.m_vec2s[0]);
      pts = make_c_array(m_work_room.m_polygon.m_pts);
      if (pts.size() < 3)
        {
          return 0;
        }
    }

  anti_alias_quality = compute_shader_anti_alias(anti_alias_quality,
                                                 shader.hq_anti_alias_support(),
                                                 shader.fastest_anti_alias_mode());
  ready_aa_polygon_attribs(pts, anti_alias_quality);
  ready_non_aa_polygon_attribs(pts);

  draw_generic(shader.item_shader(), draw,
               make_c_array(m_work_room.m_polygon.m_attribs),
               make_c_array(m_work_room.m_polygon.m_indices),
               0,
               m_work_room.m_polygon.m_fuzz_increment_z + z);

  if (anti_alias_quality != Painter::shader_anti_alias_none)
    {
      if (anti_alias_quality == Painter::shader_anti_alias_simple)
        {
          draw_generic(shader.aa_fuzz_shader(), draw,
                       make_c_array(m_work_room.m_polygon.m_aa_fuzz_attribs),
                       make_c_array(m_work_room.m_polygon.m_aa_fuzz_indices),
                       0, z);
        }
      else
        {
          draw_generic(shader.aa_fuzz_hq_shader_pass1(), draw,
                       make_c_array(m_work_room.m_polygon.m_aa_fuzz_attribs),
                       make_c_array(m_work_room.m_polygon.m_aa_fuzz_indices),
                       0, z);
          packer()->draw_break(shader.aa_fuzz_hq_action_pass1());

          draw_generic(shader.aa_fuzz_hq_shader_pass2(), draw,
                       make_c_array(m_work_room.m_polygon.m_aa_fuzz_attribs),
                       make_c_array(m_work_room.m_polygon.m_aa_fuzz_indices),
                       0, z);
          packer()->draw_break(shader.aa_fuzz_hq_action_pass2());
        }
    }

  return m_work_room.m_polygon.m_fuzz_increment_z;
}

void
PainterPrivate::
draw_half_plane_complement(const fastuidraw::PainterFillShader &shader,
                           const fastuidraw::PainterData &draw,
                           const fastuidraw::vec3 &plane)
{
  using namespace fastuidraw;
  /* Basic idea:
   *   (1) Because of that begin_layer() renders to another
   *       buffer, but potentially translated (by up to 2 units)
   *       we need to extend the normalized device plane to
   *       [-3, 3]x[-3, 3] when drawing the occluder
   *   (2) The occluder is simply the plane passed intersected
   *       the [-3, 3] square taking the other points on the
   *       correct side.
   */
  if (t_abs(plane.x()) > t_abs(plane.y()))
    {
      float a, b, c, d;
      /* a so that A * a + B * -3 + C = 0 -> a = (+3B - C) / A
       * b so that A * a + B * +3 + C = 0 -> b = (-3B - C) / A
       */
      a = (+3.0f * plane.y() - plane.z()) / plane.x();
      b = (-3.0f * plane.y() - plane.z()) / plane.x();

      /* the two points are then (a, -3) and (b, 3). Grab
       * (c, -3) and (d, 3) so that they are on the correct side
       * of the half plane
       */
      if (plane.x() > 0.0f)
        {
          /* increasing x then make the plane more positive,
           * and we want the negative side, so take c and
           * d to left of a and b.
           */
          c = t_min(-3.0f, a);
          d = t_min(-3.0f, b);
        }
      else
        {
          c = t_max(3.0f, a);
          d = t_max(3.0f, b);
        }
      /* the 4 points of the polygon are then
       * (a, -3), (c, -3), (d, 3), (b, 3).
       */
      fill_convex_polygon(false, shader, draw,
                          vecN<vec2, 4>(vec2(a, -3.0f),
                                        vec2(c, -3.0f),
                                        vec2(d, +3.0f),
                                        vec2(b, +3.0f)),
                          Painter::shader_anti_alias_none);
    }
  else if (t_abs(plane.y()) > 0.0f)
    {
      float a, b, c, d;
      a = (+3.0f * plane.x() - plane.z()) / plane.y();
      b = (-3.0f * plane.x() - plane.z()) / plane.y();

      if (plane.y() > 0.0f)
        {
          c = t_min(-3.0f, a);
          d = t_min(-3.0f, b);
        }
      else
        {
          c = t_max(3.0f, a);
          d = t_max(3.0f, b);
        }

      fill_convex_polygon(false, shader, draw,
                          vecN<vec2, 4>(vec2(-3.0f, a),
                                        vec2(-3.0f, c),
                                        vec2(+3.0f, d),
                                        vec2(+3.0f, b)),
                          Painter::shader_anti_alias_none);

    }
  else if (plane.z() <= 0.0f)
    {
      /* complement of half plane covers entire [-3, 3] x [-3, 3]
       */
      fill_convex_polygon(false, shader, draw,
                          vecN<vec2, 4>(vec2(-3.0f, -3.0f),
                                        vec2(-3.0f, +3.0f),
                                        vec2(+3.0f, +3.0f),
                                        vec2(+3.0f, -3.0f)),
                          Painter::shader_anti_alias_none);
    }
}

fastuidraw::GlyphRenderer
PainterPrivate::
compute_glyph_renderer(float pixel_size,
                       const fastuidraw::Painter::GlyphRendererChooser &chooser)
{
  if (m_clip_rect_state.matrix_type() == perspective_matrix)
    {
      return chooser.choose_glyph_render(pixel_size,
                                         m_clip_rect_state.item_matrix());
    }
  else
    {
      fastuidraw::vec2 sing(m_clip_rect_state.item_matrix_singular_values());
      return chooser.choose_glyph_render(pixel_size,
                                         m_clip_rect_state.item_matrix(),
                                         sing[0],
                                         sing[1]);
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

const fastuidraw::Painter::GlyphRendererChooser&
fastuidraw::Painter::
default_glyph_renderer_chooser(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_default_glyph_renderer_chooser;
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
      const float3x3 &initial_transformation,
      bool clear_color_buffer)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  image_atlas()->lock_resources();
  colorstop_atlas()->lock_resources();
  glyph_atlas()->lock_resources();

  d->m_viewport = surface->viewport();
  d->m_transparency_stack_entry_factory.begin(surface->dimensions(), d->m_viewport);
  d->m_root_packer->begin(surface, clear_color_buffer);
  std::fill(d->m_stats.begin(), d->m_stats.end(), 0u);
  d->m_stats[Painter::num_render_targets] = 1;
  d->m_viewport_dimensions = vec2(d->m_viewport.m_dimensions);
  d->m_viewport_dimensions.x() = t_max(1.0f, d->m_viewport_dimensions.x());
  d->m_viewport_dimensions.y() = t_max(1.0f, d->m_viewport_dimensions.y());
  d->m_one_pixel_width = 1.0f / d->m_viewport_dimensions;

  d->m_current_z = 1;
  d->m_draw_data_added_count = 0;
  d->m_clip_rect_state.reset(d->m_viewport, surface->dimensions());
  d->m_clip_store.reset(d->m_clip_rect_state.clip_equations().m_clip_equations);
  composite_shader(composite_porter_duff_src_over);
  blend_shader(blend_w3c_normal);

  Rect ncR;
  d->m_viewport.compute_normalized_clip_rect(surface->dimensions(), &ncR);
  clip_in_rect(ncR);
  concat(initial_transformation);
}

void
fastuidraw::Painter::
begin(const reference_counted_ptr<PainterBackend::Surface> &surface,
      enum screen_orientation orientation, bool clear_color_buffer)
{
  float y1, y2;
  const PainterBackend::Surface::Viewport &vwp(surface->viewport());

  if (orientation == Painter::y_increases_downwards)
    {
      y1 = vwp.m_dimensions.y();
      y2 = 0;
    }
  else
    {
      y1 = 0;
      y2 = vwp.m_dimensions.y();
    }
  float_orthogonal_projection_params ortho(0, vwp.m_dimensions.x(), y1, y2);
  begin(surface, float3x3(ortho), clear_color_buffer);
}

fastuidraw::c_array<const fastuidraw::PainterBackend::Surface* const>
fastuidraw::Painter::
end(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  /* pop the transparency stack until it is empty */
  while (!d->m_transparency_stack.empty())
    {
      end_layer();
    }

  /* pop m_clip_stack to perform necessary writes */
  while (!d->m_occluder_stack.empty())
    {
      d->m_occluder_stack.back().on_pop(d);
      d->m_occluder_stack.pop_back();
    }
  /* clear state stack as well. */
  d->m_clip_store.clear();
  d->m_state_stack.clear();

  /* issue the PainterPacker::end() to send the commands to the GPU */
  d->m_transparency_stack_entry_factory.end();
  d->m_root_packer->end();

  /* unlock resources after the commands are sent to the GPU */
  image_atlas()->unlock_resources();
  colorstop_atlas()->unlock_resources();
  glyph_atlas()->unlock_resources();

  return make_c_array(d->m_transparency_stack_entry_factory.active_surfaces());
}

enum fastuidraw::return_code
fastuidraw::Painter::
flush(void)
{
  return flush(surface());
}

enum fastuidraw::return_code
fastuidraw::Painter::
flush(const reference_counted_ptr<PainterBackend::Surface> &new_surface)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (!surface() || !new_surface)
    {
      /* not actively drawing */
      return routine_fail;
    }

  if (!d->m_transparency_stack.empty())
    {
      return routine_fail;
    }

  /* TODO: correclty deal with occluders. Instead of
   * failing if any element of the current state stack
   * has m_occluder_stack_position as non-zero to instead
   * have the occlusion stack entries have an object that
   * can issue the add of the attribute/index data
   * itself and using that to issue the draws of the
   * occluders.
   */
  for (const state_stack_entry &e : d->m_state_stack)
    {
      if (e.m_occluder_stack_position != 0)
        {
          return routine_fail;
        }
    }

  /* pop all entries of the occluder stack */
  while (!d->m_occluder_stack.empty())
    {
      d->m_occluder_stack.back().on_pop(d);
      d->m_occluder_stack.pop_back();
    }

  /* issue the PainterPacker::end() to send the commands to the GPU */
  d->m_transparency_stack_entry_factory.end();

  /* start the transparency_stack_entry up again */
  d->m_transparency_stack_entry_factory.begin(new_surface->dimensions(), d->m_viewport);
  if (new_surface == surface())
    {
      bool clear_z;
      const int clear_depth_thresh(d->packer()->hints().max_z());

      clear_z = (d->m_current_z > clear_depth_thresh);
      d->m_root_packer->flush(clear_z);
      if (clear_z)
        {
          d->m_current_z = 1;
        }
      const state_stack_entry &st(d->m_state_stack.back());
      d->packer()->composite_shader(st.m_composite, st.m_composite_mode);
      d->packer()->blend_shader(st.m_blend);
    }
  else
    {
      /* get the Image handle to the old-surface */
      reference_counted_ptr<PainterBackend::Surface> old_surface(surface());
      reference_counted_ptr<const Image> image;

      image = surface()->image(d->m_backend->image_atlas());
      new_surface->viewport(d->m_viewport);

      d->m_root_packer->end();
      d->m_current_z = 1;
      d->m_root_packer->begin(new_surface, true);

      /* blit the old surface to the surface */
      save();
        {
          const PainterBackend::Surface::Viewport &vwp(new_surface->viewport());
          PainterBrush brush;

          brush.sub_image(image, uvec2(vwp.m_origin), uvec2(vwp.m_dimensions));
          transformation(float_orthogonal_projection_params(0, vwp.m_dimensions.x(), 0, vwp.m_dimensions.y()));
          composite_shader(composite_porter_duff_src);
          blend_shader(blend_w3c_normal);
          fill_rect(PainterData(&brush),
                    Rect().min_point(0.0f, 0.0f).size(vec2(vwp.m_dimensions)),
                    shader_anti_alias_none);
        }
      restore();
    }
  return routine_success;
}

unsigned int
fastuidraw::Painter::
draw_data_added_count(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_draw_data_added_count;
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend::Surface>&
fastuidraw::Painter::
surface(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_root_packer->surface();
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
             c_array<const c_array<const PainterAttribute> > attrib_chunks,
             c_array<const c_array<const PainterIndex> > index_chunks,
             c_array<const int> index_adjusts)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  if (!d->m_clip_rect_state.m_all_content_culled)
    {
      d->draw_generic(shader, draw, attrib_chunks, index_chunks,
                      index_adjusts, c_array<const unsigned int>(),
                      d->m_current_z);
    }
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
             c_array<const c_array<const PainterAttribute> > attrib_chunks,
             c_array<const c_array<const PainterIndex> > index_chunks,
             c_array<const int> index_adjusts,
             c_array<const unsigned int> attrib_chunk_selector)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  if (!d->m_clip_rect_state.m_all_content_culled)
    {
      d->draw_generic(shader, draw, attrib_chunks, index_chunks,
                      index_adjusts, attrib_chunk_selector,
                      d->m_current_z);
    }
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
             const PainterAttributeWriter &src)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  if (!d->m_clip_rect_state.m_all_content_culled)
    {
      d->draw_generic(shader, draw, src, d->m_current_z);
    }
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
             const PainterData &draw,
             c_array<const PainterAttribute> attrib_chunk,
             c_array<const PainterIndex> index_chunk,
             range_type<int> z_range,
             int index_adjust)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  if (!d->m_clip_rect_state.m_all_content_culled)
    {
      d->m_current_z -= z_range.m_begin;
      draw_generic(shader, draw, attrib_chunk, index_chunk, index_adjust);
      d->m_current_z += z_range.m_end;
    }
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
             const PainterData &draw,
             const PainterAttributeWriter &src,
             range_type<int> z_range)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  if (!d->m_clip_rect_state.m_all_content_culled)
    {
      d->m_current_z -= z_range.m_begin;
      draw_generic(shader, draw, src);
      d->m_current_z += z_range.m_end;
    }
}

void
fastuidraw::Painter::
draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
             const PainterData &draw,
             c_array<const c_array<const PainterAttribute> > attrib_chunks,
             c_array<const c_array<const PainterIndex> > index_chunks,
             c_array<const range_type<int> > z_ranges,
             c_array<const int> index_adjusts)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  if (d->m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  FASTUIDRAWassert(attrib_chunks.size() == index_chunks.size());
  FASTUIDRAWassert(attrib_chunks.size() == z_ranges.size());
  FASTUIDRAWassert(attrib_chunks.size() == index_adjusts.size() || index_adjusts.empty());

  int total_z_increment(0);
  if (index_adjusts.empty())
    {
      d->m_work_room.m_generic_layered.m_empty_adjusts.resize(attrib_chunks.size(), 0);
      index_adjusts = make_c_array(d->m_work_room.m_generic_layered.m_empty_adjusts);
    }

  d->m_work_room.m_generic_layered.m_z_increments.clear();
  d->m_work_room.m_generic_layered.m_start_zs.clear();
  for (const auto &R : z_ranges)
    {
      int diff(R.difference());
      total_z_increment += diff;
      d->m_work_room.m_generic_layered.m_z_increments.push_back(diff);
      d->m_work_room.m_generic_layered.m_start_zs.push_back(R.m_begin);
    }

  d->draw_generic_z_layered(shader, draw,
                            make_c_array(d->m_work_room.m_generic_layered.m_z_increments),
                            total_z_increment,
                            attrib_chunks,
                            index_chunks,
                            index_adjusts,
                            make_c_array(d->m_work_room.m_generic_layered.m_start_zs),
                            d->m_current_z);
  d->m_current_z += total_z_increment;
}

void
fastuidraw::Painter::
queue_action(const reference_counted_ptr<const PainterDraw::Action> &action)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->packer()->draw_break(action);
}

void
fastuidraw::Painter::
fill_convex_polygon(const PainterFillShader &shader,
                    const PainterData &draw, c_array<const vec2> pts,
                    enum shader_anti_alias_t anti_alias_quality)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_current_z += d->fill_convex_polygon(shader, draw, pts, anti_alias_quality);
}

void
fastuidraw::Painter::
fill_convex_polygon(const PainterData &draw, c_array<const vec2> pts,
                    enum shader_anti_alias_t anti_alias_quality)
{
  fill_convex_polygon(default_shaders().fill_shader(), draw, pts, anti_alias_quality);
}

void
fastuidraw::Painter::
fill_quad(const PainterFillShader &shader, const PainterData &draw,
          const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3,
          enum shader_anti_alias_t anti_alias_quality)
{
  vecN<vec2, 4> pts;
  pts[0] = p0;
  pts[1] = p1;
  pts[2] = p2;
  pts[3] = p3;
  fill_convex_polygon(shader, draw, pts, anti_alias_quality);
}

void
fastuidraw::Painter::
fill_quad(const PainterData &draw,
          const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3,
          enum shader_anti_alias_t anti_alias_quality)
{
  fill_quad(default_shaders().fill_shader(), draw, p0, p1, p2, p3,
            anti_alias_quality);
}

void
fastuidraw::Painter::
fill_rect(const PainterFillShader &shader,
          const PainterData &draw, const Rect &rect,
          enum shader_anti_alias_t anti_alias_quality)
{
  vecN<vec2, 4> pts;

  pts[0] = vec2(rect.m_min_point.x(), rect.m_min_point.y());
  pts[1] = vec2(rect.m_min_point.x(), rect.m_max_point.y());
  pts[2] = vec2(rect.m_max_point.x(), rect.m_max_point.y());
  pts[3] = vec2(rect.m_max_point.x(), rect.m_min_point.y());
  fill_convex_polygon(shader, draw, pts, anti_alias_quality);
}

void
fastuidraw::Painter::
fill_rect(const PainterData &draw, const Rect &rect,
          enum shader_anti_alias_t anti_alias_quality)
{
  fill_rect(default_shaders().fill_shader(), draw, rect, anti_alias_quality);
}

void
fastuidraw::Painter::
fill_rounded_rect(const PainterFillShader &shader, const PainterData &draw,
                  const RoundedRect &R,
                  enum shader_anti_alias_t anti_alias_quality)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped, skip the draw */
      return;
    }
  d->fill_rounded_rect(shader, draw, R, anti_alias_quality);
}

void
fastuidraw::Painter::
fill_rounded_rect(const PainterData &draw, const RoundedRect &R,
                  enum shader_anti_alias_t anti_alias_quality)
{
  fill_rounded_rect(default_shaders().fill_shader(), draw, R, anti_alias_quality);
}

float
fastuidraw::Painter::
compute_path_thresh(const fastuidraw::Path &path)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->compute_path_thresh(path);
}

float
fastuidraw::Painter::
compute_path_thresh(const Path &path,
                    const PainterShaderData::DataBase *shader_data,
                    const reference_counted_ptr<const StrokingDataSelectorBase> &selector,
                    float *thresh)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->compute_path_thresh(path, shader_data, selector, *thresh);
}

unsigned int
fastuidraw::Painter::
select_subsets(const FilledPath &path, c_array<unsigned int> dst)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->select_subsets(path, dst);
}

unsigned int
fastuidraw::Painter::
select_subsets(const StrokedPath &path,
               float pixels_additional_room,
               float item_space_additional_room,
               c_array<unsigned int> dst)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->select_subsets(path, pixels_additional_room,
                           item_space_additional_room, dst);
}

void
fastuidraw::Painter::
select_chunks(const StrokedCapsJoins &caps_joins,
              float pixels_additional_room,
              float item_space_additional_room,
              bool select_joins_for_miter_style,
              StrokedCapsJoins::ChunkSet *dst)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->select_chunks(caps_joins,
                   pixels_additional_room,
                   item_space_additional_room,
                   select_joins_for_miter_style,
                   dst);
}

void
fastuidraw::Painter::
stroke_path(const PainterStrokeShader &shader, const PainterData &draw,
            const StrokedPath &path, float thresh,
            const StrokingStyle &stroke_style,
            enum shader_anti_alias_t anti_alias_quality)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  FASTUIDRAWassert(0 <= stroke_style.m_cap_style && stroke_style.m_cap_style < number_cap_styles);
  FASTUIDRAWassert(0 <= stroke_style.m_join_style && stroke_style.m_join_style < number_join_styles);
  d->stroke_path_common(shader, draw, path, thresh,
                        stroke_style.m_cap_style,
                        stroke_style.m_join_style,
                        anti_alias_quality);
}

void
fastuidraw::Painter::
stroke_path(const PainterStrokeShader &shader, const PainterData &draw, const Path &path,
            const StrokingStyle &stroke_style,
            enum shader_anti_alias_t anti_alias_quality,
            enum stroking_method_t stroking_method)
{
  PainterPrivate *d;
  const StrokedPath *stroked_path;
  float thresh;

  d = static_cast<PainterPrivate*>(m_d);
  stroked_path = d->select_stroked_path(path, shader, draw,
                                        anti_alias_quality,
                                        stroking_method, thresh);
  if (stroked_path)
    {
      stroke_path(shader, draw, *stroked_path, thresh, stroke_style,
                  anti_alias_quality);
    }
}

void
fastuidraw::Painter::
stroke_path(const PainterData &draw, const Path &path,
            const StrokingStyle &stroke_style,
            enum shader_anti_alias_t anti_alias_quality,
            enum stroking_method_t stroking_method)
{
  stroke_path(default_shaders().stroke_shader(), draw, path,
              stroke_style, anti_alias_quality, stroking_method);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw,
                   const StrokedPath &path, float thresh,
                   const StrokingStyle &stroke_style,
                   enum shader_anti_alias_t anti_alias_quality)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  FASTUIDRAWassert(0 <= stroke_style.m_cap_style && stroke_style.m_cap_style < number_cap_styles);
  FASTUIDRAWassert(0 <= stroke_style.m_join_style && stroke_style.m_join_style < number_join_styles);
  d->stroke_path_common(shader.shader(stroke_style.m_cap_style), draw,
                        path, thresh,
                        number_cap_styles,
                        stroke_style.m_join_style,
                        anti_alias_quality);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw, const Path &path,
                   const StrokingStyle &stroke_style,
                   enum shader_anti_alias_t anti_alias_quality,
                   enum stroking_method_t stroking_method)
{
  PainterPrivate *d;
  const StrokedPath *stroked_path;
  float thresh;

  d = static_cast<PainterPrivate*>(m_d);
  stroked_path = d->select_stroked_path(path, shader.shader(stroke_style.m_cap_style), draw,
                                        anti_alias_quality,
                                        stroking_method, thresh);
  stroke_dashed_path(shader, draw, *stroked_path, thresh,
                     stroke_style, anti_alias_quality);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterData &draw, const Path &path,
                   const StrokingStyle &stroke_style,
                   enum shader_anti_alias_t anti_alias_quality,
                   enum stroking_method_t stroking_method)
{
  stroke_dashed_path(default_shaders().dashed_stroke_shader(), draw, path,
                     stroke_style, anti_alias_quality, stroking_method);
}

void
fastuidraw::Painter::
stroke_line_strip(const PainterStrokeShader &shader, const PainterData &draw,
                  c_array<const vec2> line_strip,
                  const StrokingStyle &stroke_style,
                  enum shader_anti_alias_t anti_alias_quality)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  FASTUIDRAWassert(0 <= stroke_style.m_cap_style && stroke_style.m_cap_style < number_cap_styles);
  FASTUIDRAWassert(0 <= stroke_style.m_join_style && stroke_style.m_join_style < number_join_styles);
  d->stroke_path_common(shader, draw, line_strip,
                        stroke_style.m_cap_style,
                        stroke_style.m_join_style,
                        anti_alias_quality);
}

void
fastuidraw::Painter::
stroke_line_strip(const PainterData &draw,
                  c_array<const vec2> line_strip,
                  const StrokingStyle &stroke_style,
                  enum shader_anti_alias_t anti_alias_quality)
{
  stroke_line_strip(default_shaders().stroke_shader(), draw,
                    line_strip, stroke_style, anti_alias_quality);
}

void
fastuidraw::Painter::
stroke_dashed_line_strip(const PainterDashedStrokeShaderSet &shader,
                         const PainterData &draw,
                         c_array<const vec2> line_strip,
                         const StrokingStyle &stroke_style,
                         enum shader_anti_alias_t anti_alias_quality)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  FASTUIDRAWassert(0 <= stroke_style.m_cap_style && stroke_style.m_cap_style < number_cap_styles);
  FASTUIDRAWassert(0 <= stroke_style.m_join_style && stroke_style.m_join_style < number_join_styles);
  d->stroke_path_common(shader.shader(stroke_style.m_cap_style),
                        draw, line_strip,
                        stroke_style.m_cap_style,
                        stroke_style.m_join_style,
                        anti_alias_quality);
}

void
fastuidraw::Painter::
stroke_dashed_line_strip(const PainterData &draw,
                         c_array<const vec2> line_strip,
                         const StrokingStyle &stroke_style,
                         enum shader_anti_alias_t anti_alias_quality)
{
  stroke_dashed_line_strip(default_shaders().dashed_stroke_shader(), draw,
                           line_strip, stroke_style, anti_alias_quality);
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const FilledPath &filled_path, enum fill_rule_t fill_rule,
          enum shader_anti_alias_t anti_alias_quality)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->fill_path(shader, draw, filled_path, fill_rule,
               anti_alias_quality);
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const Path &path, enum fill_rule_t fill_rule,
          enum shader_anti_alias_t anti_alias_quality)
{
  PainterPrivate *d;

  d = static_cast<PainterPrivate*>(m_d);
  fill_path(shader, draw, d->select_filled_path(path),
            fill_rule, anti_alias_quality);
}

void
fastuidraw::Painter::
fill_path(const PainterData &draw, const Path &path, enum fill_rule_t fill_rule,
          enum shader_anti_alias_t anti_alias_quality)
{
  fill_path(default_shaders().fill_shader(), draw, path, fill_rule,
            anti_alias_quality);
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const FilledPath &filled_path, const CustomFillRuleBase &fill_rule,
          enum shader_anti_alias_t anti_alias_quality)
{
  PainterPrivate *d;

  d = static_cast<PainterPrivate*>(m_d);
  d->fill_path(shader, draw, filled_path, fill_rule,
               anti_alias_quality);
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader,
          const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
          enum shader_anti_alias_t anti_alias_quality)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  fill_path(shader, draw, d->select_filled_path(path),
            fill_rule, anti_alias_quality);
}

void
fastuidraw::Painter::
fill_path(const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
          enum shader_anti_alias_t anti_alias_quality)
{
  fill_path(default_shaders().fill_shader(), draw, path, fill_rule,
            anti_alias_quality);
}

void
fastuidraw::Painter::
fill_path(const PainterGlyphShader &sh, const PainterData &draw,
          const ShaderFilledPath &path, enum fill_rule_t fill_rule)
{
  enum glyph_type tp(path.render_type());
  const reference_counted_ptr<PainterItemShader> &shader(sh.shader(tp));
  c_array<const PainterAttribute> attribs;
  c_array<const PainterIndex> indices;

  path.render_data(glyph_atlas(), fill_rule, &attribs, &indices);
  draw_generic(shader, draw, attribs, indices);
}

void
fastuidraw::Painter::
fill_path(const PainterData &draw, const ShaderFilledPath &path,
          enum fill_rule_t fill_rule)
{
  fill_path(default_shaders().glyph_shader(), draw, path, fill_rule);
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
compute_glyph_renderer(float pixel_size)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->compute_glyph_renderer(pixel_size,
                                   d->m_default_glyph_renderer_chooser);
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
compute_glyph_renderer(float pixel_size,
                       const GlyphRendererChooser &chooser)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->compute_glyph_renderer(pixel_size, chooser);
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
            const GlyphSequence &glyph_sequence,
	    GlyphRenderer renderer)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (!renderer.valid())
    {
      renderer = d->compute_glyph_renderer(glyph_sequence.pixel_size(),
                                           d->m_default_glyph_renderer_chooser);
    }

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      return renderer;
    }

  unsigned int num;
  d->m_work_room.m_glyph.m_subsets.resize(glyph_sequence.number_subsets());
  num = glyph_sequence.select_subsets(d->m_work_room.m_glyph.m_scratch,
                                      d->m_clip_store.current(),
                                      d->m_clip_rect_state.item_matrix(),
                                      make_c_array(d->m_work_room.m_glyph.m_subsets));
  d->m_work_room.m_glyph.m_attribs.resize(num);
  d->m_work_room.m_glyph.m_indices.resize(num);
  for (unsigned int k = 0; k < num; ++k)
    {
      unsigned int I(d->m_work_room.m_glyph.m_subsets[k]);
      GlyphSequence::Subset S(glyph_sequence.subset(I));
      S.attributes_and_indices(renderer,
			       &d->m_work_room.m_glyph.m_attribs[k],
			       &d->m_work_room.m_glyph.m_indices[k]);
    }
  d->draw_generic(shader.shader(renderer.m_type),
		  draw,
		  make_c_array(d->m_work_room.m_glyph.m_attribs),
		  make_c_array(d->m_work_room.m_glyph.m_indices),
		  c_array<const int>(),
		  c_array<const unsigned int>(),
		  d->m_current_z);

  return renderer;
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterData &draw, const GlyphSequence &data,
	    GlyphRenderer render)
{
  return draw_glyphs(default_shaders().glyph_shader(), draw, data, render);
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
            const GlyphRun &glyph_run,
            unsigned int begin, unsigned int count,
            GlyphRenderer renderer)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (!renderer.valid())
    {
      renderer = d->compute_glyph_renderer(glyph_run.pixel_size(),
                                           d->m_default_glyph_renderer_chooser);
    }

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      return renderer;
    }

  d->draw_generic(shader.shader(renderer.m_type),
                  draw,
                  glyph_run.subsequence(renderer, begin, count),
                  d->m_current_z);

  return renderer;
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterData &draw, const GlyphRun &data,
            unsigned int begin, unsigned int count,
            GlyphRenderer renderer)
{
  return draw_glyphs(default_shaders().glyph_shader(), draw, data,
                     begin, count, renderer);
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
            const GlyphRun &glyph_run, GlyphRenderer renderer)
{
  return draw_glyphs(shader, draw, glyph_run, 0, glyph_run.number_glyphs(), renderer);
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterData &draw, const GlyphRun &glyph_run,
            GlyphRenderer renderer)
{
  return draw_glyphs(draw, glyph_run, 0, glyph_run.number_glyphs(), renderer);
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
            const GlyphSequence &glyph_sequence,
	    const GlyphRendererChooser &renderer_chooser)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  return draw_glyphs(shader, draw, glyph_sequence,
                     d->compute_glyph_renderer(glyph_sequence.pixel_size(), renderer_chooser));
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterData &draw, const GlyphSequence &data,
	    const GlyphRendererChooser &renderer_chooser)
{
  return draw_glyphs(default_shaders().glyph_shader(), draw, data, renderer_chooser);
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
            const GlyphRun &glyph_run,
            unsigned int begin, unsigned int count,
            const GlyphRendererChooser &renderer_chooser)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  return draw_glyphs(shader, draw, glyph_run, begin, count,
                     d->compute_glyph_renderer(glyph_run.pixel_size(), renderer_chooser));
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterData &draw, const GlyphRun &data,
            unsigned int begin, unsigned int count,
            const GlyphRendererChooser &renderer_chooser)
{
  return draw_glyphs(default_shaders().glyph_shader(), draw, data,
                     begin, count, renderer_chooser);
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
            const GlyphRun &glyph_run, const GlyphRendererChooser &renderer_chooser)
{
  return draw_glyphs(shader, draw, glyph_run, 0, glyph_run.number_glyphs(), renderer_chooser);
}

fastuidraw::GlyphRenderer
fastuidraw::Painter::
draw_glyphs(const PainterData &draw, const GlyphRun &glyph_run,
            const GlyphRendererChooser &renderer_chooser)
{
  return draw_glyphs(draw, glyph_run, 0, glyph_run.number_glyphs(), renderer_chooser);
}

const fastuidraw::float3x3&
fastuidraw::Painter::
transformation(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_clip_rect_state.item_matrix();
}

fastuidraw::c_array<const fastuidraw::vec3>
fastuidraw::Painter::
clip_equations(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_clip_store.current();
}

fastuidraw::c_array<const fastuidraw::vec3>
fastuidraw::Painter::
clip_polygon(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_clip_store.current_poly();
}

bool
fastuidraw::Painter::
clip_region_bounds(vec2 *min_pt, vec2 *max_pt)
{
  PainterPrivate *d;
  bool non_empty;

  d = static_cast<PainterPrivate*>(m_d);

  non_empty = !d->m_clip_rect_state.m_all_content_culled
    && !d->m_clip_store.current_poly().empty()
    && !d->m_clip_store.current_bb().empty();

  if (non_empty)
    {
      *min_pt = d->m_clip_store.current_bb().min_point();
      *max_pt = d->m_clip_store.current_bb().max_point();
    }
  else
    {
      *min_pt = *max_pt = vec2(0.0f, 0.0f);
    }
  return non_empty;
}

bool
fastuidraw::Painter::
clip_region_logical_bounds(vec2 *min_pt, vec2 *max_pt)
{
  if (!clip_region_bounds(min_pt, max_pt))
    {
      return false;
    }

  vec3 p0, p1;
  PainterPrivate *d;

  d = static_cast<PainterPrivate*>(m_d);

  p0 = vec3(min_pt->x(), min_pt->y(), 1.0f);
  p1 = vec3(max_pt->x(), max_pt->y(), 1.0f);

  p0 = p0 * d->m_clip_rect_state.item_matrix_inverse_transpose();
  p1 = p1 * d->m_clip_rect_state.item_matrix_inverse_transpose();

  p0 /= p0.z();
  p1 /= p1.z();

  min_pt->x() = t_min(p0.x(), p1.x());
  min_pt->y() = t_min(p0.y(), p1.y());

  max_pt->x() = t_max(p0.x(), p1.x());
  max_pt->y() = t_max(p0.y(), p1.y());

  return true;
}

void
fastuidraw::Painter::
transformation(const float3x3 &m)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_clip_rect_state.item_matrix(m, true, unclassified_matrix);
}

void
fastuidraw::Painter::
concat(const float3x3 &tr)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->concat(tr);
}

void
fastuidraw::Painter::
translate(const vec2 &p)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->translate(p);
}

void
fastuidraw::Painter::
scale(float s)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->scale(s);
}

void
fastuidraw::Painter::
shear(float sx, float sy)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->shear(sx, sy);
}

void
fastuidraw::Painter::
rotate(float angle)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->rotate(angle);
}

void
fastuidraw::Painter::
curve_flatness(float thresh)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_curve_flatness = thresh;
}

float
fastuidraw::Painter::
curve_flatness(void)
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
  st.m_composite = d->packer()->composite_shader();
  st.m_blend = d->packer()->blend_shader();
  st.m_composite_mode = d->packer()->composite_mode();
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
  d->packer()->composite_shader(st.m_composite, st.m_composite_mode);
  d->packer()->blend_shader(st.m_blend);
  d->m_curve_flatness = st.m_curve_flatness;
  while(d->m_occluder_stack.size() > st.m_occluder_stack_position)
    {
      d->m_occluder_stack.back().on_pop(d);
      d->m_occluder_stack.pop_back();
    }
  d->m_state_stack.pop_back();
  d->m_clip_store.pop();
}

void
fastuidraw::Painter::
begin_layer(const vec4 &color_modulate)
{
  PainterPrivate *d;
  TransparencyStackEntry R;
  Rect clip_region_rect;

  d = static_cast<PainterPrivate*>(m_d);

  clip_region_bounds(&clip_region_rect.m_min_point,
                     &clip_region_rect.m_max_point);
  save();

  /* Set the clipping equations to the equations coming from clip_region_rect */
  d->m_clip_rect_state.m_clip_rect = clip_rect(clip_region_rect);
  d->m_clip_rect_state.set_clip_equations_to_clip_rect(clip_rect_state::rect_in_normalized_device_coordinates);

  /* update d->m_clip_rect_state.m_clip_rect to local coordinates */
  d->m_clip_rect_state.update_rect_to_transformation();

  /* change m_clip_store so that the current value is just from clip_region_rect */
  d->m_clip_store.reset_current_to_rect(clip_region_rect);

  /* get the TransparencyStackEntry that gives the PainterPacker and what to blit
   * when the layer is done
   */
  R = d->m_transparency_stack_entry_factory.fetch(d->m_transparency_stack.size(),
                                                  clip_region_rect, d);
  R.m_state_stack_size = d->m_state_stack.size();
  R.m_modulate_color = color_modulate;

  /* set the normalized translation of d->m_clip_rect_state */
  d->m_clip_rect_state.set_normalized_device_translate(R.m_normalized_translate);

  d->m_transparency_stack.push_back(R);
  ++d->m_stats[num_layers];

  /* Set the packer's composite shader, mode and blend shader to
   * Painter default values
   */
  composite_shader(composite_porter_duff_src_over);
  blend_shader(blend_w3c_normal);
  save();
}

void
fastuidraw::Painter::
end_layer(void)
{
  //matches the save() at precisely the end of begin_layer();
  restore();

  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  TransparencyStackEntry R(d->m_transparency_stack.back());
  d->m_transparency_stack.pop_back();

  /* restore any saves() done within the layer. */
  FASTUIDRAWassert(R.m_state_stack_size == d->m_state_stack.size());
  while (R.m_state_stack_size > d->m_state_stack.size())
    {
      restore();
    }

  /* issue the restore that matches with the save() at the start
   * of begin_layer(); this will restore the clipping, blending
   * and compositing state to what it was when the begin_layer()
   * was issued */
  restore();

  /* blit the rect, we do this by blitting the rect in simple pixel
   * coordinates with the brush appropiately translated.
   */
  save();
    {
      vec2 dims;
      PainterBrush brush;

      dims = d->m_viewport_dimensions;
      transformation(float_orthogonal_projection_params(0, dims.x(), 0, dims.y()));

      brush
        .transformation_translate(R.m_brush_translate)
        .color(R.m_modulate_color)
        .image(R.m_image);

      fill_rect(PainterData(&brush),
                Rect()
                .min_point(coords_from_normalized_coords(R.m_normalized_rect.m_min_point, dims))
                .max_point(coords_from_normalized_coords(R.m_normalized_rect.m_max_point, dims)),
                shader_anti_alias_none);
    }
  restore();
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
clip_out_path(const Path &path, enum fill_rule_t fill_rule)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  clip_out_path(d->select_filled_path(path), fill_rule);
}

void
fastuidraw::Painter::
clip_out_path(const Path &path, const CustomFillRuleBase &fill_rule)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  clip_out_path(d->select_filled_path(path), fill_rule);
}

void
fastuidraw::Painter::
clip_in_path(const Path &path, enum fill_rule_t fill_rule)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  clip_in_path(d->select_filled_path(path), fill_rule);
}

void
fastuidraw::Painter::
clip_in_path(const Path &path, const CustomFillRuleBase &fill_rule)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  clip_in_path(d->select_filled_path(path), fill_rule);
}

void
fastuidraw::Painter::
clip_out_path(const FilledPath &path, enum fill_rule_t fill_rule)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  PainterBlendShader* old_blend;
  PainterCompositeShader* old_composite;
  BlendMode old_composite_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = d->packer()->blend_shader();
  old_composite = d->packer()->composite_shader();
  old_composite_mode = d->packer()->composite_mode();

  blend_shader(Painter::blend_w3c_normal);
  composite_shader(composite_porter_duff_dst);
  d->packer()->add_callback(zdatacallback);
  d->fill_path(default_shaders().fill_shader(),
               PainterData(d->m_black_brush),
               path, fill_rule, shader_anti_alias_none);
  d->packer()->remove_callback(zdatacallback);
  d->packer()->composite_shader(old_composite, old_composite_mode);
  d->packer()->blend_shader(old_blend);

  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));
}

void
fastuidraw::Painter::
clip_out_path(const FilledPath &path, const CustomFillRuleBase &fill_rule)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  PainterBlendShader* old_blend;
  PainterCompositeShader* old_composite;
  BlendMode old_composite_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = d->packer()->blend_shader();
  old_composite = d->packer()->composite_shader();
  old_composite_mode = d->packer()->composite_mode();

  blend_shader(Painter::blend_w3c_normal);
  composite_shader(composite_porter_duff_dst);
  d->packer()->add_callback(zdatacallback);
  d->fill_path(default_shaders().fill_shader(),
               PainterData(d->m_black_brush),
               path, fill_rule, shader_anti_alias_none);
  d->packer()->remove_callback(zdatacallback);
  d->packer()->composite_shader(old_composite, old_composite_mode);
  d->packer()->blend_shader(old_blend);

  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));
}

void
fastuidraw::Painter::
clip_out_custom(const reference_counted_ptr<PainterItemShader> &shader,
                const PainterData::value<PainterItemShaderData> &shader_data,
                c_array<const c_array<const PainterAttribute> > attrib_chunks,
                c_array<const c_array<const PainterIndex> > index_chunks,
                c_array<const int> index_adjusts,
                c_array<const unsigned int> attrib_chunk_selector)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  PainterBlendShader* old_blend;
  fastuidraw::PainterCompositeShader* old_composite;
  BlendMode old_composite_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = d->packer()->blend_shader();
  old_composite = d->packer()->composite_shader();
  old_composite_mode = d->packer()->composite_mode();

  blend_shader(Painter::blend_w3c_normal);
  composite_shader(composite_porter_duff_dst);
  d->packer()->add_callback(zdatacallback);
  draw_generic(shader,
               PainterData(shader_data, d->m_black_brush),
               attrib_chunks, index_chunks,
               index_adjusts, attrib_chunk_selector);
  d->packer()->remove_callback(zdatacallback);
  d->packer()->composite_shader(old_composite, old_composite_mode);
  d->packer()->blend_shader(old_blend);

  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));
}

void
fastuidraw::Painter::
clip_out_rect(const fastuidraw::Rect &rect)
{
  vecN<vec2, 4> pts;

  pts[0] = vec2(rect.m_min_point.x(), rect.m_min_point.y());
  pts[1] = vec2(rect.m_min_point.x(), rect.m_max_point.y());
  pts[2] = vec2(rect.m_max_point.x(), rect.m_max_point.y());
  pts[3] = vec2(rect.m_max_point.x(), rect.m_min_point.y());
  clip_out_convex_polygon(pts);
}

void
fastuidraw::Painter::
clip_out_rounded_rect(const RoundedRect &R)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  PainterBlendShader* old_blend;
  fastuidraw::PainterCompositeShader* old_composite;
  BlendMode old_composite_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = d->packer()->blend_shader();
  old_composite = d->packer()->composite_shader();
  old_composite_mode = d->packer()->composite_mode();

  blend_shader(Painter::blend_w3c_normal);
  composite_shader(composite_porter_duff_dst);
  d->packer()->add_callback(zdatacallback);
  d->fill_rounded_rect(default_shaders().fill_shader(),
                       PainterData(d->m_black_brush),
                       R, shader_anti_alias_none);
  d->packer()->remove_callback(zdatacallback);
  d->packer()->composite_shader(old_composite, old_composite_mode);
  d->packer()->blend_shader(old_blend);

  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));
}

void
fastuidraw::Painter::
clip_out_convex_polygon(c_array<const vec2> poly)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  d->ready_non_aa_polygon_attribs(poly);
  clip_out_custom(default_shaders().fill_shader().item_shader(),
                  PainterData::value<PainterItemShaderData>(),
                  make_c_array(d->m_work_room.m_polygon.m_attribs),
                  make_c_array(d->m_work_room.m_polygon.m_indices));
}

void
fastuidraw::Painter::
clip_in_path(const FilledPath &path, enum fill_rule_t fill_rule)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  clip_in_rect(path.bounding_box());
  clip_out_path(path, complement_fill_rule(fill_rule));
}

void
fastuidraw::Painter::
clip_in_path(const FilledPath &path, const CustomFillRuleBase &fill_rule)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter
       */
      return;
    }

  clip_in_rect(path.bounding_box());
  clip_out_path(path, ComplementFillRule(&fill_rule));
}

void
fastuidraw::Painter::
clip_in_rounded_rect(const RoundedRect &R)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter */
      return;
    }

  clip_in_rect(R);

  /* Save our transformation and clipping state because
   * we are going to shear and translate the transformation
   * matrix to draw the corner occluders.
   */
  clip_rect_state m(d->m_clip_rect_state);

  PainterBlendShader* old_blend;
  fastuidraw::PainterCompositeShader* old_composite;
  BlendMode old_composite_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;
  RoundedRectTransformations rect_transforms(R);

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = d->packer()->blend_shader();
  old_composite = d->packer()->composite_shader();
  old_composite_mode = d->packer()->composite_mode();

  blend_shader(Painter::blend_w3c_normal);
  composite_shader(composite_porter_duff_dst);
  d->packer()->add_callback(zdatacallback);
  for (int i = 0; i < 4; ++i)
    {
      translate(rect_transforms.m_translates[i]);
      shear(rect_transforms.m_shears[i].x(), rect_transforms.m_shears[i].y());
      d->fill_path(default_shaders().fill_shader(), PainterData(d->m_black_brush),
                   d->select_filled_path(d->m_rounded_corner_path_complement),
                   nonzero_fill_rule,
                   shader_anti_alias_none);
      d->m_clip_rect_state = m;
    }
  d->packer()->remove_callback(zdatacallback);
  d->packer()->composite_shader(old_composite, old_composite_mode);
  d->packer()->blend_shader(old_blend);

  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));
}

void
fastuidraw::Painter::
clip_in_rect(const Rect &rect)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  d->m_clip_rect_state.m_all_content_culled =
    d->m_clip_rect_state.m_all_content_culled ||
    d->m_clip_rect_state.rect_is_culled(rect) ||
    d->update_clip_equation_series(rect);

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
      d->m_clip_rect_state.m_clip_rect = clip_rect(rect);
      d->m_clip_rect_state.set_clip_equations_to_clip_rect(clip_rect_state::rect_in_local_coordinates);
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
      d->m_clip_rect_state.m_clip_rect.intersect(clip_rect(rect));
      d->m_clip_rect_state.set_clip_equations_to_clip_rect(clip_rect_state::rect_in_local_coordinates);
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

  d->m_clip_rect_state.m_clip_rect = clip_rect(rect);

  std::bitset<4> skip_occluder;
  skip_occluder = d->m_clip_rect_state.set_clip_equations_to_clip_rect(prev_clip,
                                                                       clip_rect_state::rect_in_local_coordinates);
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
  matrix_state = d->m_clip_rect_state.current_item_matrix_state(d->m_pool);
  FASTUIDRAWassert(matrix_state);
  d->m_clip_rect_state.item_matrix_state(d->identity_matrix(), false);

  reference_counted_ptr<ZDataCallBack> zdatacallback;
  zdatacallback = FASTUIDRAWnew ZDataCallBack();

  PainterBlendShader* old_blend;
  fastuidraw::PainterCompositeShader* old_composite;
  BlendMode old_composite_mode;

  old_blend = d->packer()->blend_shader();
  old_composite = d->packer()->composite_shader();
  old_composite_mode = d->packer()->composite_mode();

  blend_shader(Painter::blend_w3c_normal);
  composite_shader(composite_porter_duff_dst);

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

      f = t_abs(eq.x()) * d->m_one_pixel_width.x() + t_abs(eq.y()) * d->m_one_pixel_width.y();
      eq.z() += f;
    }
  d->m_clip_rect_state.clip_equations(slightly_bigger);

  /* draw the half plane occluders */
  d->packer()->add_callback(zdatacallback);
  for(unsigned int i = 0; i < 4; ++i)
    {
      if (!skip_occluder[i])
        {
          d->draw_half_plane_complement(default_shaders().fill_shader(),
                                        PainterData(d->m_black_brush),
                                        prev_clip.value().m_clip_equations[i]);
        }
    }
  d->packer()->remove_callback(zdatacallback);

  d->m_clip_rect_state.clip_equations_state(current_clip);

  /* add to occluder stack */
  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));

  d->m_clip_rect_state.item_matrix_state(matrix_state, false);
  d->packer()->composite_shader(old_composite, old_composite_mode);
  d->packer()->blend_shader(old_blend);
}

const fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas>&
fastuidraw::Painter::
glyph_atlas(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_backend->glyph_atlas();
}

const fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas>&
fastuidraw::Painter::
image_atlas(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_backend->image_atlas();
}

const fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas>&
fastuidraw::Painter::
colorstop_atlas(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_backend->colorstop_atlas();
}

fastuidraw::reference_counted_ptr<fastuidraw::PainterShaderRegistrar>
fastuidraw::Painter::
painter_shader_registrar(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_backend->painter_shader_registrar();
}

fastuidraw::PainterCompositeShader*
fastuidraw::Painter::
composite_shader(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->packer()->composite_shader();
}

fastuidraw::BlendMode
fastuidraw::Painter::
composite_mode(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->packer()->composite_mode();
}

void
fastuidraw::Painter::
composite_shader(const reference_counted_ptr<PainterCompositeShader> &h,
                 BlendMode mode)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->packer()->composite_shader(h.get(), mode);
}

fastuidraw::PainterBlendShader*
fastuidraw::Painter::
blend_shader(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->packer()->blend_shader();
}

void
fastuidraw::Painter::
blend_shader(const reference_counted_ptr<PainterBlendShader> &h)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->packer()->blend_shader(h.get());
}

const fastuidraw::PainterShaderSet&
fastuidraw::Painter::
default_shaders(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_default_shaders;
}

unsigned int
fastuidraw::Painter::
query_stat(enum query_stats_t st) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_stats[st];
}

unsigned int
fastuidraw::Painter::
number_stats(void)
{
  return PainterPacker::num_stats;
}

fastuidraw::c_string
fastuidraw::Painter::
stat_name(enum query_stats_t st)
{
#define EASY(X) case X: return #X

  switch(st)
    {
      EASY(num_attributes);
      EASY(num_indices);
      EASY(num_generic_datas);
      EASY(num_draws);
      EASY(num_headers);
      EASY(num_render_targets);
      EASY(num_ends);
      EASY(num_layers);
    default:
      return "unknown";
    }

#undef EASY
}

void
fastuidraw::Painter::
query_stats(c_array<unsigned int> dst) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  for (unsigned int i = 0; i < dst.size() && i < PainterPacker::num_stats; ++i)
    {
      dst[i] = d->m_stats[i];
    }
}
