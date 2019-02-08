/*!
 * \file glyph_render_data_banded_rays.cpp
 * \brief file glyph_render_data_banded_rays.cpp
 *
 * Copyright 2018 by Intel.
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
#include <algorithm>
#include <set>
#include <map>
#include <cstdlib>
#include <mutex>
#include <fstream>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/math.hpp>
#include <fastuidraw/text/glyph_generate_params.hpp>
#include <fastuidraw/text/glyph_render_data_banded_rays.hpp>
#include "../private/bounding_box.hpp"
#include "../private/util_private.hpp"
#include "../private/util_private_ostream.hpp"

namespace
{

  enum band_t
    {
      vertical_band = 0, /* x-range */
      horizontal_band = 1, /* y-range */
    };

  class Transformation
  {
  public:
    explicit
    Transformation(const fastuidraw::RectT<int> &glyph_rect);

    template<enum band_t>
    uint32_t
    pack_point(fastuidraw::vec2 p) const;

    static const int min_value = -fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value;
    static const int max_value = fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value;

  private:
    fastuidraw::vec2 m_scale, m_translate;
  };

  class Curve
  {
  public:
    Curve(const fastuidraw::vec2 &a,
          const fastuidraw::vec2 &b):
      m_start(a),
      m_end(b),
      m_control((a + b) * 0.5f),
      m_has_control(false)
    {}

    Curve(const fastuidraw::vec2 &a,
          const fastuidraw::vec2 &ct,
          const fastuidraw::vec2 &b):
      m_start(a),
      m_end(b),
      m_control(ct),
      m_has_control(true)
    {}

    const fastuidraw::vec2&
    start(void) const { return m_start; }

    const fastuidraw::vec2&
    end(void) const { return m_end; }

    const fastuidraw::vec2&
    control(void) const
    {
      return m_control;
    }

    bool
    has_control(void) const
    {
      return m_has_control;
    }

    template<enum band_t b>
    bool
    intersects_band(float min_v, float max_v) const
    {
      /* band_t made cleverly so that we just check
       * against [b] for the values on band interection
       */
      bool min_culls, max_culls;

      min_culls = (m_start[b] < min_v) && (m_end[b] < min_v)
        && (m_control[b] < min_v);

      max_culls = (m_start[b] > max_v) && (m_end[b] > max_v)
        && (m_control[b] > max_v);

      return !(min_culls || max_culls);
    }

    template<enum band_t b>
    bool
    present_before_split(float split_v) const
    {
      /* for splitting, we check the coord 1 - b. */
      const int c(1 - b);
      return (m_start[c] <= split_v) || (m_end[c] <= split_v)
        || (m_control[c] <= split_v);
    }

    template<enum band_t b>
    bool
    present_after_split(float split_v) const
    {
      /* for splitting, we check the coord 1 - b. */
      const int c(1 - b);
      return (m_start[c] >= split_v) || (m_end[c] >= split_v)
        || (m_control[c] >= split_v);
    }

    float
    min_coordinate(int coord) const
    {
      float v;

      v = fastuidraw::t_min(m_control[coord], m_end[coord]);
      return fastuidraw::t_min(m_start[coord], v);
    }

    float
    max_coordinate(int coord) const
    {
      float v;

      v = fastuidraw::t_max(m_control[coord], m_end[coord]);
      return fastuidraw::t_max(m_start[coord], v);
    }

  private:
    fastuidraw::vec2 m_start, m_end, m_control;
    bool m_has_control;
  };

  class Contour
  {
  public:
    Contour(void)
    {}

    explicit
    Contour(fastuidraw::vec2 pt):
      m_last_pt(pt)
    {}

    void
    line_to(fastuidraw::vec2 pt)
    {
      if (m_last_pt != pt)
        {
          m_curves.push_back(Curve(m_last_pt, pt));
          m_last_pt = pt;
        }
    }

    void
    quadratic_to(fastuidraw::vec2 ct,
                 fastuidraw::vec2 pt)
    {
      if (m_last_pt != pt)
        {
          m_curves.push_back(Curve(m_last_pt, ct, pt));
          m_last_pt = pt;
        }
    }

    bool
    is_good(void) const
    {
      return !m_curves.empty()
        && m_curves.front().start() == m_curves.back().end();
    }

    bool
    empty(void) const
    {
      return m_curves.empty();
    }

    const std::vector<Curve>&
    curves(void) const
    {
      return m_curves;
    }

  private:
    fastuidraw::vec2 m_last_pt;
    std::vector<Curve> m_curves;
  };

  template<enum band_t BandType>
  class SortCurvesCullAfterSplit
  {
  public:
    bool
    operator()(const Curve &lhs, const Curve &rhs) const
    {
      /* we want to quickly cull curves once we
       * are far along when going in increasing.
       * If we are at value V, we want to stop
       * walking the array once we know all the
       * following elements come completely before
       * V. Thus we want those whose maximum
       * coordinate is before V removed which
       * means the sorting is that larger maximum
       * values come first.
       */
      return lhs.max_coordinate(1 - BandType) > rhs.max_coordinate(1 - BandType);
    }
  };

  template<enum band_t BandType>
  class SortCurvesCullBeforeSplit
  {
  public:
    bool
    operator()(const Curve &lhs, const Curve &rhs) const
    {
      /* we want to quickly cull curves once we
       * are far along when going in decreasing.
       * If we are at value V, we want to stop
       * walking the array once we know all the
       * following elements come completely after
       * V. Thus we want those whose minimum
       * coordinate is after V removed which
       * means the sorting is that smaller minimum
       * values come first.
       */
      return lhs.min_coordinate(1 - BandType) < rhs.min_coordinate(1 - BandType);
    }
  };

  template<enum band_t BandType>
  class Band
  {
  public:
    class SplitBand
    {
    public:
      Band m_before_split;
      Band m_after_split;
      float m_split_pt;

      void
      assign_curve_list_offset(uint32_t &value)
      {
        m_before_split.assign_curve_list_offset(value);
        m_after_split.assign_curve_list_offset(value);
      }
    };

    Band(void)
    {}

    Band(float min_v, float max_v):
      m_min_v(min_v),
      m_max_v(max_v)
    {}

    void
    add(const Curve &curve)
    {
      if (curve.intersects_band<BandType>(m_min_v, m_max_v))
        {
          m_curves.push_back(curve);
        }
    }

    template<typename iterator>
    void
    add(iterator b, iterator e)
    {
      for (;b != e; ++b)
        {
          add(*b);
        }
    }

    void
    add(const Contour &contour)
    {
      const std::vector<Curve> &curves(contour.curves());
      add(curves.begin(), curves.end());
    }

    void
    split(SplitBand *dst, float split_v) const
    {
      dst->m_split_pt = split_v;

      dst->m_before_split.m_min_v = m_min_v;
      dst->m_before_split.m_max_v = m_max_v;
      dst->m_before_split.m_curves.clear();

      dst->m_after_split.m_min_v = m_min_v;
      dst->m_after_split.m_max_v = m_max_v;
      dst->m_after_split.m_curves.clear();

      for (const Curve &C : m_curves)
        {
          if (C.present_before_split<BandType>(split_v))
            {
              dst->m_before_split.m_curves.push_back(C);
            }

          if (C.present_after_split<BandType>(split_v))
            {
              dst->m_after_split.m_curves.push_back(C);
            }
        }
      std::sort(dst->m_before_split.m_curves.begin(),
                dst->m_before_split.m_curves.end(),
                SortCurvesCullBeforeSplit<BandType>());

      std::sort(dst->m_after_split.m_curves.begin(),
                dst->m_after_split.m_curves.end(),
                SortCurvesCullAfterSplit<BandType>());
    }

    const std::vector<Curve>&
    curves(void) const
    {
      return m_curves;
    }

    void
    assign_curve_list_offset(uint32_t &value)
    {
      m_curve_list_offset = value;

      // each curve takes three 32-bit values.
      value += 3 * m_curves.size();
    }

    uint32_t
    curve_list_offset(void) const
    {
      return m_curve_list_offset;
    }

    uint32_t
    pack_band_value(void) const
    {
      using namespace fastuidraw;
      typedef GlyphRenderDataBandedRays G;

      uint32_t v;
      v = pack_bits(G::band_numcurves_bit0,
                    G::band_numcurves_numbits,
                    m_curves.size())
        | pack_bits(G::band_curveoffset_bit0,
                    G::band_curveoffset_numbits,
                    m_curve_list_offset);
      return v;
    }

    void
    pack_curves(fastuidraw::c_array<fastuidraw::generic_data> dst,
                const Transformation &tr) const
    {
      uint32_t offset(m_curve_list_offset);
      for (unsigned int i = 0, endi = m_curves.size(); i < endi; ++i, offset += 3)
        {
          dst[offset + 0].u = tr.pack_point<BandType>(m_curves[i].start());
          dst[offset + 1].u = tr.pack_point<BandType>(m_curves[i].control());
          dst[offset + 2].u = tr.pack_point<BandType>(m_curves[i].end());
        }
    }

  private:
    float m_min_v, m_max_v;
    std::vector<Curve> m_curves;
    uint32_t m_curve_list_offset;
  };

  class GlyphPath
  {
  public:
    GlyphPath(void);

    void
    move_to(const fastuidraw::vec2 &pt)
    {
      if (!m_contours.empty() && m_contours.back().empty())
        {
          m_contours.pop_back();
        }
      FASTUIDRAWassert(m_contours.empty() || m_contours.back().is_good());
      m_contours.push_back(Contour(pt));
    }

    void
    quadratic_to(const fastuidraw::vec2 &ct,
                 const fastuidraw::vec2 &pt)
    {
      FASTUIDRAWassert(!m_contours.empty());
      m_contours.back().quadratic_to(ct, pt);
    }

    void
    line_to(const fastuidraw::vec2 &pt)
    {
      FASTUIDRAWassert(!m_contours.empty());
      m_contours.back().line_to(pt);
    }

    const std::vector<Contour>&
    contours(void) const
    {
      return m_contours;
    }

    void
    set_glyph_rect(const fastuidraw::RectT<int> &bb)
    {
      if (!m_contours.empty() && m_contours.back().empty())
        {
          m_contours.pop_back();
        }
      m_glyph_rect = bb;
    }

    const fastuidraw::RectT<int>&
    glyph_rect(void) const
    {
      return m_glyph_rect;
    }

    template<enum band_t b>
    void
    create_bands(std::vector<Band<b> > *dst) const;

  private:
    fastuidraw::RectT<int> m_glyph_rect;
    std::vector<Contour> m_contours;
  };

  class GlyphRenderDataBandedRaysPrivate
  {
  public:
    GlyphRenderDataBandedRaysPrivate(void)
    {
      m_glyph = FASTUIDRAWnew GlyphPath();
    }

    ~GlyphRenderDataBandedRaysPrivate(void)
    {
      if (m_glyph)
        {
          FASTUIDRAWdelete(m_glyph);
        }
    }

    GlyphPath *m_glyph;
    fastuidraw::ivec2 m_num_bands;
    enum fastuidraw::PainterEnums::fill_rule_t m_fill_rule;
    std::vector<fastuidraw::generic_data> m_render_data;
  };
}

//////////////////////////
// Transformation methods
Transformation::
Transformation(const fastuidraw::RectT<int> &glyph_rect)
{
  float V(max_value - min_value);

  m_scale = fastuidraw::vec2(V, V) / fastuidraw::vec2(glyph_rect.size());

  /*
   * Want: rect.min * scale + translate = min_value
   * Thus: translate = min_value - rect.min * scale
   */
  m_translate = fastuidraw::vec2(min_value, min_value) - fastuidraw::vec2(glyph_rect.m_min_point) * m_scale;
}

template<enum band_t BandType>
uint32_t
Transformation::
pack_point(fastuidraw::vec2 p) const
{
  fastuidraw::u16vec2 fp16;

  p = m_scale * p + m_translate;
  fastuidraw::convert_to_fp16(p, fp16);
  if (BandType == vertical_band)
    {
      std::swap(fp16.x(), fp16.y());
    }
  return fp16.x() | (fp16.y() << 16u);
}

////////////////////////////////////////////////
//GlyphPath methods
GlyphPath::
GlyphPath(void)
{
}

template<enum band_t b>
void
GlyphPath::
create_bands(std::vector<Band<b> > *dst) const
{
  unsigned num_curves(0), num_bands;
  for (const auto &c : m_contours)
    {
      num_curves += c.curves().size();
    }

  /* TODO: have a better way to decide how many
   * bands, the simple algorithm below simple
   * gives a ew band for every 8 curves in
   * the glyph.
   */
  num_bands = (num_curves >> 3u);
  if (num_curves > (num_bands << 3u))
    {
      ++num_bands;
    }

  /* control the max value a little.. */
  num_bands = fastuidraw::t_min(num_bands, 128u);

  float delta, min_v(m_glyph_rect.m_min_point[b]);
  delta = (m_glyph_rect.m_max_point[b] - m_glyph_rect.m_min_point[b]) / float(num_bands);

  dst->clear();
  dst->reserve(num_bands);

  /* we add a little slack around the band to capture
   * curves that nearly touch the boundary of a band.
   */
  const float epsilon(1e-5);
  float slack(epsilon * (m_glyph_rect.m_max_point[b] - m_glyph_rect.m_min_point[b]));

  /* TODO: make this faster by computing for each curve
   * what band range it touches, instead of testing for
   * each band seperately.
   */
  for (unsigned int i = 0; i < num_bands; ++i, min_v += delta)
    {
      dst->push_back(Band<b>(min_v - slack, min_v + delta + slack));
      dst->back().add(m_contours.begin(), m_contours.end());
    }
}

/////////////////////////////////////////////////
// fastuidraw::GlyphRenderDataBandedRays methods
fastuidraw::GlyphRenderDataBandedRays::
GlyphRenderDataBandedRays(void)
{
  m_d = FASTUIDRAWnew GlyphRenderDataBandedRaysPrivate();
}

fastuidraw::GlyphRenderDataBandedRays::
~GlyphRenderDataBandedRays()
{
  GlyphRenderDataBandedRaysPrivate *d;

  d = static_cast<GlyphRenderDataBandedRaysPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::GlyphRenderDataBandedRays::
move_to(ivec2 pt)
{
  GlyphRenderDataBandedRaysPrivate *d;

  d = static_cast<GlyphRenderDataBandedRaysPrivate*>(m_d);
  FASTUIDRAWassert(d->m_glyph);
  if (!d->m_glyph)
    {
      return;
    }
  d->m_glyph->move_to(vec2(pt));
}

void
fastuidraw::GlyphRenderDataBandedRays::
quadratic_to(ivec2 ct, ivec2 pt)
{
  GlyphRenderDataBandedRaysPrivate *d;

  d = static_cast<GlyphRenderDataBandedRaysPrivate*>(m_d);
  FASTUIDRAWassert(d->m_glyph);
  if (!d->m_glyph)
    {
      return;
    }
  d->m_glyph->quadratic_to(vec2(ct), vec2(pt));
}

void
fastuidraw::GlyphRenderDataBandedRays::
line_to(ivec2 pt)
{
  GlyphRenderDataBandedRaysPrivate *d;

  d = static_cast<GlyphRenderDataBandedRaysPrivate*>(m_d);
  FASTUIDRAWassert(d->m_glyph);
  if (!d->m_glyph)
    {
      return;
    }
  d->m_glyph->line_to(vec2(pt));
}

void
fastuidraw::GlyphRenderDataBandedRays::
finalize(enum PainterEnums::fill_rule_t f, const RectT<int> &glyph_rect)
{
  GlyphRenderDataBandedRaysPrivate *d;
  ivec2 sz;

  d = static_cast<GlyphRenderDataBandedRaysPrivate*>(m_d);
  FASTUIDRAWassert(d->m_glyph);
  if (!d->m_glyph)
    {
      return;
    }

  FASTUIDRAWassert(f == PainterEnums::odd_even_fill_rule || f == PainterEnums::nonzero_fill_rule);
  d->m_fill_rule = f;
  d->m_glyph->set_glyph_rect(glyph_rect);

  if (d->m_glyph->contours().empty())
    {
      FASTUIDRAWdelete(d->m_glyph);
      d->m_glyph = nullptr;
      d->m_num_bands[vertical_band] = 0;
      d->m_num_bands[horizontal_band] = 0;
      return;
    }

  /* step 1: break into bands, the icky part is to
   *         decide how many bands horizontally and
   *         vetically are needed;
   */
  std::vector<Band<horizontal_band> > horiz_bands;
  std::vector<Band<vertical_band> > vert_bands;

  d->m_glyph->create_bands(&horiz_bands);
  d->m_glyph->create_bands(&vert_bands);

  /* step 2: split each of the horizontal and vertical
   *         bands in the above (the splitting also
   *         automatically sorts them as well). The
   *         split is -ALWAYS- in the middle.
   */
  std::vector<Band<horizontal_band>::SplitBand> split_horiz_bands(horiz_bands.size());
  std::vector<Band<vertical_band>::SplitBand > split_vert_bands(vert_bands.size());
  vec2 split_pt;

  split_pt = 0.5f * vec2(d->m_glyph->glyph_rect().m_min_point + d->m_glyph->glyph_rect().m_max_point);
  for (unsigned int i = 0; i < horiz_bands.size(); ++i)
    {
      horiz_bands[i].split(&split_horiz_bands[i], split_pt.x());
    }
  for (unsigned int i = 0; i < vert_bands.size(); ++i)
    {
      vert_bands[i].split(&split_vert_bands[i], split_pt.y());
    }

  /* step 3: compute the offsets. The data packing is first
   *         that each band takes a single 32-bit value
   *         (that is packed as according to band_t) then
   *         comes the location of each of the curve lists.
   */
  uint32_t offset, H, V;

  H = split_horiz_bands.size();
  V = split_vert_bands.size();

  /* the starting offset for the curve lists comes after all
   * the bands
   */
  offset = 2 * (H + V);
  for (auto &p : split_horiz_bands)
    {
      p.assign_curve_list_offset(offset);
    }
  for (auto &p : split_vert_bands)
    {
      p.assign_curve_list_offset(offset);
    }

  // the needed room is now stored in offset
  d->m_render_data.resize(offset);
  c_array<generic_data> dst(make_c_array(d->m_render_data));

  /* step 4: pack the data
   * The bands come in a very specific order:
   *  - horizontal_band_plus_infinity <--> split_horiz_bands.m_after_split
   *  - horizontal_band_negative_infinity <--> split_horiz_bands.m_before_split
   *  - vertical_band_plus_infinity <--> split_vert_bands.m_after_split
   *  - vertical_band_negative_infinity <--> split_vert_bands.m_before_split
   */
  Transformation tr(glyph_rect);

  for (unsigned int i = 0; i < H; ++i)
    {
      dst[i].u = split_horiz_bands[i].m_after_split.pack_band_value();
      dst[i + H].u = split_horiz_bands[i].m_before_split.pack_band_value();

      split_horiz_bands[i].m_after_split.pack_curves(dst, tr);
      split_horiz_bands[i].m_before_split.pack_curves(dst, tr);
    }
  for (unsigned int i = 0; i < V; ++i)
    {
      dst[i + 2 * H].u = split_vert_bands[i].m_after_split.pack_band_value();
      dst[i + 2 * H + V].u = split_vert_bands[i].m_before_split.pack_band_value();

      split_vert_bands[i].m_after_split.pack_curves(dst, tr);
      split_vert_bands[i].m_before_split.pack_curves(dst, tr);
    }

  /* step 6: record the data neeed for shading */
  d->m_num_bands[vertical_band] = V;
  d->m_num_bands[horizontal_band] = H;

  FASTUIDRAWdelete(d->m_glyph);
  d->m_glyph = nullptr;
}

enum fastuidraw::return_code
fastuidraw::GlyphRenderDataBandedRays::
upload_to_atlas(GlyphAtlasProxy &atlas_proxy,
                GlyphAttribute::Array &attributes) const
{
  GlyphRenderDataBandedRaysPrivate *d;
  d = static_cast<GlyphRenderDataBandedRaysPrivate*>(m_d);

  FASTUIDRAWassert(!d->m_glyph);
  if (d->m_glyph)
    {
      return routine_fail;
    }

  int data_offset;
  data_offset = atlas_proxy.allocate_data(make_c_array(d->m_render_data));
  if (data_offset == -1)
    {
      return routine_fail;
    }

  attributes.resize(glyph_num_attributes);
  for (unsigned int c = 0; c < 4; ++c)
    {
      uint32_t x, y;

      x = (c & GlyphAttribute::right_corner_mask) ? 1u : 0u;
      y = (c & GlyphAttribute::top_corner_mask)   ? 1u : 0u;

      attributes[glyph_normalized_x].m_data[c] = x;
      attributes[glyph_normalized_y].m_data[c] = y;
    }
  attributes[glyph_num_vertical_bands].m_data = uvec4(d->m_num_bands[vertical_band]);
  attributes[glyph_num_horizontal_bands].m_data = uvec4(d->m_num_bands[horizontal_band]);

  /* If the fill rule is odd-even, the leading bit
   * of data_offset is made to be up.
   */
  FASTUIDRAWassert((data_offset & FASTUIDRAW_MASK(31u, 1)) == 0u);
  if (d->m_fill_rule == PainterEnums::odd_even_fill_rule)
    {
      data_offset |= FASTUIDRAW_MASK(31u, 1);
    }
  attributes[glyph_offset].m_data = uvec4(data_offset);

  return routine_success;
}

enum fastuidraw::return_code
fastuidraw::GlyphRenderDataBandedRays::
query(c_array<const fastuidraw::generic_data> *gpu_data) const
{
  GlyphRenderDataBandedRaysPrivate *d;
  d = static_cast<GlyphRenderDataBandedRaysPrivate*>(m_d);

  FASTUIDRAWassert(!d->m_glyph);
  if (d->m_glyph)
    {
      return routine_fail;
    }

  *gpu_data = make_c_array(d->m_render_data);

  return routine_success;
}
