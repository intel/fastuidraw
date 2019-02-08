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

  template<enum band_t BandType>
  uint32_t
  pack_point(const fastuidraw::vec2 &p)
  {
    fastuidraw::u16vec2 fp16;
    fastuidraw::convert_to_fp16(p, fp16);
    if (BandType == vertical_band)
      {
        std::swap(fp16.x(), fp16.y());
      }
    return fp16.x() | (fp16.y() << 16u);
  }

  class Transformation
  {
  public:
    explicit
    Transformation(const fastuidraw::RectT<int> &glyph_rect)
    {
      const float V(2 * fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value);
      const float M(-fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value);

      m_scale = fastuidraw::vec2(V) / fastuidraw::vec2(glyph_rect.size());
      m_translate = fastuidraw::vec2(M) - m_scale * fastuidraw::vec2(glyph_rect.m_min_point);
    }

    fastuidraw::vec2
    operator()(const fastuidraw::vec2 &p) const
    {
      return m_scale * p + m_translate;
    }

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

    void
    apply_transformation(const Transformation &tr)
    {
      m_start = tr(m_start);
      m_end = tr(m_end);
      m_control = tr(m_control);
    }

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

    void
    apply_transformation(const Transformation &tr)
    {
      for (Curve &C : m_curves)
        {
          C.apply_transformation(tr);
        }
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
    static
    void
    create_bands(std::vector<Band> *dst,
                 const std::vector<Contour> &contours);

    void
    assign_curve_list_offset(uint32_t &value)
    {
      m_before_split.assign_curve_list_offset(value);
      m_after_split.assign_curve_list_offset(value);
    }

    void
    pack_curves(fastuidraw::c_array<fastuidraw::generic_data> dst) const
    {
      m_before_split.pack_curves(dst);
      m_after_split.pack_curves(dst);
    }

    uint32_t
    before_split_packed_band_value(void) const
    {
      return m_before_split.pack_band_value();
    }

    uint32_t
    after_split_packed_band_value(void) const
    {
      return m_after_split.pack_band_value();
    }

  private:
    class SubBand
    {
    public:
      SubBand(void);
      SubBand(const SubBand *parent, float minv, float maxv);

      void
      assign_curve_list_offset(uint32_t &value);

      uint32_t
      pack_band_value(void) const;

      void
      pack_curves(fastuidraw::c_array<fastuidraw::generic_data> dst) const;

      std::vector<Curve> m_curves;
      uint32_t m_curve_list_offset;
    };

    explicit
    Band(const std::vector<Contour> &contours);

    Band(const Band *parent, float min_v, float max_v);

    float
    compute_average_curve_coverage(void) const;

    void
    divide(std::vector<Band> *dst, float slack) const;

    float m_min_v, m_max_v;
    SubBand m_before_split;
    SubBand m_after_split;
  };

  class GlyphPath
  {
  public:
    GlyphPath(void)
    {}

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
    transform_curves(const fastuidraw::RectT<int> &bb);

  private:
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

///////////////////////////////////////
// Band::SubBand methods
template<enum band_t BandType>
Band<BandType>::SubBand::
SubBand(void)
{}

template<enum band_t BandType>
Band<BandType>::SubBand::
SubBand(const SubBand *parent, float minv, float maxv)
{
  for (const Curve &C : parent->m_curves)
    {
      if (C.intersects_band<BandType>(minv, maxv))
        {
          m_curves.push_back(C);
        }
    }
}

template<enum band_t BandType>
void
Band<BandType>::SubBand::
assign_curve_list_offset(uint32_t &value)
{
  m_curve_list_offset = value;
  // each curve takes three 32-bit values.
  value += 3 * m_curves.size();
}

template<enum band_t BandType>
uint32_t
Band<BandType>::SubBand::
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

template<enum band_t BandType>
void
Band<BandType>::SubBand::
pack_curves(fastuidraw::c_array<fastuidraw::generic_data> dst) const
{
  uint32_t offset(m_curve_list_offset);
  for (unsigned int i = 0, endi = m_curves.size(); i < endi; ++i, offset += 3)
    {
      dst[offset + 0].u = pack_point<BandType>(m_curves[i].start());
      dst[offset + 1].u = pack_point<BandType>(m_curves[i].control());
      dst[offset + 2].u = pack_point<BandType>(m_curves[i].end());
    }
}

///////////////////////////////
// Band methods
template<enum band_t BandType>
Band<BandType>::
Band(const std::vector<Contour> &contours):
  m_min_v(-fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value),
  m_max_v(fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value)
{
  for (const Contour &contour : contours)
    {
      for (const Curve &C : contour.curves())
        {
          if (C.present_before_split<BandType>(0.0f))
            {
              m_before_split.m_curves.push_back(C);
            }

          if (C.present_after_split<BandType>(0.0f))
            {
              m_after_split.m_curves.push_back(C);
            }
        }
    }
}

template<enum band_t BandType>
Band<BandType>::
Band(const Band *parent, float min_v, float max_v):
  m_min_v(min_v),
  m_max_v(max_v),
  m_before_split(&parent->m_before_split, min_v, max_v),
  m_after_split(&parent->m_after_split, min_v, max_v)
{}

template<enum band_t BandType>
void
Band<BandType>::
create_bands(std::vector<Band> *pdst,
             const std::vector<Contour> &contours)
{
  const float width(2 * fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value);
  const float epsilon(1e-5);
  const float slack(epsilon * width);
  const float avg_curve_thresh(fastuidraw::GlyphGenerateParams::banded_rays_average_number_curves_thresh());
  const unsigned int max_num_iterations(fastuidraw::GlyphGenerateParams::banded_rays_max_recursion());
  float avg_num_curves;
  unsigned int num_iterations;
  std::vector<std::vector<Band> > tmp(1);

  tmp.reserve(max_num_iterations + 1);
  tmp.back().push_back(Band(contours));

  //std::cout << "\n\nCreateBands(" << BandType << ")\n";

  /* basic idea: keep breaking each band in half until
   * each band does not have too many curves.
   */
  for (num_iterations = 0, avg_num_curves = tmp.back().back().compute_average_curve_coverage();
       num_iterations < max_num_iterations && avg_num_curves > avg_curve_thresh; ++num_iterations)
    {
      const std::vector<Band> &src(tmp.back());

      //std::cout << "\tIteration #" << num_iterations << " avg_num_curves " << avg_num_curves;

      /* the src reference will stay valid because of the
       * call to reserve before the loop.
       */
      tmp.push_back(std::vector<Band>());
      tmp.back().reserve(2u << num_iterations);
      for (const Band &band : src)
        {
          band.divide(&tmp.back(), slack);
        }

      avg_num_curves = 0.0f;
      for (const Band &band : tmp.back())
        {
          avg_num_curves += band.compute_average_curve_coverage();
        }
      avg_num_curves /= static_cast<float>(tmp.back().size());
      //std::cout << " ---> " << avg_num_curves << "\n";
    }

  /* sort the curves for the shader */
  for (Band &b : tmp.back())
    {
      std::sort(b.m_before_split.m_curves.begin(),
                b.m_before_split.m_curves.end(),
                SortCurvesCullBeforeSplit<BandType>());

      std::sort(b.m_after_split.m_curves.begin(),
                b.m_after_split.m_curves.end(),
                SortCurvesCullAfterSplit<BandType>());
    }
  std::swap(*pdst, tmp.back());
  //std::cout << "\n\tNumBands = " << pdst->size() << "\n";
}

template<enum band_t BandType>
float
Band<BandType>::
compute_average_curve_coverage(void) const
{
  const float edge(fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value);
  float curve_draw(0.0f);

  for (const Curve &c : m_after_split.m_curves)
    {
      /* a curve after the split hits all the area
       * between 0 and its maximum coordinate.
       */
      float v;
      v = fastuidraw::t_min(edge, c.max_coordinate(1 - BandType));
      curve_draw += fastuidraw::t_max(0.0f, v);
    }

  for (const Curve &c : m_before_split.m_curves)
    {
      /* a curve before the split hits all the area
       * between 0 and its minimum coordinate (which is negative!).
       */
      float v;
      v = fastuidraw::t_min(edge, -c.min_coordinate(1 - BandType));
      curve_draw += fastuidraw::t_max(0.0f, v);
    }

  return curve_draw / (2.0f * edge);
}

template<enum band_t BandType>
void
Band<BandType>::
divide(std::vector<Band> *dst, float slack) const
{
  float split(0.5 * (m_min_v + m_max_v));

  /* we add a little slack around the band to capture
   * curves that nearly touch the boundary of a band.
   */
  dst->push_back(Band(this, m_min_v, split + slack));
  dst->push_back(Band(this, split - slack, m_max_v));
}

////////////////////////////////////////////////
//GlyphPath methods
void
GlyphPath::
transform_curves(const fastuidraw::RectT<int> &bb)
{
  while (!m_contours.empty() && m_contours.back().empty())
    {
      m_contours.pop_back();
    }

  Transformation tr(bb);
  for (Contour &contour : m_contours)
    {
      FASTUIDRAWassert(contour.is_good());
      contour.apply_transformation(tr);
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

  if (d->m_glyph->contours().empty()
      || glyph_rect.m_min_point.x() == glyph_rect.m_max_point.x()
      || glyph_rect.m_min_point.y() == glyph_rect.m_max_point.y())
    {
      FASTUIDRAWdelete(d->m_glyph);
      d->m_glyph = nullptr;
      d->m_num_bands[vertical_band] = 0;
      d->m_num_bands[horizontal_band] = 0;
      return;
    }

  /* Transform all the curves from coordinates
   * they are given in to [-G, G]x[-G, G]
   * where G = GlyphRenderDataBandedRays::glyph_coord_value
   */
  d->m_glyph->transform_curves(glyph_rect);

  /* step 1: break into split-bands; The split is -ALWAYS-
   *         in the middle.
   */
  std::vector<Band<horizontal_band> > split_horiz_bands;
  std::vector<Band<vertical_band> > split_vert_bands;

  Band<horizontal_band>::create_bands(&split_horiz_bands, d->m_glyph->contours());
  Band<vertical_band>::create_bands(&split_vert_bands, d->m_glyph->contours());

  /* step 2: compute the offsets. The data packing is first
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

  /* step 3: pack the data
   * The bands come in a very specific order:
   *  - horizontal_band_plus_infinity <--> split_horiz_bands.m_after_split
   *  - horizontal_band_negative_infinity <--> split_horiz_bands.m_before_split
   *  - vertical_band_plus_infinity <--> split_vert_bands.m_after_split
   *  - vertical_band_negative_infinity <--> split_vert_bands.m_before_split
   */
  for (unsigned int i = 0; i < H; ++i)
    {
      dst[i].u = split_horiz_bands[i].after_split_packed_band_value();
      dst[i + H].u = split_horiz_bands[i].before_split_packed_band_value();

      split_horiz_bands[i].pack_curves(dst);
    }
  for (unsigned int i = 0; i < V; ++i)
    {
      dst[i + 2 * H].u = split_vert_bands[i].after_split_packed_band_value();
      dst[i + 2 * H + V].u = split_vert_bands[i].before_split_packed_band_value();

      split_vert_bands[i].pack_curves(dst);
    }

  /* step 4: record the data neeed for shading */
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
