/*!
 * \file glyph_render_data_restricted_rays.cpp
 * \brief file glyph_render_data_restricted_rays.cpp
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
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/text/glyph_generate_params.hpp>
#include <fastuidraw/text/glyph_render_data_restricted_rays.hpp>
#include "../private/bounding_box.hpp"
#include "../private/util_private.hpp"

namespace
{
  inline
  uint32_t
  bias_winding(int w)
  {
    typedef fastuidraw::GlyphRenderDataRestrictedRays G;

    w += G::winding_bias;
    FASTUIDRAWassert(w >= 0);
    FASTUIDRAWassert(w <= int(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(G::winding_value_numbits)));

    return uint32_t(w);
  }

  inline
  uint32_t
  pack_point(fastuidraw::ivec2 p)
  {
    typedef fastuidraw::GlyphRenderDataRestrictedRays G;
    uint32_t x, y;

    FASTUIDRAWassert(p.x() >= 0);
    FASTUIDRAWassert(p.x() <= int(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(G::point_coordinate_numbits)));
    FASTUIDRAWassert(p.y() >= 0);
    FASTUIDRAWassert(p.y() <= int(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(G::point_coordinate_numbits)));

    x = fastuidraw::pack_bits(G::point_x_coordinate_bit0,
                              G::point_coordinate_numbits,
                              uint32_t(p.x()));

    y = fastuidraw::pack_bits(G::point_y_coordinate_bit0,
                              G::point_coordinate_numbits,
                              uint32_t(p.y()));
    return x | y;
  }

  class GlyphPath;

  class CurveID
  {
  public:
    CurveID& contour(int v) { m_ID.x() = v; return *this; }
    CurveID& curve(int v) { m_ID.y() = v; return *this; }
    unsigned int contour(void) const { return m_ID.x(); }
    unsigned int curve(void) const { return m_ID.y(); }
    bool operator<(const CurveID &rhs) const { return m_ID < rhs.m_ID; }
  private:
    fastuidraw::uvec2 m_ID;
  };

  class Edge
  {
  public:
    Edge(int a, int b):
      m_start(a),
      m_end(b)
    {}

    static
    bool
    compare_edges(const Edge &lhs, const Edge &rhs)
    {
      return fastuidraw::t_min(lhs.m_start, lhs.m_end)
        < fastuidraw::t_max(rhs.m_start, rhs.m_end);
    }

    int m_start, m_end;
  };

  class TrackedEdge:public std::pair<Edge, CurveID>
  {
  public:
    TrackedEdge(const Edge &E, const CurveID &C):
      std::pair<Edge, CurveID>(E, C)
    {}

    const Edge&
    edge(void) const { return first; }

    const CurveID&
    curve_id(void) const { return second; }
  };

  typedef std::map<int, std::vector<Edge> > EdgesOfContour; //keyed by CurveID::curve()
  typedef std::map<int, EdgesOfContour> EdgesOfPath; //keyed by CurveID::contour()
  typedef fastuidraw::vecN<fastuidraw::BoundingBox<float>, 2> BoxPair;

  void
  create_enlarged_boxes(const BoxPair &src,
                        float x_thresh, float y_thresh,
                        fastuidraw::vecN<BoxPair, 2> &dst)
  {
    for (int i = 0; i < 2; ++i)
      {
        src[i].enlarge(fastuidraw::vec2(x_thresh, 0.0f), &dst[i].x());
        src[i].enlarge(fastuidraw::vec2(0.0f, y_thresh), &dst[i].y());
      }
  }

  class Curve
  {
  public:
    Curve(const fastuidraw::ivec2 &a,
          const fastuidraw::ivec2 &b):
      m_offset(-1),
      m_cancelled_edge(false),
      m_start(a),
      m_end(b),
      m_control((a + b) / 2),
      m_fstart(a),
      m_fend(b),
      m_fcontrol((m_fstart + m_fend) * 0.5f),
      m_has_control(false)
    {
      m_fbbox.union_point(m_fstart);
      m_fbbox.union_point(m_fend);
      compute_data();
    }

    Curve(const fastuidraw::ivec2 &a,
          const fastuidraw::ivec2 &ct,
          const fastuidraw::ivec2 &b):
      m_offset(-1),
      m_cancelled_edge(false),
      m_start(a),
      m_end(b),
      m_control(ct),
      m_fstart(a),
      m_fend(b),
      m_fcontrol(ct),
      m_has_control(!is_flat(a, ct, b))
    {
      m_fbbox.union_point(m_fstart);
      m_fbbox.union_point(m_fcontrol);
      m_fbbox.union_point(m_fend);
      compute_data();
    }

    Curve(fastuidraw::ivec2 scale_down, const Curve &C):
      m_offset(-1),
      m_cancelled_edge(false),
      m_start(C.m_start / scale_down),
      m_end(C.m_end / scale_down),
      m_control(C.m_control / scale_down),
      m_fstart(m_start),
      m_fend(m_end),
      m_fcontrol(m_control),
      m_has_control(C.m_has_control && !is_flat(m_start, m_control, m_end))
    {
      FASTUIDRAWassert(C.m_offset == -1);
      m_fbbox.union_point(m_fstart);
      m_fbbox.union_point(m_fend);
      if (m_has_control)
        {
          m_fbbox.union_point(m_fcontrol);
        }
      compute_data();
    }

    const fastuidraw::ivec2&
    start(void) const { return m_start; }

    const fastuidraw::ivec2&
    end(void) const { return m_end; }

    const fastuidraw::ivec2&
    control(void) const
    {
      FASTUIDRAWassert(m_has_control);
      return m_control;
    }

    const fastuidraw::vec2&
    fstart(void) const { return m_fstart; }

    const fastuidraw::vec2&
    fend(void) const { return m_fend; }

    const fastuidraw::vec2&
    fcontrol(void) const
    {
      FASTUIDRAWassert(m_has_control);
      return m_fcontrol;
    }

    bool
    is_fixed_line(int coord, int *fixed_value) const
    {
      *fixed_value = m_start[coord];
      return (m_start[coord] == m_end[coord] && !m_has_control);
    }

    bool
    has_control(void) const
    {
      return m_has_control;
    }

    void
    pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const
    {
      FASTUIDRAWassert(m_offset >= 0);

      unsigned int p(m_offset);
      dst[p++].u = pack_point(start());
      if (has_control())
        {
          dst[p++].u = pack_point(control());
        }
      dst[p++].u = pack_point(end());
    }

    fastuidraw::ivec2
    compute_winding_contribution(fastuidraw::vec2 xy, float *dist) const;

    bool
    intersects(const fastuidraw::BoundingBox<float> &box,
               const BoxPair &enlarged_boxes,
               fastuidraw::vec2 near_thresh) const;

    void
    translate(const fastuidraw::ivec2 &v)
    {
      fastuidraw::vec2 fv(v);

      m_start += v;
      m_end += v;
      m_control += v;
      m_fstart += fv;
      m_fend += fv;
      m_fcontrol += fv;
      m_fbbox.translate(fv);
    }

    bool
    is_opposite(const Curve &C)
    {
      return (C.start() == end()
              && C.end() == start()
              && C.has_control() == has_control()
              && (!has_control() || C.control() == control()));
    }

    /* Assigned in GlyphRenderDataRestrictedRays::finalize() */
    int m_offset;
    bool m_cancelled_edge;

  private:
    static
    bool
    is_flat(fastuidraw::ivec2 a,
            fastuidraw::ivec2 b,
            fastuidraw::ivec2 c)
    {
      fastuidraw::i64vec2 v(b - a), w(c - a);
      int64_t det;

      det = v.x() * w.y() - v.y() * w.x();
      return (det == 0);
    }

    void
    compute_data(void);

    void
    compute_l1_crits(void);

    void
    compute_axis_of_symmetry(void);

    int
    compute_winding_contribution_impl(int coord,
                                      fastuidraw::vec2 A,
                                      fastuidraw::vec2 B,
                                      fastuidraw::vec2 C,
                                      uint32_t code,
                                      float *dist) const;

    float
    compute_at(float t, int coordinate) const;

    bool
    intersects_edge_test_at_time(float t, int coordinate, float min, float max) const;

    bool
    intersects_edge(float v, int coordinate, float min, float max) const;

    bool
    intersects_vertical_edge(float x, float miny, float maxy) const
    {
      return intersects_edge(x, 0, miny, maxy);
    }

    bool
    intersects_horizontal_edge(float y, float minx, float maxx) const
    {
      return intersects_edge(y, 1, minx, maxx);
    }

    fastuidraw::ivec2 m_start, m_end, m_control;
    fastuidraw::vec2 m_fstart, m_fend, m_fcontrol;
    fastuidraw::vec2 m_axis_of_symmetry_vector, m_axis_of_symmetry_pt;
    fastuidraw::BoundingBox<float> m_fbbox;
    bool m_has_control;
    unsigned int m_num_crits;
    fastuidraw::vecN<fastuidraw::vec2, 2> m_crits;
  };

  class SortableCurve
  {
  public:
    SortableCurve(const Curve &C):
      m_has_control(C.has_control()),
      m_a(fastuidraw::t_min(C.start(), C.end())),
      m_b(fastuidraw::t_max(C.start(), C.end()))
    {
      if (m_has_control)
        {
          m_ctl = C.control();
        }
    }

    bool
    operator<(const SortableCurve &rhs) const
    {
      if (m_has_control != rhs.m_has_control)
        {
          return m_has_control < rhs.m_has_control;
        }

      if (m_has_control && m_ctl != rhs.m_ctl)
        {
          return m_ctl < rhs.m_ctl;
        }

      if (m_a != rhs.m_a)
        {
          return m_a < rhs.m_a;
        }

      return m_b < rhs.m_b;
    }

    static
    bool
    compare_curves(const Curve *lhs, const Curve *rhs)
    {
      if (lhs->has_control() != rhs->has_control())
        {
          return lhs->has_control() < rhs->has_control();
        }

      if (lhs->has_control() && lhs->control() != rhs->control())
        {
          return lhs->control() < rhs->control();
        }

      if (lhs->start() != rhs->start())
        {
          return lhs->start() < rhs->start();
        }

      return lhs->end() < rhs->end();
    }

  private:
    bool m_has_control;
    fastuidraw::ivec2 m_a, m_b, m_ctl;
  };

  class Contour
  {
  public:
    Contour(void)
    {}

    explicit
    Contour(fastuidraw::ivec2 pt):
      m_last_pt(pt)
    {}

    Contour(fastuidraw::ivec2 scale_down,
            const Contour &contour);

    Contour(const Contour &C,
            const EdgesOfContour *cH,
            const EdgesOfContour *cV);

    void
    line_to(fastuidraw::ivec2 pt)
    {
      if (m_last_pt != pt)
        {
          m_curves.push_back(Curve(m_last_pt, pt));
          m_last_pt = pt;
        }
    }

    void
    quadratic_to(fastuidraw::ivec2 ct,
                 fastuidraw::ivec2 pt)
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

    const Curve&
    operator[](unsigned int C) const
    {
      FASTUIDRAWassert(C < m_curves.size());
      return m_curves[C];
    }

    Curve&
    operator[](unsigned int C)
    {
      FASTUIDRAWassert(C < m_curves.size());
      return m_curves[C];
    }

    void
    translate(const fastuidraw::ivec2 &v);

    unsigned int
    num_curves(void) const
    {
      return m_curves.size();
    }

    bool
    empty(void) const
    {
      return m_curves.empty();
    }

    void
    assign_curve_offsets(unsigned int &current_offset);

    void
    pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const;

  private:
    void
    add_curve(int curve_id, const Curve &curve,
              const EdgesOfContour *cH,
              const EdgesOfContour *cV);

    fastuidraw::ivec2 m_last_pt;
    std::vector<Curve> m_curves;
  };

  class CurveList
  {
  public:
    CurveList(void):
      m_offset(-1),
      m_p(nullptr)
    {}

    unsigned int
    init(const GlyphPath *p,
         const fastuidraw::vec2 &min_pt,
         const fastuidraw::vec2 &max_pt);

    unsigned int //returns the splitting coordinate
    split(CurveList &out_pre, CurveList &out_post,
          fastuidraw::vec2 near_thresh) const;

    const std::vector<CurveID>&
    curves(void) const
    {
      FASTUIDRAWassert(m_p);
      return m_curves;
    }

    const fastuidraw::BoundingBox<float>&
    box(void) const
    {
      return m_box;
    }

    const GlyphPath&
    glyph_path(void) const { return *m_p; }

    /* Assigned in GlyphRenderDataRestrictedRays::finalize() */
    int m_offset;

  private:
    const GlyphPath *m_p;
    std::vector<CurveID> m_curves;
    fastuidraw::BoundingBox<float> m_box;
  };

  class CurveListPacker
  {
  public:
    CurveListPacker(fastuidraw::c_array<fastuidraw::generic_data> dst,
                    unsigned int offset):
      m_dst(dst),
      m_current_offset(offset),
      m_sub_offset(0u),
      m_current_value(0u)
    {}

    void
    add_curve(uint32_t location, bool is_quadratic);

    void
    end_list(void);

    static
    unsigned int
    room_required(unsigned int num_curves);

  private:
    fastuidraw::c_array<fastuidraw::generic_data> m_dst;
    unsigned int m_current_offset, m_sub_offset;
    uint32_t m_current_value;
  };

  class CurveListCollection
  {
  public:
    explicit
    CurveListCollection(unsigned int start_offset):
      m_start_offset(start_offset),
      m_current_offset(start_offset)
    {}

    unsigned int
    start_offset(void) const
    {
      return m_start_offset;
    }

    unsigned int
    current_offset(void) const
    {
      return m_current_offset;
    }

    void
    assign_offset(CurveList &C);

    void
    pack_data(const GlyphPath *p,
              fastuidraw::c_array<fastuidraw::generic_data> data) const;

  private:
    void
    pack_element(const GlyphPath *p,
                 const std::vector<CurveID> &curves,
                 unsigned int offset,
                 fastuidraw::c_array<fastuidraw::generic_data> data) const;

    unsigned int m_start_offset, m_current_offset;
    std::map<std::vector<CurveID>, unsigned int> m_offsets;
  };

  class CurveListHierarchy
  {
  public:
    CurveListHierarchy(const GlyphPath *p,
                       const fastuidraw::ivec2 &min_pt,
                       const fastuidraw::ivec2 &max_pt,
                       unsigned int max_recursion,
                       unsigned int split_thresh,
                       fastuidraw::vec2 near_thresh):
      m_child(nullptr, nullptr),
      m_offset(-1),
      m_splitting_coordinate(3),
      m_generation(0)
    {
      m_curves.init(p,
                    fastuidraw::vec2(min_pt),
                    fastuidraw::vec2(max_pt));
      subdivide(max_recursion, split_thresh, near_thresh);
    }

    ~CurveListHierarchy()
    {
      if (has_children())
        {
          FASTUIDRAWdelete(m_child[0]);
          FASTUIDRAWdelete(m_child[1]);
        }
    }

    unsigned int
    assign_tree_offsets(void)
    {
      unsigned int size(0);

      assign_tree_offsets(size);
      return size;
    }

    void
    assign_curve_list_offsets(CurveListCollection &C);

    void
    pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const;

  private:
    explicit
    CurveListHierarchy(unsigned int parent_gen):
      m_child(nullptr, nullptr),
      m_offset(-1),
      m_splitting_coordinate(3),
      m_generation(parent_gen + 1)
    {}

    void
    subdivide(unsigned int max_recursion, unsigned int split_thresh,
              fastuidraw::vec2 near_thresh);

    void
    assign_tree_offsets(unsigned int &start);

    bool
    has_children(void) const
    {
      FASTUIDRAWassert((m_child[0] == nullptr) == (m_child[1] == nullptr));
      return m_child[0] != nullptr;
    }

    void
    pack_texel(fastuidraw::c_array<fastuidraw::generic_data> dst) const;

    void
    pack_sample_point(unsigned int &offset,
                      fastuidraw::c_array<fastuidraw::generic_data> dst) const;

    fastuidraw::vecN<CurveListHierarchy*, 2> m_child;
    unsigned int m_offset, m_splitting_coordinate, m_generation;
    CurveList m_curves;

    int m_winding;
    fastuidraw::ivec2 m_delta;
  };

  class EdgeTracker
  {
  public:
    void
    add_edge(const TrackedEdge &T)
    {
      const Edge &E(T.edge());
      m_pts.insert(E.m_start);
      m_pts.insert(E.m_end);
      m_edges.push_back(T);
    }

    void
    finalize(void);

    const EdgesOfPath&
    data(void) const
    {
      return m_data;
    }

  private:
    std::set<int> m_pts;
    std::vector<TrackedEdge> m_edges;
    EdgesOfPath m_data;
  };

  class GlyphPath
  {
  public:
    GlyphPath(void);

    int
    compute_winding_number(fastuidraw::vec2 xy, float *dist) const;

    void
    move_to(const fastuidraw::ivec2 &pt)
    {
      if (!m_contours.empty() && m_contours.back().empty())
        {
          m_contours.pop_back();
        }
      FASTUIDRAWassert(m_contours.empty() || m_contours.back().is_good());
      m_bbox.union_point(pt);
      m_contours.push_back(Contour(pt));
    }

    void
    quadratic_to(const fastuidraw::ivec2 &ct,
                 const fastuidraw::ivec2 &pt)
    {
      FASTUIDRAWassert(!m_contours.empty());
      m_bbox.union_point(ct);
      m_bbox.union_point(pt);
      m_contours.back().quadratic_to(ct, pt);
    }

    void
    line_to(const fastuidraw::ivec2 &pt)
    {
      FASTUIDRAWassert(!m_contours.empty());
      m_bbox.union_point(pt);
      m_contours.back().line_to(pt);
    }

    const std::vector<Contour>&
    contours(void) const
    {
      return m_contours;
    }

    unsigned int
    assign_curve_offsets(unsigned int start_offset);

    void
    pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const;

    const Curve&
    fetch_curve(const CurveID &id) const
    {
      FASTUIDRAWassert(id.contour() < m_contours.size());
      FASTUIDRAWassert(id.curve() < m_contours[id.contour()].num_curves());
      return m_contours[id.contour()][id.curve()];
    }

    void
    mark_cancel_curves(void);

    void
    compute_edges_of_path(int coord, EdgesOfPath *dst);

    void
    sort_edges_of_path(int coord, EdgesOfPath *dst);

    void
    rebuild_path(const EdgesOfPath &sH, const EdgesOfPath &sV);

    void
    rebuild_contour(Contour &contour, unsigned int contourID,
                    const EdgesOfPath &sH, const EdgesOfPath &sV);

    void
    mark_exact_cancel_edges(void);

    void
    translate(const fastuidraw::ivec2 &v);

    void
    scale_down(const fastuidraw::ivec2 &v);

    void
    set_glyph_rect(const fastuidraw::RectT<int> &bb)
    {
      if (!m_contours.empty() && m_contours.back().empty())
        {
          m_contours.pop_back();
        }
      m_glyph_rect_min = bb.m_min_point;
      m_glyph_rect_max = bb.m_max_point;
      m_bbox.union_point(bb.m_min_point);
      m_bbox.union_point(bb.m_max_point);
    }

    const fastuidraw::BoundingBox<int>&
    bbox(void) const
    {
      return m_bbox;
    }

    const fastuidraw::ivec2&
    glyph_rect_min(void) const
    {
      return m_glyph_rect_min;
    }

    const fastuidraw::ivec2&
    glyph_rect_max(void) const
    {
      return m_glyph_rect_max;
    }

  private:
    fastuidraw::BoundingBox<int> m_bbox;
    fastuidraw::ivec2 m_glyph_rect_min, m_glyph_rect_max;
    std::vector<Contour> m_contours;
  };

  /* The main trickiness we have hear is that we store
   * the input point times two (forcing it to be even).
   * By doing so, we can always make the needed control
   * point for line_to().
   */
  class GlyphRenderDataRestrictedRaysPrivate
  {
  public:
    GlyphRenderDataRestrictedRaysPrivate(void)
    {
      m_glyph = FASTUIDRAWnew GlyphPath();
    }

    ~GlyphRenderDataRestrictedRaysPrivate(void)
    {
      if (m_glyph)
        {
          FASTUIDRAWdelete(m_glyph);
        }
    }

    GlyphPath *m_glyph;
    fastuidraw::ivec2 m_min, m_max, m_size;
    enum fastuidraw::PainterEnums::fill_rule_t m_fill_rule;
    std::vector<fastuidraw::generic_data> m_render_data;
  };
}

////////////////////////////////////////////////
// Curve methods
void
Curve::
compute_data(void)
{
  compute_l1_crits();
  compute_axis_of_symmetry();
}

void
Curve::
compute_axis_of_symmetry(void)
{
  using namespace fastuidraw;

  if (!m_has_control)
    {
      return;
    }

  vec2 A, B, beta, J, K;
  float b, magAsq;
  float2x2 R, Rinverse;

  /* compute the coefficients so that the curve is
   * given by
   *
   *    f(t) =  A * t * t + B * t + m_fstart
   */
  A = m_fstart - 2.0f * m_fcontrol + m_fend;
  B = 2.0f * (m_fcontrol - m_fstart);

  /* Let M(p) = R(p - m_startf) where R is the rotation
   * so that R(f(t) - m_startf) is given by
   *
   *   x(t) = beta_x * t
   *   y(t) = t * t + beta_y * t
   */
  const float tol = 1e-9;

  magAsq = dot(A, A);
  if (magAsq < tol)
    {
      m_has_control = false;
      return;
    }

  R(0, 0) =  A.y() / magAsq;
  R(0, 1) = -A.x() / magAsq;
  R(1, 0) =  A.x() / magAsq;
  R(1, 1) =  A.y() / magAsq;

  beta = R * B;

  /* x(t) = beta_x * t gives us that t = x / beta_x
   * which gives the relation
   *
   *   y = x * x + b * x
   *
   * where
   *
   *  b = beta.y / beta.x
   */
  b = beta.y() / beta.x();

  /* The axis of symmetry for the parabola y = a * x * x + b * x
   * is given by S = {(-b/2, t) | t a real }. Then M(L) = S
   * where L = { J + K * t | t real} where
   *
   *   J = m_startf + Rinverse(-b/2, 0)
   *   K = Rinverse(0, 1)
   */
  Rinverse(0, 0) =  R(1, 1) * magAsq;
  Rinverse(0, 1) = -R(0, 1) * magAsq;
  Rinverse(1, 0) = -R(1, 0) * magAsq;
  Rinverse(1, 1) =  R(0, 0) * magAsq;

  J = m_fstart + Rinverse * vec2(-0.5f * b, 0.0f);
  K = Rinverse * vec2(0.0f, 1.0f);

  m_axis_of_symmetry_pt = J;
  K.normalize();
  m_axis_of_symmetry_vector = vec2(K.y(), -K.x());
}

void
Curve::
compute_l1_crits(void)
{
  using namespace fastuidraw;

  m_num_crits = 0;
  if (!m_has_control)
    {
      return;
    }

  vec2 A, B, C;
  float d0, d1, n0, n1;

  A = m_fstart - 2.0 * m_fcontrol + m_fend;
  B = m_fstart - m_fcontrol;
  C = m_fstart;

  /* curve is given by
   *
   *   f(t) = (A * t - 2 * B) * t + C
   *
   * thus,
   *
   *   f'(t) = 2 * A * t - 2 * B
   *
   * and we are hunting for x'(t) = y'(t) and x'(t) - y'(t) = 0;
   * those occur at:
   *
   *   A_x * t - B_x = +A_y * t - B_y  --> t = (B_x - B_y) / (A_x - A_y)
   *   A_x * t - B_x = -A_y * t + B_y  --> t = (B_x + B_y) / (A_x + A_y)
   */

  n0 = B.x() + B.y();
  d0 = A.x() + A.y();
  n0 *= t_sign(d0);
  d0 = t_abs(d0);

  n1 = B.x() - B.y();
  d1 = A.x() - A.y();
  n1 *= t_sign(d1);
  d1 = t_abs(d1);

  if (n0 > 0.0 && d0 > n0)
    {
      float t0;

      t0 = n0 / d0;
      m_crits[m_num_crits++] = (A * t0 - 2.0 * B) * t0 + C;
    }

  if (n1 > 0.0 && d1 > n1)
    {
      float t1;

      t1 = n1 / d1;
      m_crits[m_num_crits++] = (A * t1 - 2.0 * B) * t1 + C;
    }
}

fastuidraw::ivec2
Curve::
compute_winding_contribution(fastuidraw::vec2 p, float *dist) const
{
  using namespace fastuidraw;
  vec2 p1(m_fstart - p), p2(m_fcontrol - p), p3(m_fend - p);
  uint32_t codex, codey;
  ivec2 R;
  vec2 A, B, C;

  A = p1 - 2.0 * p2 + p3;
  B = p1 - p2;
  C = p1;

  *dist = t_min(*dist, p1.L1norm());
  *dist = t_min(*dist, p3.L1norm());
  for (unsigned int crit = 0; crit < m_num_crits; ++crit)
    {
      vec2 q(m_crits[crit] - p);
      *dist = t_min(*dist, q.L1norm());
    }

  codex = (p1.x() > 0.0f ? 2u : 0u)
    + (p2.x() > 0.0f ? 4u : 0u)
    + (p3.x() > 0.0f ? 8u : 0u);

  codey = (p1.y() > 0.0f ? 2u : 0u)
    + (p2.y() > 0.0f ? 4u : 0u)
    + (p3.y() > 0.0f ? 8u : 0u);

  R.x() = -compute_winding_contribution_impl(0, A, B, C, codex, dist);
  R.y() = compute_winding_contribution_impl(1, A, B, C, codey, dist);

  if (m_has_control)
    {
      /* also include the distance to the axis of
       * symmetry of the parabola;
       */
      float f;
      f = dot(p - m_axis_of_symmetry_pt, m_axis_of_symmetry_vector);
      *dist = t_min(t_abs(f), *dist);
    }

  return R;
}

int
Curve::
compute_winding_contribution_impl(int coord,
                                  fastuidraw::vec2 A,
                                  fastuidraw::vec2 B,
                                  fastuidraw::vec2 C,
                                  uint32_t code,
                                  float *dist) const
{
  using namespace fastuidraw;

  code = (0x2E74u >> code) & 0x3u;

  int iA;
  float t1, t2, x1, x2;

  iA = m_start[coord] - 2 * m_control[coord] + m_end[coord];

  if (m_has_control && iA != 0)
    {
      float D, rA = 1.0f / A[coord];

      D = B[coord] * B[coord] - A[coord] * C[coord];
      D = t_sqrt(t_max(0.0f, D));
      t1 = (B[coord] - D) * rA;
      t2 = (B[coord] + D) * rA;
    }
  else
    {
      t1 = t2 = 0.5f * C[coord] / B[coord];
    }

  x1 = (A[1 - coord] * t1 - B[1 - coord] * 2.0f) * t1 + C[1 - coord];
  x2 = (A[1 - coord] * t2 - B[1 - coord] * 2.0f) * t2 + C[1 - coord];

  if (t1 <= 1.0 && t1 >= 0.0f)
    {
      *dist = t_min(*dist, t_abs(x1));
    }

  if (t2 <= 1.0 && t2 >= 0.0f)
    {
      *dist = t_min(*dist, t_abs(x2));
    }

  int r(0);
  if ((code & 1u) != 0u && x1 > 0.0f)
    {
      ++r;
    }

  if (code > 1u && x2 > 0.0f)
    {
      --r;
    }

  return r;
}

bool
Curve::
intersects(const fastuidraw::BoundingBox<float> &box,
           const BoxPair &enlarged_boxes,
           fastuidraw::vec2 near_thresh) const
{
  /* if the curve is almost horizontal or vertical
   * and within the near_thresh of the box, add it.
   * We say the curve is vertical if its width is
   * lequal to near_thresh.x and we say it is
   * horizontal if its height is less than
   * near_thresh.y
   */
  fastuidraw::vec2 sz(m_fbbox.size());
  for (int coord = 0; coord < 2; ++coord)
    {
      if (sz[coord] < near_thresh[coord] && m_fbbox.intersects(enlarged_boxes[coord]))
        {
          return true;
        }
    }

  if (!m_fbbox.intersects(box))
    {
      return false;
    }

  if (box.contains(m_fstart)
      || box.contains(m_fend))
    {
      return true;
    }

  return intersects_vertical_edge(box.min_point().x(), box.min_point().y(), box.max_point().y())
    || intersects_vertical_edge(box.max_point().x(), box.min_point().y(), box.max_point().y())
    || intersects_horizontal_edge(box.min_point().y(), box.min_point().x(), box.max_point().x())
    || intersects_horizontal_edge(box.max_point().y(), box.min_point().x(), box.max_point().x());

  /* TODO:
   *   - scheme to save the polynomial solves computed.
   */
}

float
Curve::
compute_at(float t, int d) const
{
  float A, B, C;

  A = m_fstart[d] - 2.0f * m_fcontrol[d] + m_fend[d];
  B = m_fstart[d] - m_fcontrol[d];
  C = m_fstart[d];

  return (A * t - B * 2.0f) * t + C;
}

bool
Curve::
intersects_edge_test_at_time(float t, int coordinate, float min, float max) const
{
  float v;

  v = compute_at(t, 1 - coordinate);
  return v >= min && v <= max;
}

bool
Curve::
intersects_edge(float v, int i, float min, float max) const
{
  float A, B, C;
  int iA;

  iA = m_start[i] - 2 * m_control[i] + m_end[i];
  A = m_fstart[i] - 2.0f * m_fcontrol[i] + m_fend[i];
  B = m_fstart[i] - m_fcontrol[i];
  C = m_fstart[i] - v;

  if (m_has_control && iA != 0)
    {
      float t1, t2, D, rA = 1.0f / A;

      D = B * B - A * C;
      if (D < 0.0f)
        {
          return false;
        }

      D = fastuidraw::t_sqrt(D);
      t1 = (B - D) * rA;
      t2 = (B + D) * rA;

      return intersects_edge_test_at_time(t1, i, min, max)
        || intersects_edge_test_at_time(t2, i, min, max);
    }
  else
    {
      float t;
      t = 0.5f * C / B;
      return intersects_edge_test_at_time(t, i, min, max);
    }
}

/////////////////////////////////////////
// Contour methods
Contour::
Contour(fastuidraw::ivec2 scale_down,
        const Contour &contour)
{
  for (const Curve &C : contour.m_curves)
    {
      Curve D(scale_down, C);
      if (D.start() != D.end())
        {
          m_curves.push_back(D);
        }
    }
  FASTUIDRAWassert(is_good());
}

Contour::
Contour(const Contour &contour,
        const EdgesOfContour *cH,
        const EdgesOfContour *cV)
{
  for (unsigned int c = 0, endc = contour.m_curves.size(); c < endc; ++c)
    {
      add_curve(c, contour.m_curves[c], cH, cV);
    }
  FASTUIDRAWassert(is_good());
}

void
Contour::
add_curve(int c, const Curve &curve,
          const EdgesOfContour *cH,
          const EdgesOfContour *cV)
{
  using namespace fastuidraw;

  /* Check if the passed curve is in either cH or cV.
   * If it is, it is in only one of them and replace
   * that edge with the edge broken into pieces listed
   * in cH or cV.
   */
  if (cH != nullptr)
    {
      EdgesOfContour::const_iterator iter;
      iter = cH->find(c);
      if (iter != cH->end())
        {
          for (const Edge &E : iter->second)
            {
              int f(curve.start()[0]);
              Curve C(ivec2(f, E.m_start),
                      ivec2(f, E.m_end));

              FASTUIDRAWassert(f == curve.end()[0]);
              m_curves.push_back(C);
            }
          return;
        }
    }

  if (cV != nullptr)
    {
      EdgesOfContour::const_iterator iter;
      iter = cV->find(c);
      if (iter != cV->end())
        {
          for (const Edge &E : iter->second)
            {
              int f(curve.start()[1]);
              Curve C(ivec2(E.m_start, f),
                      ivec2(E.m_end, f));

              FASTUIDRAWassert(f == curve.end()[1]);
              m_curves.push_back(C);
            }
          return;
        }
    }

  m_curves.push_back(curve);
}

void
Contour::
translate(const fastuidraw::ivec2 &v)
{
  FASTUIDRAWassert(is_good());
  for (Curve &C : m_curves)
    {
      C.translate(v);
    }
  FASTUIDRAWassert(is_good());
}

void
Contour::
assign_curve_offsets(unsigned int &current_offset)
{
  FASTUIDRAWassert(is_good());
  for (Curve &curve : m_curves)
    {
      unsigned int cnt;

      curve.m_offset = current_offset;
      cnt = (curve.has_control()) ? 2 : 1;
      current_offset += cnt;
    }

  /* the last point of the last curve which is the same as
   * the first point of the first curve needs to be added
   * so that that the last curve access has the point
   */
  ++current_offset;
}

void
Contour::
pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const
{
  for (const Curve &curve : m_curves)
    {
      curve.pack_data(dst);
    }
}

//////////////////////////////////////
// CurveList methods
unsigned int
CurveList::
init(const GlyphPath *p,
     const fastuidraw::vec2 &min_pt,
     const fastuidraw::vec2 &max_pt)
{
  FASTUIDRAWassert(!m_p);
  m_p = p;
  m_box = fastuidraw::BoundingBox<float>(min_pt, max_pt);

  const std::vector<Contour> &g(m_p->contours());
  for (unsigned int o = 0, endo = g.size(); o < endo; ++o)
    {
      for (unsigned int c = 0, endc = g[o].num_curves(); c < endc; ++c)
        {
          if (!g[o][c].m_cancelled_edge)
            {
              m_curves.push_back(CurveID()
                                 .curve(c)
                                 .contour(o));
            }
        }
    }
  return m_curves.size();
}

unsigned int
CurveList::
split(CurveList &out_pre, CurveList &out_post,
      fastuidraw::vec2 near_thresh) const
{
  using namespace fastuidraw;
  typedef vecN<BoundingBox<float>, 2> BoxPair;

  vecN<std::vector<CurveID>, 2> splitX;
  vecN<std::vector<CurveID>, 2> splitY;
  BoxPair splitX_box(m_box.split_x()), splitY_box(m_box.split_y());
  vecN<BoxPair, 2> enlarged_splitX_box, enlarged_splitY_box;
  vec2 box_size(m_box.size());
  ivec2 sz;
  int return_value;

  /* if there are curves already in this box, then going further
   * than the box size gives us nothing because the curves within
   * the box_size are all already closer.
   */
  near_thresh.x() = t_min(near_thresh.x(), box_size.x());
  near_thresh.y() = t_min(near_thresh.y(), box_size.y());

  create_enlarged_boxes(splitX_box,
                        near_thresh.x(), near_thresh.y(),
                        enlarged_splitX_box);

  create_enlarged_boxes(splitY_box,
                        near_thresh.x(), near_thresh.y(),
                        enlarged_splitY_box);

  /* choose the partition with the smallest sum of curves */
  FASTUIDRAWassert(std::is_sorted(m_curves.begin(), m_curves.end()));
  for (const CurveID &id : m_curves)
    {
      const Curve &curve(m_p->fetch_curve(id));
      for (int i = 0; i < 2; ++i)
        {
          if (curve.intersects(splitX_box[i], enlarged_splitX_box[i], near_thresh))
            {
              splitX[i].push_back(id);
            }
          if (curve.intersects(splitY_box[i], enlarged_splitY_box[i], near_thresh))
            {
              splitY[i].push_back(id);
            }
        }
    }

  out_pre.m_p = m_p;
  out_post.m_p = m_p;

  sz.x() = splitX[0].size() + splitX[1].size();
  sz.y() = splitY[0].size() + splitY[1].size();

  if (sz.x() == sz.y())
    {
      return_value = (box_size.x() > box_size.y()) ?
        0 : 1;
    }
  else if (sz.x() < sz.y())
    {
      return_value = 0;
    }
  else
    {
      return_value = 1;
    }

  if (return_value == 0)
    {
      std::swap(out_pre.m_curves, splitX[0]);
      std::swap(out_post.m_curves, splitX[1]);
      out_pre.m_box = splitX_box[0];
      out_post.m_box = splitX_box[1];
    }
  else
    {
      std::swap(out_pre.m_curves, splitY[0]);
      std::swap(out_post.m_curves, splitY[1]);
      out_pre.m_box = splitY_box[0];
      out_post.m_box = splitY_box[1];
    }

  return return_value;
}

//////////////////////////////////
// CurveListPacker methods
void
CurveListPacker::
add_curve(uint32_t curve_location, bool curve_is_quadratic)
{
  typedef fastuidraw::GlyphRenderDataRestrictedRays G;
  uint32_t v(curve_location);

  if (curve_is_quadratic)
    {
      v |= (1u << G::curve_is_quadratic_bit);
    }

  if (m_sub_offset == 0u)
    {
      m_current_value = (v << G::curve_entry0_bit0);
      ++m_sub_offset;
    }
  else
    {
      FASTUIDRAWassert(m_sub_offset == 1);

      m_dst[m_current_offset++].u = m_current_value | (v << G::curve_entry1_bit0);
      m_current_value = 0u;
      m_sub_offset = 0;
    }
}

unsigned int
CurveListPacker::
room_required(unsigned int cnt)
{
  /* round up to even */
  cnt = (cnt + 1u) & ~1u;

  /* every 2 curves takes takes an entry */
  cnt = (cnt >> 1u);

  return cnt;
}

void
CurveListPacker::
end_list(void)
{
  if (m_sub_offset == 1)
    {
      m_dst[m_current_offset++].u = m_current_value;
    }
}

///////////////////////////////////////////
// CurveListCollection methods
void
CurveListCollection::
assign_offset(CurveList &C)
{
  if (C.curves().empty())
    {
      C.m_offset = 0;
      return;
    }

  std::map<std::vector<CurveID>, unsigned int>::iterator iter;

  iter = m_offsets.find(C.curves());
  if (iter != m_offsets.end())
    {
      C.m_offset = iter->second;
    }
  else
    {
      C.m_offset = m_current_offset;
      iter = m_offsets.insert(std::make_pair(C.curves(), C.m_offset)).first;
      m_current_offset += CurveListPacker::room_required(C.curves().size());
    }
}

void
CurveListCollection::
pack_data(const GlyphPath *p,
          fastuidraw::c_array<fastuidraw::generic_data> data) const
{
  for (const auto &element : m_offsets)
    {
      pack_element(p, element.first, element.second, data);
    }
}

void
CurveListCollection::
pack_element(const GlyphPath *p,
             const std::vector<CurveID> &curves,
             unsigned int offset,
             fastuidraw::c_array<fastuidraw::generic_data> dst) const
{
  CurveListPacker curve_list_packer(dst, offset);

  FASTUIDRAWassert(!curves.empty());
  FASTUIDRAWassert(std::is_sorted(curves.begin(), curves.end()));
  for (const CurveID &id : curves)
    {
      const Curve &curve(p->fetch_curve(id));
      bool has_ctl(curve.has_control());

      FASTUIDRAWassert(curve.m_offset > 0);
      curve_list_packer.add_curve(curve.m_offset, has_ctl);
    }

  curve_list_packer.end_list();
}

///////////////////////////////////
// CurveListHierarchy methods
void
CurveListHierarchy::
pack_sample_point(unsigned int &offset,
                  fastuidraw::c_array<fastuidraw::generic_data> dst) const
{
  using namespace fastuidraw;
  typedef GlyphRenderDataRestrictedRays G;

  /* bits 0-15 for winding, biased
   * bits 16-23 for dx
   * bits 24-31 for dy
   */
  dst[offset++].u = pack_bits(G::winding_value_bit0,
                              G::winding_value_numbits,
                              bias_winding(m_winding))
    | pack_bits(G::delta_x_bit0, G::delta_numbits, m_delta.x())
    | pack_bits(G::delta_y_bit0, G::delta_numbits, m_delta.y());
}

void
CurveListHierarchy::
pack_texel(fastuidraw::c_array<fastuidraw::generic_data> dst) const
{
  using namespace fastuidraw;
  typedef GlyphRenderDataRestrictedRays G;

  if (!has_children())
    {
      unsigned int offset(m_offset);

      dst[offset++].u = pack_bits(G::hierarchy_leaf_curve_list_bit0,
                                  G::hierarchy_leaf_curve_list_numbits,
                                  m_curves.m_offset)
        | pack_bits(G::hierarchy_leaf_curve_list_size_bit0,
                    G::hierarchy_leaf_curve_list_size_numbits,
                    m_curves.curves().size());

      pack_sample_point(offset, dst);
    }
  else
    {
      FASTUIDRAWassert(m_splitting_coordinate <= 1u);

      /* pack the splitting data */
      dst[m_offset].u =
        (1u << G::hierarchy_is_node_bit) // bit flag to indicate a node
        | (m_splitting_coordinate << G::hierarchy_splitting_coordinate_bit) // splitting coordinate
        | pack_bits(G::hierarchy_child0_offset_bit0,
                    G::hierarchy_child_offset_numbits,
                    m_child[0]->m_offset) // pre-split child
        | pack_bits(G::hierarchy_child1_offset_bit0,
                    G::hierarchy_child_offset_numbits,
                    m_child[1]->m_offset); // post-split child
    }
}

void
CurveListHierarchy::
pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const
{
  pack_texel(dst);
  if (has_children())
    {
      m_child[0]->pack_data(dst);
      m_child[1]->pack_data(dst);
    }
}

void
CurveListHierarchy::
assign_tree_offsets(unsigned int &current)
{
  m_offset = current++;
  if (has_children())
    {
      m_child[0]->assign_tree_offsets(current);
      m_child[1]->assign_tree_offsets(current);
    }
  else
    {
      /* room needed for the sample point */
      current += 1u;
    }
}

void
CurveListHierarchy::
assign_curve_list_offsets(CurveListCollection &C)
{
  if (has_children())
    {
      m_child[0]->assign_curve_list_offsets(C);
      m_child[1]->assign_curve_list_offsets(C);
    }
  else
    {
      C.assign_offset(m_curves);
    }
}

void
CurveListHierarchy::
subdivide(unsigned int max_recursion, unsigned int split_thresh,
          fastuidraw::vec2 near_thresh)
{
  using namespace fastuidraw;
  if (m_generation < max_recursion && m_curves.curves().size() > split_thresh)
    {
      m_child[0] = FASTUIDRAWnew CurveListHierarchy(m_generation);
      m_child[1] = FASTUIDRAWnew CurveListHierarchy(m_generation);

      m_splitting_coordinate = m_curves.split(m_child[0]->m_curves,
                                              m_child[1]->m_curves,
                                              near_thresh);
      m_child[0]->subdivide(max_recursion, split_thresh, near_thresh);
      m_child[1]->subdivide(max_recursion, split_thresh, near_thresh);

      if (!m_child[0]->has_children()
          && !m_child[1]->has_children()
          && m_child[0]->m_curves.curves().size() == m_curves.curves().size()
          && m_child[1]->m_curves.curves().size() == m_curves.curves().size())
        {
          /* both children have no children and
           * the same curve list, thus there is
           * no point of subdivding this node
           */
          FASTUIDRAWdelete(m_child[0]); m_child[0] = nullptr;
          FASTUIDRAWdelete(m_child[1]); m_child[1] = nullptr;
          m_splitting_coordinate = 3;
        }
    }

  if (!has_children())
    {
      float best_dist, thresh;
      vec2 pt(m_curves.box().min_point());
      vec2 box_size(m_curves.box().size());
      vec2 factor(box_size / float(GlyphRenderDataRestrictedRays::delta_div_factor));
      const int MAX_TRIES = 50;

      thresh = t_sqrt(box_size.x() * box_size.y()) * 0.01f;
      m_delta = ivec2(128, 128);
      m_winding = m_curves.glyph_path().compute_winding_number(pt + vec2(m_delta) * factor,
							       &best_dist);
      for (int i = 0; i < MAX_TRIES && best_dist < thresh; ++i)
        {
          ivec2 idelta;
          vec2 delta;
          float dist;
          int winding;

          idelta.x() = std::rand() % GlyphRenderDataRestrictedRays::delta_div_factor;
          idelta.y() = std::rand() % GlyphRenderDataRestrictedRays::delta_div_factor;
          delta = vec2(idelta) * factor;

          winding = m_curves.glyph_path().compute_winding_number(delta + pt, &dist);
          if (dist > best_dist)
            {
              m_delta = idelta;
              m_winding = winding;
              best_dist = dist;
            }
        }
    }
}

//////////////////////////////////////////
// EdgeTracker methods
void
EdgeTracker::
finalize(void)
{
  /* break each edge into edges that have no
   * points in m_pts interior to them.
   */
  std::vector<TrackedEdge> tmp;
  bool has_partioned_edges(false);

  for (const TrackedEdge &t : m_edges)
    {
      const Edge &e(t.edge());
      std::set<int>::const_iterator a, b;
      int prev;

      a = m_pts.find(e.m_start);
      b = m_pts.find(e.m_end);
      FASTUIDRAWassert(a != m_pts.end());
      FASTUIDRAWassert(b != m_pts.end());

      if (e.m_start > e.m_end)
        {
          std::swap(a, b);
        }

      for (prev = *a, ++a, ++b; a != b; ++a)
        {
          int st(prev), ed(*a);
          if (e.m_start > e.m_end)
            {
              std::swap(st, ed);
            }
          tmp.push_back(TrackedEdge(Edge(st, ed), t.curve_id()));
          prev = *a;
        }
    }

  for (const TrackedEdge &T : tmp)
    {
      const CurveID &id(T.curve_id());
      std::vector<Edge> &edge_list(m_data[id.contour()][id.curve()]);

      edge_list.push_back(T.edge());
      has_partioned_edges = has_partioned_edges || edge_list.size() >= 2;
    }

  if (!has_partioned_edges)
    {
      m_data.clear();
    }
}

////////////////////////////////////////////////
//GlyphPath methods
GlyphPath::
GlyphPath(void)
{
}

void
GlyphPath::
translate(const fastuidraw::ivec2 &v)
{
  for (Contour &C : m_contours)
    {
      C.translate(v);
    }
  m_bbox.translate(v);
  m_glyph_rect_min += v;
  m_glyph_rect_max += v;
}

void
GlyphPath::
scale_down(const fastuidraw::ivec2 &v)
{
  std::vector<Contour> tmp;

  std::swap(tmp, m_contours);
  for (const Contour &C : tmp)
    {
      Contour D(v, C);
      if (!D.empty())
        {
          m_contours.push_back(D);
          FASTUIDRAWassert(D.is_good());
        }
    }
  m_bbox.scale_down(v);
  m_glyph_rect_min /= v;
  m_glyph_rect_max /= v;
}

void
GlyphPath::
compute_edges_of_path(int coord, EdgesOfPath *dst)
{
  std::map<int, std::vector<CurveID> > hoard;

  /* find all curves that have the named coordinate fixed
   * and add them to a map keyed by coordinate value with
   * values as an array of CurveID values.
   */
  for (unsigned int i = 0, endi = m_contours.size(); i < endi; ++i)
    {
      const Contour &contour(m_contours[i]);
      for (unsigned int j = 0, endj = contour.num_curves(); j < endj; ++j)
        {
          const Curve &curve(contour[j]);
          int fixed_coord;

          if (curve.is_fixed_line(coord, &fixed_coord))
            {
              hoard[fixed_coord].push_back(CurveID().contour(i).curve(j));
            }
        }
    }

  for (const auto &v : hoard)
    {
      /* Use EdgeTracker to split overlapping curves so that
       * end points of overlapping curves precisely match
       */
      if (v.second.size() > 1)
        {
          EdgeTracker E;

          /* split curves so that areas of overlap have identical endpoints.
           */
          for (const CurveID &ID : v.second)
            {
              const Curve &curve(m_contours[ID.contour()][ID.curve()]);
              Edge edge(curve.start()[1 - coord], curve.end()[1 - coord]);

              E.add_edge(TrackedEdge(edge, ID));
            }
          E.finalize();
          dst->insert(E.data().begin(), E.data().end());
        }
    }

  /* We need to sort the produced edges so that they are
   * ordered exactly as the original curves were.
   */
  sort_edges_of_path(coord, dst);
}

void
GlyphPath::
sort_edges_of_path(int coord, EdgesOfPath *dst)
{
  for (auto &v : *dst)
    {
      for (auto &w : v.second)
        {
          const Curve &C(m_contours[v.first][w.first]);

          FASTUIDRAWassert(C.start()[coord] == C.end()[coord]);
          std::sort(w.second.begin(), w.second.end(), Edge::compare_edges);
          if (C.start()[1 - coord] > C.end()[1 - coord])
            {
              std::reverse(w.second.begin(), w.second.end());
            }
        }
    }
}

void
GlyphPath::
rebuild_contour(Contour &contour, unsigned int contourID,
                const EdgesOfPath &sH, const EdgesOfPath &sV)
{
  EdgesOfPath::const_iterator iH, iV;

  /* Check if the named contour is within sH or sV;
   * if it is in atleast one of them, rebuild the contour
   * with partitioned edges in sH and sV replacing the
   * original edges.
   */
  iH = sH.find(contourID);
  iV = sV.find(contourID);
  if (iH == sH.end() && iV == sV.end())
    {
      m_contours.push_back(Contour());
      std::swap(m_contours.back(), contour);
    }
  else
    {
      const EdgesOfContour *cH, *cV;

      cH = (iH != sH.end()) ? &iH->second : nullptr;
      cV = (iV != sV.end()) ? &iV->second : nullptr;
      m_contours.push_back(Contour(contour, cH, cV));
    }
}

void
GlyphPath::
rebuild_path(const EdgesOfPath &sH, const EdgesOfPath &sV)
{
  std::vector<Contour> tmp;

  std::swap(tmp, m_contours);
  for (unsigned int i = 0, endi = tmp.size(); i < endi; ++i)
    {
      rebuild_contour(tmp[i], i, sH, sV);
    }
}

void
GlyphPath::
mark_exact_cancel_edges(void)
{
  /* simple code to detect exact opposite edges and to mark
   * them in pairs as cancelled.
   */
  std::map<SortableCurve, std::vector<Curve*> > tmp;
  for (Contour &C : m_contours)
    {
      for (unsigned int i = 0, endi = C.num_curves(); i < endi; ++i)
        {
          SortableCurve S(C[i]);
          tmp[S].push_back(&C[i]);
        }
    }

  for (auto &v : tmp)
    {
      fastuidraw::c_array<Curve*> curves(fastuidraw::make_c_array(v.second));
      std::sort(curves.begin(), curves.end(), SortableCurve::compare_curves);

      while(curves.size() >= 2 && curves.front()->is_opposite(*curves.back()))
        {
          curves.front()->m_cancelled_edge = true;
          curves.back()->m_cancelled_edge = true;
          curves = curves.sub_array(1, curves.size() - 2);
        }
    }
}

void
GlyphPath::
mark_cancel_curves(void)
{
  fastuidraw::vecN<EdgesOfPath, 2> replacements;
  for (int coord = 0; coord < 2; ++coord)
    {
      compute_edges_of_path(coord, &replacements[coord]);
    }

  if (!replacements[0].empty() || !replacements[1].empty())
    {
      rebuild_path(replacements[0], replacements[1]);
    }

  mark_exact_cancel_edges();
}

unsigned int
GlyphPath::
assign_curve_offsets(unsigned int current_offset)
{
  for (Contour &C : m_contours)
    {
      C.assign_curve_offsets(current_offset);
    }
  return current_offset;
}

void
GlyphPath::
pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const
{
  for (const Contour &C : m_contours)
    {
      C.pack_data(dst);
    }
}

int
GlyphPath::
compute_winding_number(fastuidraw::vec2 xy, float *dist) const
{
  fastuidraw::ivec2 w(0, 0);

  *dist = 65535.0f;
  for (const Contour &contour : m_contours)
    {
      for (unsigned int i = 0, endi = contour.num_curves(); i < endi; ++i)
        {
          w += contour[i].compute_winding_contribution(xy, dist);
        }
    }

  /* NOTE: if the point is on the curve, then the
   * winding value can be different in x and y.
   */
  return w.y();
}

/////////////////////////////////////////////////
// fastuidraw::GlyphRenderDataRestrictedRays methods
fastuidraw::GlyphRenderDataRestrictedRays::
GlyphRenderDataRestrictedRays(void)
{
  m_d = FASTUIDRAWnew GlyphRenderDataRestrictedRaysPrivate();
}

fastuidraw::GlyphRenderDataRestrictedRays::
~GlyphRenderDataRestrictedRays()
{
  GlyphRenderDataRestrictedRaysPrivate *d;

  d = static_cast<GlyphRenderDataRestrictedRaysPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::GlyphRenderDataRestrictedRays::
move_to(ivec2 pt)
{
  GlyphRenderDataRestrictedRaysPrivate *d;

  d = static_cast<GlyphRenderDataRestrictedRaysPrivate*>(m_d);
  FASTUIDRAWassert(d->m_glyph);
  if (!d->m_glyph)
    {
      return;
    }
  d->m_glyph->move_to(pt);
}

void
fastuidraw::GlyphRenderDataRestrictedRays::
quadratic_to(ivec2 ct, ivec2 pt)
{
  GlyphRenderDataRestrictedRaysPrivate *d;

  d = static_cast<GlyphRenderDataRestrictedRaysPrivate*>(m_d);
  FASTUIDRAWassert(d->m_glyph);
  if (!d->m_glyph)
    {
      return;
    }
  d->m_glyph->quadratic_to(ct, pt);
}

void
fastuidraw::GlyphRenderDataRestrictedRays::
line_to(ivec2 pt)
{
  GlyphRenderDataRestrictedRaysPrivate *d;

  d = static_cast<GlyphRenderDataRestrictedRaysPrivate*>(m_d);
  FASTUIDRAWassert(d->m_glyph);
  if (!d->m_glyph)
    {
      return;
    }
  d->m_glyph->line_to(pt);
}

void
fastuidraw::GlyphRenderDataRestrictedRays::
finalize(enum PainterEnums::fill_rule_t f,
         const RectT<int> &bounding_box,
         float units_per_EM)
{
  float min_size(GlyphGenerateParams::restricted_rays_minimum_render_size());
  vec2 near_thresh(units_per_EM / t_max(8.0f, min_size) * t_sign(min_size));
  finalize(f, bounding_box,
           GlyphGenerateParams::restricted_rays_split_thresh(),
           GlyphGenerateParams::restricted_rays_max_recursion(),
           near_thresh);
}

void
fastuidraw::GlyphRenderDataRestrictedRays::
finalize(enum PainterEnums::fill_rule_t f,
         const RectT<int> &glyph_rect,
         int split_thresh, int max_recursion,
         vec2 near_thresh)
{
  GlyphRenderDataRestrictedRaysPrivate *d;
  ivec2 sz;

  d = static_cast<GlyphRenderDataRestrictedRaysPrivate*>(m_d);
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
      d->m_min = ivec2(0, 0);
      d->m_max = ivec2(0, 0);
      d->m_size = ivec2(0, 0);
      return;
    }

  /* Step 1: Scale the value to be in the range [0, 65535] */
  d->m_glyph->translate(-d->m_glyph->bbox().min_point());

  FASTUIDRAWassert(d->m_glyph->bbox().min_point().x() == 0);
  FASTUIDRAWassert(d->m_glyph->bbox().min_point().y() == 0);
  sz = d->m_glyph->bbox().max_point();

  /* if necessary, scale the glyph down (by a power of 2)
   * so that we need only 16-bits to correctly hold a
   * coordinate.
   */
  ivec2 div_scale(1, 1);
  for (int coord = 0; coord < 2; ++coord)
    {
      while (sz[coord] > 65535)
        {
          div_scale[coord] *= 2;
          sz[coord] /= 2;
          near_thresh[coord] *= 0.5f;
        }
      near_thresh[coord] = t_max(0.0f, near_thresh[coord]);
    }

  if (div_scale != ivec2(1, 1))
    {
      d->m_glyph->scale_down(div_scale);
    }

  /* Step 2: Mark any curve or portions of curves that are
   * cancelled another curve
   */
  d->m_glyph->mark_cancel_curves();

  /* step 3: create the tree */
  CurveListHierarchy hierarchy(d->m_glyph,
                               d->m_glyph->glyph_rect_min(),
                               d->m_glyph->glyph_rect_max(),
                               max_recursion,
                               split_thresh,
                               near_thresh);

  /* step 4: assign tree offsets */
  unsigned int tree_size, total_size;
  tree_size = hierarchy.assign_tree_offsets();

  /* step 5: assign the curve list offsets */
  CurveListCollection curve_lists(tree_size);
  hierarchy.assign_curve_list_offsets(curve_lists);

  /* step 6: assign the offsets to each of the curves */
  total_size = d->m_glyph->assign_curve_offsets(curve_lists.current_offset());

  /* step 7: pack the data */
  d->m_render_data.resize(total_size);
  c_array<generic_data> render_data(make_c_array(d->m_render_data));

  hierarchy.pack_data(render_data);
  curve_lists.pack_data(d->m_glyph, render_data);
  d->m_glyph->pack_data(render_data);

  /* step 8: record the data neeed for shading */
  d->m_min = d->m_glyph->glyph_rect_min();
  d->m_max = d->m_glyph->glyph_rect_max();
  d->m_size = d->m_max - d->m_min;

  FASTUIDRAWdelete(d->m_glyph);
  d->m_glyph = nullptr;
}

enum fastuidraw::return_code
fastuidraw::GlyphRenderDataRestrictedRays::
upload_to_atlas(GlyphAtlasProxy &atlas_proxy,
                GlyphAttribute::Array &attributes) const
{
  GlyphRenderDataRestrictedRaysPrivate *d;
  d = static_cast<GlyphRenderDataRestrictedRaysPrivate*>(m_d);

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

  attributes.resize(7);
  for (unsigned int c = 0; c < 4; ++c)
    {
      int x, y;

      x = (c & GlyphAttribute::right_corner_mask) ? d->m_size.x() : 0;
      y = (c & GlyphAttribute::top_corner_mask)   ? d->m_size.y() : 0;

      attributes[glyph_coordinate_delta_from_min_x].m_data[c] = x;
      attributes[glyph_coordinate_delta_from_min_y].m_data[c] = y;
    }
  attributes[glyph_width].m_data = uvec4(d->m_size.x());
  attributes[glyph_height].m_data = uvec4(d->m_size.y());
  attributes[glyph_min_corner_x].m_data = uvec4(d->m_min.x());
  attributes[glyph_min_corner_y].m_data = uvec4(d->m_min.y());

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
fastuidraw::GlyphRenderDataRestrictedRays::
query(RectT<int> *glyph_rect,
      c_array<const fastuidraw::generic_data> *gpu_data) const
{
  GlyphRenderDataRestrictedRaysPrivate *d;
  d = static_cast<GlyphRenderDataRestrictedRaysPrivate*>(m_d);

  FASTUIDRAWassert(!d->m_glyph);
  if (d->m_glyph)
    {
      return routine_fail;
    }

  *gpu_data = make_c_array(d->m_render_data);
  glyph_rect->m_min_point = d->m_min;
  glyph_rect->m_max_point = d->m_max;

  return routine_success;
}
