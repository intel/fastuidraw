/*!
 * \file glyph_render_data_curve_pair.cpp
 * \brief glyph_render_data_curve_pair.cpp
 *
 * entry_ctor_helper code heavily inspired from WRATHTextureFontFreeType_CurveAnalytic.cpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 */

#include <vector>
#include <fastuidraw/text/glyph_render_data_curve_pair.hpp>

#include "../private/util_private.hpp"

namespace
{
  void
  pack_curve_pair(const fastuidraw::GlyphRenderDataCurvePair::entry &src,
                  fastuidraw::vec2 delta,
                  fastuidraw::c_array<float> dst)
  {
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_p_x] = src.m_p.x() + delta.x();
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_p_y] = src.m_p.y() + delta.y();
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_zeta] = src.m_zeta;
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_combine_rule] = (src.m_use_min) ? 1.0f : 0.0f;

    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_curve0_m0] = src.m_curve0.m_m0;
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_curve0_m1] = src.m_curve0.m_m1;
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_curve0_q_x] = src.m_curve0.m_q.x();
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_curve0_q_y] = src.m_curve0.m_q.y();
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_curve0_quad_coeff] = src.m_curve0.m_quad_coeff;

    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_curve1_m0] = src.m_curve1.m_m0;
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_curve1_m1] = src.m_curve1.m_m1;
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_curve1_q_x] = src.m_curve1.m_q.x();
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_curve1_q_y] = src.m_curve1.m_q.y();
    dst[fastuidraw::GlyphRenderDataCurvePair::pack_offset_curve1_quad_coeff] = src.m_curve1.m_quad_coeff;
  }

  void
  pack_curve_pairs(fastuidraw::c_array<const fastuidraw::GlyphRenderDataCurvePair::entry> src,
                   fastuidraw::GlyphLocation L,
                   unsigned int entry_size, std::vector<fastuidraw::generic_data> &dst)
  {
    FASTUIDRAWassert(entry_size >= fastuidraw::GlyphRenderDataCurvePair::number_elements_to_pack);
    fastuidraw::generic_data init_value;
    fastuidraw::vec2 delta(L.location());

    init_value.f = 0.0f;
    dst.resize(entry_size * src.size(), init_value);
    fastuidraw::c_array<fastuidraw::generic_data> dst_ptr;
    dst_ptr = make_c_array(dst);

    for(unsigned int i = 0, endi = src.size(), j = 0;
        i < endi; ++i, j += entry_size)
      {
        fastuidraw::c_array<float> sub_dst;

        sub_dst = dst_ptr.sub_array(j, entry_size).reinterpret_pointer<float>();
        pack_curve_pair(src[i], delta, sub_dst);
      }
  }


  class entry_ctor_helper
  {
  public:
    entry_ctor_helper(fastuidraw::c_array<const fastuidraw::vec2> pts, bool reverse);

    bool
    tangled(const fastuidraw::vec2 &v) const;

    bool
    sub_tangled(const fastuidraw::vec2 &v) const;

    fastuidraw::GlyphRenderDataCurvePair::per_curve m_c;
    bool m_is_quadratic;
    fastuidraw::vec2 m_derivative, m_ray, m_accelleration;
  };

  fastuidraw::vec2
  apply_j(const fastuidraw::vec2 &v)
  {
    return fastuidraw::vec2(v.y(), -v.x());
  }

  class GlyphRenderDataCurvePairPrivate
  {
  public:
    GlyphRenderDataCurvePairPrivate(void):
      m_resolution(0, 0)
    {}

    void
    resize(fastuidraw::ivec2 sz)
    {
      FASTUIDRAWassert(sz.x() >= 0);
      FASTUIDRAWassert(sz.y() >= 0);
      m_texels.resize(sz.x() * sz.y());
      m_resolution = sz;
    }

    fastuidraw::ivec2 m_resolution;
    std::vector<uint16_t> m_texels;
    std::vector<fastuidraw::GlyphRenderDataCurvePair::entry> m_geometry_data;
  };
}


//////////////////////////////////
//entry_ctor_helper methods
entry_ctor_helper::
entry_ctor_helper(fastuidraw::c_array<const fastuidraw::vec2> pts, bool reverse)
{
  fastuidraw::vecN<fastuidraw::vec2, 3> work_room, poly_room;
  fastuidraw::c_array<fastuidraw::vec2> work(work_room), poly(poly_room);

  FASTUIDRAWassert(pts.size() == 2 || pts.size() == 3);
  work = work.sub_array(0, pts.size());
  poly = poly.sub_array(0, pts.size());

  /* copy control points */
  if(reverse)
    {
      std::copy(pts.rbegin(), pts.rend(), work.begin());
    }
  else
    {
      std::copy(pts.begin(), pts.end(), work.begin());
    }

  m_is_quadratic = (pts.size() == 3);

  /* move to origin */
  for(fastuidraw::c_array<fastuidraw::vec2>::reverse_iterator
        iter = work.rbegin(), end = work.rend(); iter != end; ++iter)
    {
      *iter -= work[0];
    }

  if(m_is_quadratic)
    {
      float div_q;

      /* realize Bernstein polynomial as usual polynomial*/
      poly[0] = work[0];
      poly[1] = 2.0f * ( -work[0] + work[1] );
      poly[2] = work[0] - 2.0f * work[1] + work[2];

      div_q = poly[2].magnitude();

      /* generage values for m_c */
      m_c.m_q = fastuidraw::vec2(poly[2].y(), -poly[2].x()) / div_q;
      m_c.m_m0 = fastuidraw::dot(m_c.m_q, poly[1]);
      m_c.m_m1 = fastuidraw::dot(poly[2], poly[1]) / div_q;
      m_c.m_quad_coeff = div_q;

      /* generate needed data for computing zeta in GlyphRenderDataCurvePair::entry */
      float m0sgn;
      m0sgn = (m_c.m_m0 > 0.0f) ? 1.0f : -1.0f;
      m_ray = m0sgn * fastuidraw::vec2(poly[2].y(), -poly[2].x());
      m_accelleration = poly[2];
      m_derivative = poly[1];
    }
  else
    {
      /* realize Bernstein polynomial as usual polynomial*/
      poly[0] = work[0];
      poly[1] = work[1] - work[0];

      /* generage values for m_c */
      float mag;
      mag = poly[1].magnitude();
      m_c.m_q = poly[1] / mag;
      m_c.m_m0 = mag;
      m_c.m_m1 = 0.0f;
      m_c.m_quad_coeff = 0.0f;

      /* generate needed data for computing zeta in GlyphRenderDataCurvePair::entry */
      m_derivative = poly[1];
      m_ray = m_derivative;
      m_accelleration = fastuidraw::vec2(0.0f, 0.0f);
    }

  m_c.m_m0 = 1.0f / m_c.m_m0;

}

bool
entry_ctor_helper::
tangled(const fastuidraw::vec2 &v) const
{
  return m_is_quadratic and sub_tangled(v);
}

bool
entry_ctor_helper::
sub_tangled(const fastuidraw::vec2 &v) const
{
  float r, rr, d, dd;
  fastuidraw::vec2 vv(-v.y(), v.x());

  r = fastuidraw::dot(v, m_ray);
  rr = fastuidraw::dot(vv, m_ray);

  d = fastuidraw::dot(v, m_derivative);
  dd = fastuidraw::dot(vv, m_derivative);

  return (r > 0.0f) and (d > 0.0f) and ( (rr > 0.0f) xor (dd > 0.0f) );
}

//////////////////////////////////////////////
// fastuidraw::GlyphRenderDataCurvePair::entry methods
fastuidraw::GlyphRenderDataCurvePair::entry::
entry(c_array<const vec2> pts, int curve0_count):
  m_type(entry_has_curves)
{
  c_array<const vec2> curve0_pts, curve1_pts;

  FASTUIDRAWassert(curve0_count > 0);
  curve0_pts = pts.sub_array(0, curve0_count);
  curve1_pts = pts.sub_array(curve0_count - 1);

  entry_ctor_helper c0(curve0_pts, true), c1(curve1_pts, false);

  m_p = curve1_pts[0];
  m_curve0 = c0.m_c;
  m_curve1 = c1.m_c;

  /* compute whether or not to use max or min for
     the curve pair; the value is determined by
     the derivatives at the point where they meet.
  */
  float d;
  bool tangential(false), tangled;

  d = dot(c0.m_derivative, apply_j(c1.m_derivative));
  if(d == 0.0f || d == -0.0f)
    {
      float rescale;

      tangential = true;
      rescale = c1.m_derivative.magnitude() / c0.m_derivative.magnitude();
      d = dot(apply_j(c1.m_accelleration), c0.m_derivative)
        + rescale * dot(apply_j(c1.m_derivative), c0.m_accelleration);
    }

  if(d < 0.0f)
    {
      /* only one need have positive pseudo-distance, so use max */
      m_use_min = false;
    }
  else
    {
      /* both need have positive pseudo-distance, so use min */
      m_use_min = true;
    }

  tangled = c0.tangled(c1.m_ray) or c1.tangled(c0.m_ray)
    or (!tangential and (c0.tangled(c1.m_derivative) or c1.tangled(c0.m_derivative)) );


  if(tangled xor m_use_min)
    {
      m_zeta = 1.0f;
    }
  else
    {
      m_zeta = -1.0f;
    }
}


fastuidraw::GlyphRenderDataCurvePair::entry::
entry(bool inside):
  m_p(0.0f, 0.0f)
{
  /* The pseudo-distance computation computes:
      t = x / m_m0
     and ignores the computation if t < 0.0
     and uses zeta. So, what we do is make sure
     t is always negative, we do this be assuming
     that the x coordinate is always positive.
   */

  m_curve0.m_m0 = -1.0f;
  m_curve0.m_m1 = 0.0f;
  m_curve0.m_q = vec2(1.0f, 0.0f);
  m_curve0.m_quad_coeff = 0.0f;
  m_curve1 = m_curve0;
  if(inside)
    {
      m_use_min = false;
      m_type = entry_completely_covered;
    }
  else
    {
      m_use_min = true;
      m_type = entry_completely_uncovered;
    }

  bool tangled = false;
  if(tangled xor m_use_min)
    {
      m_zeta = 1.0f;
    }
  else
    {
      m_zeta = -1.0f;
    }
}

/////////////////////////////////////
// fastuidraw::GlyphRenderDataCurvePair methods
fastuidraw::GlyphRenderDataCurvePair::
GlyphRenderDataCurvePair(void)
{
  m_d = FASTUIDRAWnew GlyphRenderDataCurvePairPrivate();
}

fastuidraw::GlyphRenderDataCurvePair::
~GlyphRenderDataCurvePair()
{
  GlyphRenderDataCurvePairPrivate *d;
  d = static_cast<GlyphRenderDataCurvePairPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::ivec2
fastuidraw::GlyphRenderDataCurvePair::
resolution(void) const
{
  GlyphRenderDataCurvePairPrivate *d;
  d = static_cast<GlyphRenderDataCurvePairPrivate*>(m_d);
  return d->m_resolution;
}

fastuidraw::c_array<const uint16_t>
fastuidraw::GlyphRenderDataCurvePair::
active_curve_pair(void) const
{
  GlyphRenderDataCurvePairPrivate *d;
  d = static_cast<GlyphRenderDataCurvePairPrivate*>(m_d);
  return make_c_array(d->m_texels);
}

fastuidraw::c_array<uint16_t>
fastuidraw::GlyphRenderDataCurvePair::
active_curve_pair(void)
{
  GlyphRenderDataCurvePairPrivate *d;
  d = static_cast<GlyphRenderDataCurvePairPrivate*>(m_d);
  return make_c_array(d->m_texels);
}

fastuidraw::c_array<const fastuidraw::GlyphRenderDataCurvePair::entry>
fastuidraw::GlyphRenderDataCurvePair::
geometry_data(void) const
{
  GlyphRenderDataCurvePairPrivate *d;
  d = static_cast<GlyphRenderDataCurvePairPrivate*>(m_d);
  return make_c_array(d->m_geometry_data);
}

fastuidraw::c_array<fastuidraw::GlyphRenderDataCurvePair::entry>
fastuidraw::GlyphRenderDataCurvePair::
geometry_data(void)
{
  GlyphRenderDataCurvePairPrivate *d;
  d = static_cast<GlyphRenderDataCurvePairPrivate*>(m_d);
  return make_c_array(d->m_geometry_data);
}

void
fastuidraw::GlyphRenderDataCurvePair::
resize_active_curve_pair(ivec2 sz)
{
  GlyphRenderDataCurvePairPrivate *d;
  d = static_cast<GlyphRenderDataCurvePairPrivate*>(m_d);
  d->resize(sz);
}

void
fastuidraw::GlyphRenderDataCurvePair::
resize_geometry_data(int sz)
{
  GlyphRenderDataCurvePairPrivate *d;
  d = static_cast<GlyphRenderDataCurvePairPrivate*>(m_d);
  FASTUIDRAWassert(sz >= 0);
  d->m_geometry_data.resize(sz, fastuidraw::GlyphRenderDataCurvePair::entry(false));
}

enum fastuidraw::return_code
fastuidraw::GlyphRenderDataCurvePair::
upload_to_atlas(const reference_counted_ptr<GlyphAtlas> &atlas,
                GlyphLocation &atlas_location,
                GlyphLocation &secondary_atlas_location,
                int &geometry_offset,
                int &geometry_length) const
{
  GlyphRenderDataCurvePairPrivate *d;
  d = static_cast<GlyphRenderDataCurvePairPrivate*>(m_d);

  GlyphAtlas::Padding padding;
  padding.m_right = 1;
  padding.m_bottom = 1;

  std::vector<uint8_t> primary, secondary;
  std::vector<float> geometry;
  bool has_secondary(false);
  int num_texels(d->m_resolution.x() * d->m_resolution.y());

  /* fill primary
   */
  primary.resize(num_texels, 0);
  for(int y = 0, J = 0; y < d->m_resolution.y(); ++y)
    {
      for(int x = 0; x < d->m_resolution.x(); ++x, ++J)
        {
          uint16_t v;
          v = d->m_texels[J];
          if(v == completely_full_texel)
            {
              v = 1;
            }
          else if(v == completely_empty_texel)
            {
              v = 0;
            }
          else
            {
              v += 2;
            }
          primary[J] = v & 0xFF;
          if(v > 0xFF)
            {
              if(!has_secondary)
                {
                  has_secondary = true;
                  secondary.resize(num_texels, 0);
                }
              secondary[J] = (v >> 8);
            }
        }
    }

  secondary_atlas_location = GlyphLocation();
  geometry_offset = -1;
  geometry_length = 0;

  atlas_location = atlas->allocate(d->m_resolution, make_c_array(primary), padding);
  if(atlas_location.valid())
    {
      bool success(true);

      if(has_secondary)
        {
          secondary_atlas_location = atlas->allocate(d->m_resolution, make_c_array(secondary), padding);
          success = secondary_atlas_location.valid();
        }

      if(success && !d->m_geometry_data.empty())
        {
          unsigned int entry_size, alignment;
          std::vector<generic_data> geometry_data;

          alignment = atlas->geometry_store()->alignment();
          entry_size = round_up_to_multiple(number_elements_to_pack, alignment);
          pack_curve_pairs(make_c_array(d->m_geometry_data), atlas_location, entry_size, geometry_data);
          geometry_offset = atlas->allocate_geometry_data(make_c_array(geometry_data));
          geometry_length = geometry_data.size() / alignment;

          success = (geometry_offset != -1);
        }

      if(!success)
        {
          atlas->deallocate(atlas_location);
          atlas_location = GlyphLocation();
          if(secondary_atlas_location.valid())
            {
              atlas->deallocate(secondary_atlas_location);
              secondary_atlas_location = GlyphLocation();
            }
        }
    }

  return atlas_location.valid() ?
    routine_success :
    routine_fail;

}
