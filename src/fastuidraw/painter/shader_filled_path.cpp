/*!
 * \file shader_filled_path.cpp
 * \brief file shader_filled_path.cpp
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
#include <fastuidraw/path.hpp>
#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/text/font.hpp>
#include <fastuidraw/text/glyph.hpp>
#include <fastuidraw/text/glyph_render_data_banded_rays.hpp>
#include <fastuidraw/text/glyph_render_data_restricted_rays.hpp>
#include <fastuidraw/painter/shader_filled_path.hpp>
#include <private/util_private.hpp>
#include <private/bounding_box.hpp>
#include <private/bezier_util.hpp>

namespace
{
  /* RestrictedRays, although slower in performance, gives more robust
   * anti-aliasing when zoomed in a great deal than BandedRays.
   */
#if 1
  typedef fastuidraw::GlyphRenderDataRestrictedRays RenderData;
  const enum fastuidraw::glyph_type GlyphType = fastuidraw::restricted_rays_glyph;

  inline
  void
  finalize_render_data(enum fastuidraw::PainterEnums::fill_rule_t f,
                       const fastuidraw::BoundingBox<float> &bbox,
                       fastuidraw::GlyphRenderDataRestrictedRays *data)
  {
    data->finalize(f, bbox.as_rect(),
                   4, //aim for 4 curves per box
                   12, //recurse up to 12 times.
                   fastuidraw::vec2(-1.0f, -1.0f));
  }

#else
  typedef fastuidraw::GlyphRenderDataBandedRays RenderData;
  const enum fastuidraw::glyph_type GlyphType = fastuidraw::banded_rays_glyph;

  inline
  void
  finalize_render_data(enum fastuidraw::PainterEnums::fill_rule_t f,
                       const fastuidraw::BoundingBox<float> &bbox,
                       fastuidraw::GlyphRenderDataBandedRays *data)
  {
    data->finalize(f, bbox.as_rect());
  }

#endif

  /* A BuilderPoint represents a line_to() or quadratic_to() command.
   */
  class BuilderCommand
  {
  public:
    enum type_t
      {
        type_move_to,
        type_line_to,
        type_quadratic_to,
      };

    static
    BuilderCommand
    move_to(fastuidraw::vec2 p)
    {
      BuilderCommand R;
      R.m_type = type_move_to;
      R.m_pt = p;
      return R;
    }

    static
    BuilderCommand
    line_to(fastuidraw::vec2 p)
    {
      BuilderCommand R;
      R.m_type = type_line_to;
      R.m_pt = p;
      return R;
    }

    static
    BuilderCommand
    quadratic_to(fastuidraw::vec2 c, fastuidraw::vec2 p)
    {
      BuilderCommand R;
      R.m_type = type_quadratic_to;
      R.m_control = c;
      R.m_pt = p;
      return R;
    }

    const fastuidraw::vec2&
    pt(void) const
    {
      return m_pt;
    }

    const fastuidraw::vec2&
    control(void) const
    {
      FASTUIDRAWassert(m_type == type_quadratic_to);
      return m_control;
    }

    enum type_t
    type(void) const
    {
      return m_type;
    }

  private:
    enum type_t m_type;
    fastuidraw::vec2 m_control;
    fastuidraw::vec2 m_pt;
  };

  class BuilderPrivate
  {
  public:
    void
    build_render_data(RenderData *dst, RenderData::query_info *dst_query) const;

    std::vector<BuilderCommand> m_data;
    fastuidraw::BoundingBox<float> m_bbox;
  };

  class AddPath
  {
  public:
    explicit
    AddPath(float tol,
            const fastuidraw::Path *path,
            fastuidraw::ShaderFilledPath::Builder *b):
      m_tol(tol),
      m_b(b),
      m_path(path),
      m_tess(nullptr)
    {}

    void
    add_to_render_data(void);

  private:
    void
    add_contour(unsigned int contour);

    void
    add_interpolator(const fastuidraw::PathContour *contour,
                     unsigned int contour_id, unsigned int interpolator_id);

    void
    add_interpolator_from_tess(unsigned int contour_id, unsigned int interpolator_id);

    float m_tol;
    fastuidraw::ShaderFilledPath::Builder *m_b;
    const fastuidraw::Path *m_path;
    const fastuidraw::TessellatedPath *m_tess;
  };

  class PerFillRule
  {
  public:
    fastuidraw::vecN<fastuidraw::PainterAttribute, 4> m_attribs;
    fastuidraw::vecN<fastuidraw::PainterIndex, 6> m_indices;
    fastuidraw::vecN<fastuidraw::GlyphAttribute, RenderData::glyph_num_attributes> m_glyph_attribs;
  };

  class ShaderFilledPathPrivate
  {
  public:
    explicit
    ShaderFilledPathPrivate(BuilderPrivate &B);

    ~ShaderFilledPathPrivate();

    const PerFillRule&
    per_fill_rule(fastuidraw::GlyphAtlas &atlas,
                  enum fastuidraw::PainterEnums::fill_rule_t fill_rule);

  private:
    void
    update_attribute_data(void);

    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> m_atlas;
    fastuidraw::BoundingBox<float> m_bbox;
    int m_allocation;
    std::vector<fastuidraw::generic_data> m_gpu_data;
    RenderData::query_info m_query_data;

    fastuidraw::vecN<PerFillRule, fastuidraw::PainterEnums::number_fill_rule> m_per_fill_rule;
    unsigned int m_number_times_atlas_cleared;
  };
}

////////////////////////////
// BuilderPrivate methods
void
BuilderPrivate::
build_render_data(RenderData *dst, RenderData::query_info *dst_query) const
{
  for (const BuilderCommand &cmd : m_data)
    {
      switch(cmd.type())
        {
        case BuilderCommand::type_move_to:
          dst->move_to(cmd.pt());
          break;
        case BuilderCommand::type_line_to:
          dst->line_to(cmd.pt());
          break;
        case BuilderCommand::type_quadratic_to:
          dst->quadratic_to(cmd.control(), cmd.pt());
          break;
        default:
          FASTUIDRAWassert(!"Bad enum!");
        }
    }
  /* the fill rule actually does not matter, since ShaderFilledPath
   * constructs its own attribute data.
   */
  finalize_render_data(fastuidraw::PainterEnums::nonzero_fill_rule, m_bbox, dst);
  dst->query(dst_query);
}

////////////////////////////////
// AddPath methods
void
AddPath::
add_to_render_data(void)
{
  for (unsigned int i = 0, endi = m_path->number_contours(); i < endi; ++i)
    {
      add_contour(i);
    }
}

void
AddPath::
add_contour(unsigned int contour_id)
{
  using namespace fastuidraw;

  reference_counted_ptr<const PathContour> contour(m_path->contour(contour_id));
  if (contour->number_interpolators() == 0)
    {
      return;
    }

  m_b->move_to(contour->point(0));
  for (unsigned int i = 0, endi = contour->number_interpolators(); i < endi; ++i)
    {
      add_interpolator(contour.get(), contour_id, i);
    }

  if (!contour->closed())
    {
      m_b->line_to(contour->point(0));
    }
}

void
AddPath::
add_interpolator(const fastuidraw::PathContour *contour,
                 unsigned int contour_id, unsigned int interpolator_id)
{
  using namespace fastuidraw;

  enum return_code R;
  const PathContour::interpolator_base *interpolator(contour->interpolator(interpolator_id).get());

  R = interpolator->add_to_builder(m_b, m_tol);
  if (R == routine_fail)
    {
      add_interpolator_from_tess(contour_id, interpolator_id);
    }
}

void
AddPath::
add_interpolator_from_tess(unsigned int contour_id, unsigned int interpolator_id)
{
  using namespace fastuidraw;

  float half_tol(0.5f * m_tol);
  if (!m_tess)
    {
      m_tess = m_path->tessellation(half_tol).get();
    }

  /* walk through the segments of the named interpolator */
  c_array<const TessellatedPath::segment> segments;
  const TessellatedPath::segment *start_arc(nullptr), *prev_segment(nullptr);

  segments = m_tess->edge_segment_data(contour_id, interpolator_id);
  for (const TessellatedPath::segment &S : segments)
    {
      if (prev_segment && start_arc
          && (S.m_type == TessellatedPath::line_segment
              || !S.m_continuation_with_predecessor))
        {
          unsigned int max_recustion(5);
          FASTUIDRAWassert(prev_segment->m_type == TessellatedPath::arc_segment);
          detail::add_arc_as_cubics(max_recustion, m_b, half_tol,
                                    start_arc->m_start_pt, prev_segment->m_end_pt,
                                    start_arc->m_center, start_arc->m_radius,
                                    start_arc->m_arc_angle.m_begin,
                                    prev_segment->m_arc_angle.m_end - start_arc->m_arc_angle.m_begin);
          start_arc = nullptr;
        }

      if (S.m_type == TessellatedPath::line_segment)
        {
          m_b->line_to(S.m_end_pt);
          start_arc = nullptr;
        }
      else
        {
          if (!start_arc)
            {
              start_arc = &S;
            }
        }
      prev_segment = &S;
    }

  if (start_arc)
    {
      unsigned int max_recustion(5);
      FASTUIDRAWassert(prev_segment->m_type == TessellatedPath::arc_segment);
      detail::add_arc_as_cubics(max_recustion, m_b, half_tol,
                                start_arc->m_start_pt, prev_segment->m_end_pt,
                                start_arc->m_center, start_arc->m_radius,
                                start_arc->m_arc_angle.m_begin,
                                prev_segment->m_arc_angle.m_end - start_arc->m_arc_angle.m_begin);
    }
}

////////////////////////////////////
// ShaderFilledPathPrivate methods
ShaderFilledPathPrivate::
ShaderFilledPathPrivate(BuilderPrivate &B):
  m_bbox(B.m_bbox),
  m_allocation(-1),
  m_number_times_atlas_cleared(0)
{
  RenderData render_data;
  B.build_render_data(&render_data, &m_query_data);

  /* Because B will go out of scope, we need to save the GPU data
   * to our own private array.
   */
  m_gpu_data.resize(m_query_data.m_gpu_data.size());
  std::copy(m_query_data.m_gpu_data.begin(), m_query_data.m_gpu_data.end(), m_gpu_data.begin());
  m_query_data.m_gpu_data = make_c_array(m_gpu_data);
}

ShaderFilledPathPrivate::
~ShaderFilledPathPrivate()
{
  if (m_allocation != -1 && m_number_times_atlas_cleared == m_atlas->number_times_cleared())
    {
      m_atlas->deallocate_data(m_allocation, m_gpu_data.size());
    }
}

const PerFillRule&
ShaderFilledPathPrivate::
per_fill_rule(fastuidraw::GlyphAtlas &atlas,
              enum fastuidraw::PainterEnums::fill_rule_t fill_rule)
{
  using namespace fastuidraw;

  if (m_atlas.get() != &atlas)
    {
      if (m_allocation != -1)
        {
          m_atlas->deallocate_data(m_allocation, m_gpu_data.size());
          m_allocation = -1;
        }
      m_atlas = &atlas;
    }

  if (m_allocation == -1 || m_number_times_atlas_cleared != m_atlas->number_times_cleared())
    {
      m_number_times_atlas_cleared = m_atlas->number_times_cleared();
      m_allocation = m_atlas->allocate_data(make_c_array(m_gpu_data));
      update_attribute_data();
    }
  FASTUIDRAWassert(m_allocation != -1);
  return m_per_fill_rule[fill_rule];
}

void
ShaderFilledPathPrivate::
update_attribute_data(void)
{
  using namespace fastuidraw;

  FASTUIDRAWassert(m_allocation != -1);
  for (unsigned int f = 0; f < m_per_fill_rule.size(); ++f)
    {
      m_query_data.set_glyph_attributes(&m_per_fill_rule[f].m_glyph_attribs,
                                        static_cast<enum PainterEnums::fill_rule_t>(f),
                                        m_allocation);
      Glyph::pack_raw(m_per_fill_rule[f].m_glyph_attribs,
                      0, m_per_fill_rule[f].m_attribs,
                      0, m_per_fill_rule[f].m_indices,
                      m_bbox.min_point(), m_bbox.max_point());
    }
}

/////////////////////////////////////////////////
// fastuidraw::ShaderFilledPath::Builder methods
fastuidraw::ShaderFilledPath::Builder::
Builder(void)
{
  m_d = FASTUIDRAWnew BuilderPrivate();
}

fastuidraw::ShaderFilledPath::Builder::
~Builder()
{
  BuilderPrivate *d;
  d = static_cast<BuilderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::ShaderFilledPath::Builder::
move_to(vec2 pt)
{
  BuilderPrivate *d;
  d = static_cast<BuilderPrivate*>(m_d);
  d->m_data.push_back(BuilderCommand::move_to(pt));
  d->m_bbox.union_point(pt);
}

void
fastuidraw::ShaderFilledPath::Builder::
line_to(vec2 pt)
{
  BuilderPrivate *d;
  d = static_cast<BuilderPrivate*>(m_d);
  d->m_data.push_back(BuilderCommand::line_to(pt));
  d->m_bbox.union_point(pt);
}

void
fastuidraw::ShaderFilledPath::Builder::
quadratic_to(vec2 ct, vec2 pt)
{
  BuilderPrivate *d;
  d = static_cast<BuilderPrivate*>(m_d);
  d->m_data.push_back(BuilderCommand::quadratic_to(ct, pt));
  d->m_bbox.union_point(pt);
  d->m_bbox.union_point(ct);
}

void
fastuidraw::ShaderFilledPath::Builder::
add_path(float tol, const Path &path)
{
  AddPath add_path(tol, &path, this);
  add_path.add_to_render_data();
}

////////////////////////////////////////
// fastuidraw::ShaderFilledPath methods
fastuidraw::ShaderFilledPath::
ShaderFilledPath(const Builder &B)
{
  BuilderPrivate *bd;
  bd = static_cast<BuilderPrivate*>(B.m_d);

  m_d = FASTUIDRAWnew ShaderFilledPathPrivate(*bd);
}

fastuidraw::ShaderFilledPath::
~ShaderFilledPath()
{
  ShaderFilledPathPrivate *d;
  d = static_cast<ShaderFilledPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::ShaderFilledPath::
render_data(GlyphAtlas &atlas,
            enum PainterEnums::fill_rule_t fill_rule,
            c_array<const PainterAttribute> *out_attribs,
            c_array<const PainterIndex> *out_indices) const
{
  ShaderFilledPathPrivate *d;
  d = static_cast<ShaderFilledPathPrivate*>(m_d);

  const PerFillRule &F(d->per_fill_rule(atlas, fill_rule));
  *out_attribs = F.m_attribs;
  *out_indices = F.m_indices;
}

enum fastuidraw::glyph_type
fastuidraw::ShaderFilledPath::
render_type(void) const
{
  return GlyphType;
}
