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

  bool
  is_flat(float a, float b, float c)
  {
    fastuidraw::vec3 v(a, b, c);
    fastuidraw::vecN<uint16_t, 3> hv;

    fastuidraw::convert_to_fp16(v, hv);
    return hv[0] == hv[1] && hv[1] == hv[2];
  }

  bool
  is_flat(float a, float b)
  {
    fastuidraw::vec2 v(a, b);
    fastuidraw::vecN<uint16_t, 2> hv;

    fastuidraw::convert_to_fp16(v, hv);
    return hv[0] == hv[1];
  }

  class GlyphPath;

  class Transformation
  {
  public:
    explicit
    Transformation(const fastuidraw::Rect &glyph_rect)
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
    Curve(fastuidraw::vec2 a,
          fastuidraw::vec2 b):
      m_start(a),
      m_end(b),
      m_control((m_start + m_end) * 0.5f),
      m_is_horizontal(is_flat(a.y(), b.y())),
      m_is_vertical(is_flat(a.x(), b.x()))
    {}

    Curve(fastuidraw::vec2 a,
          fastuidraw::vec2 ct,
          fastuidraw::vec2 b):
      m_start(a),
      m_end(b),
      m_control(ct),
      m_is_horizontal(is_flat(a.y(), b.y(), ct.y())),
      m_is_vertical(is_flat(a.x(), b.x(), ct.x()))
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

    bool
    ignore_curve(enum band_t BandType) const
    {
      return (BandType == vertical_band) ?
        m_is_vertical :
        m_is_horizontal;
    }

  private:
    fastuidraw::vec2 m_start, m_end, m_control;
    bool m_is_horizontal, m_is_vertical;
  };

  class CurveID
  {
  public:
    unsigned int& contour(void) { return m_ID.x(); }
    unsigned int& curve(void) { return m_ID.y(); }
    unsigned int contour(void) const { return m_ID.x(); }
    unsigned int curve(void) const { return m_ID.y(); }
    bool operator<(const CurveID &rhs) const { return m_ID < rhs.m_ID; }
  private:
    fastuidraw::uvec2 m_ID;
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
  class CurveListHoard:fastuidraw::noncopyable
  {
  public:
    uint32_t
    fetch_curve_list_offset(const std::vector<CurveID> &p, uint32_t &value);

    void
    pack_curves(const GlyphPath &path,
                fastuidraw::c_array<fastuidraw::generic_data> dst) const;

  private:
    std::map<std::vector<CurveID>, uint32_t> m_offsets;
  };

  template<enum band_t BandType>
  class Band
  {
  public:
    static
    float
    create_bands(std::vector<Band> *dst, const GlyphPath &path,
                 unsigned int max_num_iterations, float avg_curve_thresh);

    void
    assign_curve_list_offset(CurveListHoard<BandType> &hoard, uint32_t &value)
    {
      m_before_split.assign_curve_list_offset(hoard, value);
      m_after_split.assign_curve_list_offset(hoard, value);
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
      SubBand(const GlyphPath &path, const SubBand *parent,
              float minv, float maxv);

      void
      assign_curve_list_offset(CurveListHoard<BandType> &hoard, uint32_t &value);

      uint32_t
      pack_band_value(void) const;

      std::vector<CurveID> m_curves;
      uint32_t m_curve_list_offset;
    };

    explicit
    Band(const GlyphPath &path);

    Band(const GlyphPath &path, const Band *parent, float min_v, float max_v);

    float
    compute_average_curve_coverage(const GlyphPath &path) const;

    void
    divide(std::vector<Band> *dst, const GlyphPath &path, float slack) const;

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
    move_to(fastuidraw::vec2 pt)
    {
      if (!m_contours.empty() && m_contours.back().empty())
        {
          m_contours.pop_back();
        }
      FASTUIDRAWassert(m_contours.empty() || m_contours.back().is_good());
      m_contours.push_back(Contour(pt));
    }

    void
    quadratic_to(fastuidraw::vec2 ct,
                 fastuidraw::vec2 pt)
    {
      FASTUIDRAWassert(!m_contours.empty());
      m_contours.back().quadratic_to(ct, pt);
    }

    void
    line_to(fastuidraw::vec2 pt)
    {
      FASTUIDRAWassert(!m_contours.empty());
      m_contours.back().line_to(pt);
    }

    unsigned int
    num_contours(void) const
    {
      return m_contours.size();
    }

    unsigned int
    num_curves(unsigned int C) const
    {
      return m_contours[C].curves().size();
    }

    const Curve&
    curve(CurveID ID) const
    {
      FASTUIDRAWassert(ID.contour() < m_contours.size());
      FASTUIDRAWassert(ID.curve() < m_contours[ID.contour()].curves().size());
      return m_contours[ID.contour()].curves()[ID.curve()];
    }

    unsigned int
    total_number_curves(void) const
    {
      unsigned int return_value(0);
      for (const auto &C : m_contours)
        {
          return_value += C.curves().size();
        }
      return return_value;
    }

    void
    transform_curves(const fastuidraw::Rect &bb);

  private:
    std::vector<Contour> m_contours;
  };

  template<enum band_t BandType>
  class SortCurvesCullAfterSplit
  {
  public:
    explicit
    SortCurvesCullAfterSplit(const GlyphPath &path):
      m_path(path)
    {}

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

    bool
    operator()(CurveID lhs, CurveID rhs) const
    {
      return operator()(m_path.curve(lhs), m_path.curve(rhs));
    }

  private:
    const GlyphPath &m_path;
  };

  template<enum band_t BandType>
  class SortCurvesCullBeforeSplit
  {
  public:
    explicit
    SortCurvesCullBeforeSplit(const GlyphPath &path):
      m_path(path)
    {}

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

    bool
    operator()(CurveID lhs, CurveID rhs) const
    {
      return operator()(m_path.curve(lhs), m_path.curve(rhs));
    }

  private:
    const GlyphPath &m_path;
  };

  enum
    {
      horizontal_band_avg_curve_count,
      vertical_band_avg_curve_count,
      number_horizontal_bands,
      number_vertical_bands,
      total_number_curves,

      num_costs
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
    fastuidraw::vecN<float, num_costs> m_render_cost;
  };
}


/////////////////////////////////
// CurveListHoard methods
template<enum band_t BandType>
uint32_t
CurveListHoard<BandType>::
fetch_curve_list_offset(const std::vector<CurveID> &p, uint32_t &value)
{
  std::map<std::vector<CurveID>, uint32_t>::iterator iter;
  uint32_t offset;

  iter = m_offsets.find(p);
  if (iter == m_offsets.end())
    {
      offset = value;
      value += 3 * p.size();
      m_offsets[p] = offset;
    }
  else
    {
      offset = iter->second;
    }
  return offset;
}

template<enum band_t BandType>
void
CurveListHoard<BandType>::
pack_curves(const GlyphPath &path,
            fastuidraw::c_array<fastuidraw::generic_data> dst) const
{
  for (const auto &e : m_offsets)
    {
      unsigned int offset(e.second);
      for (const CurveID &id : e.first)
        {
          const Curve &curve(path.curve(id));
          dst[offset++].u = pack_point<BandType>(curve.start());
          dst[offset++].u = pack_point<BandType>(curve.control());
          dst[offset++].u = pack_point<BandType>(curve.end());
        }
    }
}

///////////////////////////////////////
// Band::SubBand methods
template<enum band_t BandType>
Band<BandType>::SubBand::
SubBand(void)
{}

template<enum band_t BandType>
Band<BandType>::SubBand::
SubBand(const GlyphPath &path, const SubBand *parent,
        float minv, float maxv)
{
  for (const CurveID &C : parent->m_curves)
    {
      if (path.curve(C).intersects_band<BandType>(minv, maxv))
        {
          m_curves.push_back(C);
        }
    }
}

template<enum band_t BandType>
void
Band<BandType>::SubBand::
assign_curve_list_offset(CurveListHoard<BandType> &hoard, uint32_t &value)
{
  m_curve_list_offset = hoard.fetch_curve_list_offset(m_curves, value);
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

///////////////////////////////
// Band methods
template<enum band_t BandType>
Band<BandType>::
Band(const GlyphPath &path):
  m_min_v(-fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value),
  m_max_v(fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value)
{
  CurveID ID;
  unsigned int contour_end(path.num_contours());

  for (ID.contour() = 0; ID.contour() < contour_end; ++ID.contour())
    {
      unsigned int curve_end(path.num_curves(ID.contour()));

      for (ID.curve() = 0; ID.curve() < curve_end; ++ID.curve())
        {
          if (!path.curve(ID).ignore_curve(BandType))
            {
              if (path.curve(ID).present_before_split<BandType>(0.0f))
                {
                  m_before_split.m_curves.push_back(ID);
                }

              if (path.curve(ID).present_after_split<BandType>(0.0f))
                {
                  m_after_split.m_curves.push_back(ID);
                }
            }
        }
    }
}

template<enum band_t BandType>
Band<BandType>::
Band(const GlyphPath &path, const Band *parent, float min_v, float max_v):
  m_min_v(min_v),
  m_max_v(max_v),
  m_before_split(path, &parent->m_before_split, min_v, max_v),
  m_after_split(path, &parent->m_after_split, min_v, max_v)
{}

template<enum band_t BandType>
float
Band<BandType>::
create_bands(std::vector<Band> *pdst, const GlyphPath &path,
             unsigned int max_num_iterations, float avg_curve_thresh)
{
  const float width(2 * fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value);
  const float epsilon(1e-5);
  const float slack(epsilon * width);
  float avg_num_curves;
  unsigned int num_iterations;
  std::vector<std::vector<Band> > tmp(1);

  tmp.reserve(max_num_iterations + 1);
  tmp.back().push_back(Band(path));

  //std::cout << "\n\nCreateBands(" << BandType << ")\n";

  /* basic idea: keep breaking each band in half until
   * each band does not have too many curves.
   */
  for (num_iterations = 0, avg_num_curves = tmp.back().back().compute_average_curve_coverage(path);
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
          band.divide(&tmp.back(), path, slack);
        }

      avg_num_curves = 0.0f;
      for (const Band &band : tmp.back())
        {
          avg_num_curves += band.compute_average_curve_coverage(path);
        }
      avg_num_curves /= static_cast<float>(tmp.back().size());
      //std::cout << " ---> " << avg_num_curves << "\n";
    }

  /* sort the curves for the shader */
  for (Band &b : tmp.back())
    {
      std::sort(b.m_before_split.m_curves.begin(),
                b.m_before_split.m_curves.end(),
                SortCurvesCullBeforeSplit<BandType>(path));

      std::sort(b.m_after_split.m_curves.begin(),
                b.m_after_split.m_curves.end(),
                SortCurvesCullAfterSplit<BandType>(path));
    }
  std::swap(*pdst, tmp.back());
  //std::cout << "\n\tNumBands = " << pdst->size() << "\n";

  //std::cout << "Glyph complexity: " << avg_num_curves << "\n";
  return avg_num_curves;
}

template<enum band_t BandType>
float
Band<BandType>::
compute_average_curve_coverage(const GlyphPath &path) const
{
  const float edge(fastuidraw::GlyphRenderDataBandedRays::glyph_coord_value);
  float curve_draw(0.0f);

  for (const CurveID id : m_after_split.m_curves)
    {
      const Curve &c(path.curve(id));
      /* a curve after the split hits all the area
       * between 0 and its maximum coordinate.
       */
      float v;
      v = fastuidraw::t_min(edge, c.max_coordinate(1 - BandType));
      curve_draw += fastuidraw::t_max(0.0f, v);
    }

  for (const CurveID id : m_before_split.m_curves)
    {
      const Curve &c(path.curve(id));
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
divide(std::vector<Band> *dst, const GlyphPath &path, float slack) const
{
  float split(0.5 * (m_min_v + m_max_v));

  /* we add a little slack around the band to capture
   * curves that nearly touch the boundary of a band.
   */
  dst->push_back(Band(path, this, m_min_v, split + slack));
  dst->push_back(Band(path, this, split - slack, m_max_v));
}

////////////////////////////////////////////////
//GlyphPath methods
void
GlyphPath::
transform_curves(const fastuidraw::Rect &bb)
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
move_to(vec2 pt)
{
  GlyphRenderDataBandedRaysPrivate *d;

  d = static_cast<GlyphRenderDataBandedRaysPrivate*>(m_d);
  FASTUIDRAWassert(d->m_glyph);
  if (!d->m_glyph)
    {
      return;
    }
  d->m_glyph->move_to(pt);
}

void
fastuidraw::GlyphRenderDataBandedRays::
quadratic_to(vec2 ct, vec2 pt)
{
  GlyphRenderDataBandedRaysPrivate *d;

  d = static_cast<GlyphRenderDataBandedRaysPrivate*>(m_d);
  FASTUIDRAWassert(d->m_glyph);
  if (!d->m_glyph)
    {
      return;
    }
  d->m_glyph->quadratic_to(ct, pt);
}

void
fastuidraw::GlyphRenderDataBandedRays::
line_to(vec2 pt)
{
  GlyphRenderDataBandedRaysPrivate *d;

  d = static_cast<GlyphRenderDataBandedRaysPrivate*>(m_d);
  FASTUIDRAWassert(d->m_glyph);
  if (!d->m_glyph)
    {
      return;
    }
  d->m_glyph->line_to(pt);
}

void
fastuidraw::GlyphRenderDataBandedRays::
finalize(enum PainterEnums::fill_rule_t f, const Rect &glyph_rect)
{
  finalize(f, glyph_rect, GlyphGenerateParams::banded_rays_max_recursion(),
           GlyphGenerateParams::banded_rays_average_number_curves_thresh());
}

void
fastuidraw::GlyphRenderDataBandedRays::
finalize(enum PainterEnums::fill_rule_t f, const Rect &glyph_rect,
         int max_recursion, float avg_num_curves_thresh)
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

  if (d->m_glyph->num_contours() == 0
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

  d->m_render_cost[horizontal_band_avg_curve_count]
    = Band<horizontal_band>::create_bands(&split_horiz_bands, *d->m_glyph,
                                          max_recursion, avg_num_curves_thresh);

  d->m_render_cost[vertical_band_avg_curve_count]
    = Band<vertical_band>::create_bands(&split_vert_bands, *d->m_glyph,
                                        max_recursion, avg_num_curves_thresh);

  d->m_render_cost[number_horizontal_bands] = split_horiz_bands.size();
  d->m_render_cost[number_vertical_bands] = split_vert_bands.size();

  d->m_render_cost[total_number_curves] = d->m_glyph->total_number_curves();

  /* step 2: compute the offsets. The data packing is first
   *         that each band takes a single 32-bit value
   *         (that is packed as according to band_t) then
   *         comes the location of each of the curve lists.
   */
  uint32_t offset, H, V;
  CurveListHoard<horizontal_band> horiz_hoard;
  CurveListHoard<vertical_band> vert_hoard;

  H = split_horiz_bands.size();
  V = split_vert_bands.size();

  /* the starting offset for the curve lists comes after all
   * the bands
   */
  offset = 2 * (H + V);
  for (auto &p : split_horiz_bands)
    {
      p.assign_curve_list_offset(horiz_hoard, offset);
    }
  for (auto &p : split_vert_bands)
    {
      p.assign_curve_list_offset(vert_hoard, offset);
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
  horiz_hoard.pack_curves(*d->m_glyph, dst);
  vert_hoard.pack_curves(*d->m_glyph, dst);
  for (unsigned int i = 0; i < H; ++i)
    {
      dst[i].u = split_horiz_bands[i].after_split_packed_band_value();
      dst[i + H].u = split_horiz_bands[i].before_split_packed_band_value();
    }
  for (unsigned int i = 0; i < V; ++i)
    {
      dst[i + 2 * H].u = split_vert_bands[i].after_split_packed_band_value();
      dst[i + 2 * H + V].u = split_vert_bands[i].before_split_packed_band_value();
    }

  /* step 4: record the data neeed for shading */
  d->m_num_bands[vertical_band] = V;
  d->m_num_bands[horizontal_band] = H;

  FASTUIDRAWdelete(d->m_glyph);
  d->m_glyph = nullptr;
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::GlyphRenderDataBandedRays::
render_info_labels(void) const
{
  static c_string s[num_costs] =
    {
      [horizontal_band_avg_curve_count] = "AverageHorizontalCurveCount",
      [vertical_band_avg_curve_count] = "AverageVerticalCurveCount",
      [number_horizontal_bands] = "NumberHorizontalBands",
      [number_vertical_bands] = "NumberVerticalBands",
      [total_number_curves] = "TotalNumberOfCurves",
    };
  return c_array<const c_string>(s, num_costs);
}

enum fastuidraw::return_code
fastuidraw::GlyphRenderDataBandedRays::
upload_to_atlas(GlyphAtlasProxy &atlas_proxy,
                GlyphAttribute::Array &attributes,
                c_array<float> render_cost) const
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

  for (unsigned int i = 0; i < num_costs; ++i)
    {
      render_cost[i] = d->m_render_cost[i];
    }

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
