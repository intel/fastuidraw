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
#include <fastuidraw/painter/effects/painter_effect_color_modulate.hpp>
#include <fastuidraw/painter/painter.hpp>

#include <private/util_private.hpp>
#include <private/util_private_math.hpp>
#include <private/util_private_ostream.hpp>
#include <private/clip.hpp>
#include <private/bounding_box.hpp>
#include <private/rect_atlas.hpp>
#include <private/painter_backend/painter_packer.hpp>

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
          d[fastuidraw::PainterHeader::blend_shader_data_location_offset].u = fastuidraw::PainterHeader::drawing_occluder;
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

  inline
  bool
  all_pts_culled_by_half_plane(fastuidraw::c_array<const fastuidraw::vec3> pts,
                               const fastuidraw::vec3 &eq)
  {
    for (const fastuidraw::vec3 &pt : pts)
      {
        if (fastuidraw::dot(pt, eq) >= 0.0f)
          {
            return false;
          }
      }
    return true;
  }

  inline
  bool
  all_pts_culled_by_one_half_plane(fastuidraw::c_array<const fastuidraw::vec3> pts,
                                   const fastuidraw::PainterClipEquations &eq)
  {
    for(int i = 0; i < 4; ++i)
      {
        if (all_pts_culled_by_half_plane(pts, eq.m_clip_equations[i]))
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
  const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>*
  stroke_shader(const fastuidraw::PainterStrokeShader &shader,
                bool use_arc_shading, bool apply_anti_aliasing)
  {
    enum fastuidraw::PainterStrokeShader::shader_type_t tp;
    enum fastuidraw::Painter::stroking_method_t st;

    tp = (apply_anti_aliasing) ?
      fastuidraw::PainterStrokeShader::aa_shader:
      fastuidraw::PainterStrokeShader::non_aa_shader;

    st = (use_arc_shading) ?
      fastuidraw::Painter::stroking_method_arc:
      fastuidraw::Painter::stroking_method_linear;

    return &shader.shader(st, tp);
  }

  class ExtendedPool:public fastuidraw::PainterPackedValuePool
  {
  public:
    typedef fastuidraw::detail::PackedValuePool<fastuidraw::PainterClipEquations> ClipEquationsPool;
    typedef fastuidraw::detail::PackedValuePool<fastuidraw::PainterItemMatrix> ItemMatrixPool;
    typedef fastuidraw::detail::PackedValuePool<fastuidraw::PainterBrushAdjust> BrushAdjustPool;
    typedef ClipEquationsPool::ElementHandle PackedClipEquations;
    typedef ItemMatrixPool::ElementHandle PackedItemMatrix;
    typedef BrushAdjustPool::ElementHandle PackedBrushAdjust;

    using fastuidraw::PainterPackedValuePool::create_packed_value;

    PackedClipEquations
    create_packed_value(const fastuidraw::PainterClipEquations &cp)
    {
      return m_clip_equations_pool.allocate(cp);
    }

    PackedItemMatrix
    create_packed_value(const fastuidraw::PainterItemMatrix &cp)
    {
      return m_item_matrix_pool.allocate(cp);
    }

    PackedBrushAdjust
    create_packed_value(const fastuidraw::PainterBrushAdjust &cp)
    {
      return m_brush_adjust_pool.allocate(cp);
    }

  private:
    ClipEquationsPool::Holder m_clip_equations_pool;
    ItemMatrixPool::Holder m_item_matrix_pool;
    BrushAdjustPool::Holder m_brush_adjust_pool;
  };

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
  class ClipRectState
  {
  public:
    enum rect_coordinate_type_t
      {
        rect_in_local_coordinates,
        rect_in_normalized_device_coordinates,
      };

    ClipRectState(void):
      m_all_content_culled(false),
      m_item_matrix_transition_tricky(false),
      m_inverse_transpose_ready(true),
      m_item_matrix_singular_values_ready(false)
    {}

    void
    apply_item_matrix(fastuidraw::c_array<const fastuidraw::vec2> in_pts,
                      fastuidraw::c_array<fastuidraw::vec3> out_pts) const
    {
      const fastuidraw::float3x3 &m(item_matrix());

      FASTUIDRAWassert(in_pts.size() == out_pts.size());
      for (unsigned int i = 0; i < in_pts.size(); ++i)
        {
          out_pts[i] = m * fastuidraw::vec3(in_pts[i].x(), in_pts[i].y(), 1.0f);
        }
    }

    void
    apply_item_matrix(const fastuidraw::Rect &rect,
                      fastuidraw::vecN<fastuidraw::vec3, 4> &out_pts) const
    {
      fastuidraw::vecN<fastuidraw::vec2, 4> pts;
      pts[0] = rect.point(fastuidraw::Rect::minx_miny_corner);
      pts[1] = rect.point(fastuidraw::Rect::minx_maxy_corner);
      pts[2] = rect.point(fastuidraw::Rect::maxx_maxy_corner);
      pts[3] = rect.point(fastuidraw::Rect::maxx_miny_corner);
      apply_item_matrix(pts, out_pts);
    }

    void
    reset(const fastuidraw::PainterSurface::Viewport &vwp,
          fastuidraw::ivec2 surface_dims);

    void
    set_clip_equations_to_clip_rect(enum rect_coordinate_type_t c);

    std::bitset<4>
    set_clip_equations_to_clip_rect(const ExtendedPool::PackedClipEquations &prev_clip,
                                    enum rect_coordinate_type_t c);

    const fastuidraw::float3x3&
    item_matrix_inverse_transpose(void) const;

    const fastuidraw::float3x3&
    item_matrix(void) const
    {
      if (m_override_matrix_state)
        {
          return m_override_matrix_state.unpacked_value().m_item_matrix;
        }

      return (m_item_matrix_state) ?
        m_item_matrix_state.unpacked_value().m_item_matrix :
        m_item_matrix.m_item_matrix;
    }

    const fastuidraw::vec2&
    item_matrix_singular_values(void) const;

    float
    item_matrix_operator_norm(void) const
    {
      return item_matrix_singular_values()[0];
    }

    enum matrix_type_t
    matrix_type(void) const;

    void //a negative value for singular_value_scaled indicates to recompute singular values
    item_matrix(const fastuidraw::float3x3 &v,
                bool trick_transition, enum matrix_type_t M,
                float singular_value_scaled = -1.0f);

    const fastuidraw::PainterClipEquations&
    clip_equations(void) const
    {
      return m_clip_equations;
    }

    void
    clip_equations(const fastuidraw::PainterClipEquations &v)
    {
      m_clip_equations = v;
      m_clip_equations_state.reset();
    }

    const ExtendedPool::PackedItemMatrix&
    current_item_matrix_state(ExtendedPool &pool) const
    {
      if (m_override_matrix_state)
        {
          return m_override_matrix_state;
        }

      if (!m_item_matrix_state)
        {
          m_item_matrix_state = pool.create_packed_value(m_item_matrix);
        }
      return m_item_matrix_state;
    }

    /* Dangerous function: override the current value for the item_matrix_state();
     * All logic of checking if various other state and values are ready are not
     * applied; the use case is to temporarily change the matrix state to a given
     * state value and to restore it back to the previous state.
     */
    void
    override_item_matrix_state(const ExtendedPool::PackedItemMatrix &v)
    {
      m_override_matrix_state = v;
    }

    void
    stop_override_item_matrix_state(void)
    {
      m_override_matrix_state.reset();
    }

    const ExtendedPool::PackedItemMatrix&
    current_item_matrix_coverage_buffer_state(ExtendedPool &pool)
    {
      FASTUIDRAWassert(!m_override_matrix_state);
      if (!m_item_matrix_coverage_buffer_state)
        {
          fastuidraw::PainterItemMatrix tmp;

          tmp = m_item_matrix;
          tmp.m_normalized_translate = m_coverage_buffer_normalized_translate;
          m_item_matrix_coverage_buffer_state = pool.create_packed_value(tmp);
        }
      return m_item_matrix_coverage_buffer_state;
    }

    void
    coverage_buffer_normalized_translate(fastuidraw::vec2 v)
    {
      m_item_matrix_coverage_buffer_state.reset();
      m_coverage_buffer_normalized_translate = v;
    }

    const ExtendedPool::PackedClipEquations&
    clip_equations_state(ExtendedPool &pool) const
    {
      if (!m_clip_equations_state)
        {
          m_clip_equations_state = pool.create_packed_value(m_clip_equations);
        }
      return m_clip_equations_state;
    }

    void
    clip_equations_state(const ExtendedPool::PackedClipEquations &v)
    {
      m_clip_equations_state = v;
      m_clip_equations = v.unpacked_value();
    }

    bool
    item_matrix_transition_tricky(void) const
    {
      return m_item_matrix_transition_tricky;
    }

    void
    clip_polygon(fastuidraw::c_array<const fastuidraw::vec2> pts,
                 std::vector<fastuidraw::vec2> &out_pts,
                 std::vector<fastuidraw::vec2> &work_vec2s);

    bool
    poly_is_culled(fastuidraw::c_array<const fastuidraw::vec3> pts) const;

    void
    set_normalized_device_translate(const fastuidraw::vec2 &v)
    {
      m_item_matrix_state.reset();
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
    mutable enum matrix_type_t m_matrix_type;
    bool m_item_matrix_transition_tricky;
    fastuidraw::PainterItemMatrix m_item_matrix;
    mutable ExtendedPool::PackedItemMatrix m_item_matrix_state;
    fastuidraw::PainterClipEquations m_clip_equations;
    mutable ExtendedPool::PackedClipEquations m_clip_equations_state;
    fastuidraw::vec2 m_coverage_buffer_normalized_translate;
    ExtendedPool::PackedItemMatrix m_item_matrix_coverage_buffer_state;
    mutable bool m_inverse_transpose_ready;
    fastuidraw::vec2 m_viewport_dimensions;
    mutable fastuidraw::float3x3 m_item_matrix_inverse_transpose;
    mutable bool m_item_matrix_singular_values_ready;
    mutable fastuidraw::vec2 m_item_matrix_singular_values;

    ExtendedPool::PackedItemMatrix m_override_matrix_state;
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
      m_blend(nullptr)
    {}

    unsigned int m_occluder_stack_position;
    fastuidraw::PainterBlendShader *m_blend;
    fastuidraw::BlendMode m_blend_mode;
    fastuidraw::range_type<unsigned int> m_clip_equation_series;

    ClipRectState m_clip_rect_state;
    float m_curve_flatness;

    // checking
    unsigned int m_deferred_coverage_buffer_depth;
  };

  class BufferRect
  {
  public:
    BufferRect(const fastuidraw::Rect &normalized_rect,
               fastuidraw::ivec2 useable_size,
               PainterPrivate *d);

    fastuidraw::vec2
    compute_normalized_translate(fastuidraw::ivec2 rect_dst,
                                 const fastuidraw::PainterSurface::Viewport &vwp) const
    {
      /* we need to have that:
       *   return_value + normalized_rect = R
       * where R is rect in normalized device coordinates.
       */
      fastuidraw::vec2 Rndc;
      Rndc = vwp.compute_normalized_device_coords(fastuidraw::vec2(rect_dst));
      return Rndc - m_normalized_rect.m_min_point;
    }

    fastuidraw::ivec2 m_bl, m_tr, m_dims;
    fastuidraw::vec2 m_fbl, m_ftr;
    fastuidraw::Rect m_normalized_rect;
    fastuidraw::RectT<int> m_pixel_rect;
  };

  class EffectsBuffer:
    public fastuidraw::reference_counted<EffectsBuffer>::non_concurrent
  {
  public:
    EffectsBuffer(const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker> &packer,
                       const fastuidraw::reference_counted_ptr<fastuidraw::PainterSurface> &surface,
                       const fastuidraw::reference_counted_ptr<const fastuidraw::Image> &image,
                       fastuidraw::ivec2 useable_size):
      m_packer(packer),
      m_surface(surface),
      m_image(image),
      m_rect_atlas(useable_size)
    {}

    /* the PainterPacker used */
    fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker> m_packer;

    /* the depth of effects buffer */
    unsigned int m_depth;

    /* the surface used by the EffectsBuffer */
    fastuidraw::reference_counted_ptr<fastuidraw::PainterSurface> m_surface;

    /* the surface realized as an image */
    fastuidraw::reference_counted_ptr<const fastuidraw::Image> m_image;

    /* the atlas to track what regions are free */
    fastuidraw::detail::RectAtlas m_rect_atlas;
  };

  class EffectsLayer
  {
  public:
    void
    blit_rect(const fastuidraw::reference_counted_ptr<fastuidraw::PainterEffectPass> &fx_pass,
              fastuidraw::vec2 viewport_dims,
              fastuidraw::Painter *p);

    /* The region of the effects in normalized coords */
    fastuidraw::Rect m_normalized_rect;

    /* The region of the effects region in pixel coords */
    fastuidraw::RectT<int> m_pixel_rect;

    /* the image to blit, together with what pixels of the image to blit */
    const fastuidraw::Image *m_image;
    fastuidraw::vec2 m_brush_translate;

    /* The translation is from the root surface of Painter::begin()
     * to the location within EffectsBuffer::m_surface.
     */
    fastuidraw::PainterPacker *m_packer;
    fastuidraw::vec2 m_normalized_translate;

    /* The bottom left hand corner of the rect within
     * EffectsBuffer::m_surface
     */
    fastuidraw::ivec2 m_min_corner_in_surface;

    /* identity matrix state; this is needed because PainterItemMatrix
     * also stores the normalized translation.
     */
    ExtendedPool::PackedItemMatrix m_identity_matrix;
  };

  class EffectsLayerFactory
  {
  public:
    EffectsLayerFactory(void):
      m_current_backing_size(0, 0),
      m_current_backing_useable_size(0, 0)
    {}

    /* clears each of the m_rect_atlas within the pool;
     * if the surface_sz changes then also clears the
     * pool entirely.
     */
    void
    begin(fastuidraw::PainterSurface &surface);

    /* creates a EffectsLayer value using the
     * available pools.
     */
    EffectsLayer
    fetch(unsigned int depth,
          const fastuidraw::Rect &normalized_rect,
          PainterPrivate *d);

    /* issues PainterPacker::end() in the correct order
     * on all elements that were fetched within the
     * begin/end pair.
     */
    void
    end(void);

  private:
    typedef std::vector<fastuidraw::reference_counted_ptr<EffectsBuffer> > PerActiveDepth;

    fastuidraw::PainterSurface::Viewport m_effects_buffer_viewport;
    fastuidraw::ivec2 m_current_backing_size, m_current_backing_useable_size;
    std::vector<fastuidraw::reference_counted_ptr<EffectsBuffer> > m_unused_buffers;
    std::vector<PerActiveDepth> m_per_active_depth;
  };

  class DeferredCoverageBuffer:
    public fastuidraw::reference_counted<DeferredCoverageBuffer>::non_concurrent
  {
  public:
    explicit
    DeferredCoverageBuffer(const fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker> &packer,
                           const fastuidraw::reference_counted_ptr<fastuidraw::PainterSurface> &surface,
                           fastuidraw::ivec2 useable_size):
      m_packer(packer),
      m_surface(surface),
      m_rect_atlas(useable_size)
    {}

    fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker> m_packer;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterSurface> m_surface;
    fastuidraw::detail::RectAtlas m_rect_atlas;
  };

  class DeferredCoverageBufferStackEntry
  {
  public:
    DeferredCoverageBufferStackEntry(fastuidraw::PainterPacker *p_packer,
                                     fastuidraw::ivec2 p_min_corner_in_surface,
                                     const BufferRect &buffer_rect,
                                     const fastuidraw::PainterSurface::Viewport &vwp,
                                     const ExtendedPool::PackedClipEquations &p_clip_eq_state):
      m_packer(p_packer),
      m_normalized_translate(buffer_rect.compute_normalized_translate(p_min_corner_in_surface, vwp)),
      m_root_surface_min_corner(buffer_rect.m_bl),
      m_clip_eq_state(p_clip_eq_state),
      m_min_corner_in_surface(p_min_corner_in_surface)
    {
      FASTUIDRAWassert(m_packer);
      FASTUIDRAWassert(m_min_corner_in_surface.x() >= 0);
      FASTUIDRAWassert(m_min_corner_in_surface.y() >= 0);
    }

    /* represents that coverage buffer takes no pixel area and
     * that there is effectivly no coverage buffer.
     */
    DeferredCoverageBufferStackEntry(void):
      m_packer(nullptr)
    {}

    void
    update_coverage_buffer_offset(const PainterPrivate *d);

    fastuidraw::PainterPacker*
    packer(void) const
    {
      return m_packer;
    }

    fastuidraw::ivec2
    min_corner_in_surface(void) const
    {
      return m_min_corner_in_surface;
    }

    fastuidraw::ivec2
    coverage_buffer_offset(void) const
    {
      return m_coverage_buffer_offset;
    }

    fastuidraw::vec2
    normalized_translate(void) const
    {
      return m_normalized_translate;
    }

    const ExtendedPool::PackedClipEquations&
    clip_eq_state(void) const
    {
      return m_clip_eq_state ;
    }

  private:
    fastuidraw::PainterPacker *m_packer;

    /* the translate from root surface normalized
     * device coordinates to the deferred coverage
     * surface normalized device coordinates; this
     * value does not change regardless if the
     * effects layer changes because the vertex
     * uber-shader for a Painter operates in root
     * surface normalized device coordinates until
     * just the end when it adjusts the value to
     * the surface; since the coverage buffer surfce
     * stays the same regardless if the effects
     * layer changes, this value is a constant.
     */
    fastuidraw::vec2 m_normalized_translate;

    /* the location within the root surface of
     * the bottom left corner.
     */
    fastuidraw::ivec2 m_root_surface_min_corner;

    /* the clip-equation state guarantees that the
     * drawing is clipped to within the region
     * of the coverage for this entry only
     */
    ExtendedPool::PackedClipEquations m_clip_eq_state;

    /* The bottom left corner of the source rect
     * relative to the root surface.
     */
    fastuidraw::vec2 m_root_bottom_left_normalized;

    /* The bottom left hand corner of the rect within
     * DeferredCoverageBuffer::m_surface
     */
    fastuidraw::ivec2 m_min_corner_in_surface;

    /* computed from the current state of PainterPacker, updated
     * whenever the effects stack changes
     */
    fastuidraw::ivec2 m_coverage_buffer_offset;
  };

  class DeferredCoverageBufferStackEntryFactory
  {
  public:
    DeferredCoverageBufferStackEntryFactory(void):
      m_current_backing_size(0, 0),
      m_current_backing_useable_size(0, 0)
    {}

    void
    begin(fastuidraw::PainterSurface &surface);

    DeferredCoverageBufferStackEntry
    fetch(const fastuidraw::Rect &normalized_rect, PainterPrivate *d);

    void
    end(void);

    const fastuidraw::PainterSurface::Viewport&
    coverage_buffer_viewport(void) const
    {
      return m_coverage_buffer_viewport;
    }

  private:
    fastuidraw::PainterSurface::Viewport m_coverage_buffer_viewport;
    fastuidraw::ivec2 m_current_backing_size, m_current_backing_useable_size;
    std::vector<fastuidraw::reference_counted_ptr<DeferredCoverageBuffer> > m_unused_buffers;
    std::vector<fastuidraw::reference_counted_ptr<DeferredCoverageBuffer> > m_active_buffers;
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

  void
  compute_clip_equations_from_clip_polygon(fastuidraw::c_array<const fastuidraw::vec3> in_poly,
                                           fastuidraw::c_array<fastuidraw::vec3> out_clip_eq)
  {
    FASTUIDRAWassert(in_poly.size() == out_clip_eq.size());
    /* extract the clip-equations from the clipping polygon which
     * is in clip coordinates. The algorithm is based off the following:
     *
     * Let {p_i} be the points of clipped_rect. Then each of the
     * points lie on a common plane P. A point p is within the
     * clipping region if there is an L > 0 so that
     *   (1) L * p in on of the plane P
     *   (2) L * p is within the convex hull of {p_i}.
     *
     * Geometrically, conditions (1) and (2) are equivalent
     * to that p is an element of the set S where S is the
     * intersection of the half-spaces H_i where the plane of
     * the triangle [0, p_i, [p_{i+1}] is the plane of H_i and
     * H_i contains the average of the {p_i}. Thus the i'th
     * clip-equation is n_i = A_i * cross_product(p_i, p_{i+1})
     * where A_i is so that dot(n_i, center) >= 0. Since the
     * expression is homogenous in center, we can take any
     * multiple of center as well.
     */
    fastuidraw::vec3 multiple_of_center(0.0f, 0.0f, 0.0f);
    for (const fastuidraw::vec3 &pt : in_poly)
      {
        multiple_of_center += pt;
      }

    for (unsigned int i = 0; i < in_poly.size(); ++i)
      {
        unsigned int next_i(i + 1);
        if (next_i == in_poly.size())
          {
            next_i = 0;
          }

        const fastuidraw::vec3 &p(in_poly[i]);
        const fastuidraw::vec3 &next_p(in_poly[next_i]);
        fastuidraw::vec3 n(fastuidraw::cross_product(p, next_p));

        if (dot(n, multiple_of_center) < 0.0f)
          {
            n = -n;
          }
        out_clip_eq[i] = n;
      }
  }

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

    bool //returns true if clipping becomes so that all is clipped
    intersect_current_against_polygon(fastuidraw::c_array<const fastuidraw::vec3> poly);

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

    fastuidraw::vecN<std::vector<fastuidraw::vec3>, 2> m_scratch;
    Vec3Stack m_clip, m_poly;

    fastuidraw::BoundingBox<float> m_current_bb;
    std::vector<fastuidraw::BoundingBox<float> > m_bb_stack;
  };

  class RoundedRectTransformations
  {
  public:
    explicit
    RoundedRectTransformations(const fastuidraw::RoundedRect &R,
                               ExtendedPool *pool = nullptr)
    {
      using namespace fastuidraw;

      /* min-x/max-y */
      m_adjusts[0].m_translate = vec2(R.m_min_point.x(), R.m_max_point.y() - R.m_corner_radii[Rect::minx_maxy_corner].y());
      m_adjusts[0].m_shear = R.m_corner_radii[Rect::minx_maxy_corner];

      /* max-x/max-y */
      m_adjusts[1].m_translate = vec2(R.m_max_point.x(), R.m_max_point.y() - R.m_corner_radii[Rect::maxx_maxy_corner].y());
      m_adjusts[1].m_shear = vec2(-1.0f, 1.0f) * R.m_corner_radii[Rect::maxx_maxy_corner];

      /* min-x/min-y */
      m_adjusts[2].m_translate = vec2(R.m_min_point.x(), R.m_min_point.y() + R.m_corner_radii[Rect::minx_miny_corner].y());
      m_adjusts[2].m_shear = vec2(1.0f, -1.0f) * R.m_corner_radii[RoundedRect::minx_miny_corner];

      /* max-x/min-y */
      m_adjusts[3].m_translate = vec2(R.m_max_point.x(), R.m_min_point.y() + R.m_corner_radii[Rect::maxx_miny_corner].y());
      m_adjusts[3].m_shear = -R.m_corner_radii[Rect::maxx_miny_corner];

      if (pool)
        {
          m_packed_adjusts[0] = pool->create_packed_value(m_adjusts[0]);
          m_packed_adjusts[1] = pool->create_packed_value(m_adjusts[1]);
          m_packed_adjusts[2] = pool->create_packed_value(m_adjusts[2]);
          m_packed_adjusts[3] = pool->create_packed_value(m_adjusts[3]);
        }
    }

    fastuidraw::vecN<fastuidraw::PainterBrushAdjust, 4> m_adjusts;
    fastuidraw::vecN<ExtendedPool::PackedBrushAdjust, 4> m_packed_adjusts;
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

  class PackedAntiAliasEdgeData
  {
  public:
    fastuidraw::BoundingBox<float> m_box;
    fastuidraw::range_type<unsigned int> m_attrib_range;
    fastuidraw::range_type<unsigned int> m_index_range;
  };

  void
  pack_anti_alias_edge(const fastuidraw::vec2 &start, const fastuidraw::vec2 &end,
                       int z,
                       PackedAntiAliasEdgeData *out_data,
                       std::vector<fastuidraw::PainterAttribute> &dst_attribs,
                       std::vector<fastuidraw::PainterIndex> &dst_idx)
  {
    out_data->m_box.union_point(start);
    out_data->m_box.union_point(end);
    out_data->m_attrib_range.m_begin = dst_attribs.size();
    out_data->m_index_range.m_begin = dst_idx.size();
    pack_anti_alias_edge(start, end, z, dst_attribs, dst_idx);
    out_data->m_attrib_range.m_end = dst_attribs.size();
    out_data->m_index_range.m_end = dst_idx.size();
  }

  /* to prevent t-junctions when rects overlap, but do
   * not share corners, a RectWithSidePoints has
   * those additional points of neighbor rects (up to 2).
   */
  class RectWithSidePoints:public fastuidraw::Rect
  {
  public:
    template<bool reverse_ordering>
    class Side
    {
    public:
      Side(void):
        m_count(0)
      {}

      void
      insert(float f)
      {
        m_values[m_count++] = f;
        FASTUIDRAWassert(m_count <= 2);
        if (m_count == 2 && (m_values[0] < m_values[1]) == reverse_ordering)
          {
            std::swap(m_values[0], m_values[1]);
          }
      }

      unsigned int
      size(void) const { return m_count; }

      float
      operator[](unsigned int i) const
      {
        FASTUIDRAWassert(i < m_count);
        return m_values[i];
      }

    private:
      unsigned int m_count;
      fastuidraw::vecN<float, 2> m_values;
    };

    Side<false> m_min_y, m_max_x;
    Side<true> m_max_y, m_min_x;
  };

  class ClipperWorkRoom:fastuidraw::noncopyable
  {
  public:
    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_vec2s;
    fastuidraw::vecN<std::vector<fastuidraw::vec3>, 2> m_vec3s;
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
    std::vector<const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>* > m_shaders;
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
    fastuidraw::BoundingBox<float> m_normalized_device_coords_bounding_box;
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
    class PerCorner
    {
    public:
      OpaqueFillWorkRoom m_opaque_fill;
      AntiAliasFillWorkRoom m_aa_fuzz;
      FillSubsetWorkRoom m_subsets;
      const fastuidraw::FilledPath* m_filled_path;
    };

    fastuidraw::vecN<PerCorner, 4> m_per_corner;
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

  class ComputeClipIntersectRectWorkRoom:fastuidraw::noncopyable
  {
  public:
    fastuidraw::vecN<fastuidraw::vec3, 4> m_rect_pts;
    fastuidraw::vecN<std::vector<fastuidraw::vec3>, 2> m_scratch;
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
    ComputeClipIntersectRectWorkRoom m_compute_clip_intersect_rect;
  };

  class EffectsStackEntry
  {
  public:
    unsigned int m_effects_layer_stack_size;
    unsigned int m_state_stack_size;
  };

  class PainterPrivate
  {
  public:
    typedef fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> ItemShaderRef;
    typedef const ItemShaderRef ConstItemShaderRef;
    typedef ConstItemShaderRef *ConstItemShaderRefPtr;

    explicit
    PainterPrivate(const fastuidraw::reference_counted_ptr<fastuidraw::PainterEngine> &backend_factory);

    ~PainterPrivate();

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

    fastuidraw::BoundingBox<float>
    compute_clip_intersect_rect(const fastuidraw::Rect &logical_rect,
                                float additional_pixel_slack,
                                float additional_logical_slack);

    void
    begin_coverage_buffer(void);

    void
    begin_coverage_buffer_normalized_rect(const fastuidraw::Rect &normalized_rect,
                                          bool non_empty);

    void
    end_coverage_buffer(void);

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
                    bool apply_anti_aliasing);

    void
    stroke_path_common(const fastuidraw::PainterStrokeShader &shader,
                       const fastuidraw::PainterData &draw,
                       const fastuidraw::StrokedPath &path, float thresh,
                       enum fastuidraw::Painter::cap_style cp,
                       enum fastuidraw::Painter::join_style js,
                       bool apply_anti_aliasing);

    fastuidraw::BoundingBox<float>
    compute_bounding_box_of_path(const fastuidraw::StrokedPath &stroked_path,
                                 fastuidraw::c_array<const unsigned int> stroked_subset_ids,
                                 float pixels_additional_room, float item_space_additional_room);

    fastuidraw::BoundingBox<float>
    compute_bounding_box_of_miter_joins(fastuidraw::c_array<const fastuidraw::vec2> join_positions,
                                        float miter_pixels_additional_room,
                                        float miter_item_space_additional_room);

    fastuidraw::BoundingBox<float>
    compute_bounding_box_of_stroked_path(const fastuidraw::StrokedPath &stroked_path,
                                         fastuidraw::c_array<const unsigned int> stroked_subset_ids,
                                         fastuidraw::c_array<const float> geometry_inflation,
                                         enum fastuidraw::Painter::join_style js,
                                         fastuidraw::c_array<const fastuidraw::vec2> join_positions);

    void
    pre_draw_anti_alias_fuzz(const fastuidraw::FilledPath &filled_path,
                             const FillSubsetWorkRoom &fill_subset,
                             AntiAliasFillWorkRoom *output);

    void
    draw_anti_alias_fuzz(const fastuidraw::PainterFillShader &shader,
                         const fastuidraw::PainterData &draw,
                         const AntiAliasFillWorkRoom &data,
                         int z);

    void
    draw_anti_alias_fuzz(const fastuidraw::PainterFillShader &shader,
                         const fastuidraw::PainterData &draw,
                         const AntiAliasFillWorkRoom &data)
    {
      draw_anti_alias_fuzz(shader, draw, data, m_current_z);
    }

    template<typename T>
    void
    fill_path(const fastuidraw::PainterFillShader &shader,
              const fastuidraw::PainterData &draw,
              const fastuidraw::FilledPath &filled_path,
              const T &fill_rule,
              bool apply_anti_aliasing);

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
                      bool apply_anti_aliasing);

    void
    ready_non_aa_polygon_attribs(fastuidraw::c_array<const fastuidraw::vec2> pts);

    void
    ready_aa_polygon_attribs(fastuidraw::c_array<const fastuidraw::vec2> pts,
                             bool apply_anti_aliasing);

    int
    fill_convex_polygon(bool allow_sw_clipping,
                        const fastuidraw::PainterFillShader &shader,
                        const fastuidraw::PainterData &draw,
                        fastuidraw::c_array<const fastuidraw::vec2> pts,
                        bool apply_anti_aliasing, int z);

    int
    fill_convex_polygon(const fastuidraw::PainterFillShader &shader,
                        const fastuidraw::PainterData &draw,
                        fastuidraw::c_array<const fastuidraw::vec2> pts,
                        bool apply_anti_aliasing, int z)
    {
      return fill_convex_polygon(true, shader, draw, pts, apply_anti_aliasing, z);
    }

    int
    fill_convex_polygon(bool allow_sw_clipping,
                        const fastuidraw::PainterFillShader &shader,
                        const fastuidraw::PainterData &draw,
                        fastuidraw::c_array<const fastuidraw::vec2> pts,
                        bool apply_anti_aliasing)
    {
      return fill_convex_polygon(allow_sw_clipping, shader, draw, pts, apply_anti_aliasing, m_current_z);
    }

    int
    fill_convex_polygon(const fastuidraw::PainterFillShader &shader,
                        const fastuidraw::PainterData &draw,
                        fastuidraw::c_array<const fastuidraw::vec2> pts,
                        bool apply_anti_aliasing)
    {
      return fill_convex_polygon(true, shader, draw, pts, apply_anti_aliasing, m_current_z);
    }

    int
    fill_rect(const fastuidraw::PainterFillShader &shader,
              const fastuidraw::PainterData &draw,
              const fastuidraw::Rect &rect,
              bool apply_anti_aliasing,
              int z)
    {
      using namespace fastuidraw;

      vecN<vec2, 4> pts;
      pts[0] = vec2(rect.m_min_point.x(), rect.m_min_point.y());
      pts[1] = vec2(rect.m_min_point.x(), rect.m_max_point.y());
      pts[2] = vec2(rect.m_max_point.x(), rect.m_max_point.y());
      pts[3] = vec2(rect.m_max_point.x(), rect.m_min_point.y());
      return fill_convex_polygon(shader, draw, pts, apply_anti_aliasing, z);
    }

    void
    fill_rect_with_side_points(const fastuidraw::PainterFillShader &shader,
                               const fastuidraw::PainterData &draw,
                               const RectWithSidePoints &rect,
                               bool apply_anti_aliasing,
                               int z)
    {
      using namespace fastuidraw;
      vecN<vec2, 12> pts;
      unsigned int c(0);

      pts[c++] = rect.point(Rect::minx_miny_corner);
      for (unsigned int i = 0; i < rect.m_min_y.size(); ++i)
        {
          pts[c++] = vec2(rect.m_min_y[i], rect.m_min_point.y());
        }

      pts[c++] = rect.point(Rect::maxx_miny_corner);
      for(unsigned int i = 0; i < rect.m_max_x.size(); ++i)
        {
          pts[c++] = vec2(rect.m_max_point.x(), rect.m_max_x[i]);
        }

      pts[c++] = rect.point(Rect::maxx_maxy_corner);
      for (unsigned int i = 0; i < rect.m_max_y.size(); ++i)
        {
          pts[c++] = vec2(rect.m_max_y[i], rect.m_max_point.y());
        }

      pts[c++] = rect.point(Rect::minx_maxy_corner);
      for(unsigned int i = 0; i < rect.m_min_x.size(); ++i)
        {
          pts[c++] = vec2(rect.m_min_point.x(), rect.m_min_x[i]);
        }

      fill_convex_polygon(shader, draw,
                          c_array<const vec2>(pts).sub_array(0, c),
                          apply_anti_aliasing, z);
    }

    void
    draw_half_plane_complement(const fastuidraw::PainterFillShader &shader,
                               const fastuidraw::PainterData &draw,
                               const fastuidraw::vec3 &plane);

    float
    compute_magnification(const fastuidraw::Rect &rect);

    float
    compute_max_magnification_at_clip_points(fastuidraw::c_array<const fastuidraw::vec3> poly);

    float
    compute_magnification(const fastuidraw::Path &path);

    float
    compute_path_thresh(const fastuidraw::Path &path);

    float
    compute_path_thresh(const fastuidraw::Path &path,
                        const fastuidraw::c_array<const fastuidraw::generic_data> shader_data,
                        const fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase> &selector,
                        float &thresh);

    unsigned int
    select_subsets(const fastuidraw::FilledPath &path,
                   fastuidraw::c_array<unsigned int> dst);

    unsigned int
    select_subsets(const fastuidraw::StrokedPath &path,
                   fastuidraw::c_array<const float> geometry_inflation,
                   fastuidraw::c_array<unsigned int> dst);

    void
    select_chunks(const fastuidraw::StrokedCapsJoins &caps_joins,
                  fastuidraw::c_array<const float> geometry_inflation,
                  enum fastuidraw::Painter::join_style js,
                  enum fastuidraw::Painter::cap_style cp,
                  fastuidraw::StrokedCapsJoins::ChunkSet *dst);

    const fastuidraw::StrokedPath*
    select_stroked_path(const fastuidraw::Path &path,
                        const fastuidraw::PainterStrokeShader &shader,
                        const fastuidraw::PainterData &draw,
                        bool apply_anti_aliasing,
                        enum fastuidraw::Painter::stroking_method_t stroking_method,
                        float &out_thresh);

    const fastuidraw::FilledPath&
    select_filled_path(const fastuidraw::Path &path);

    fastuidraw::PainterPacker*
    packer(void)
    {
      return m_effects_layer_stack.empty() ?
        m_root_packer.get() :
        m_effects_layer_stack.back().m_packer;
    }

    fastuidraw::PainterPacker*
    deferred_coverage_packer(void)
    {
      return m_deferred_coverage_stack.empty() ?
        nullptr :
        m_deferred_coverage_stack.back().packer();
    }

    const ExtendedPool::PackedItemMatrix&
    identity_matrix(void)
    {
      return m_effects_layer_stack.empty() ?
        m_root_identity_matrix :
        m_effects_layer_stack.back().m_identity_matrix;
    }

    fastuidraw::GlyphRenderer
    compute_glyph_renderer(float pixel_size,
                           const fastuidraw::Painter::GlyphRendererChooser &chooser);

    DefaultGlyphRendererChooser m_default_glyph_renderer_chooser;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterPacker> m_root_packer;
    ExtendedPool::PackedItemMatrix m_root_identity_matrix;
    fastuidraw::vecN<unsigned int, fastuidraw::PainterPacker::num_stats> m_stats;
    fastuidraw::PainterSurface::Viewport m_viewport;
    fastuidraw::vec2 m_viewport_dimensions;
    fastuidraw::vec2 m_one_pixel_width;
    float m_curve_flatness;
    int m_current_z, m_draw_data_added_count;
    ClipRectState m_clip_rect_state;
    std::vector<occluder_stack_entry> m_occluder_stack;
    std::vector<state_stack_entry> m_state_stack;
    EffectsLayerFactory m_effects_layer_factory;
    std::vector<EffectsLayer> m_effects_layer_stack;
    std::vector<EffectsStackEntry> m_effects_stack;
    DeferredCoverageBufferStackEntryFactory m_deferred_coverage_stack_entry_factory;
    std::vector<DeferredCoverageBufferStackEntry> m_deferred_coverage_stack;
    std::vector<const fastuidraw::PainterSurface*> m_active_surfaces;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterEngine> m_backend_factory;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend> m_backend;
    fastuidraw::PainterEngine::PerformanceHints m_hints;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterEffectColorModulate> m_color_modulate_fx;
    fastuidraw::PainterShaderSet m_default_shaders;
    fastuidraw::PainterBrushShader *m_default_brush_shader;
    ExtendedPool m_pool;
    fastuidraw::PainterData::brush_value m_black_brush;
    const ExtendedPool::PackedBrushAdjust *m_current_brush_adjust;
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
// ClipRectState methods
void
ClipRectState::
reset(const fastuidraw::PainterSurface::Viewport &vwp,
      fastuidraw::ivec2 surface_dims)
{
  using namespace fastuidraw;

  PainterClipEquations clip_eq;

  m_all_content_culled = false;
  m_item_matrix_transition_tricky = false;
  m_inverse_transpose_ready = true;
  m_matrix_type = non_scaling_matrix;
  m_viewport_dimensions = vec2(vwp.m_dimensions);

  vwp.compute_clip_equations(surface_dims, &clip_eq.m_clip_equations);
  m_clip_rect.m_enabled = false;
  clip_equations(clip_eq);

  /* set the fields, without marking the transition as tricky
   * but resetting the singular values as not yet computed.
   */
  item_matrix(float3x3(), false, non_scaling_matrix, -1.0f);
}

const fastuidraw::vec2&
ClipRectState::
item_matrix_singular_values(void) const
{
  FASTUIDRAWassert(!m_override_matrix_state);
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
ClipRectState::
item_matrix(const fastuidraw::float3x3 &v,
            bool trick_transition, enum matrix_type_t M,
            float singular_value_scaled)
{
  FASTUIDRAWassert(!m_override_matrix_state);

  m_item_matrix_transition_tricky = m_item_matrix_transition_tricky || trick_transition;
  m_inverse_transpose_ready = false;
  m_item_matrix.m_item_matrix = v;
  m_item_matrix_state.reset();
  m_item_matrix_coverage_buffer_state.reset();
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
ClipRectState::
matrix_type(void) const
{
  FASTUIDRAWassert(!m_override_matrix_state);
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
ClipRectState::
item_matrix_inverse_transpose(void) const
{
  FASTUIDRAWassert(!m_override_matrix_state);
  if (!m_inverse_transpose_ready)
    {
      m_inverse_transpose_ready = true;
      m_item_matrix.m_item_matrix.inverse_transpose(m_item_matrix_inverse_transpose);
    }
  return m_item_matrix_inverse_transpose;
}

void
ClipRectState::
set_clip_equations_to_clip_rect(enum rect_coordinate_type_t c)
{
  ExtendedPool::PackedClipEquations null;
  set_clip_equations_to_clip_rect(null, c);
}

std::bitset<4>
ClipRectState::
set_clip_equations_to_clip_rect(const ExtendedPool::PackedClipEquations &pcl,
                                enum rect_coordinate_type_t c)
{
  FASTUIDRAWassert(!m_override_matrix_state);
  if (m_clip_rect.empty())
    {
      m_all_content_culled = true;
      return std::bitset<4>();
    }

  m_item_matrix_transition_tricky = false;

  const fastuidraw::float3x3 &m(item_matrix());
  fastuidraw::vecN<fastuidraw::vec3, 4> rect_pts;
  fastuidraw::PainterClipEquations cl;

  rect_pts[0] = fastuidraw::vec3(m_clip_rect.m_min.x(), m_clip_rect.m_min.y(), 1.0f);
  rect_pts[1] = fastuidraw::vec3(m_clip_rect.m_min.x(), m_clip_rect.m_max.y(), 1.0f);
  rect_pts[2] = fastuidraw::vec3(m_clip_rect.m_max.x(), m_clip_rect.m_max.y(), 1.0f);
  rect_pts[3] = fastuidraw::vec3(m_clip_rect.m_max.x(), m_clip_rect.m_min.y(), 1.0f);

  if (c == rect_in_local_coordinates)
    {
      for (fastuidraw::vec3 &pt : rect_pts)
        {
          pt = m * pt;
        }
    }
  compute_clip_equations_from_clip_polygon(rect_pts, cl.m_clip_equations);
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

  /* see if the vertices of the clipping rectangle are all within the
   * passed clipped equations.
   */
  const fastuidraw::PainterClipEquations &eq(pcl.unpacked_value());
  std::bitset<4> return_value;

  /* return_value[i] is true exactly when each point of the rectangle is inside
   *                 the i'th clip equation.
   */
  for(int i = 0; i < 4; ++i)
    {
      return_value[i] = dot(rect_pts[0], eq.m_clip_equations[i]) >= 0.0f
        && dot(rect_pts[1], eq.m_clip_equations[i]) >= 0.0f
        && dot(rect_pts[2], eq.m_clip_equations[i]) >= 0.0f
        && dot(rect_pts[3], eq.m_clip_equations[i]) >= 0.0f;
    }

  return return_value;
}

void
ClipRectState::
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
ClipRectState::
poly_is_culled(fastuidraw::c_array<const fastuidraw::vec3> pts) const
{
  /* apply the current transformation matrix to
   * the corners of the clipping rectangle and check
   * if there is a clipping plane for which all
   * those points are on the wrong size.
   */
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

void
ClipEquationStore::
reset_current_to_rect(const fastuidraw::Rect &R)
{
  using namespace fastuidraw;
  vecN<vec2, 4> pts;

  FASTUIDRAWassert(R.m_min_point.x() >= -1.0f);
  FASTUIDRAWassert(R.m_min_point.y() >= -1.0f);
  FASTUIDRAWassert(R.m_max_point.x() <= +1.0f);
  FASTUIDRAWassert(R.m_max_point.y() <= +1.0f);
  FASTUIDRAWassert(R.m_max_point.x() >= R.m_min_point.x());
  FASTUIDRAWassert(R.m_max_point.y() >= R.m_min_point.y());

  /* the Rect R is in normalized device coordinates, so
   * the transformation to apply is just the identity
   */
  pts[0] = vec2(R.m_min_point.x(), R.m_min_point.y());
  pts[1] = vec2(R.m_min_point.x(), R.m_max_point.y());
  pts[2] = vec2(R.m_max_point.x(), R.m_max_point.y());
  pts[3] = vec2(R.m_max_point.x(), R.m_min_point.y());

  m_poly.current().clear();
  m_clip.current().clear();

  /* the current clip region in normalized device
   * coordinates becomes exactly R:
   */
  m_current_bb = BoundingBox<float>();
  m_current_bb.union_point(R.m_min_point);
  m_current_bb.union_point(R.m_max_point);

  m_poly.current().push_back(vec3(R.m_min_point.x(), R.m_min_point.y(), 1.0f));
  m_poly.current().push_back(vec3(R.m_min_point.x(), R.m_max_point.y(), 1.0f));
  m_poly.current().push_back(vec3(R.m_max_point.x(), R.m_max_point.y(), 1.0f));
  m_poly.current().push_back(vec3(R.m_max_point.x(), R.m_min_point.y(), 1.0f));

  /* The clip-equations are simple as well:
   *    R.m_min_poimt.x() <= x <= R.m_max_point.x()
   *    R.m_min_poimt.y() <= y <= R.m_max_point.y()
   * becomes (using w = 1)
   *    +1.0 * x + -R.m_min_point.x() * w >= 0
   *    -1.0 * x +  R.m_max_point.x() * w >= 0
   *    +1.0 * y + -R.m_min_point.y() * w >= 0
   *    -1.0 * y +  R.m_max_point.y() * w >= 0
   */
  m_clip.current().push_back(vec3(+1.0f, 0.0f, -R.m_min_point.x()));
  m_clip.current().push_back(vec3(-1.0f, 0.0f, +R.m_max_point.x()));
  m_clip.current().push_back(vec3(0.0f, +1.0f, -R.m_min_point.y()));
  m_clip.current().push_back(vec3(0.0f, -1.0f, +R.m_max_point.y()));
}

bool //returns true if clipping becomes so that all is clipped
ClipEquationStore::
intersect_current_against_polygon(fastuidraw::c_array<const fastuidraw::vec3> poly)
{
  using namespace fastuidraw;

  /* clip poly against the current clipping equations*/
  c_array<const vec3> clipped_poly;
  detail::clip_against_planes(current(), poly, &clipped_poly, m_scratch);

  m_poly.current().resize(clipped_poly.size());
  m_clip.current().resize(clipped_poly.size());
  m_current_bb = BoundingBox<float>();
  for (unsigned int i = 0; i < clipped_poly.size(); ++i)
    {
      const vec3 &clip_pt(clipped_poly[i]);

      m_poly.current()[i] = clip_pt;
      m_current_bb.union_point(vec2(clip_pt) / clip_pt.z());
    }

  /* because of floating point round-off (in particular
   * from the perspective-divide), we need to bound the
   * m_current_bb against the normalized-rect.
   */
  m_current_bb.intersect_against(BoundingBox<float>(vec2(-1.0f, -1.0f),
                                                    vec2(+1.0f, +1.0f)));
  compute_clip_equations_from_clip_polygon(clipped_poly,
                                           make_c_array(m_clip.current()));
  return clipped_poly.empty();
}

///////////////////////////////////////
// BufferRect methods
BufferRect::
BufferRect(const fastuidraw::Rect &normalized_rect,
           fastuidraw::ivec2 useable_size,
           PainterPrivate *d)
{
  using namespace fastuidraw;

  /* we need to discretize the rectangle, so that
   * the size is correct. Using the normalized_rect.size()
   * scaled is actually WRONG. Rather we need to look
   * at the range of -sample- points the rect hits.
   */
  m_fbl = d->m_viewport.compute_viewport_coordinates(normalized_rect.m_min_point);
  m_ftr = d->m_viewport.compute_viewport_coordinates(normalized_rect.m_max_point);

  m_bl = fastuidraw::ivec2(m_fbl);
  m_tr = fastuidraw::ivec2(m_ftr);

  m_dims = m_tr - m_bl;
  if (m_ftr.x() > m_tr.x() && m_dims.x() < useable_size.x())
    {
      ++m_dims.x();
      ++m_tr.x();
    }
  if (m_ftr.y() > m_tr.y() && m_dims.y() < useable_size.y())
    {
      ++m_dims.y();
      ++m_tr.y();
    }

  m_fbl = vec2(m_bl);
  m_ftr = vec2(m_tr);

  /* we are NOT taking the original normalized rect,
   * instead we are taking the normalized rect made
   * from bl, tr which are the discretizations of
   * the original rect to pixels.
   */
  m_normalized_rect
    .min_point(d->m_viewport.compute_normalized_device_coords_from_viewport_coords(m_fbl))
    .max_point(d->m_viewport.compute_normalized_device_coords_from_viewport_coords(m_ftr));

  m_pixel_rect
    .min_point(m_bl)
    .max_point(m_tr);
}

///////////////////////////////////
// EffectsLayer methods
void
EffectsLayer::
blit_rect(const fastuidraw::reference_counted_ptr<fastuidraw::PainterEffectPass> &fx_pass,
          fastuidraw::vec2 dims, fastuidraw::Painter *p)
{
  using namespace fastuidraw;
  Rect brush_rect;

  brush_rect
    .min_point(m_brush_translate + PainterSurface::Viewport::compute_viewport_coordinates(m_normalized_rect.m_min_point, dims))
    .max_point(m_brush_translate + PainterSurface::Viewport::compute_viewport_coordinates(m_normalized_rect.m_max_point, dims));

  /* TODO: instead of doing a full-blown Painter::save(), we should
   * instead just override the current transformation.
   */
  p->save();

  p->transformation(float_orthogonal_projection_params(0, dims.x(), 0, dims.y()));
  p->translate(-m_brush_translate);

  p->fill_rect(PainterData(fx_pass->brush(m_image, brush_rect)),
               brush_rect,
               false);

  p->restore();
}

////////////////////////////////////////
// EffectsLayerFactory methods
void
EffectsLayerFactory::
begin(fastuidraw::PainterSurface &surface)
{
  bool clear_buffers;
  const fastuidraw::PainterSurface::Viewport &vwp(surface.viewport());
  fastuidraw::ivec2 surface_sz(surface.dimensions());

  /* We can freely translate normalized device coords, but we
   * cannot scale them. Thus set our viewport to the same size
   * as the passed viewport but the origin at (0, 0).
   */
  m_effects_buffer_viewport.m_origin = fastuidraw::ivec2(0, 0);
  m_effects_buffer_viewport.m_dimensions = vwp.m_dimensions;

  /* We can only use the portions of the backing store
   * that is within m_ransparency_buffer_viewport.
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
}

EffectsLayer
EffectsLayerFactory::
fetch(unsigned int effects_depth,
      const fastuidraw::Rect &normalized_rect,
      PainterPrivate *d)
{
  using namespace fastuidraw;

  EffectsLayer return_value;
  BufferRect buffer_rect(normalized_rect, m_current_backing_useable_size, d);
  ivec2 rect(-1, -1);

  if (effects_depth >= m_per_active_depth.size())
    {
      m_per_active_depth.resize(effects_depth + 1);
    }

  for (unsigned int i = 0, endi = m_per_active_depth[effects_depth].size();
       (rect.x() < 0 || rect.y() < 0) && i < endi; ++i)
    {
      rect = m_per_active_depth[effects_depth][i]->m_rect_atlas.add_rectangle(buffer_rect.m_dims);
      if (rect.x() >= 0 && rect.y() >= 0)
        {
          return_value.m_image = m_per_active_depth[effects_depth][i]->m_image.get();
          return_value.m_packer = m_per_active_depth[effects_depth][i]->m_packer.get();
        }
    }

  if (rect.x() < 0 || rect.y() < 0)
    {
      reference_counted_ptr<EffectsBuffer> TB;

      if (m_unused_buffers.empty())
        {
          reference_counted_ptr<PainterPacker> packer;
          reference_counted_ptr<PainterSurface> surface;
          reference_counted_ptr<const Image> image;

          packer = FASTUIDRAWnew PainterPacker(d->m_default_brush_shader,
                                               d->m_stats, d->m_backend,
                                               d->m_backend_factory->configuration_base());
          surface = d->m_backend_factory->create_surface(m_current_backing_size,
                                                         PainterSurface::color_buffer_type);
          surface->clear_color(vec4(0.0f, 0.0f, 0.0f, 0.0f));
          image = surface->image(d->m_backend_factory->image_atlas());
          TB = FASTUIDRAWnew EffectsBuffer(packer, surface, image, m_current_backing_useable_size);
        }
      else
        {
          TB = m_unused_buffers.back();
          m_unused_buffers.pop_back();
        }

      ++d->m_stats[Painter::num_render_targets];
      TB->m_depth = effects_depth;
      TB->m_surface->viewport(m_effects_buffer_viewport);
      m_per_active_depth[effects_depth].push_back(TB);
      d->m_active_surfaces.push_back(TB->m_surface.get());

      rect = TB->m_rect_atlas.add_rectangle(buffer_rect.m_dims);
      return_value.m_image = TB->m_image.get();
      return_value.m_packer = TB->m_packer.get();
      return_value.m_packer->begin(TB->m_surface, true);
    }

  FASTUIDRAWassert(return_value.m_image);
  FASTUIDRAWassert(return_value.m_packer);
  FASTUIDRAWassert(rect.x() >= 0 && rect.y() >= 0);

  return_value.m_normalized_rect = buffer_rect.m_normalized_rect;
  return_value.m_pixel_rect
    .min_point(buffer_rect.m_bl)
    .max_point(buffer_rect.m_tr);
  return_value.m_min_corner_in_surface = rect;

  /* we need to have that:
   *   return_value.m_normalized_translate + normalized_rect = R
   * where R is rect in normalized device coordinates.
   */
  vec2 Rndc;
  Rndc = m_effects_buffer_viewport.compute_normalized_device_coords(vec2(rect));
  return_value.m_normalized_translate = Rndc - return_value.m_normalized_rect.m_min_point;

  /* the drawing of the fill_rect in end_layer() draws normalized_rect
   * scaled by m_effects_buffer_viewport.m_dimensions. We need to
   * have that
   *   return_value.m_brush_translate + S = rect
   * where S is normalized_rect in -pixel- coordinates
   */
  vec2 Spc;
  Spc = m_effects_buffer_viewport.compute_pixel_coordinates(return_value.m_normalized_rect.m_min_point);
  return_value.m_brush_translate = vec2(rect) - Spc ;

  PainterItemMatrix M;
  M.m_normalized_translate = return_value.m_normalized_translate;
  return_value.m_identity_matrix = d->m_pool.create_packed_value(M);

  return return_value;
}

void
EffectsLayerFactory::
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

/////////////////////////////////
// DeferredCoverageBufferStackEntry methods
void
DeferredCoverageBufferStackEntry::
update_coverage_buffer_offset(const PainterPrivate *d)
{
  using namespace fastuidraw;

  /* We need to give the translation value from pixels of
   * m_packer->surface() to return_value.m_surface. The
   * convention is that pixel coordinates are coordinates
   * of the -SURFACE-, not relative to the viewport.
   * The value m_coverage_buffer_offset needs to satsify:
   *
   *   m_coverage_buffer_offset + m_color_surface_viewport.m_origin + T
   *    == m_coverage_buffer_viewport.m_origin + rect
   *
   * where T is
   *   if d->m_effects_layer_stack.empty()  --> T = (0, 0)
   *   if !d->m_effects_layer_stack.empty() --> T = m_min_corner_in_surface - m_pixel_rect.min_point()
   */
  ivec2 T, color_surface_viewport_origin;
  if (d->m_effects_layer_stack.empty())
    {
      T = ivec2(0, 0);
      color_surface_viewport_origin = d->m_viewport.m_origin;
    }
  else
    {
      T = d->m_effects_layer_stack.back().m_min_corner_in_surface
        - d->m_effects_layer_stack.back().m_pixel_rect.m_min_point;
      color_surface_viewport_origin = ivec2(0, 0);
    }

  m_coverage_buffer_offset =
    d->m_deferred_coverage_stack_entry_factory.coverage_buffer_viewport().m_origin
    - m_root_surface_min_corner
    + m_min_corner_in_surface - color_surface_viewport_origin - T;
}

////////////////////////////////////////////////
// DeferredCoverageBufferStackEntryFactory methods
void
DeferredCoverageBufferStackEntryFactory::
begin(fastuidraw::PainterSurface &surface)
{
  bool clear_buffers;

  /* We can freely translate normalized device coords, but we
   * cannot scale them. Thus set our viewport to the same size
   * as the passed viewport but the origin at (0, 0).
   */
  m_coverage_buffer_viewport.m_origin = fastuidraw::ivec2(0, 0);
  m_coverage_buffer_viewport.m_dimensions = surface.viewport().m_dimensions;

  m_current_backing_useable_size = surface.compute_visible_dimensions();

  clear_buffers = (m_current_backing_useable_size.x() > m_current_backing_size.x())
    || (m_current_backing_useable_size.y() > m_current_backing_size.y())
    || (m_current_backing_size.x() > 2 * m_current_backing_useable_size.x())
    || (m_current_backing_size.y() > 2 * m_current_backing_useable_size.y());

  if (clear_buffers)
    {
      m_active_buffers.clear();
      m_unused_buffers.clear();
      m_current_backing_size = m_current_backing_useable_size;
    }
  else
    {
      for (const auto &r : m_active_buffers)
        {
          r->m_rect_atlas.clear(m_current_backing_useable_size);
          m_unused_buffers.push_back(r);
        }
      m_active_buffers.clear();
    }
}

DeferredCoverageBufferStackEntry
DeferredCoverageBufferStackEntryFactory::
fetch(const fastuidraw::Rect &normalized_rect, PainterPrivate *d)
{
  using namespace fastuidraw;

  ivec2 rect;
  BufferRect buffer_rect(normalized_rect, m_current_backing_useable_size, d);
  PainterPacker *return_packer(nullptr);

  /* Tallocate a region from m_active_buffers (or m_unused_buffers) */
  for (unsigned int i = 0, endi = m_active_buffers.size();
       !return_packer && i < endi; ++i)
    {
      rect = m_active_buffers[i]->m_rect_atlas.add_rectangle(buffer_rect.m_dims);
      if (rect.x() >= 0 && rect.y() >= 0)
        {
          return_packer = m_active_buffers[i]->m_packer.get();
        }
    }

  if (!return_packer)
    {
      reference_counted_ptr<DeferredCoverageBuffer> TB;

      if (m_unused_buffers.empty())
        {
          reference_counted_ptr<PainterPacker> packer;
          reference_counted_ptr<PainterSurface> surface;

          packer = FASTUIDRAWnew PainterPacker(d->m_default_brush_shader,
                                               d->m_stats, d->m_backend,
                                               d->m_backend_factory->configuration_base());
          surface = d->m_backend_factory->create_surface(m_current_backing_size,
                                                         PainterSurface::deferred_coverage_buffer_type);
          surface->clear_color(vec4(0.0f, 0.0f, 0.0f, 0.0f));
          TB = FASTUIDRAWnew DeferredCoverageBuffer(packer, surface, m_current_backing_useable_size);
        }
      else
        {
          TB = m_unused_buffers.back();
          m_unused_buffers.pop_back();
        }

      ++d->m_stats[Painter::num_render_targets];
      TB->m_surface->viewport(m_coverage_buffer_viewport);
      m_active_buffers.push_back(TB);

      rect = TB->m_rect_atlas.add_rectangle(buffer_rect.m_dims);
      return_packer = TB->m_packer.get();
      return_packer->begin(TB->m_surface, true);
      d->m_active_surfaces.push_back(TB->m_surface.get());
    }

  /* compute the clip equations made from rect. */
  PainterClipEquations clip_eq;
  clip_eq.m_clip_equations[0] = vec3( 1.0f,  0.0f, -normalized_rect.m_min_point.x());
  clip_eq.m_clip_equations[1] = vec3(-1.0f,  0.0f,  normalized_rect.m_max_point.x());
  clip_eq.m_clip_equations[2] = vec3( 0.0f,  1.0f, -normalized_rect.m_min_point.y());
  clip_eq.m_clip_equations[3] = vec3( 0.0f, -1.0f,  normalized_rect.m_max_point.y());

  DeferredCoverageBufferStackEntry return_value(return_packer, rect, buffer_rect,
                                                m_coverage_buffer_viewport,
                                                d->m_pool.create_packed_value(clip_eq));

  return return_value;
}

void
DeferredCoverageBufferStackEntryFactory::
end(void)
{
  for (const auto &r : m_active_buffers)
    {
      r->m_packer->end();
    }
}

//////////////////////////////////
// PainterPrivate methods
PainterPrivate::
PainterPrivate(const fastuidraw::reference_counted_ptr<fastuidraw::PainterEngine> &backend_factory):
  m_viewport_dimensions(1.0f, 1.0f),
  m_one_pixel_width(1.0f, 1.0f),
  m_curve_flatness(0.5f),
  m_backend_factory(backend_factory),
  m_backend(backend_factory->create_backend()),
  m_hints(backend_factory->hints()),
  m_current_brush_adjust(nullptr)
{
  /* By calling PainterBackend::default_shaders(), we make the shaders
   * registered. By setting m_default_shaders to its return value,
   * and using that for the return value of Painter::default_shaders(),
   * we skip the check in PainterBackend::default_shaders() to register
   * the shaders as well.
   */
  m_default_shaders = m_backend_factory->default_shaders();
  m_default_brush_shader = m_default_shaders.brush_shader().get();
  m_color_modulate_fx = FASTUIDRAWnew fastuidraw::PainterEffectColorModulate();
  m_root_packer = FASTUIDRAWnew fastuidraw::PainterPacker(m_default_brush_shader, m_stats, m_backend,
                                                          m_backend_factory->configuration_base());
  m_black_brush = m_pool.create_packed_brush(fastuidraw::PainterBrush()
                                             .color(0.0f, 0.0f, 0.0f, 0.0f));
  m_root_identity_matrix = m_pool.create_packed_value(fastuidraw::PainterItemMatrix());
  m_current_z = 1;
  m_draw_data_added_count = 0;
  m_max_attribs_per_block = m_backend->attribs_per_mapping();
  m_max_indices_per_block = m_backend->indices_per_mapping();

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

PainterPrivate::
~PainterPrivate()
{
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

fastuidraw::BoundingBox<float>
PainterPrivate::
compute_clip_intersect_rect(const fastuidraw::Rect &logical_rect,
                            float additional_pixel_slack,
                            float additional_logical_slack)
{
  using namespace fastuidraw;

  const float3x3 &transform(m_clip_rect_state.item_matrix());
  c_array<const vec3> poly;
  fastuidraw::BoundingBox<float> return_value;

  FASTUIDRAWassert(additional_pixel_slack >= 0.0f);
  FASTUIDRAWassert(additional_logical_slack >= 0.0f);
  FASTUIDRAWassert(logical_rect.m_max_point.x() >= logical_rect.m_min_point.x());
  FASTUIDRAWassert(logical_rect.m_max_point.y() >= logical_rect.m_min_point.y());

  m_work_room.m_compute_clip_intersect_rect.m_rect_pts[0] =
    transform * vec3(logical_rect.m_min_point.x() - additional_logical_slack,
                     logical_rect.m_min_point.y() - additional_logical_slack,
                     1.0f);

  m_work_room.m_compute_clip_intersect_rect.m_rect_pts[1]
    = transform * vec3(logical_rect.m_min_point.x() - additional_logical_slack,
                       logical_rect.m_max_point.y() + additional_logical_slack,
                       1.0f);

  m_work_room.m_compute_clip_intersect_rect.m_rect_pts[2] =
    transform * vec3(logical_rect.m_max_point.x() + additional_logical_slack,
                     logical_rect.m_max_point.y() + additional_logical_slack,
                     1.0f);

  m_work_room.m_compute_clip_intersect_rect.m_rect_pts[3] =
    transform * vec3(logical_rect.m_max_point.x() + additional_logical_slack,
                     logical_rect.m_min_point.y() - additional_logical_slack,
                     1.0f);

  detail::clip_against_planes(m_clip_store.current(),
                              m_work_room.m_compute_clip_intersect_rect.m_rect_pts,
                              &poly, m_work_room.m_compute_clip_intersect_rect.m_scratch);

  for (const vec3 &clip_pt : poly)
    {
      float recip_z(1.0f / clip_pt.z());
      return_value.union_point(vec2(clip_pt) * recip_z);
    }

  /* convert additional_pixel_slack from pixel coordinates
   * to normalized-device coordinates
   */
  return_value.enlarge(additional_pixel_slack * m_one_pixel_width);

  /* intersect against the the current clipping bound */
  return_value.intersect_against(m_clip_store.current_bb());

  return return_value;
}

void
PainterPrivate::
begin_coverage_buffer_normalized_rect(const fastuidraw::Rect &normalized_rect,
                                      bool non_empty)
{
  if (non_empty)
    {
      /* intersect normalized_rect with the current */
      m_deferred_coverage_stack.push_back(m_deferred_coverage_stack_entry_factory.fetch(normalized_rect, this));
      m_deferred_coverage_stack.back().update_coverage_buffer_offset(this);
      m_clip_rect_state.coverage_buffer_normalized_translate(m_deferred_coverage_stack.back().normalized_translate());
    }
  else
    {
      m_deferred_coverage_stack.push_back(DeferredCoverageBufferStackEntry());
    }
  ++m_stats[fastuidraw::Painter::num_deferred_coverages];
}

void
PainterPrivate::
begin_coverage_buffer(void)
{
  bool non_empty;

  non_empty = !m_clip_rect_state.m_all_content_culled
    && !m_clip_store.current_poly().empty()
    && !m_clip_store.current_bb().empty();
  begin_coverage_buffer_normalized_rect(m_clip_store.current_bb().as_rect(), non_empty);
}

void
PainterPrivate::
end_coverage_buffer(void)
{
  FASTUIDRAWassert(!m_deferred_coverage_stack.empty());
  FASTUIDRAWassert(m_state_stack.empty() || m_state_stack.back().m_deferred_coverage_buffer_depth < m_deferred_coverage_stack.size());

  m_deferred_coverage_stack.pop_back();
  if (!m_deferred_coverage_stack.empty() && m_deferred_coverage_stack.back().packer())
    {
      m_deferred_coverage_stack.back().update_coverage_buffer_offset(this);
      m_clip_rect_state.coverage_buffer_normalized_translate(m_deferred_coverage_stack.back().normalized_translate());
    }
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
  using namespace fastuidraw;

  /* early out to avoid clipping the rect against the
   * clip-equations
   */
  const float3x3 &m(m_clip_rect_state.item_matrix());
  if (!matrix_has_perspective(m))
    {
      return m_clip_rect_state.item_matrix_operator_norm() / t_abs(m(2, 2));
    }

  /* Rect is in local coordinates; get the rect in clip-coordinates */
  vecN<vec3, 4> poly;
  c_array<const vec3> clipped_poly;

  poly[0] = m * vec3(rect.m_min_point.x(), rect.m_min_point.y(), 1.0f);
  poly[1] = m * vec3(rect.m_min_point.x(), rect.m_max_point.y(), 1.0f);
  poly[2] = m * vec3(rect.m_max_point.x(), rect.m_max_point.y(), 1.0f);
  poly[3] = m * vec3(rect.m_max_point.x(), rect.m_min_point.y(), 1.0f);

  /* clip poly against the clip-equations */
  detail::clip_against_planes(m_clip_store.current(), poly,
                              &clipped_poly, m_work_room.m_clipper.m_vec3s);

  /* now compute the worst case magnification */
  return compute_max_magnification_at_clip_points(clipped_poly);
}

float
PainterPrivate::
compute_max_magnification_at_clip_points(fastuidraw::c_array<const fastuidraw::vec3> poly)
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
  for(const vec3 &q: poly)
    {
      const float tol_w = 1e-6;

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
                    const fastuidraw::c_array<const fastuidraw::generic_data> shader_data,
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
                    bool apply_anti_aliasing,
                    enum fastuidraw::Painter::stroking_method_t stroking_method,
                    float &thresh)
{
  using namespace fastuidraw;

  float t;
  c_array<const generic_data> data(draw.m_item_shader_data.m_packed_value.packed_data());
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

  if (stroking_method == Painter::stroking_method_fastest)
    {
      stroking_method = (apply_anti_aliasing) ?
        shader.fastest_anti_aliased_stroking_method() :
        shader.fastest_non_anti_aliased_stroking_method();
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
               fastuidraw::c_array<const float> geometry_inflation,
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
                             geometry_inflation,
                             m_max_attribs_per_block,
                             m_max_indices_per_block,
                             dst);
}

void
PainterPrivate::
select_chunks(const fastuidraw::StrokedCapsJoins &caps_joins,
              fastuidraw::c_array<const float> geometry_inflation,
              enum fastuidraw::Painter::join_style js,
              enum fastuidraw::Painter::cap_style cp,
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
                            geometry_inflation,
                            js, cp,
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
  fastuidraw::PainterPacker *cvg_packer(deferred_coverage_packer());
  fastuidraw::ivec2 coverage_buffer_offset(0, 0);

  if (shader->coverage_shader() && cvg_packer)
    {
      FASTUIDRAWassert(!m_deferred_coverage_stack.empty());
      p.m_clip = m_deferred_coverage_stack.back().clip_eq_state();
      p.m_matrix = m_clip_rect_state.current_item_matrix_coverage_buffer_state(m_pool);
      coverage_buffer_offset = m_deferred_coverage_stack.back().coverage_buffer_offset();

      FASTUIDRAWassert(p.m_clip);
      FASTUIDRAWassert(p.m_matrix);
      cvg_packer->draw_generic(shader->coverage_shader(), p,
                               attrib_chunks, index_chunks, index_adjusts, attrib_chunk_selector);
      packer()->set_coverage_surface(cvg_packer->surface());
    }
  else if (shader->coverage_shader())
    {
      FASTUIDRAWwarning(!"Warning: coverage_shader present but no coverage buffer present\n");
    }

  p.m_clip = m_clip_rect_state.clip_equations_state(m_pool);
  p.m_matrix = m_clip_rect_state.current_item_matrix_state(m_pool);
  FASTUIDRAWassert(p.m_clip);
  FASTUIDRAWassert(p.m_matrix);
  if (m_current_brush_adjust)
    {
      p.m_brush_adjust = *m_current_brush_adjust;
      FASTUIDRAWassert(p.m_brush_adjust);
    }

  packer()->draw_generic(coverage_buffer_offset, shader, p,
                         attrib_chunks, index_chunks, index_adjusts,
                         attrib_chunk_selector, z);
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
  fastuidraw::PainterPacker *cvg_packer(deferred_coverage_packer());
  fastuidraw::ivec2 coverage_buffer_offset(0, 0);

  if (shader->coverage_shader() && cvg_packer)
    {
      FASTUIDRAWassert(!m_deferred_coverage_stack.empty());
      p.m_clip = m_deferred_coverage_stack.back().clip_eq_state();
      p.m_matrix = m_clip_rect_state.current_item_matrix_coverage_buffer_state(m_pool);
      coverage_buffer_offset = m_deferred_coverage_stack.back().coverage_buffer_offset();

      FASTUIDRAWassert(p.m_clip);
      FASTUIDRAWassert(p.m_matrix);
      cvg_packer->draw_generic(shader->coverage_shader(), p, src);
      packer()->set_coverage_surface(cvg_packer->surface());
    }
  else if (shader->coverage_shader())
    {
      FASTUIDRAWwarning(!"Warning: coverage_shader present but no coverage buffer present\n");
    }

  p.m_clip = m_clip_rect_state.clip_equations_state(m_pool);
  p.m_matrix = m_clip_rect_state.current_item_matrix_state(m_pool);
  FASTUIDRAWassert(p.m_clip);
  FASTUIDRAWassert(p.m_matrix);
  if (m_current_brush_adjust)
    {
      p.m_brush_adjust = *m_current_brush_adjust;
      FASTUIDRAWassert(p.m_brush_adjust);
    }
  packer()->draw_generic(coverage_buffer_offset, shader, p, src, z);
  ++m_draw_data_added_count;
}

void
PainterPrivate::
pre_draw_anti_alias_fuzz(const fastuidraw::FilledPath &filled_path,
                         const FillSubsetWorkRoom &fill_subset,
                         AntiAliasFillWorkRoom *output)
{
  using namespace fastuidraw;

  output->m_total_increment_z = 0;
  output->m_z_increments.clear();
  output->m_attrib_chunks.clear();
  output->m_index_chunks.clear();
  output->m_index_adjusts.clear();
  output->m_start_zs.clear();

  BoundingBox<float> bb;
  for(unsigned int s : fill_subset.m_subsets)
    {
      bool include_subset_bb(false);
      FilledPath::Subset subset(filled_path.subset(s));
      c_array<const int> winding_numbers(subset.winding_numbers());

      for(int w : winding_numbers)
        {
          if (fill_subset.m_ws(w))
            {
              include_subset_bb = true;
              break;
            }
        }

      if (include_subset_bb)
        {
          bb.union_box(subset.bounding_box());
        }
    }

  if (!bb.empty())
    {
      output->m_normalized_device_coords_bounding_box = compute_clip_intersect_rect(bb.as_rect(), 1.0f, 0.0f);
    }
  else
    {
      output->m_normalized_device_coords_bounding_box.clear();
      return;
    }

  for(unsigned int s : fill_subset.m_subsets)
    {
      FilledPath::Subset subset(filled_path.subset(s));
      const PainterAttributeData &data(subset.aa_fuzz_painter_data());
      c_array<const int> winding_numbers(subset.winding_numbers());

      for(int w : winding_numbers)
        {
          if (fill_subset.m_ws(w))
            {
              unsigned int ch;
              range_type<int> R;

              ch = FilledPath::Subset::aa_fuzz_chunk_from_winding_number(w);
              R = data.z_range(ch);

              output->m_attrib_chunks.push_back(data.attribute_data_chunk(ch));
              output->m_index_chunks.push_back(data.index_data_chunk(ch));
              output->m_index_adjusts.push_back(data.index_adjust_chunk(ch));
              output->m_z_increments.push_back(R.difference());
              output->m_start_zs.push_back(R.m_begin);
              output->m_total_increment_z += output->m_z_increments.back();
            }
        }
    }
}

void
PainterPrivate::
draw_anti_alias_fuzz(const fastuidraw::PainterFillShader &shader,
                     const fastuidraw::PainterData &draw,
                     const AntiAliasFillWorkRoom &data,
                     int z)
{
  using namespace fastuidraw;

  if (data.m_normalized_device_coords_bounding_box.empty())
    {
      return;
    }

  /* TODO: instead of allocating a coverage buffer area the size of
   * the entire Path, we should instead walk through each Subset
   * seperately. For those that have anti-alias fuzz, allocate the
   * coverage buffer for their area and draw them.
   */
  bool requires_coverage_buffer(shader.aa_fuzz_shader()->coverage_shader());

  if (requires_coverage_buffer)
    {
      begin_coverage_buffer_normalized_rect(data.m_normalized_device_coords_bounding_box.as_rect(), true);
    }

  draw_generic_z_layered(shader.aa_fuzz_shader(), draw,
                         make_c_array(data.m_z_increments),
                         data.m_total_increment_z,
                         make_c_array(data.m_attrib_chunks),
                         make_c_array(data.m_index_chunks),
                         make_c_array(data.m_index_adjusts),
                         make_c_array(data.m_start_zs),
                         z);
  if (shader.aa_fuzz_shader()->coverage_shader())
    {
      end_coverage_buffer();
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

fastuidraw::BoundingBox<float>
PainterPrivate::
compute_bounding_box_of_path(const fastuidraw::StrokedPath &path,
                             fastuidraw::c_array<const unsigned int> stroked_subset_ids,
                             float pixels_additional_room, float item_space_additional_room)
{
  fastuidraw::BoundingBox<float> bbox, in_bbox;

  for (unsigned int subset_id : stroked_subset_ids)
    {
      const fastuidraw::Rect &rect(path.subset(subset_id).bounding_box());
      in_bbox.union_point(rect.m_min_point);
      in_bbox.union_point(rect.m_max_point);
    }

  if (!in_bbox.empty())
    {
      return compute_clip_intersect_rect(in_bbox.as_rect(),
                                         pixels_additional_room,
                                         item_space_additional_room);
    }
  return in_bbox;
}

fastuidraw::BoundingBox<float>
PainterPrivate::
compute_bounding_box_of_miter_joins(fastuidraw::c_array<const fastuidraw::vec2> join_positions,
                                    float miter_pixels_additional_room,
                                    float miter_item_space_additional_room)
{
  /* TODO:
   *   1. Take into account the direction of the joins.
   *   2. Take into account the the maximum value the miter-length
   *      the join can be.
   */
  fastuidraw::BoundingBox<float> return_value;
  for (const fastuidraw::vec2 &p : join_positions)
    {
      fastuidraw::BoundingBox<float> tmp;
      tmp = compute_clip_intersect_rect(fastuidraw::Rect().min_point(p).max_point(p),
                                        miter_pixels_additional_room,
                                        miter_item_space_additional_room);
      return_value.union_box(tmp);
    }
  return return_value;
}

fastuidraw::BoundingBox<float>
PainterPrivate::
compute_bounding_box_of_stroked_path(const fastuidraw::StrokedPath &stroked_path,
                                     fastuidraw::c_array<const unsigned int> stroked_subset_ids,
                                     fastuidraw::c_array<const float> additional_room,
                                     enum fastuidraw::Painter::join_style js,
                                     fastuidraw::c_array<const fastuidraw::vec2> join_positions)
{
  using namespace fastuidraw;

  BoundingBox<float> return_value;
  return_value =
    compute_bounding_box_of_path(stroked_path, stroked_subset_ids,
                                 additional_room[StrokingDataSelectorBase::pixel_space_distance],
                                 additional_room[StrokingDataSelectorBase::item_space_distance]);

  if (Painter::is_miter_join(js))
    {
      BoundingBox<float> tmp;
      float pixel_dist(additional_room[StrokingDataSelectorBase::pixel_space_distance_miter_joins]);
      float item_dist(additional_room[StrokingDataSelectorBase::item_space_distance_miter_joins]);

      tmp = compute_bounding_box_of_miter_joins(join_positions, pixel_dist, item_dist);
      return_value.union_box(tmp);
    }
  return return_value;
}

void
PainterPrivate::
stroke_path_common(const fastuidraw::PainterStrokeShader &shader,
                   const fastuidraw::PainterData &pdraw,
                   const fastuidraw::StrokedPath &path, float thresh,
                   enum fastuidraw::Painter::cap_style cp,
                   enum fastuidraw::Painter::join_style js,
                   bool apply_anti_aliasing)
{
  using namespace fastuidraw;

  if (m_clip_rect_state.m_all_content_culled)
    {
      return;
    }

  const PainterAttributeData *cap_data(nullptr), *join_data(nullptr);
  c_array<const generic_data> raw_data;
  const StrokedCapsJoins &caps_joins(path.caps_joins());
  bool edge_arc_shader(path.has_arcs()), cap_arc_shader(false), join_arc_shader(false);
  bool requires_coverage_buffer(false);
  PainterData draw(pdraw);

  /* if any of the data elements of draw are NOT packed state,
   * make them as packed state so that they are reused
   * to prevent filling up the data buffer with repeated
   * state data. In addition, we need for the PainterItemShaderData
   * to be packed so that we can query it correctly.
   */
  draw.make_packed(m_pool);
  raw_data = draw.m_item_shader_data.m_packed_value.packed_data();

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
      join_data = &caps_joins.miter_clip_joins();
      break;

    case Painter::miter_bevel_joins:
      join_data = &caps_joins.miter_bevel_joins();
      break;

    case Painter::miter_joins:
      join_data = &caps_joins.miter_joins();
      break;

    case Painter::bevel_joins:
      join_data = &caps_joins.bevel_joins();
      break;

    case Painter::rounded_joins:
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
    }

  vecN<float, StrokingDataSelectorBase::path_geometry_inflation_index_count> additional_room(0.0f);
  shader.stroking_data_selector()->stroking_distances(raw_data, additional_room);

  unsigned int subset_count;
  m_work_room.m_stroke.m_subsets.resize(path.number_subsets());

  subset_count = path.select_subsets(m_work_room.m_stroke.m_path_scratch,
                                     m_clip_store.current(),
                                     m_clip_rect_state.item_matrix(),
                                     m_one_pixel_width,
                                     additional_room,
                                     m_max_attribs_per_block,
                                     m_max_indices_per_block,
                                     make_c_array(m_work_room.m_stroke.m_subsets));

  FASTUIDRAWassert(subset_count <= m_work_room.m_stroke.m_subsets.size());
  m_work_room.m_stroke.m_subsets.resize(subset_count);

  caps_joins.compute_chunks(m_work_room.m_stroke.m_caps_joins_scratch,
                            m_clip_store.current(),
                            m_clip_rect_state.item_matrix(),
                            m_one_pixel_width,
                            additional_room,
                            js, cp,
                            m_work_room.m_stroke.m_caps_joins_chunk_set);

  if (m_work_room.m_stroke.m_caps_joins_chunk_set.join_chunks().empty())
    {
      join_data = nullptr;
    }

  if (m_work_room.m_stroke.m_caps_joins_chunk_set.cap_chunks().empty())
    {
      cap_data = nullptr;
    }

  requires_coverage_buffer =
    (subset_count > 0 && (*stroke_shader(shader, edge_arc_shader, apply_anti_aliasing))->coverage_shader())
    || (join_data && (*stroke_shader(shader, join_arc_shader, apply_anti_aliasing))->coverage_shader())
    || (cap_data && (*stroke_shader(shader, cap_arc_shader, apply_anti_aliasing))->coverage_shader());

  if (requires_coverage_buffer)
    {
      BoundingBox<float> coverage_buffer_bb;
      coverage_buffer_bb =
        compute_bounding_box_of_stroked_path(path, make_c_array(m_work_room.m_stroke.m_subsets),
                                             additional_room, js,
                                             m_work_room.m_stroke.m_caps_joins_chunk_set.join_positions());
      if (coverage_buffer_bb.empty())
        {
          return;
        }
      begin_coverage_buffer_normalized_rect(coverage_buffer_bb.as_rect(), !coverage_buffer_bb.empty());
    }

  stroke_path_raw(shader, edge_arc_shader, join_arc_shader, cap_arc_shader, draw,
                  &path, make_c_array(m_work_room.m_stroke.m_subsets),
                  cap_data, m_work_room.m_stroke.m_caps_joins_chunk_set.cap_chunks(),
                  join_data, m_work_room.m_stroke.m_caps_joins_chunk_set.join_chunks(),
                  apply_anti_aliasing);

  if (requires_coverage_buffer)
    {
      end_coverage_buffer();
    }
}

void
PainterPrivate::
stroke_path_raw(const fastuidraw::PainterStrokeShader &shader,
                bool edge_use_arc_shaders,
                bool join_use_arc_shaders,
                bool cap_use_arc_shaders,
                const fastuidraw::PainterData &draw,
                const fastuidraw::StrokedPath *stroked_path,
                fastuidraw::c_array<const unsigned int> stroked_subset_ids,
                const fastuidraw::PainterAttributeData *cap_data,
                fastuidraw::c_array<const unsigned int> cap_chunks,
                const fastuidraw::PainterAttributeData* join_data,
                fastuidraw::c_array<const unsigned int> join_chunks,
                bool apply_anti_aliasing)
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

  unsigned int zinc_sum(0), current(0);
  c_array<c_array<const PainterAttribute> > attrib_chunks;
  c_array<c_array<const PainterIndex> > index_chunks;
  c_array<int> index_adjusts;
  c_array<int> z_increments;
  c_array<int> start_zs;
  c_array<const reference_counted_ptr<PainterItemShader>* > shaders;

  m_work_room.m_stroke.m_attrib_chunks.resize(total_chunks);
  m_work_room.m_stroke.m_index_chunks.resize(total_chunks);
  m_work_room.m_stroke.m_increment_zs.resize(total_chunks);
  m_work_room.m_stroke.m_index_adjusts.resize(total_chunks);
  m_work_room.m_stroke.m_start_zs.resize(total_chunks);
  m_work_room.m_stroke.m_shaders.resize(total_chunks);

  attrib_chunks = make_c_array(m_work_room.m_stroke.m_attrib_chunks);
  index_chunks = make_c_array(m_work_room.m_stroke.m_index_chunks);
  z_increments = make_c_array(m_work_room.m_stroke.m_increment_zs);
  index_adjusts = make_c_array(m_work_room.m_stroke.m_index_adjusts);
  start_zs = make_c_array(m_work_room.m_stroke.m_start_zs);
  shaders = make_c_array(m_work_room.m_stroke.m_shaders);
  current = 0;

  if (!stroked_subset_ids.empty())
    {
      const reference_counted_ptr<PainterItemShader> *edge_shader;

      edge_shader = stroke_shader(shader, edge_use_arc_shaders, apply_anti_aliasing);
      for(unsigned int E = 0; E < stroked_subset_ids.size(); ++E, ++current)
        {
          StrokedPath::Subset S(stroked_path->subset(stroked_subset_ids[E]));

          attrib_chunks[current] = S.painter_data().attribute_data_chunk(stroked_path_chunk);
          index_chunks[current] = S.painter_data().index_data_chunk(stroked_path_chunk);
          index_adjusts[current] = S.painter_data().index_adjust_chunk(stroked_path_chunk);
          z_increments[current] = S.painter_data().z_range(stroked_path_chunk).difference();
          start_zs[current] = S.painter_data().z_range(stroked_path_chunk).m_begin;
          shaders[current] = edge_shader;
          zinc_sum += z_increments[current];
        }
    }

  if (!join_chunks.empty())
    {
      const reference_counted_ptr<PainterItemShader> *join_shader;

      join_shader = stroke_shader(shader, join_use_arc_shaders, apply_anti_aliasing);
      for(unsigned int J = 0; J < join_chunks.size(); ++J, ++current)
        {
          attrib_chunks[current] = join_data->attribute_data_chunk(join_chunks[J]);
          index_chunks[current] = join_data->index_data_chunk(join_chunks[J]);
          index_adjusts[current] = join_data->index_adjust_chunk(join_chunks[J]);
          z_increments[current] = join_data->z_range(join_chunks[J]).difference();
          start_zs[current] = join_data->z_range(join_chunks[J]).m_begin;
          shaders[current] = join_shader;
          zinc_sum += z_increments[current];
        }
    }

  if (!cap_chunks.empty())
    {
      const reference_counted_ptr<PainterItemShader> *cap_shader;

      cap_shader = stroke_shader(shader, cap_use_arc_shaders, apply_anti_aliasing);
      for(unsigned int C = 0; C < cap_chunks.size(); ++C, ++current)
        {
          attrib_chunks[current] = cap_data->attribute_data_chunk(cap_chunks[C]);
          index_chunks[current] = cap_data->index_data_chunk(cap_chunks[C]);
          index_adjusts[current] = cap_data->index_adjust_chunk(cap_chunks[C]);
          z_increments[current] = cap_data->z_range(cap_chunks[C]).difference();
          start_zs[current] = cap_data->z_range(cap_chunks[C]).m_begin;
          shaders[current] = cap_shader;
          zinc_sum += z_increments[current];
        }
    }

  draw_generic_z_layered(shaders, draw, z_increments, zinc_sum,
                         attrib_chunks, index_chunks, index_adjusts,
                         start_zs, m_current_z);

  m_current_z += zinc_sum;
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
          bool apply_anti_aliasing)
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

  if (apply_anti_aliasing)
    {
      pre_draw_anti_alias_fuzz(filled_path,
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

  if (apply_anti_aliasing)
    {
      draw_anti_alias_fuzz(shader, draw, m_work_room.m_fill_aa_fuzz);
    }

  m_current_z += m_work_room.m_fill_aa_fuzz.m_total_increment_z;
}

void
PainterPrivate::
fill_rounded_rect(const fastuidraw::PainterFillShader &shader,
                  const fastuidraw::PainterData &draw,
                  const fastuidraw::RoundedRect &R,
                  bool apply_anti_aliasing)
{
  using namespace fastuidraw;

  /* Save our transformation and clipping state */
  ClipRectState m(m_clip_rect_state);
  RoundedRectTransformations rect_transforms(R, &m_pool);
  int total_incr_z(0);
  RectWithSidePoints interior_rect, r_min, r_max, r_min_extra, r_max_extra;
  float wedge_miny, wedge_maxy;
  vecN<PackedAntiAliasEdgeData, 4> per_line;

  wedge_miny = t_max(R.m_corner_radii[Rect::minx_miny_corner].y(), R.m_corner_radii[Rect::maxx_miny_corner].y());
  wedge_maxy = t_max(R.m_corner_radii[Rect::minx_maxy_corner].y(), R.m_corner_radii[Rect::maxx_maxy_corner].y());

  interior_rect.m_min_point = vec2(R.m_min_point.x(), R.m_min_point.y() + wedge_miny);
  interior_rect.m_max_point = vec2(R.m_max_point.x(), R.m_max_point.y() - wedge_maxy);

  r_min.m_min_point = vec2(R.m_min_point.x() + R.m_corner_radii[Rect::minx_miny_corner].x(),
                           R.m_min_point.y());
  r_min.m_max_point = vec2(R.m_max_point.x() - R.m_corner_radii[Rect::maxx_miny_corner].x(),
                           R.m_min_point.y() + wedge_miny);
  interior_rect.m_min_y.insert(r_min.min_x());
  interior_rect.m_min_y.insert(r_min.max_x());

  r_max.m_min_point = vec2(R.m_min_point.x() + R.m_corner_radii[Rect::minx_maxy_corner].x(),
                           R.m_max_point.y() - wedge_maxy);
  r_max.m_max_point = vec2(R.m_max_point.x() - R.m_corner_radii[Rect::maxx_maxy_corner].x(),
                           R.m_max_point.y());
  interior_rect.m_max_y.insert(r_max.min_x());
  interior_rect.m_max_y.insert(r_max.max_x());

  if (apply_anti_aliasing)
    {
      int incr;
      vecN<vec2, 4> start_pts, end_pts;

      incr = 1;

      start_pts[0] = r_min.m_min_point;
      end_pts[0] = vec2(r_min.m_max_point.x(), r_min.m_min_point.y());

      start_pts[1] = vec2(r_max.m_min_point.x(), r_max.m_max_point.y());
      end_pts[1] = r_max.m_max_point;

      start_pts[2] = vec2(R.m_min_point.x(), R.m_min_point.y() + R.m_corner_radii[Rect::minx_miny_corner].y());
      end_pts[2] = vec2(R.m_min_point.x(), R.m_max_point.y() - R.m_corner_radii[Rect::minx_maxy_corner].y());

      start_pts[3] = vec2(R.m_max_point.x(), R.m_min_point.y() + R.m_corner_radii[Rect::maxx_miny_corner].y());
      end_pts[3] = vec2(R.m_max_point.x(), R.m_max_point.y() - R.m_corner_radii[Rect::maxx_maxy_corner].y());

      m_work_room.m_rounded_rect.m_rect_fuzz_attributes.clear();
      m_work_room.m_rounded_rect.m_rect_fuzz_indices.clear();
      for (int i = 0; i < 4; ++i)
        {
          pack_anti_alias_edge(start_pts[i], end_pts[i],
                               total_incr_z, &per_line[i],
                               m_work_room.m_rounded_rect.m_rect_fuzz_attributes,
                               m_work_room.m_rounded_rect.m_rect_fuzz_indices);
          total_incr_z += incr;
        }
    }

  for (int i = 0; i < 4; ++i)
    {
      m_current_brush_adjust = &rect_transforms.m_packed_adjusts[i];

      translate(rect_transforms.m_adjusts[i].m_translate);
      shear(rect_transforms.m_adjusts[i].m_shear.x(),
            rect_transforms.m_adjusts[i].m_shear.y());
      m_work_room.m_rounded_rect.m_per_corner[i].m_filled_path = &select_filled_path(m_rounded_corner_path);
      fill_path_compute_opaque_chunks(*m_work_room.m_rounded_rect.m_per_corner[i].m_filled_path,
                                      Painter::nonzero_fill_rule,
                                      m_work_room.m_rounded_rect.m_per_corner[i].m_subsets,
                                      &m_work_room.m_rounded_rect.m_per_corner[i].m_opaque_fill);

      if (apply_anti_aliasing)
        {
          pre_draw_anti_alias_fuzz(*m_work_room.m_rounded_rect.m_per_corner[i].m_filled_path,
                                   m_work_room.m_rounded_rect.m_per_corner[i].m_subsets,
                                   &m_work_room.m_rounded_rect.m_per_corner[i].m_aa_fuzz);
          total_incr_z += m_work_room.m_rounded_rect.m_per_corner[i].m_aa_fuzz.m_total_increment_z;
        }
      else
        {
          m_work_room.m_rounded_rect.m_per_corner[i].m_aa_fuzz.m_total_increment_z = 0;
        }
      m_clip_rect_state = m;
      m_current_brush_adjust = nullptr;
    }

  if (R.m_corner_radii[Rect::minx_miny_corner].y() > R.m_corner_radii[Rect::maxx_miny_corner].y())
    {
      Rect r;

      r.m_min_point.x() = r_min.m_max_point.x();
      r.m_min_point.y() = R.m_min_point.y() + R.m_corner_radii[Rect::maxx_miny_corner].y();
      r.m_max_point.x() = R.m_max_point.x();
      r.m_max_point.y() = r_min.m_max_point.y();
      fill_rect(shader, draw, r, false, m_current_z + total_incr_z);

      r_min.m_max_x.insert(r.min_y());
    }
  else if (R.m_corner_radii[Rect::minx_miny_corner].y() < R.m_corner_radii[Rect::maxx_miny_corner].y())
    {
      Rect r;

      r.m_min_point.x() = R.m_min_point.x();
      r.m_min_point.y() = R.m_min_point.y() + R.m_corner_radii[Rect::minx_miny_corner].y();
      r.m_max_point.x() = R.m_min_point.x() + R.m_corner_radii[Rect::minx_miny_corner].x();
      r.m_max_point.y() = r_min.m_max_point.y();
      fill_rect(shader, draw, r, false, m_current_z + total_incr_z);

      r_min.m_min_x.insert(r.min_y());
    }

  if (R.m_corner_radii[Rect::minx_maxy_corner].y() > R.m_corner_radii[Rect::maxx_maxy_corner].y())
    {
      Rect r;

      r.m_min_point.x() = r_max.m_max_point.x();
      r.m_min_point.y() = r_max.m_min_point.y();
      r.m_max_point.x() = R.m_max_point.x();
      r.m_max_point.y() = R.m_max_point.y() - R.m_corner_radii[Rect::maxx_maxy_corner].y();
      fill_rect(shader, draw, r, false, m_current_z + total_incr_z);

      r_max.m_min_x.insert(r.max_y());
    }
  else if (R.m_corner_radii[Rect::minx_maxy_corner].y() < R.m_corner_radii[Rect::maxx_maxy_corner].y())
    {
      Rect r;

      r.m_min_point.x() = R.m_min_point.x();
      r.m_min_point.y() = r_max.m_min_point.y();
      r.m_max_point.x() = R.m_min_point.x() + R.m_corner_radii[Rect::minx_maxy_corner].x();
      r.m_max_point.y() = R.m_max_point.y() - R.m_corner_radii[Rect::minx_maxy_corner].y();
      fill_rect(shader, draw, r, false, m_current_z + total_incr_z);

      r_max.m_max_x.insert(r.max_y());
    }

  fill_rect_with_side_points(shader, draw, interior_rect, false, m_current_z + total_incr_z);
  fill_rect_with_side_points(shader, draw, r_min, false, m_current_z + total_incr_z);
  fill_rect_with_side_points(shader, draw, r_max, false, m_current_z + total_incr_z);

  for (int i = 0; i < 4; ++i)
    {
      m_current_brush_adjust = &rect_transforms.m_packed_adjusts[i];

      translate(rect_transforms.m_adjusts[i].m_translate);
      shear(rect_transforms.m_adjusts[i].m_shear.x(),
            rect_transforms.m_adjusts[i].m_shear.y());
      draw_generic(shader.item_shader(), draw,
                   make_c_array(m_work_room.m_rounded_rect.m_per_corner[i].m_opaque_fill.m_attrib_chunks),
                   make_c_array(m_work_room.m_rounded_rect.m_per_corner[i].m_opaque_fill.m_index_chunks),
                   make_c_array(m_work_room.m_rounded_rect.m_per_corner[i].m_opaque_fill.m_index_adjusts),
                   make_c_array(m_work_room.m_rounded_rect.m_per_corner[i].m_opaque_fill.m_chunk_selector),
                   m_current_z + total_incr_z);
      m_clip_rect_state = m;

      m_current_brush_adjust = nullptr;
    }

  if (apply_anti_aliasing)
    {
      int incr_z(total_incr_z);
      c_array<const PainterAttribute> attribs;
      c_array<const PainterIndex> indices;

      attribs = make_c_array(m_work_room.m_rounded_rect.m_rect_fuzz_attributes);
      indices = make_c_array(m_work_room.m_rounded_rect.m_rect_fuzz_indices);

      incr_z -= 4;
      for (int i = 0; i < 4; ++i)
        {
          BoundingBox<float> bb;

          bb = compute_clip_intersect_rect(per_line[i].m_box.as_rect(), 1.0f, 0.0f);
          if (!bb.empty())
            {
              begin_coverage_buffer_normalized_rect(bb.as_rect(), true);
              draw_generic(shader.aa_fuzz_shader(), draw,
                           attribs.sub_array(per_line[i].m_attrib_range),
                           indices.sub_array(per_line[i].m_index_range),
                           -per_line[i].m_attrib_range.m_begin,
                           m_current_z + incr_z);
              end_coverage_buffer();
            }
        }

      for (int i = 0; i < 4; ++i)
        {
          m_current_brush_adjust = &rect_transforms.m_packed_adjusts[i];

          incr_z -= m_work_room.m_rounded_rect.m_per_corner[i].m_aa_fuzz.m_total_increment_z;
          translate(rect_transforms.m_adjusts[i].m_translate);
          shear(rect_transforms.m_adjusts[i].m_shear.x(),
                rect_transforms.m_adjusts[i].m_shear.y());
          draw_anti_alias_fuzz(shader, draw,
                               m_work_room.m_rounded_rect.m_per_corner[i].m_aa_fuzz,
                               m_current_z + incr_z);
          m_clip_rect_state = m;

          m_current_brush_adjust = nullptr;
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
                         bool apply_anti_aliasing)
{
  using namespace fastuidraw;

  m_work_room.m_polygon.m_fuzz_increment_z = 0;
  if (apply_anti_aliasing)
    {
      m_work_room.m_polygon.m_aa_fuzz_attribs.clear();
      m_work_room.m_polygon.m_aa_fuzz_indices.clear();

      for (unsigned int i = 0; i < pts.size(); ++i, m_work_room.m_polygon.m_fuzz_increment_z += 1)
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
    }
}

int
PainterPrivate::
fill_convex_polygon(bool allow_sw_clipping,
                    const fastuidraw::PainterFillShader &shader,
                    const fastuidraw::PainterData &draw,
                    fastuidraw::c_array<const fastuidraw::vec2> pts,
                    bool apply_anti_aliasing, int z)
{
  using namespace fastuidraw;

  if (pts.size() < 3 || m_clip_rect_state.m_all_content_culled)
    {
      return 0;
    }

  if (allow_sw_clipping && !m_hints.clipping_via_hw_clip_planes())
    {
      m_clip_rect_state.clip_polygon(pts, m_work_room.m_polygon.m_pts,
                                     m_work_room.m_clipper.m_vec2s[0]);
      pts = make_c_array(m_work_room.m_polygon.m_pts);
      if (pts.size() < 3)
        {
          return 0;
        }
    }

  BoundingBox<float> cvg_bb;
  if (apply_anti_aliasing)
    {
      BoundingBox<float> in_bb;

      in_bb.union_points(pts.begin(), pts.end());
      cvg_bb = compute_clip_intersect_rect(in_bb.as_rect(), 1.0f, 0.0f);
      if (cvg_bb.empty())
        {
          return 0;
        }
    }

  ready_aa_polygon_attribs(pts, apply_anti_aliasing);
  ready_non_aa_polygon_attribs(pts);

  draw_generic(shader.item_shader(), draw,
               make_c_array(m_work_room.m_polygon.m_attribs),
               make_c_array(m_work_room.m_polygon.m_indices),
               0,
               m_work_room.m_polygon.m_fuzz_increment_z + z);

  if (apply_anti_aliasing)
    {
      begin_coverage_buffer_normalized_rect(cvg_bb.as_rect(), true);
      draw_generic(shader.aa_fuzz_shader(), draw,
                   make_c_array(m_work_room.m_polygon.m_aa_fuzz_attribs),
                   make_c_array(m_work_room.m_polygon.m_aa_fuzz_indices),
                   0, z);
      end_coverage_buffer();
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
                          false);
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
                          false);

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
                          false);
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
Painter(const reference_counted_ptr<PainterEngine> &backend)
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
begin(const reference_counted_ptr<PainterSurface> &surface,
      const float3x3 &initial_transformation,
      bool clear_color_buffer)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  image_atlas().lock_resources();
  colorstop_atlas().lock_resources();
  glyph_atlas().lock_resources();

  d->m_backend->on_painter_begin();
  d->m_viewport = surface->viewport();
  d->m_effects_layer_factory.begin(*surface);
  d->m_deferred_coverage_stack_entry_factory.begin(*surface);
  d->m_root_packer->begin(surface, clear_color_buffer);
  d->m_active_surfaces.clear();
  std::fill(d->m_stats.begin(), d->m_stats.end(), 0u);
  d->m_stats[Painter::num_render_targets] = 1;
  d->m_viewport_dimensions = vec2(d->m_viewport.m_dimensions);
  d->m_viewport_dimensions.x() = t_max(1.0f, d->m_viewport_dimensions.x());
  d->m_viewport_dimensions.y() = t_max(1.0f, d->m_viewport_dimensions.y());
  /* m_one_pixel_width holds the size of a pixel in
   * normalized device coordinates whose range is [-1, 1].
   * Thus, the value is twice the reciprocal of the viewport
   * dimensions.
   */
  d->m_one_pixel_width = 2.0f / d->m_viewport_dimensions;

  d->m_current_z = 1;
  d->m_draw_data_added_count = 0;
  d->m_clip_rect_state.reset(d->m_viewport, surface->dimensions());
  d->m_clip_store.reset(d->m_clip_rect_state.clip_equations().m_clip_equations);
  blend_shader(blend_porter_duff_src_over);

  Rect ncR;
  d->m_viewport.compute_normalized_clip_rect(surface->dimensions(), &ncR);
  clip_in_rect(ncR);
  concat(initial_transformation);
}

void
fastuidraw::Painter::
begin(const reference_counted_ptr<PainterSurface> &surface,
      enum screen_orientation orientation, bool clear_color_buffer)
{
  float y1, y2;
  const PainterSurface::Viewport &vwp(surface->viewport());

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

fastuidraw::c_array<const fastuidraw::PainterSurface* const>
fastuidraw::Painter::
end(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  /* pop the effects stack until it is empty */
  while (!d->m_effects_layer_stack.empty())
    {
      end_layer();
    }

  /* pop the deferred coverage stack until it is empty */
  while (!d->m_deferred_coverage_stack.empty())
    {
      d->end_coverage_buffer();
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

  /* issue the PainterPacker::end() to send the commands to the GPU;
   * we issue the end()'s in the order:
   *   1. m_deferred_coverage_stack_entry_factory because a effects
   *      color buffer might use a deferred coverage buffer to render
   *   2. m_effects_layer_factory (in reverse depth order)
   *      so that layer renders are ready for root
   *   3. m_root_packer last.
   */
  d->m_deferred_coverage_stack_entry_factory.end();
  d->m_effects_layer_factory.end();
  d->m_root_packer->end();

  /* unlock resources after the commands are sent to the GPU */
  image_atlas().unlock_resources();
  colorstop_atlas().unlock_resources();
  glyph_atlas().unlock_resources();

  return make_c_array(d->m_active_surfaces);
}

enum fastuidraw::return_code
fastuidraw::Painter::
flush(void)
{
  return flush(surface());
}

enum fastuidraw::return_code
fastuidraw::Painter::
flush(const reference_counted_ptr<PainterSurface> &new_surface)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (!surface() || !new_surface)
    {
      /* not actively drawing */
      return routine_fail;
    }

  if (!d->m_effects_layer_stack.empty())
    {
      return routine_fail;
    }

  if (!d->m_deferred_coverage_stack.empty())
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
  d->m_deferred_coverage_stack_entry_factory.end();
  d->m_effects_layer_factory.end();

  /* clear the list of active surfaces */
  d->m_active_surfaces.clear();

  /* start the effects_stack_entry up again */
  if (new_surface == surface())
    {
      bool clear_z;
      const int clear_depth_thresh(d->m_hints.max_z());

      clear_z = (d->m_current_z > clear_depth_thresh);
      d->m_root_packer->flush(clear_z);
      d->m_effects_layer_factory.begin(*new_surface);
      d->m_deferred_coverage_stack_entry_factory.begin(*new_surface);
      if (clear_z)
        {
          d->m_current_z = 1;
        }
      const state_stack_entry &st(d->m_state_stack.back());
      d->packer()->blend_shader(st.m_blend, st.m_blend_mode);
    }
  else
    {
      /* get the Image handle to the old-surface */
      reference_counted_ptr<PainterSurface> old_surface(surface());
      reference_counted_ptr<const Image> image;

      image = surface()->image(d->m_backend_factory->image_atlas());
      new_surface->viewport(d->m_viewport);

      d->m_root_packer->end();
      d->m_current_z = 1;
      d->m_root_packer->begin(new_surface, true);
      d->m_deferred_coverage_stack_entry_factory.begin(*new_surface);
      d->m_effects_layer_factory.begin(*new_surface);

      /* blit the old surface to the surface */
      save();
        {
          const PainterSurface::Viewport &vwp(new_surface->viewport());
          PainterBrush brush;

          brush.sub_image(image, uvec2(vwp.m_origin), uvec2(vwp.m_dimensions));
          transformation(float_orthogonal_projection_params(0, vwp.m_dimensions.x(), 0, vwp.m_dimensions.y()));
          blend_shader(blend_porter_duff_src);
          fill_rect(PainterData(&brush),
                    Rect().min_point(0.0f, 0.0f).size(vec2(vwp.m_dimensions)),
                    false);
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

const fastuidraw::reference_counted_ptr<fastuidraw::PainterSurface>&
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
queue_action(const reference_counted_ptr<const PainterDrawBreakAction> &action)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->packer()->draw_break(action);
}

void
fastuidraw::Painter::
fill_convex_polygon(const PainterFillShader &shader,
                    const PainterData &draw, c_array<const vec2> pts,
                    bool apply_shader_anti_aliasing)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_current_z += d->fill_convex_polygon(shader, draw, pts, apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
fill_convex_polygon(const PainterData &draw, c_array<const vec2> pts,
                    bool apply_shader_anti_aliasing)
{
  fill_convex_polygon(default_shaders().fill_shader(), draw, pts, apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
fill_rect(const PainterFillShader &shader,
          const PainterData &draw, const Rect &rect,
          bool apply_shader_anti_aliasing)
{
  vecN<vec2, 4> pts;

  /* TODO: the code for fill_convex_polygon() if it does
   * anti-aliasing with deferred coverage buffer will
   * use an area of the entire rect instead of four
   * smaller areas for the sides of the rect.
   */
  pts[0] = vec2(rect.m_min_point.x(), rect.m_min_point.y());
  pts[1] = vec2(rect.m_min_point.x(), rect.m_max_point.y());
  pts[2] = vec2(rect.m_max_point.x(), rect.m_max_point.y());
  pts[3] = vec2(rect.m_max_point.x(), rect.m_min_point.y());
  fill_convex_polygon(shader, draw, pts, apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
fill_rect(const PainterData &draw, const Rect &rect,
          bool apply_shader_anti_aliasing)
{
  fill_rect(default_shaders().fill_shader(), draw, rect, apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
fill_rounded_rect(const PainterFillShader &shader, const PainterData &draw,
                  const RoundedRect &R,
                  bool apply_shader_anti_aliasing)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped, skip the draw */
      return;
    }
  d->fill_rounded_rect(shader, draw, R, apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
fill_rounded_rect(const PainterData &draw, const RoundedRect &R,
                  bool apply_shader_anti_aliasing)
{
  fill_rounded_rect(default_shaders().fill_shader(), draw, R, apply_shader_anti_aliasing);
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
                    const c_array<const generic_data> shader_data,
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
               c_array<const float> geometry_inflation,
               c_array<unsigned int> dst)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->select_subsets(path, geometry_inflation, dst);
}

void
fastuidraw::Painter::
select_chunks(const StrokedCapsJoins &caps_joins,
              fastuidraw::c_array<const float> geometry_inflation,
              enum fastuidraw::Painter::join_style js,
              enum fastuidraw::Painter::cap_style cp,
              StrokedCapsJoins::ChunkSet *dst)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->select_chunks(caps_joins, geometry_inflation, js, cp, dst);
}

void
fastuidraw::Painter::
stroke_path(const PainterStrokeShader &shader, const PainterData &draw,
            const StrokedPath &path, float thresh,
            const StrokingStyle &stroke_style,
            bool apply_shader_anti_aliasing)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  FASTUIDRAWassert(0 <= stroke_style.m_cap_style && stroke_style.m_cap_style < number_cap_styles);
  FASTUIDRAWassert(0 <= stroke_style.m_join_style && stroke_style.m_join_style < number_join_styles);
  d->stroke_path_common(shader, draw, path, thresh,
                        stroke_style.m_cap_style,
                        stroke_style.m_join_style,
                        apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
stroke_path(const PainterStrokeShader &shader, const PainterData &pdraw, const Path &path,
            const StrokingStyle &stroke_style,
            bool apply_shader_anti_aliasing,
            enum stroking_method_t stroking_method)
{
  PainterPrivate *d;
  const StrokedPath *stroked_path;
  float thresh;
  PainterData draw(pdraw);

  d = static_cast<PainterPrivate*>(m_d);
  draw.make_packed(d->m_pool);
  stroked_path = d->select_stroked_path(path, shader, draw,
                                        apply_shader_anti_aliasing,
                                        stroking_method, thresh);
  if (stroked_path)
    {
      stroke_path(shader, draw, *stroked_path, thresh, stroke_style,
                  apply_shader_anti_aliasing);
    }
}

void
fastuidraw::Painter::
stroke_path(const PainterData &draw, const Path &path,
            const StrokingStyle &stroke_style,
            bool apply_shader_anti_aliasing,
            enum stroking_method_t stroking_method)
{
  stroke_path(default_shaders().stroke_shader(), draw, path,
              stroke_style, apply_shader_anti_aliasing, stroking_method);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw,
                   const StrokedPath &path, float thresh,
                   const StrokingStyle &stroke_style,
                   bool apply_shader_anti_aliasing)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  FASTUIDRAWassert(0 <= stroke_style.m_cap_style && stroke_style.m_cap_style < number_cap_styles);
  FASTUIDRAWassert(0 <= stroke_style.m_join_style && stroke_style.m_join_style < number_join_styles);
  d->stroke_path_common(shader.shader(stroke_style.m_cap_style), draw,
                        path, thresh,
                        number_cap_styles,
                        stroke_style.m_join_style,
                        apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &pdraw, const Path &path,
                   const StrokingStyle &stroke_style,
                   bool apply_shader_anti_aliasing,
                   enum stroking_method_t stroking_method)
{
  PainterPrivate *d;
  const StrokedPath *stroked_path;
  float thresh;
  PainterData draw(pdraw);

  d = static_cast<PainterPrivate*>(m_d);
  draw.make_packed(d->m_pool);
  stroked_path = d->select_stroked_path(path, shader.shader(stroke_style.m_cap_style), draw,
                                        apply_shader_anti_aliasing,
                                        stroking_method, thresh);
  stroke_dashed_path(shader, draw, *stroked_path, thresh,
                     stroke_style, apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
stroke_dashed_path(const PainterData &draw, const Path &path,
                   const StrokingStyle &stroke_style,
                   bool apply_shader_anti_aliasing,
                   enum stroking_method_t stroking_method)
{
  stroke_dashed_path(default_shaders().dashed_stroke_shader(), draw, path,
                     stroke_style, apply_shader_anti_aliasing, stroking_method);
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const FilledPath &filled_path, enum fill_rule_t fill_rule,
          bool apply_shader_anti_aliasing)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->fill_path(shader, draw, filled_path, fill_rule,
               apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const Path &path, enum fill_rule_t fill_rule,
          bool apply_shader_anti_aliasing)
{
  PainterPrivate *d;

  d = static_cast<PainterPrivate*>(m_d);
  fill_path(shader, draw, d->select_filled_path(path),
            fill_rule, apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
fill_path(const PainterData &draw, const Path &path, enum fill_rule_t fill_rule,
          bool apply_shader_anti_aliasing)
{
  fill_path(default_shaders().fill_shader(), draw, path, fill_rule,
            apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader, const PainterData &draw,
          const FilledPath &filled_path, const CustomFillRuleBase &fill_rule,
          bool apply_shader_anti_aliasing)
{
  PainterPrivate *d;

  d = static_cast<PainterPrivate*>(m_d);
  d->fill_path(shader, draw, filled_path, fill_rule,
               apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
fill_path(const PainterFillShader &shader,
          const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
          bool apply_shader_anti_aliasing)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  fill_path(shader, draw, d->select_filled_path(path),
            fill_rule, apply_shader_anti_aliasing);
}

void
fastuidraw::Painter::
fill_path(const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
          bool apply_shader_anti_aliasing)
{
  fill_path(default_shaders().fill_shader(), draw, path, fill_rule,
            apply_shader_anti_aliasing);
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
      renderer = d->compute_glyph_renderer(glyph_sequence.format_size(),
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
      renderer = d->compute_glyph_renderer(glyph_run.format_size(),
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
                     d->compute_glyph_renderer(glyph_sequence.format_size(), renderer_chooser));
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
                     d->compute_glyph_renderer(glyph_run.format_size(), renderer_chooser));
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
  st.m_blend = d->packer()->blend_shader();
  st.m_blend_mode = d->packer()->blend_mode();
  st.m_clip_rect_state = d->m_clip_rect_state;
  st.m_curve_flatness = d->m_curve_flatness;
  st.m_deferred_coverage_buffer_depth = d->m_deferred_coverage_stack.size();

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
  d->packer()->blend_shader(st.m_blend, st.m_blend_mode);
  d->m_curve_flatness = st.m_curve_flatness;
  while(d->m_occluder_stack.size() > st.m_occluder_stack_position)
    {
      d->m_occluder_stack.back().on_pop(d);
      d->m_occluder_stack.pop_back();
    }
  FASTUIDRAWassert(st.m_deferred_coverage_buffer_depth == d->m_deferred_coverage_stack.size());
  d->m_state_stack.pop_back();
  d->m_clip_store.pop();
}

void
fastuidraw::Painter::
begin_layer(const vec4 &color_modulate)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->m_color_modulate_fx->color(color_modulate);
  begin_layer(d->m_color_modulate_fx);
}

void
fastuidraw::Painter::
begin_layer(const reference_counted_ptr<PainterEffect> &effect)
{
  begin_layer(effect->passes());
}

void
fastuidraw::Painter::
begin_layer(c_array<const reference_counted_ptr<PainterEffectPass> > passes)
{
  PainterPrivate *d;
  Rect clip_region_rect;

  d = static_cast<PainterPrivate*>(m_d);

  clip_region_bounds(&clip_region_rect.m_min_point,
                     &clip_region_rect.m_max_point);

  /* This save() is to save the current clipping state because it
   * will get set to just clip the rect giving by clip_region_rect
   */
  save();

  EffectsStackEntry fx_entry;

  fx_entry.m_effects_layer_stack_size = d->m_effects_layer_stack.size();
  fx_entry.m_state_stack_size = d->m_state_stack.size();;
  d->m_effects_stack.push_back(fx_entry);

  fastuidraw::PainterBlendShader* old_blend(d->packer()->blend_shader());
  fastuidraw::PainterBlendShader* copy_blend(d->m_default_shaders.blend_shaders().shader(blend_porter_duff_src).get());
  BlendMode old_blend_mode(d->packer()->blend_mode());
  BlendMode copy_blend_mode(d->m_default_shaders.blend_shaders().blend_mode(blend_porter_duff_src));

  for (auto ibegin = passes.rbegin(), iend = passes.rend(), i = ibegin;
       i != iend; ++i)
    {
      /* get the EffectsLayer that gives the PainterPacker and what to blit
       * when the layer is done
       */
      EffectsLayer R;

      R = d->m_effects_layer_factory.fetch(d->m_effects_layer_stack.size(), clip_region_rect, d);

      /* Set the blend mode to copy for those rects that fed to the next
       * effect; the last blit has the blend mode set to what it was before.
       */
      if (i != ibegin)
        {
          d->packer()->blend_shader(copy_blend, copy_blend_mode);
        }
      else
        {
          d->packer()->blend_shader(old_blend, old_blend_mode);
        }

      /* We *add* the command to the current packer() to blit the rect of R
       * now. Recall that the PainterPacker used for the layer will have its
       * commands executed before the current packer(), regardless in what
       * order we add them.
       */
      R.blit_rect(*i, d->m_viewport_dimensions, this);

      /* after issuing the blit command, then add R to m_effects_layer_stack */
      d->m_effects_layer_stack.push_back(R);
      ++d->m_stats[num_layers];

      /* Set the clipping equations to the equations coming from clip_region_rect */
      d->m_clip_rect_state.m_clip_rect = clip_rect(clip_region_rect);
      d->m_clip_rect_state.set_clip_equations_to_clip_rect(ClipRectState::rect_in_normalized_device_coordinates);

      /* update d->m_clip_rect_state.m_clip_rect to local coordinates */
      d->m_clip_rect_state.update_rect_to_transformation();

      /* change m_clip_store so that the current value is just from clip_region_rect */
      d->m_clip_store.reset_current_to_rect(clip_region_rect);

      /* set the normalized translation of d->m_clip_rect_state */
      d->m_clip_rect_state.set_normalized_device_translate(d->m_effects_layer_stack.back().m_normalized_translate);

      if (!d->m_deferred_coverage_stack.empty()
          && d->m_deferred_coverage_stack.back().packer())
        {
          d->m_deferred_coverage_stack.back().update_coverage_buffer_offset(d);
        }
    }

  /* Set the packer's blend shader, mode and blend shader to
   * Painter default values
   */
  blend_shader(blend_porter_duff_src_over);
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

  EffectsStackEntry R(d->m_effects_stack.back());

  /* restore any saves() done within the layer. */
  FASTUIDRAWassert(R.m_state_stack_size == d->m_state_stack.size());
  while (R.m_state_stack_size > d->m_state_stack.size())
    {
      restore();
    }
  d->m_effects_layer_stack.resize(R.m_effects_layer_stack_size);

  /* issue the restore that matches with the save() at the start
   * of begin_layer(); this will restore the clipping and
   * blending state to what it was when the begin_layer()
   * was issued */
  restore();

  if (!d->m_deferred_coverage_stack.empty()
      && d->m_deferred_coverage_stack.back().packer())
    {
      d->m_deferred_coverage_stack.back().update_coverage_buffer_offset(d);
    }
}

void
fastuidraw::Painter::
begin_coverage_buffer(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->begin_coverage_buffer();
}

void
fastuidraw::Painter::
begin_coverage_buffer(const Rect &logical_rect,
                      float additional_pixel_slack)
{
  fastuidraw::BoundingBox<float> nr;
  PainterPrivate *d;

  d = static_cast<PainterPrivate*>(m_d);
  nr = d->compute_clip_intersect_rect(logical_rect, additional_pixel_slack, 0.0f);
  d->begin_coverage_buffer_normalized_rect(nr.as_rect(), !nr.empty());
}

void
fastuidraw::Painter::
end_coverage_buffer(void)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->end_coverage_buffer();
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
  BlendMode old_blend_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = d->packer()->blend_shader();
  old_blend_mode = d->packer()->blend_mode();

  blend_shader(blend_porter_duff_dst);
  d->packer()->add_callback(zdatacallback);
  d->fill_path(default_shaders().fill_shader(),
               PainterData(d->m_black_brush),
               path, fill_rule, false);
  d->packer()->remove_callback(zdatacallback);
  d->packer()->blend_shader(old_blend, old_blend_mode);

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
  BlendMode old_blend_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = d->packer()->blend_shader();
  old_blend_mode = d->packer()->blend_mode();

  blend_shader(blend_porter_duff_dst);
  d->packer()->add_callback(zdatacallback);
  d->fill_path(default_shaders().fill_shader(),
               PainterData(d->m_black_brush),
               path, fill_rule, false);
  d->packer()->remove_callback(zdatacallback);
  d->packer()->blend_shader(old_blend, old_blend_mode);

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

  fastuidraw::PainterBlendShader* old_blend;
  BlendMode old_blend_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = d->packer()->blend_shader();
  old_blend_mode = d->packer()->blend_mode();

  blend_shader(blend_porter_duff_dst);
  d->packer()->add_callback(zdatacallback);
  draw_generic(shader,
               PainterData(shader_data, d->m_black_brush),
               attrib_chunks, index_chunks,
               index_adjusts, attrib_chunk_selector);
  d->packer()->remove_callback(zdatacallback);
  d->packer()->blend_shader(old_blend, old_blend_mode);

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

  fastuidraw::PainterBlendShader* old_blend;
  BlendMode old_blend_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = d->packer()->blend_shader();
  old_blend_mode = d->packer()->blend_mode();

  blend_shader(blend_porter_duff_dst);
  d->packer()->add_callback(zdatacallback);
  d->fill_rounded_rect(default_shaders().fill_shader(),
                       PainterData(d->m_black_brush),
                       R, false);
  d->packer()->remove_callback(zdatacallback);
  d->packer()->blend_shader(old_blend, old_blend_mode);

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
  ClipRectState m(d->m_clip_rect_state);

  fastuidraw::PainterBlendShader* old_blend;
  BlendMode old_blend_mode;
  reference_counted_ptr<ZDataCallBack> zdatacallback;
  RoundedRectTransformations rect_transforms(R);

  /* zdatacallback generates a list of PainterDraw::DelayedAction
   * objects (held in m_actions) who's action is to write the correct
   * z-value to occlude elements drawn after clipOut but not after
   * the next time m_occluder_stack is popped.
   */
  zdatacallback = FASTUIDRAWnew ZDataCallBack();
  old_blend = d->packer()->blend_shader();
  old_blend_mode = d->packer()->blend_mode();

  blend_shader(blend_porter_duff_dst);
  d->packer()->add_callback(zdatacallback);
  for (int i = 0; i < 4; ++i)
    {
      translate(rect_transforms.m_adjusts[i].m_translate);
      shear(rect_transforms.m_adjusts[i].m_shear.x(),
            rect_transforms.m_adjusts[i].m_shear.y());
      d->fill_path(default_shaders().fill_shader(), PainterData(d->m_black_brush),
                   d->select_filled_path(d->m_rounded_corner_path_complement),
                   nonzero_fill_rule,
                   false);
      d->m_clip_rect_state = m;
    }
  d->packer()->remove_callback(zdatacallback);
  d->packer()->blend_shader(old_blend, old_blend_mode);

  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));
}

void
fastuidraw::Painter::
clip_in_rect(const Rect &rect)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  d->m_clip_rect_state.m_all_content_culled = d->m_clip_rect_state.m_all_content_culled
    || rect.m_min_point.x() >= rect.m_max_point.x()
    || rect.m_min_point.y() >= rect.m_max_point.y();

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter */
      return;
    }

  vecN<vec3, 4> rect_clip_pts;

  d->m_clip_rect_state.apply_item_matrix(rect, rect_clip_pts);
  d->m_clip_rect_state.m_all_content_culled = d->m_clip_rect_state.m_all_content_culled
    || d->m_clip_rect_state.poly_is_culled(rect_clip_pts)
    || d->m_clip_store.intersect_current_against_polygon(rect_clip_pts);

  if (d->m_clip_rect_state.m_all_content_culled)
    {
      /* everything is clipped anyways, adding more clipping does not matter */
      return;
    }

  if (!d->m_clip_rect_state.m_clip_rect.m_enabled)
    {
      /* no clipped rect defined yet, just take the arguments
       * as the clipping window
       */
      d->m_clip_rect_state.m_clip_rect = clip_rect(rect);
      d->m_clip_rect_state.set_clip_equations_to_clip_rect(ClipRectState::rect_in_local_coordinates);
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
      d->m_clip_rect_state.set_clip_equations_to_clip_rect(ClipRectState::rect_in_local_coordinates);
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
  ExtendedPool::PackedClipEquations prev_clip, current_clip;

  prev_clip = d->m_clip_rect_state.clip_equations_state(d->m_pool);
  FASTUIDRAWassert(prev_clip);

  d->m_clip_rect_state.m_clip_rect = clip_rect(rect);

  std::bitset<4> skip_occluder;
  skip_occluder = d->m_clip_rect_state.set_clip_equations_to_clip_rect(prev_clip,
                                                                       ClipRectState::rect_in_local_coordinates);
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
  d->m_clip_rect_state.override_item_matrix_state(d->identity_matrix());

  reference_counted_ptr<ZDataCallBack> zdatacallback;
  zdatacallback = FASTUIDRAWnew ZDataCallBack();

  fastuidraw::PainterBlendShader* old_blend;
  BlendMode old_blend_mode;

  old_blend = d->packer()->blend_shader();
  old_blend_mode = d->packer()->blend_mode();

  blend_shader(blend_porter_duff_dst);

  /* we temporarily set the clipping to a slightly
   * larger rectangle when drawing the occluders.
   * We do this because round off error can have us
   * miss a few pixels when drawing the occluder
   */
  PainterClipEquations slightly_bigger(current_clip.unpacked_value());
  for(unsigned int i = 0; i < 4; ++i)
    {
      float f;
      vec3 &eq(slightly_bigger.m_clip_equations[i]);

      f = t_abs(eq.x()) * d->m_one_pixel_width.x() + t_abs(eq.y()) * d->m_one_pixel_width.y();
      eq.z() += 0.5 * f;
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
                                        prev_clip.unpacked_value().m_clip_equations[i]);
        }
    }
  d->packer()->remove_callback(zdatacallback);

  d->m_clip_rect_state.clip_equations_state(current_clip);

  /* add to occluder stack */
  d->m_occluder_stack.push_back(occluder_stack_entry(zdatacallback->m_actions));

  d->m_clip_rect_state.stop_override_item_matrix_state();
  d->packer()->blend_shader(old_blend, old_blend_mode);
}

fastuidraw::GlyphAtlas&
fastuidraw::Painter::
glyph_atlas(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_backend_factory->glyph_atlas();
}

fastuidraw::ImageAtlas&
fastuidraw::Painter::
image_atlas(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_backend_factory->image_atlas();
}

fastuidraw::ColorStopAtlas&
fastuidraw::Painter::
colorstop_atlas(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_backend_factory->colorstop_atlas();
}

fastuidraw::GlyphCache&
fastuidraw::Painter::
glyph_cache(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_backend_factory->glyph_cache();
}

fastuidraw::PainterShaderRegistrar&
fastuidraw::Painter::
painter_shader_registrar(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->m_backend_factory->painter_shader_registrar();
}

fastuidraw::PainterBlendShader*
fastuidraw::Painter::
blend_shader(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->packer()->blend_shader();
}

fastuidraw::BlendMode
fastuidraw::Painter::
blend_mode(void) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  return d->packer()->blend_mode();
}

void
fastuidraw::Painter::
blend_shader(const reference_counted_ptr<PainterBlendShader> &h,
             BlendMode mode)
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);
  d->packer()->blend_shader(h.get(), mode);
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

void
fastuidraw::Painter::
query_stats(c_array<unsigned int> dst) const
{
  PainterPrivate *d;
  d = static_cast<PainterPrivate*>(m_d);

  FASTUIDRAWassert(dst.size() == PainterPacker::num_stats);
  for (unsigned int i = 0; i < dst.size() && i < PainterPacker::num_stats; ++i)
    {
      dst[i] = d->m_stats[i];
    }
}

/////////////////////////////////////
// PainterEnums methods
fastuidraw::c_string
fastuidraw::PainterEnums::
label(enum query_stats_t st)
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
      EASY(num_deferred_coverages);
    default:
      return "unknown";
    }

#undef EASY
}
