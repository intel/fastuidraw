/*!
 * \file filled_path.cpp
 * \brief file filled_path.cpp
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
#include <list>
#include <map>
#include <set>
#include <algorithm>
#include <ctime>
#include <math.h>

#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/filled_path.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include "../private/util_private.hpp"
#include "../private/util_private_ostream.hpp"
#include "../private/bounding_box.hpp"
#include "../private/clip.hpp"
#include "../../3rd_party/glu-tess/glu-tess.hpp"

/* Actual triangulation is handled by GLU-tess.
   The main complexity in creating a FilledPath
   comes from two elements:
    - handling overlapping edges
    - creating a hierarchy for creating triangulations
      and for culling.

   The first is needed because GLU-tess will fail
   if any two edges overlap (we say a pair of edges
   overlap if they intersect at more than just a single
   point). We handle this by observing that GLU-tess
   takes doubles but TessellatedPath is floats. When
   we feed the coordinates to GLU-tess, we offset the
   values by an amount that is visible in fp64 but not
   in fp32. In addition, we also want to merge points
   that are close in fp32 as well. The details are
   handled in CoordinateCoverter, PointHoard and
   tesser.

   The second is needed for primarily to speed up
   tessellation. If a TessellatedPath has a large
   number of vertices, then that is likely because
   it is a high level of detail and likely zoomed in
   a great deal. To handle that, we need only to
   have the triangulation of a smaller portion of
   it ready. Thus we break the original path into
   a hierarchy of paths. The partitioning is done
   a single half plane at a time. A contour from
   the original path is computed by simply removing
   any points on the wrong side of the half plane
   and inserting the points where the path crossed
   the half plane. The sub-path objects are computed
   via the class SubPath. The class SubsetPrivate
   is the one that represents an element in the
   hierarchy that is triangulated on demand.
 */

/* Values to define how to create Subset objects.
 */
namespace SubsetConstants
{
  enum
    {
      recursion_depth = 12,
      points_per_subset = 64
    };

  /* if negative, aspect ratio is not
     enfored.
   */
  const float size_max_ratio = 4.0f;
}

/* Constants for CoordinateConverter.
   CoordinateConverter's purpose is to remap
   the bounding box of a fastuidraw::TessellatedPath
   to [0, 2 ^ N] x [0,  2 ^ N]
   and then apply a fudge offset to the point
   that an fp64 sees but an fp32 does not.

   We do this to allow for the input TessellatedPath
   to have overlapping edges. The value for the
   fudge offset is to be incremented on each point.

   An fp32 has a 23-bit significand that allows it
   to represent any integer in the range [-2^24, 2^24]
   exactly. An fp64 has a 52 bit significand.

   We set N to be 24 and the fudginess to be 2^-20
   (leaving 9-bits for GLU to use for intersections).
 */
namespace CoordinateConverterConstants
{
  enum
    {
      log2_box_dim = 16,
      negative_log2_fudge = 20,
      box_dim = (1 << log2_box_dim),
    };
}

namespace
{
  class Edge:public fastuidraw::uvec2
  {
  public:
    Edge(unsigned int a, unsigned int b):
      fastuidraw::uvec2(fastuidraw::t_min(a, b), fastuidraw::t_max(a, b))
    {
    }
  };

  class EdgeData
  {
  public:
    class edge_split
    {
    public:
      edge_split(unsigned int v, const Edge &e):
        m_split_pt(v),
        m_with_edge(e)
      {}

      unsigned int m_split_pt;
      Edge m_with_edge;
    };

    class per_entry
    {
    public:
      uint64_t m_area;
      int m_winding;
      unsigned int m_vertex;

      /* NOTE: reverse sorted by AREA
       */
      bool
      operator<(const per_entry &rhs) const
      {
        return m_area > rhs.m_area;
      }

      bool
      canidate(int w, unsigned int v) const
      {
        return v != m_vertex || w != m_winding;
      }
    };

    EdgeData(void):
      m_sorted(false),
      m_is_split(false)
    {}

    void
    add_winding(uint64_t area, int w, unsigned int v);

    /* Giving a vertex v, return the winding
       number of the triangle that shares the
       edge. If there are multiple choices
       for what vertex to choose that is not v,
       choose the vertex that:
       a) is on the opposite side of the edge
       b) maximizes area
    */
    int
    opposite_winding(int w, unsigned int v);

    fastuidraw::const_c_array<per_entry>
    filtered_entries(void) const;

    fastuidraw::const_c_array<per_entry>
    unfiltered_entries(void) const
    {
      return fastuidraw::make_c_array(m_entries);
    }

    void
    split_edge(void)
    {
      m_is_split = true;
    }

    bool
    is_split(void) const
    {
      return m_is_split;
    }

  private:
    void
    sort_entries(void) const;

    static
    bool
    compare_entry_reverse_area(const per_entry &lhs, const per_entry &rhs)
    {
      return lhs.m_area > rhs.m_area;
    }

    static
    bool
    compare_entry_vertex(const per_entry &lhs, const per_entry &rhs)
    {
      return lhs.m_vertex < rhs.m_vertex;
    }

    mutable std::vector<per_entry> m_entries, m_filtered_entries;
    mutable bool m_sorted;
    bool m_is_split;
  };

  class AAEdge
  {
  public:
    explicit
    AAEdge(const Edge &pedge):
      m_edge(pedge),
      m_count(0)
    {}

    void
    add_entry(const EdgeData::per_entry &entry)
    {
      assert(m_count < 2);
      m_winding[m_count] = entry.m_winding;
      m_opposite[m_count] = entry.m_vertex;
    }

    const Edge&
    edge(void) const
    {
      return m_edge;
    }

    int
    winding(int v) const
    {
      assert(v == 0 || v == 1);
      assert(0 <= m_count && m_count <= 2);
      v = fastuidraw::t_min(m_count - 1, v);
      return (v >= 0) ?
        m_winding[v] :
        0;
    }

    int
    count(void) const
    {
      return m_count;
    }

  private:
    Edge m_edge;
    fastuidraw::vecN<int, 2> m_winding;
    fastuidraw::vecN<unsigned int, 2> m_opposite;
    int m_count;
  };

  class AAEdgeListCounter:fastuidraw::noncopyable
  {
  public:
    AAEdgeListCounter(void):
      m_largest_edge_count(0)
    {}

    void
    add_edge(const AAEdge &edge)
    {
      int w0, w1;
      unsigned int K;

      w0 = edge.winding(0);
      w1 = edge.winding(1);
      K = fastuidraw::FilledPath::Subset::chunk_for_aa_fuzz(w0, w1);
      if(K >= m_edge_count.size())
        {
          m_edge_count.resize(K + 1, 0);
        }
      ++m_edge_count[K];
      m_largest_edge_count = fastuidraw::t_max(m_largest_edge_count, m_edge_count[K]);
    }

    void
    add_counts(const AAEdgeListCounter &obj)
    {
      if(obj.m_edge_count.size() > m_edge_count.size())
        {
          m_edge_count.resize(obj.m_edge_count.size(), 0);
        }

      for(unsigned int i = 0, endi = obj.m_edge_count.size(); i < endi; ++i)
        {
          m_edge_count[i] += obj.m_edge_count[i];
          m_largest_edge_count = fastuidraw::t_max(m_largest_edge_count, m_edge_count[i]);
        }
    }

    unsigned int
    edge_count(unsigned int chunk) const
    {
      return (chunk < m_edge_count.size()) ?
        m_edge_count[chunk] :
        0;
    }

    unsigned int
    largest_edge_count(void) const
    {
      return m_largest_edge_count;
    }

  private:
    unsigned int m_largest_edge_count;
    std::vector<unsigned int> m_edge_count;
  };

  class AAEdgeList
  {
  public:
    explicit
    AAEdgeList(AAEdgeListCounter *counter,
               std::vector<AAEdge> *list):
      m_counter(*counter),
      m_list(*list)
    {}

    void
    add_edge(const AAEdge &edge)
    {
      m_list.push_back(edge);
      m_counter.add_edge(edge);
    }
  private:
    AAEdgeListCounter &m_counter;
    std::vector<AAEdge> &m_list;
  };

  class BoundaryEdgeTracker:fastuidraw::noncopyable
  {
  public:
    explicit
    BoundaryEdgeTracker(const std::vector<fastuidraw::vec2> *pts):
      m_pts(*pts)
    {}

    void
    record_path_bb_edge(unsigned int v0, unsigned int v1)
    {
      m_path_bb_edges.push_back(Edge(v0, v1));
    }

    void
    record_contour_edge(unsigned int v0, unsigned int v1);

    void
    split_edge(const Edge &e, unsigned int v, const Edge &oe);

    void
    record_triangle(int w, uint64_t area,
                    unsigned int v0, unsigned int v1, unsigned int v2);

    void
    create_aa_edges(AAEdgeList &out_aa_edges);

  private:
    void
    record_triangle_edge(int w, uint64_t area,
                         unsigned int e0, unsigned int e1,
                         unsigned int opposite);

    std::map<Edge, EdgeData> m_data;
    std::vector<Edge> m_path_bb_edges;
    const std::vector<fastuidraw::vec2> &m_pts;
  };

  class per_winding_data:
    public fastuidraw::reference_counted<per_winding_data>::non_concurrent
  {
  public:
    per_winding_data(void):
      m_count(0)
    {}

    void
    add_index(unsigned int idx)
    {
      m_indices.push_back(idx);
      ++m_count;
    }

    unsigned int
    count(void) const
    {
      return m_count;
    }

    void
    fill_at(unsigned int &offset,
            fastuidraw::c_array<unsigned int> dest,
            fastuidraw::const_c_array<unsigned int> &sub_range)
    {
      assert(count() + offset <= dest.size());
      std::copy(m_indices.begin(), m_indices.end(), &dest[offset]);
      sub_range = dest.sub_array(offset, count());
      offset += count();
    }

  private:
    std::list<unsigned int> m_indices;
    unsigned int m_count;
  };

  typedef std::map<int, fastuidraw::reference_counted_ptr<per_winding_data> > winding_index_hoard;

  bool
  is_even(int v)
  {
    return (v % 2) == 0;
  }

  class CoordinateConverter
  {
  public:
    explicit
    CoordinateConverter(const fastuidraw::vec2 &fpmin, const fastuidraw::vec2 &fpmax)
    {
      fastuidraw::vecN<double, 2> delta, pmin, pmax;

      pmin = fastuidraw::vecN<double, 2>(fpmin);
      pmax = fastuidraw::vecN<double, 2>(fpmax);
      delta = pmax - pmin;
      m_scale = fastuidraw::vecN<double, 2>(1.0, 1.0) / delta;
      m_scale *= static_cast<double>(CoordinateConverterConstants::box_dim);
      m_translate = pmin;
      m_delta_fudge = ::exp2(static_cast<double>(-CoordinateConverterConstants::negative_log2_fudge));
      m_scale_f = fastuidraw::vec2(m_scale);
      m_translate_f = fastuidraw::vec2(m_translate);
    }

    fastuidraw::vecN<double, 2>
    apply(const fastuidraw::vec2 &pt, unsigned int fudge_count) const
    {
      fastuidraw::vecN<double, 2> r, qt(pt);
      double fudge;

      r = m_scale * (qt - m_translate);
      fudge = static_cast<double>(fudge_count) * m_delta_fudge;
      r.x() += fudge;
      r.y() += fudge;
      return r;
    }

    fastuidraw::ivec2
    iapply(const fastuidraw::vec2 &pt) const
    {
      fastuidraw::vec2 r;
      fastuidraw::ivec2 return_value;

      r = m_scale_f * (pt - m_translate_f);
      return_value.x() = static_cast<int>(r.x());
      return_value.y() = static_cast<int>(r.y());
      return return_value;
    }

    fastuidraw::vec2
    unapply(const fastuidraw::ivec2 &ipt) const
    {
      fastuidraw::vec2 p(ipt.x(), ipt.y());
      p /= m_scale_f;
      p += m_translate_f;
      return p;
    }

    double
    fudge_delta(void) const
    {
      return m_delta_fudge;
    }

  private:
    double m_delta_fudge;
    fastuidraw::vecN<double, 2> m_scale, m_translate;
    fastuidraw::vec2 m_scale_f, m_translate_f;
  };

  enum
    {
      box_max_x_flag = 1,
      box_max_y_flag = 2,
      box_min_x_min_y = 0 | 0,
      box_min_x_max_y = 0 | box_max_y_flag,
      box_max_x_max_y = box_max_x_flag | box_max_y_flag,
      box_max_x_min_y = box_max_x_flag,
    };

  class SubPath
  {
  public:
    class SubContourPoint
    {
    public:
      enum from_split_bits
        {
          from_split_on_min_x_boundary = 1,
          from_split_on_max_x_boundary = 2,
          from_split_on_min_y_boundary = 4,
          from_split_on_max_y_boundary = 8
        };

      explicit
      SubContourPoint(const fastuidraw::vec2 &p = fastuidraw::vec2()):
        m_pt(p),
        m_boundary_bits(0)
      {}

      SubContourPoint(const SubContourPoint &a,
                      const SubContourPoint &b,
                      const fastuidraw::vec2 &pt,
                      uint32_t boundary_bits_from_split):
        m_pt(pt),
        m_boundary_bits(a.boundary_bits() | b.boundary_bits() | boundary_bits_from_split)
      {}

      const fastuidraw::vec2&
      pt(void) const
      {
        return m_pt;
      }

      uint32_t
      boundary_bits(void) const
      {
        return m_boundary_bits;
      }

      static
      uint32_t
      boundary_bit_flag(int coordinate, bool is_max)
      {
        uint32_t bit;

        assert(coordinate == 0 || coordinate == 1);
        bit = coordinate * 2 + int(is_max);
        return 1u << bit;
      }

    private:
      fastuidraw::vec2 m_pt;
      uint32_t m_boundary_bits;
    };

    typedef std::vector<SubContourPoint> SubContour;

    explicit
    SubPath(const fastuidraw::TessellatedPath &P);

    const std::vector<SubContour>&
    contours(void) const
    {
      return m_contours;
    }

    const fastuidraw::BoundingBox&
    bounds(void) const
    {
      return m_bounds;
    }

    unsigned int
    total_points(void) const
    {
      return m_total_points;
    }

    fastuidraw::vecN<SubPath*, 2>
    split(int &splitting_coordinate) const;

  private:
    SubPath(const fastuidraw::BoundingBox &bb, std::vector<SubContour> &contours);

    int
    choose_splitting_coordinate(fastuidraw::vec2 mid_pt) const;

    static
    void
    copy_contour(SubContour &dst,
                 const fastuidraw::TessellatedPath &src, unsigned int C);

    static
    void
    split_contour(const SubContour &src,
                  int splitting_coordinate, float spitting_value,
                  SubContour &minC, SubContour &maxC);

    static
    fastuidraw::vec2
    compute_spit_point(fastuidraw::vec2 a,
                       fastuidraw::vec2 b,
                       int splitting_coordinate, float spitting_value);

    unsigned int m_total_points;
    fastuidraw::BoundingBox m_bounds;
    std::vector<SubContour> m_contours;
  };

  class PointHoard:fastuidraw::noncopyable
  {
  public:
    typedef std::pair<unsigned int, bool> ContourPoint;
    typedef std::vector<ContourPoint> Contour;
    typedef std::list<Contour> Path;

    explicit
    PointHoard(const fastuidraw::BoundingBox &bounds,
               std::vector<fastuidraw::vec2> &pts):
      m_converter(bounds.min_point(), bounds.max_point()),
      m_pts(pts)
    {
      assert(!bounds.empty());
    }

    unsigned int
    fetch(const fastuidraw::vec2 &pt);

    void
    generate_path(const SubPath &input, Path &output);

    const fastuidraw::vec2&
    operator[](unsigned int v) const
    {
      assert(v < m_pts.size());
      return m_pts[v];
    }

    const fastuidraw::ivec2&
    ipt(unsigned int v) const
    {
      assert(v < m_ipts.size());
      return m_ipts[v];
    }

    const CoordinateConverter&
    converter(void) const
    {
      return m_converter;
    }

  private:
    void
    generate_contour(const SubPath::SubContour &input, Contour &output);

    CoordinateConverter m_converter;
    std::map<fastuidraw::ivec2, unsigned int> m_map;
    std::vector<fastuidraw::ivec2> m_ipts;
    std::vector<fastuidraw::vec2> &m_pts;
  };

  class tesser:fastuidraw::noncopyable
  {
  protected:
    explicit
    tesser(PointHoard &points,
           BoundaryEdgeTracker &tr,
           int winding_offset);

    virtual
    ~tesser(void);

    void
    start(void);

    void
    stop(void);

    void
    add_path(const PointHoard::Path &P);

    void
    add_path_boundary(const SubPath &P,
                      fastuidraw::vecN<bool, 2> have_prev_neighbor,
                      fastuidraw::vecN<bool, 2> have_next_neighbor);

    bool
    triangulation_failed(void)
    {
      return m_triangulation_failed;
    }

    virtual
    void
    on_begin_polygon(void) = 0;

    virtual
    void
    on_add_triangle(unsigned int v0, unsigned int v1, unsigned int v2) = 0;

    virtual
    FASTUIDRAW_GLUboolean
    fill_region(int winding_number) = 0;

    int
    current_winding(void)
    {
      return m_current_winding;
    }

  private:
    void
    add_contour(const PointHoard::Contour &C);

    static
    void
    begin_callBack(FASTUIDRAW_GLUenum type, int winding_number, void *tess);

    static
    void
    vertex_callBack(unsigned int vertex_data, void *tess);

    static
    void
    combine_callback(double x, double y, unsigned int data[4],
                     double weight[4],  unsigned int *outData,
                     void *tess);

    static
    FASTUIDRAW_GLUboolean
    winding_callBack(int winding_number, void *tess);

    bool
    temp_verts_non_degenerate_triangle(uint64_t &area);

    BoundaryEdgeTracker &m_boundary_edge_tracker;
    unsigned int m_point_count;
    fastuidraw_GLUtesselator *m_tess;
    PointHoard &m_points;
    fastuidraw::vecN<unsigned int, 3> m_temp_verts;
    unsigned int m_temp_vert_count;
    bool m_triangulation_failed;
    int m_current_winding, m_winding_offset;
    bool m_current_winding_inited;
  };

  class non_zero_tesser:private tesser
  {
  public:
    static
    bool
    execute_path(PointHoard &points,
                 const PointHoard::Path &P,
                 const SubPath &path,
                 winding_index_hoard &hoard,
                 BoundaryEdgeTracker &tr)
    {
      non_zero_tesser NZ(points, P, path, hoard, tr);
      return NZ.triangulation_failed();
    }

  private:
    non_zero_tesser(PointHoard &points,
                    const PointHoard::Path &P,
                    const SubPath &path,
                    winding_index_hoard &hoard,
                    BoundaryEdgeTracker &tr);

    virtual
    void
    on_begin_polygon(void);

    virtual
    void
    on_add_triangle(unsigned int v0, unsigned int v1, unsigned int v2);

    virtual
    FASTUIDRAW_GLUboolean
    fill_region(int winding_number);

    winding_index_hoard &m_hoard;
    fastuidraw::reference_counted_ptr<per_winding_data> m_current_indices;
  };

  class zero_tesser:private tesser
  {
  public:
    static
    bool
    execute_path(PointHoard &points,
                 const PointHoard::Path &P,
                 const SubPath &path,
                 winding_index_hoard &hoard,
                 fastuidraw::vecN<bool, 2> have_prev_neighbor,
                 fastuidraw::vecN<bool, 2> have_next_neighbor,
                 BoundaryEdgeTracker &tr)
    {
      zero_tesser Z(points, P, path, hoard,
                    have_prev_neighbor, have_next_neighbor,
                    tr);
      return Z.triangulation_failed();
    }

  private:

    zero_tesser(PointHoard &points,
                const PointHoard::Path &P,
                const SubPath &path,
                winding_index_hoard &hoard,
                fastuidraw::vecN<bool, 2> have_prev_neighbor,
                fastuidraw::vecN<bool, 2> have_next_neighbor,
                BoundaryEdgeTracker &tr);

    virtual
    void
    on_begin_polygon(void);

    virtual
    void
    on_add_triangle(unsigned int v0, unsigned int v1, unsigned int v2);

    virtual
    FASTUIDRAW_GLUboolean
    fill_region(int winding_number);

    fastuidraw::reference_counted_ptr<per_winding_data> &m_indices;
  };

  class builder:fastuidraw::noncopyable
  {
  public:
    explicit
    builder(const SubPath &P, std::vector<fastuidraw::vec2> &pts,
            fastuidraw::vecN<bool, 2> have_prev_neighbor,
            fastuidraw::vecN<bool, 2> have_next_neighbor,
            BoundaryEdgeTracker &tr);

    ~builder();

    void
    fill_indices(std::vector<unsigned int> &indices,
                 std::map<int, fastuidraw::const_c_array<unsigned int> > &winding_map,
                 unsigned int &even_non_zero_start,
                 unsigned int &zero_start);

    bool
    triangulation_failed(void)
    {
      return m_failed;
    }

  private:
    winding_index_hoard m_hoard;
    PointHoard m_points;
    bool m_failed;
  };

  class AttributeDataMerger:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    AttributeDataMerger(const fastuidraw::PainterAttributeData &a,
                        const fastuidraw::PainterAttributeData &b,
                        bool common_chunking):
      m_a(a), m_b(b),
      m_common_chunking(common_chunking)
    {
    }

    virtual
    void
    compute_sizes(unsigned int &number_attributes,
                  unsigned int &number_indices,
                  unsigned int &number_attribute_chunks,
                  unsigned int &number_index_chunks,
                  unsigned int &number_z_increments) const;

    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
              fastuidraw::c_array<fastuidraw::PainterIndex> indices,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<unsigned int> zincrements,
              fastuidraw::c_array<int> index_adjusts) const;

  private:
    const fastuidraw::PainterAttributeData &m_a, &m_b;
    bool m_common_chunking;
  };

  class EdgeAttributeDataFiller:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    EdgeAttributeDataFiller(int min_winding, int max_winding,
                            const std::vector<fastuidraw::vec2> *pts,
                            const std::vector<AAEdge> *edges):
      m_min_winding(min_winding),
      m_max_winding(max_winding),
      m_pts(*pts),
      m_edges(*edges)
    {}

    virtual
    void
    compute_sizes(unsigned int &number_attributes,
                  unsigned int &number_indices,
                  unsigned int &number_attribute_chunks,
                  unsigned int &number_index_chunks,
                  unsigned int &number_z_increments) const;
    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
              fastuidraw::c_array<fastuidraw::PainterIndex> indices,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<unsigned int> zincrements,
              fastuidraw::c_array<int> index_adjusts) const;

  private:
    void
    pack_attribute(const Edge &edge,
                   fastuidraw::c_array<fastuidraw::PainterAttribute> dst) const;

    int m_min_winding, m_max_winding;
    const std::vector<fastuidraw::vec2> &m_pts;
    const std::vector<AAEdge> &m_edges;
  };

  class AttributeDataFiller:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    std::vector<fastuidraw::vec2> m_points;

    /* Carefully organize indices as follows:
       - first all elements with odd winding number
       - then all elements with even and non-zero winding number
       - then all element with zero winding number.
       By doing so, the following are continuous in the array:
       - non-zero
       - odd-even fill rule
       - complement of odd-even fill
       - complement of non-zero
     */
    std::vector<unsigned int> m_indices;
    fastuidraw::const_c_array<unsigned int> m_nonzero_winding_indices;
    fastuidraw::const_c_array<unsigned int> m_zero_winding_indices;
    fastuidraw::const_c_array<unsigned int> m_odd_winding_indices;
    fastuidraw::const_c_array<unsigned int> m_even_winding_indices;

    /* m_per_fill[w] gives the indices to the triangles
       with the winding number w. The value points into
       indices
    */
    std::map<int, fastuidraw::const_c_array<unsigned int> > m_per_fill;

    virtual
    void
    compute_sizes(unsigned int &number_attributes,
                  unsigned int &number_indices,
                  unsigned int &number_attribute_chunks,
                  unsigned int &number_index_chunks,
                  unsigned int &number_z_increments) const;
    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
              fastuidraw::c_array<fastuidraw::PainterIndex> indices,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<unsigned int> zincrements,
              fastuidraw::c_array<int> index_adjusts) const;

    static
    fastuidraw::PainterAttribute
    generate_attribute(const fastuidraw::vec2 &src)
    {
      fastuidraw::PainterAttribute dst;

      dst.m_attrib0 = fastuidraw::pack_vec4(src.x(), src.y(), 0.0f, 0.0f);
      dst.m_attrib1 = fastuidraw::uvec4(0u, 0u, 0u, 0u);
      dst.m_attrib2 = fastuidraw::uvec4(0u, 0u, 0u, 0u);

      return dst;
    }
  };

  class ScratchSpacePrivate
  {
  public:
    std::vector<fastuidraw::vec3> m_adjusted_clip_eqs;
    std::vector<fastuidraw::vec2> m_clipped_rect;

    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_clip_scratch_vec2s;
    std::vector<float> m_clip_scratch_floats;
  };

  class SubsetPrivate
  {
  public:
    ~SubsetPrivate(void);

    unsigned int
    select_subsets(ScratchSpacePrivate &scratch,
                   fastuidraw::const_c_array<fastuidraw::vec3> clip_equations,
                   const fastuidraw::float3x3 &clip_matrix_local,
                   unsigned int max_attribute_cnt,
                   unsigned int max_index_cnt,
                   fastuidraw::c_array<unsigned int> dst);

    void
    make_ready(void);

    fastuidraw::const_c_array<int>
    winding_numbers(void)
    {
      assert(m_painter_data != NULL);
      return fastuidraw::make_c_array(m_winding_numbers);
    }

    const fastuidraw::PainterAttributeData&
    painter_data(void)
    {
      assert(m_painter_data != NULL);
      return *m_painter_data;
    }

    const fastuidraw::PainterAttributeData&
    fuzz_painter_data(void)
    {
      assert(m_fuzz_painter_data != NULL);
      return *m_fuzz_painter_data;
    }

    static
    SubsetPrivate*
    create_root_subset(SubPath *P, std::vector<SubsetPrivate*> &out_values);

    static
    unsigned int
    signed_to_unsigned(int w)
    {
      int v, s, r;

      v = fastuidraw::t_abs(w);
      s = (w < 0) ? -1 : 0;
      r = 2 * v + s;

      assert(r >= 0);
      return r;
    }

    static
    unsigned int
    unique_combine(unsigned int a0, unsigned int a1)
    {
      int64_t w0, w1;
      w0 = fastuidraw::t_min(a0, a1);
      w1 = fastuidraw::t_max(a0, a1);
      return w0 + (w1 * (w1 + 1)) / 2;
    }

  private:

    SubsetPrivate(SubPath *P, int max_recursion,
                  std::vector<SubsetPrivate*> &out_values);

    void
    select_subsets_implement(ScratchSpacePrivate &scratch,
                             fastuidraw::c_array<unsigned int> dst,
                             unsigned int max_attribute_cnt,
                             unsigned int max_index_cnt,
                             unsigned int &current);

    void
    select_subsets_all_unculled(fastuidraw::c_array<unsigned int> dst,
                                unsigned int max_attribute_cnt,
                                unsigned int max_index_cnt,
                                unsigned int &current);

    void
    make_ready_from_children(void);

    void
    make_ready_from_sub_path(void);

    void
    assign_neighbor_values(SubsetPrivate *parent, int child_id);

    /* m_ID represents an index into the std::vector<>
       passed into create_hierarchy() where this element
       is found.
     */
    unsigned int m_ID;

    /* The bounds of this SubsetPrivate used in
       select_subsets().
     */
    fastuidraw::BoundingBox m_bounds;

    /* if this SubsetPrivate has children then
       m_painter_data is made by "merging" the
       data of m_painter_data from m_children[0]
       and m_children[1]. We do this merging so
       that we can avoid recursing if the entirity
       of the bounding box is contained in the
       clipping region.
     */
    fastuidraw::PainterAttributeData *m_painter_data;
    std::vector<int> m_winding_numbers;

    /*
     */
    fastuidraw::PainterAttributeData *m_fuzz_painter_data;
    AAEdgeListCounter m_aa_edge_list_counter;

    bool m_sizes_ready;
    unsigned int m_num_attributes;
    unsigned int m_largest_index_block;

    /* m_sub_path is non-NULL only if this SubsetPrivate
       has no children. In addition, it is set to NULL
       and deleted when m_painter_data is created from
       it.
     */
    SubPath *m_sub_path;
    fastuidraw::vecN<SubsetPrivate*, 2> m_children;
    int m_splitting_coordinate;

    /* having neighbors
     */
    fastuidraw::vecN<bool, 2> m_have_prev_neighbor, m_have_next_neighbor;
  };

  class FilledPathPrivate
  {
  public:
    explicit
    FilledPathPrivate(const fastuidraw::TessellatedPath &P);

    ~FilledPathPrivate();

    SubsetPrivate *m_root;
    std::vector<SubsetPrivate*> m_subsets;
  };
}

/////////////////////////////////
// EdgeData methods
fastuidraw::const_c_array<EdgeData::per_entry>
EdgeData::
filtered_entries(void) const
{
  if(!m_sorted)
    {
      sort_entries();
    }
  return fastuidraw::make_c_array(m_filtered_entries);
}

void
EdgeData::
add_winding(uint64_t area, int w, unsigned int v)
{
  per_entry p;

  assert(area > 0);
  assert(!m_sorted);
  p.m_area = area;
  p.m_winding = w;
  p.m_vertex = v;
  m_entries.push_back(p);
}

void
EdgeData::
sort_entries(void) const
{
  assert(!m_sorted);
  m_sorted = true;

  /* first we remove any entries that share vertices.
   */
  std::sort(m_entries.begin(), m_entries.end(), compare_entry_vertex);
  for(unsigned int i = 0, endi = m_entries.size(); i < endi;)
    {
      unsigned int ct, start;
      for(ct = 0, start = i; i < endi && m_entries[i].m_vertex == m_entries[start].m_vertex; ++ct, ++i)
        {}

      assert(ct >= 1);
      if(ct == 1)
        {
          m_filtered_entries.push_back(m_entries[start]);
        }
    }

  std::sort(m_filtered_entries.begin(), m_filtered_entries.end(), compare_entry_reverse_area);
  std::sort(m_entries.begin(), m_entries.end(), compare_entry_reverse_area);
}

///////////////////////////////
// BoundaryEdgeTracker methods
void
BoundaryEdgeTracker::
record_contour_edge(unsigned int v0, unsigned int v1)
{
  if(v0 != v1)
    {
      m_data[Edge(v0, v1)];
    }
}

void
BoundaryEdgeTracker::
split_edge(const Edge &e, unsigned int v, const Edge &oe)
{
  if(e[0] == v || e[1] == v || oe[0] == v || oe[1] == v)
    {
      return;
    }

  std::map<Edge, EdgeData>::iterator iter;
  iter = m_data.find(e);
  if(iter != m_data.end())
    {
      iter->second.split_edge();
      record_contour_edge(e[0], v);
      record_contour_edge(e[1], v);
    }
}

void
BoundaryEdgeTracker::
record_triangle_edge(int w, uint64_t area,
                     unsigned int a, unsigned int b,
                     unsigned int opposite)
{
  Edge E(a, b);
  std::map<Edge, EdgeData>::iterator iter;

  iter = m_data.find(E);
  if(iter != m_data.end())
    {
      iter->second.add_winding(area, w, opposite);
    }
}

void
BoundaryEdgeTracker::
record_triangle(int w, uint64_t area,
                unsigned int v0, unsigned int v1, unsigned int v2)
{
  assert(area > 0);
  record_triangle_edge(w, area, v0, v1, v2);
  record_triangle_edge(w, area, v1, v2, v0);
  record_triangle_edge(w, area, v2, v0, v1);
}

void
BoundaryEdgeTracker::
create_aa_edges(AAEdgeList &out_aa_edges)
{
  /* basic idea: take the first two elements
     with the biggest area.
   */
  for(std::map<Edge, EdgeData>::const_iterator iter = m_data.begin(),
        end = m_data.end(); iter != end; ++iter)
    {
      Edge edge(iter->first);
      const EdgeData &data(iter->second);
      fastuidraw::const_c_array<EdgeData::per_entry> entries(data.filtered_entries());

      if(data.is_split() && !entries.empty())
        {
          /* we take the two largest, which means the
             first two elements from entries()
           */
          AAEdge aa_edge(edge);

          aa_edge.add_entry(entries[0]);
          if(entries.size() > 1)
            {
              aa_edge.add_entry(entries[1]);
            }
          out_aa_edges.add_edge(aa_edge);
        }
    }

  for(unsigned int i = 0, endi = m_path_bb_edges.size(); i < endi; ++i)
    {
      AAEdge aa_edge(m_path_bb_edges[i]);
      out_aa_edges.add_edge(aa_edge);
    }
}

/////////////////////////////////////
// SubPath methods
SubPath::
SubPath(const fastuidraw::BoundingBox &bb,
        std::vector<SubContour> &contours):
  m_total_points(0),
  m_bounds(bb)
{
  m_contours.swap(contours);
  for(std::vector<SubContour>::const_iterator c_iter = m_contours.begin(),
        c_end = m_contours.end(); c_iter != c_end; ++c_iter)
    {
      m_total_points += c_iter->size();
    }
}

SubPath::
SubPath(const fastuidraw::TessellatedPath &P):
  m_total_points(0),
  m_bounds(P.bounding_box_min(),
           P.bounding_box_max()),
  m_contours(P.number_contours())
{
  for(unsigned int c = 0, endc = m_contours.size(); c < endc; ++c)
    {
      copy_contour(m_contours[c], P, c);
      m_total_points += m_contours[c].size();
    }
}

void
SubPath::
copy_contour(SubContour &dst,
             const fastuidraw::TessellatedPath &src, unsigned int C)
{
  for(unsigned int e = 0, ende = src.number_edges(C); e < ende; ++e)
    {
      fastuidraw::range_type<unsigned int> R;

      R = src.edge_range(C, e);
      dst.push_back(SubContourPoint(src.point_data()[R.m_begin].m_p));
      for(unsigned int v = R.m_begin + 1; v + 1 < R.m_end; ++v)
        {
          SubContourPoint pt(src.point_data()[v].m_p);
          dst.push_back(pt);
        }
    }
}

int
SubPath::
choose_splitting_coordinate(fastuidraw::vec2 mid_pt) const
{
  /* do not allow the box to be too far from being a square.
     TODO: if the balance of points heavily favors the other
     side, we should ignore the size_max_ratio. Perhaps a
     wieght factor between the different in # of points
     of the sides and the ratio?
   */
  if(SubsetConstants::size_max_ratio > 0.0f)
    {
      fastuidraw::vec2 wh;
      wh = m_bounds.max_point() - m_bounds.min_point();
      if(wh.x() >= SubsetConstants::size_max_ratio * wh.y())
        {
          return 0;
        }
      else if(wh.y() >= SubsetConstants::size_max_ratio * wh.x())
        {
          return 1;
        }
    }

  /* first find which of splitting in X or splitting in Y
     is optimal.
   */
  fastuidraw::ivec2 number_points_before(0, 0);
  fastuidraw::ivec2 number_points_after(0, 0);
  fastuidraw::ivec2 number_points;

  for(std::vector<SubContour>::const_iterator c_iter = m_contours.begin(),
        c_end = m_contours.end(); c_iter != c_end; ++c_iter)
    {
      fastuidraw::vec2 prev_pt(c_iter->back().pt());
      for(SubContour::const_iterator iter = c_iter->begin(),
            end = c_iter->end(); iter != end; ++iter)
        {
          fastuidraw::vec2 pt(iter->pt());
          for(int i = 0; i < 2; ++i)
            {
              bool prev_b, b;

              prev_b = prev_pt[i] < mid_pt[i];
              b = pt[i] < mid_pt[i];

              if(b || pt[i] == mid_pt[i])
                {
                  ++number_points_before[i];
                }

              if(!b || pt[i] == mid_pt[i])
                {
                  ++number_points_after[i];
                }

              if(prev_pt[i] != mid_pt[i] && prev_b != b)
                {
                  ++number_points_before[i];
                  ++number_points_after[i];
                }
            }
          prev_pt = pt;
        }
    }

  /* choose a splitting that:
      - minimizes number_points_before[i] + number_points_after[i]
   */
  number_points = number_points_before + number_points_after;
  if(number_points.x() < number_points.y())
    {
      return 0;
    }
  else
    {
      return 1;
    }
}

fastuidraw::vec2
SubPath::
compute_spit_point(fastuidraw::vec2 a, fastuidraw::vec2 b,
                   int splitting_coordinate, float splitting_value)
{
  float t, n, d, aa, bb;
  fastuidraw::vec2 return_value;

  n = splitting_value - a[splitting_coordinate];
  d = b[splitting_coordinate] - a[splitting_coordinate];
  t = n / d;

  return_value[splitting_coordinate] = splitting_value;

  aa = a[1 - splitting_coordinate];
  bb = b[1 - splitting_coordinate];
  return_value[1 - splitting_coordinate] = (1.0f - t) * aa + t * bb;

  return return_value;
}

void
SubPath::
split_contour(const SubContour &src,
              int splitting_coordinate, float splitting_value,
              SubContour &C0, SubContour &C1)
{
  SubContourPoint prev_pt(src.back());
  for(SubContour::const_iterator iter = src.begin(),
        end = src.end(); iter != end; ++iter)
    {
      bool b0, prev_b0;
      bool b1, prev_b1;
      fastuidraw::vec2 split_pt;
      const SubContourPoint &pt(*iter);

      prev_b0 = prev_pt.pt()[splitting_coordinate] <= splitting_value;
      b0 = pt.pt()[splitting_coordinate] <= splitting_value;

      prev_b1 = prev_pt.pt()[splitting_coordinate] >= splitting_value;
      b1 = pt.pt()[splitting_coordinate] >= splitting_value;

      if(prev_b0 != b0 || prev_b1 != b1)
        {
          split_pt = compute_spit_point(prev_pt.pt(), pt.pt(),
                                        splitting_coordinate, splitting_value);
        }

      if(prev_b0 != b0)
        {
          SubContourPoint s(prev_pt, pt, split_pt,
                            SubContourPoint::boundary_bit_flag(splitting_coordinate, true));
          C0.push_back(s);
        }

      if(b0)
        {
          C0.push_back(pt);
        }

      if(prev_b1 != b1)
        {
          SubContourPoint s(prev_pt, pt, split_pt,
                            SubContourPoint::boundary_bit_flag(splitting_coordinate, false));
          C1.push_back(s);
        }

      if(b1)
        {
          C1.push_back(pt);
        }

      prev_pt = pt;
    }
}

fastuidraw::vecN<SubPath*, 2>
SubPath::
split(int &splitting_coordinate) const
{
  fastuidraw::vecN<SubPath*, 2> return_value(NULL, NULL);
  fastuidraw::vec2 mid_pt;

  mid_pt = 0.5f * (m_bounds.max_point() + m_bounds.min_point());
  splitting_coordinate = choose_splitting_coordinate(mid_pt);

  /* now split each contour.
   */
  fastuidraw::vec2 B0_max, B1_min;
  B0_max[1 - splitting_coordinate] = m_bounds.max_point()[1 - splitting_coordinate];
  B0_max[splitting_coordinate] = mid_pt[splitting_coordinate];

  B1_min[1 - splitting_coordinate] = m_bounds.min_point()[1 - splitting_coordinate];
  B1_min[splitting_coordinate] = mid_pt[splitting_coordinate];

  fastuidraw::BoundingBox B0(m_bounds.min_point(), B0_max);
  fastuidraw::BoundingBox B1(B1_min, m_bounds.max_point());
  std::vector<SubContour> C0, C1;

  C0.reserve(m_contours.size());
  C1.reserve(m_contours.size());
  for(std::vector<SubContour>::const_iterator c_iter = m_contours.begin(),
        c_end = m_contours.end(); c_iter != c_end; ++c_iter)
    {
      C0.push_back(SubContour());
      C1.push_back(SubContour());
      split_contour(*c_iter, splitting_coordinate,
                    mid_pt[splitting_coordinate],
                    C0.back(), C1.back());

      if(C0.back().empty())
        {
          C0.pop_back();
        }

      if(C1.back().empty())
        {
          C1.pop_back();
        }
    }

  return_value[0] = FASTUIDRAWnew SubPath(B0, C0);
  return_value[1] = FASTUIDRAWnew SubPath(B1, C1);

  return return_value;
}

//////////////////////////////////////
// PointHoard methods
unsigned int
PointHoard::
fetch(const fastuidraw::vec2 &pt)
{
  std::map<fastuidraw::ivec2, unsigned int>::iterator iter;
  fastuidraw::ivec2 ipt;
  unsigned int return_value;

  assert(m_pts.size() == m_ipts.size());

  ipt = m_converter.iapply(pt);
  iter = m_map.find(ipt);
  if(iter != m_map.end())
    {
      return_value = iter->second;
    }
  else
    {
      return_value = m_pts.size();
      m_pts.push_back(m_converter.unapply(ipt));
      m_ipts.push_back(ipt);
      m_map[ipt] = return_value;
    }
  return return_value;
}

void
PointHoard::
generate_path(const SubPath &input, Path &output)
{
  output.clear();
  const std::vector<SubPath::SubContour> &contours(input.contours());
  for(std::vector<SubPath::SubContour>::const_iterator iter = contours.begin(),
        end = contours.end(); iter != end; ++iter)
    {
      const SubPath::SubContour &C(*iter);
      output.push_back(Contour());
      generate_contour(C, output.back());
    }
}

void
PointHoard::
generate_contour(const SubPath::SubContour &C, Contour &output)
{
  std::vector<fastuidraw::BoundingBox> boxes(1);
  unsigned int total_cnt(0), cnt(0);

  for(unsigned int v = 0, endv = C.size(); v < endv; ++v,  ++cnt, ++total_cnt)
    {
      bool no_boundary_bits;
      unsigned int I;

      no_boundary_bits = (0u == C[v].boundary_bits());
      I = fetch(C[v].pt());
      output.push_back(ContourPoint(I, no_boundary_bits));
    }
}

////////////////////////////////////////
// tesser methods
tesser::
tesser(PointHoard &points, BoundaryEdgeTracker &tr,
       int winding_offset):
  m_boundary_edge_tracker(tr),
  m_point_count(0),
  m_points(points),
  m_triangulation_failed(false),
  m_current_winding(0),
  m_winding_offset(winding_offset),
  m_current_winding_inited(false)
{
  m_tess = fastuidraw_gluNewTess;
  fastuidraw_gluTessCallbackBegin(m_tess, &begin_callBack);
  fastuidraw_gluTessCallbackVertex(m_tess, &vertex_callBack);
  fastuidraw_gluTessCallbackCombine(m_tess, &combine_callback);
  fastuidraw_gluTessCallbackFillRule(m_tess, &winding_callBack);
  fastuidraw_gluTessPropertyBoundaryOnly(m_tess, FASTUIDRAW_GLU_FALSE);
}

tesser::
~tesser(void)
{
  fastuidraw_gluDeleteTess(m_tess);
}


void
tesser::
start(void)
{
  fastuidraw_gluTessBeginPolygon(m_tess, this);
}

void
tesser::
stop(void)
{
  fastuidraw_gluTessEndPolygon(m_tess);
}

void
tesser::
add_path(const PointHoard::Path &P)
{
  for(PointHoard::Path::const_iterator iter = P.begin(),
        end = P.end(); iter != end; ++iter)
    {
      add_contour(*iter);
    }
}

void
tesser::
add_contour(const PointHoard::Contour &C)
{
  assert(!C.empty());

  PointHoard::ContourPoint lastI(C.back());

  fastuidraw_gluTessBeginContour(m_tess, FASTUIDRAW_GLU_TRUE);
  for(unsigned int v = 0, endv = C.size(); v < endv; ++v)
    {
      fastuidraw::vecN<double, 2> p;
      PointHoard::ContourPoint I;

      /* TODO: Incrementing the amount by which to apply
         fudge is not the correct thing to do. Rather, we
         should only increment and apply fudge on overlapping
         and degenerate edges.
      */
      I = C[v];
      p = m_points.converter().apply(m_points[I.first], m_point_count);
      ++m_point_count;
      if(I.second || lastI.second)
        {
          m_boundary_edge_tracker.record_contour_edge(lastI.first, I.first);
        }

      fastuidraw_gluTessVertex(m_tess, p.x(), p.y(), I.first);
      lastI = I;
    }
  fastuidraw_gluTessEndContour(m_tess);
}

void
tesser::
add_path_boundary(const SubPath &P,
                  fastuidraw::vecN<bool, 2> have_prev_neighbor,
                  fastuidraw::vecN<bool, 2> have_next_neighbor)
{
  fastuidraw::vec2 pmin, pmax;
  unsigned int vertex_ids[4];
  const unsigned int src[4] =
    {
      box_min_x_min_y,
      box_min_x_max_y,
      box_max_x_max_y,
      box_max_x_min_y,
    };

  pmin = P.bounds().min_point();
  pmax = P.bounds().max_point();

  fastuidraw_gluTessBeginContour(m_tess, FASTUIDRAW_GLU_TRUE);
  for(unsigned int i = 0; i < 4; ++i)
    {
      double slack, x, y;
      unsigned int k;
      fastuidraw::vec2 p;

      k = src[i];
      slack = static_cast<double>(m_point_count) * m_points.converter().fudge_delta();

      if(k & box_max_x_flag)
        {
          x = slack + static_cast<double>(CoordinateConverterConstants::box_dim);
          p.x() = pmax.x();
        }
      else
        {
          x = -slack;
          p.x() = pmin.x();
        }

      if(k & box_max_y_flag)
        {
          y = slack + static_cast<double>(CoordinateConverterConstants::box_dim);
          p.y() = pmax.y();
        }
      else
        {
          y = -slack;
          p.y() = pmin.y();
        }
      vertex_ids[k] = m_points.fetch(p);
      fastuidraw_gluTessVertex(m_tess, x, y, vertex_ids[k]);
    }

  if(!have_prev_neighbor.x())
    {
      m_boundary_edge_tracker.record_path_bb_edge(vertex_ids[box_min_x_min_y],
                                                  vertex_ids[box_min_x_max_y]);
    }

  if(!have_next_neighbor.x())
    {
      m_boundary_edge_tracker.record_path_bb_edge(vertex_ids[box_max_x_min_y],
                                                  vertex_ids[box_max_x_max_y]);
    }

  if(!have_prev_neighbor.y())
    {
      m_boundary_edge_tracker.record_path_bb_edge(vertex_ids[box_min_x_min_y],
                                                  vertex_ids[box_max_x_min_y]);
    }

  if(!have_next_neighbor.y())
    {
      m_boundary_edge_tracker.record_path_bb_edge(vertex_ids[box_min_x_max_y],
                                                  vertex_ids[box_max_x_max_y]);
    }

  fastuidraw_gluTessEndContour(m_tess);
}

bool
tesser::
temp_verts_non_degenerate_triangle(uint64_t &area)
{
  if(m_temp_verts[0] == m_temp_verts[1]
     || m_temp_verts[0] == m_temp_verts[2]
     || m_temp_verts[1] == m_temp_verts[2])
    {
      return false;
    }

  fastuidraw::i64vec2 p0(m_points.ipt(m_temp_verts[0]));
  fastuidraw::i64vec2 p1(m_points.ipt(m_temp_verts[1]));
  fastuidraw::i64vec2 p2(m_points.ipt(m_temp_verts[2]));
  fastuidraw::i64vec2 v(p1 - p0), w(p2 - p0);
  bool return_value;

  /* we only reject a triangle if its area is zero.
   */
  area = fastuidraw::t_abs(v.x() * w.y() - v.y() * w.x());
  return_value = (area > 0);
  return return_value;
}

void
tesser::
begin_callBack(FASTUIDRAW_GLUenum type, int winding_number, void *tess)
{
  tesser *p;
  p = static_cast<tesser*>(tess);
  assert(FASTUIDRAW_GLU_TRIANGLES == type);
  FASTUIDRAWunused(type);

  p->m_temp_vert_count = 0;
  if(!p->m_current_winding_inited || p->m_current_winding != winding_number)
    {
      p->m_current_winding_inited = true;
      p->m_current_winding = winding_number;
      p->on_begin_polygon();
    }
}

void
tesser::
vertex_callBack(unsigned int vertex_id, void *tess)
{
  tesser *p;
  p = static_cast<tesser*>(tess);

  if(vertex_id == FASTUIDRAW_GLU_NULL_CLIENT_ID)
    {
      p->m_triangulation_failed = true;
    }

  /* Cache adds vertices in groups of 3 (triangles),
     then if all vertices are NOT FASTUIDRAW_GLU_NULL_CLIENT_ID,
     then add them.
   */
  p->m_temp_verts[p->m_temp_vert_count] = vertex_id;
  p->m_temp_vert_count++;
  if(p->m_temp_vert_count == 3)
    {
      uint64_t area(0u);
      p->m_temp_vert_count = 0;
      /*
        if vertex_id is FASTUIDRAW_GLU_NULL_CLIENT_ID, that means
        the triangle is junked.
      */
      if(p->m_temp_verts[0] != FASTUIDRAW_GLU_NULL_CLIENT_ID
         && p->m_temp_verts[1] != FASTUIDRAW_GLU_NULL_CLIENT_ID
         && p->m_temp_verts[2] != FASTUIDRAW_GLU_NULL_CLIENT_ID
         && p->temp_verts_non_degenerate_triangle(area))
        {
          assert(area > 0);
          p->m_boundary_edge_tracker.record_triangle(p->current_winding() + p->m_winding_offset,
                                                     area,
                                                     p->m_temp_verts[0],
                                                     p->m_temp_verts[1],
                                                     p->m_temp_verts[2]);
          p->on_add_triangle(p->m_temp_verts[0],
                             p->m_temp_verts[1],
                             p->m_temp_verts[2]);
        }
    }
}

void
tesser::
combine_callback(double x, double y, unsigned int data[4],
                 double weight[4],  unsigned int *outData,
                 void *tess)
{
  FASTUIDRAWunused(x);
  FASTUIDRAWunused(y);

  tesser *p;
  unsigned int v;
  Edge e0(data[0], data[1]);
  Edge e1(data[2], data[3]);
  fastuidraw::vec2 pt(0.0f, 0.0f);

  p = static_cast<tesser*>(tess);
  for(unsigned int i = 0; i < 4; ++i)
    {
      if(data[i] != FASTUIDRAW_GLU_NULL_CLIENT_ID)
        {
          pt += float(weight[i]) * p->m_points[data[i]];
        }
    }
  v = p->m_points.fetch(pt);

  p->m_boundary_edge_tracker.split_edge(e0, v, e1);
  p->m_boundary_edge_tracker.split_edge(e1, v, e0);

  *outData = v;
}

FASTUIDRAW_GLUboolean
tesser::
winding_callBack(int winding_number, void *tess)
{
  tesser *p;
  FASTUIDRAW_GLUboolean return_value;

  p = static_cast<tesser*>(tess);
  return_value = p->fill_region(winding_number);
  return return_value;
}

///////////////////////////////////
// non_zero_tesser methods
non_zero_tesser::
non_zero_tesser(PointHoard &points,
                const PointHoard::Path &P,
                const SubPath &path,
                winding_index_hoard &hoard,
                BoundaryEdgeTracker &tr):
  tesser(points, tr, 0),
  m_hoard(hoard)
{
  start();
  add_path(P);
  stop();
  FASTUIDRAWunused(path);
}

void
non_zero_tesser::
on_begin_polygon(void)
{
  int w(current_winding());

  fastuidraw::reference_counted_ptr<per_winding_data> &h(m_hoard[w]);
  if(!h)
    {
      h = FASTUIDRAWnew per_winding_data();
    }
  m_current_indices = h;
}

void
non_zero_tesser::
on_add_triangle(unsigned int v0, unsigned int v1, unsigned int v2)
{
  m_current_indices->add_index(v0);
  m_current_indices->add_index(v1);
  m_current_indices->add_index(v2);
}


FASTUIDRAW_GLUboolean
non_zero_tesser::
fill_region(int winding_number)
{
  return winding_number != 0 ?
    FASTUIDRAW_GLU_TRUE :
    FASTUIDRAW_GLU_FALSE;
}

///////////////////////////////
// zero_tesser methods
zero_tesser::
zero_tesser(PointHoard &points,
            const PointHoard::Path &P,
            const SubPath &path,
            winding_index_hoard &hoard,
            fastuidraw::vecN<bool, 2> have_prev_neighbor,
            fastuidraw::vecN<bool, 2> have_next_neighbor,
            BoundaryEdgeTracker &tr):
  tesser(points, tr, 1),
  m_indices(hoard[0])
{
  if(!m_indices)
    {
      m_indices = FASTUIDRAWnew per_winding_data();
    }

  start();
  add_path(P);
  add_path_boundary(path, have_prev_neighbor, have_next_neighbor);
  stop();
}

void
zero_tesser::
on_begin_polygon(void)
{
  assert(current_winding() == -1);
}

void
zero_tesser::
on_add_triangle(unsigned int v0, unsigned int v1, unsigned int v2)
{
  m_indices->add_index(v0);
  m_indices->add_index(v1);
  m_indices->add_index(v2);
}

FASTUIDRAW_GLUboolean
zero_tesser::
fill_region(int winding_number)
{
  return winding_number == -1 ?
    FASTUIDRAW_GLU_TRUE :
    FASTUIDRAW_GLU_FALSE;
}

/////////////////////////////////////////
// builder methods
builder::
builder(const SubPath &P, std::vector<fastuidraw::vec2> &points,
        fastuidraw::vecN<bool, 2> have_prev_neighbor,
        fastuidraw::vecN<bool, 2> have_next_neighbor,
        BoundaryEdgeTracker &tr):
  m_points(P.bounds(), points)
{
  bool failZ, failNZ;
  PointHoard::Path path;

  m_points.generate_path(P, path);
  failNZ = non_zero_tesser::execute_path(m_points, path, P, m_hoard, tr);
  failZ = zero_tesser::execute_path(m_points, path, P, m_hoard,
                                    have_prev_neighbor, have_next_neighbor,
                                    tr);
  m_failed = failNZ || failZ;
}

builder::
~builder()
{
}

void
builder::
fill_indices(std::vector<unsigned int> &indices,
             std::map<int, fastuidraw::const_c_array<unsigned int> > &winding_map,
             unsigned int &even_non_zero_start,
             unsigned int &zero_start)
{
  winding_index_hoard::iterator iter, end;
  unsigned int total(0), num_odd(0), num_even_non_zero(0), num_zero(0);

  /* compute number indices needed */
  for(iter = m_hoard.begin(), end = m_hoard.end(); iter != end; ++iter)
    {
      unsigned int cnt;

      cnt = iter->second->count();
      total += cnt;
      if(iter->first == 0)
        {
          num_zero += cnt;
        }
      else if (is_even(iter->first))
        {
          num_even_non_zero += cnt;
        }
      else
        {
          num_odd += cnt;
        }
    }

  /* pack as follows:
      - odd
      - even non-zero
      - zero
   */
  unsigned int current_odd(0), current_even_non_zero(num_odd);
  unsigned int current_zero(num_even_non_zero + num_odd);

  indices.resize(total);
  for(iter = m_hoard.begin(), end = m_hoard.end(); iter != end; ++iter)
    {
      if(iter->first == 0)
        {
          if(iter->second->count() > 0)
            {
              iter->second->fill_at(current_zero,
                                    fastuidraw::make_c_array(indices),
                                    winding_map[iter->first]);
            }
        }
      else if(is_even(iter->first))
        {
          if(iter->second->count() > 0)
            {
              iter->second->fill_at(current_even_non_zero,
                                    fastuidraw::make_c_array(indices),
                                    winding_map[iter->first]);
            }
        }
      else
        {
          if(iter->second->count() > 0)
            {
              iter->second->fill_at(current_odd,
                                    fastuidraw::make_c_array(indices),
                                    winding_map[iter->first]);
            }
        }
    }

  assert(current_zero == total);
  assert(current_odd == num_odd);
  assert(current_even_non_zero == current_odd + num_even_non_zero);

  even_non_zero_start = num_odd;
  zero_start = current_odd + num_even_non_zero;

}

////////////////////////////////
// AttributeDataMerger methods
void
AttributeDataMerger::
compute_sizes(unsigned int &number_attributes,
              unsigned int &number_indices,
              unsigned int &number_attribute_chunks,
              unsigned int &number_index_chunks,
              unsigned int &number_z_increments) const
{
  number_z_increments = 0;

  number_attribute_chunks = fastuidraw::t_max(m_a.attribute_data_chunks().size(),
                                              m_b.attribute_data_chunks().size());
  number_attributes = 0;
  for(unsigned int c = 0; c < number_attribute_chunks; ++c)
    {
      unsigned int a_sz, b_sz;

      a_sz = m_a.attribute_data_chunk(c).size();
      b_sz = m_b.attribute_data_chunk(c).size();
      number_attributes += (a_sz + b_sz);
    }

  number_index_chunks = fastuidraw::t_max(m_a.index_data_chunks().size(),
                                          m_b.index_data_chunks().size());
  number_indices = 0;
  for(unsigned int c = 0; c < number_index_chunks; ++c)
    {
      unsigned int a_sz, b_sz;

      a_sz = m_a.index_data_chunk(c).size();
      b_sz = m_b.index_data_chunk(c).size();
      number_indices += (a_sz + b_sz);
    }
}


void
AttributeDataMerger::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
          fastuidraw::c_array<fastuidraw::PainterIndex> indices,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<unsigned int> zincrements,
          fastuidraw::c_array<int> index_adjusts) const
{
  FASTUIDRAWunused(zincrements);

  for(unsigned int i = 0, dst_offset = 0; i < attrib_chunks.size(); ++i)
    {
      fastuidraw::c_array<fastuidraw::PainterAttribute> dst;
      fastuidraw::const_c_array<fastuidraw::PainterAttribute> src;
      unsigned int start(dst_offset), size(0);

      src = m_a.attribute_data_chunk(i);
      if(!src.empty())
        {
          dst = attributes.sub_array(dst_offset, src.size());
          dst_offset += dst.size();
          size += dst.size();
          std::copy(src.begin(), src.end(), dst.begin());
        }

      src = m_b.attribute_data_chunk(i);
      if(!src.empty())
        {
          dst = attributes.sub_array(dst_offset, src.size());
          dst_offset += dst.size();
          size += dst.size();
          std::copy(src.begin(), src.end(), dst.begin());
        }
      attrib_chunks[i] = attributes.sub_array(start, size);
    }

  /* copy indices is trickier; we need to copy with correct chunking
     AND adjust the values for the indices coming from m_b (because
     m_b attributes are placed after m_a attributes).
   */
  for(unsigned int i = 0, dst_offset = 0; i < index_chunks.size(); ++i)
    {
      fastuidraw::c_array<fastuidraw::PainterIndex> dst;
      fastuidraw::const_c_array<fastuidraw::PainterIndex> src;
      unsigned int start(dst_offset), size(0);

      index_adjusts[i] = 0;

      src = m_a.index_data_chunk(i);
      if(!src.empty())
        {
          dst = indices.sub_array(dst_offset, src.size());
          dst_offset += dst.size();
          size += dst.size();
          std::copy(src.begin(), src.end(), dst.begin());
        }

      src = m_b.index_data_chunk(i);
      if(!src.empty())
        {
          unsigned int adjust_chunk, adjust;

          dst = indices.sub_array(dst_offset, src.size());
          dst_offset += dst.size();
          size += dst.size();

          adjust_chunk = (m_common_chunking) ? 0 : i;
          adjust = m_a.attribute_data_chunk(adjust_chunk).size();
          for(unsigned int k = 0; k < src.size(); ++k)
            {
              dst[k] = src[k] + adjust;
            }
        }
      index_chunks[i] = indices.sub_array(start, size);
    }
}

////////////////////////////////////
// EdgeAttributeDataFiller methods
void
EdgeAttributeDataFiller::
compute_sizes(unsigned int &number_attributes,
              unsigned int &number_indices,
              unsigned int &number_attribute_chunks,
              unsigned int &number_index_chunks,
              unsigned int &number_z_increments) const
{
  unsigned int a;

  a = fastuidraw::t_max(SubsetPrivate::signed_to_unsigned(m_min_winding),
                        SubsetPrivate::signed_to_unsigned(m_max_winding));
  number_attribute_chunks = number_index_chunks = 1 + SubsetPrivate::unique_combine(a, a);

  /* each edge is 4 attributes and 6 indices
   */
  number_attributes = 4 * m_edges.size();
  number_indices = 6 * m_edges.size();
  number_z_increments = 0;
}

void
EdgeAttributeDataFiller::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
          fastuidraw::c_array<fastuidraw::PainterIndex> indices,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<unsigned int> zincrements,
          fastuidraw::c_array<int> index_adjusts) const
{
  assert(attributes.size() == 4 * m_edges.size());
  assert(indices.size() == 6 * m_edges.size());
  assert(attrib_chunks.size() == index_chunks.size());
  FASTUIDRAWunused(zincrements);

  std::vector<unsigned int> tmp(attrib_chunks.size(), 0);

  for(std::vector<AAEdge>::const_iterator iter = m_edges.begin(),
        end = m_edges.end(); iter != end; ++iter)
    {
      int w0, w1;
      unsigned int ch;

      if(iter->count() > 0)
        {
          w0 = iter->winding(0);
          w1 = iter->winding(1);
        }
      else
        {
          w0 = w1 = 0;
        }
      ch = fastuidraw::FilledPath::Subset::chunk_for_aa_fuzz(w0, w1);
      assert(ch < tmp.size());
      tmp[ch]++;
    }

  for(unsigned int ch = 0, dst_offset = 0; ch < attrib_chunks.size(); ++ch)
    {
      unsigned int sz;

      sz = tmp[ch];
      attrib_chunks[ch] = attributes.sub_array(4 * dst_offset, 4 * sz);
      index_chunks[ch] = indices.sub_array(6 * dst_offset, 6 * sz);
      dst_offset += sz;
      index_adjusts[ch] = 0;
      tmp[ch] = 0;
    }

  for(std::vector<AAEdge>::const_iterator iter = m_edges.begin(),
        end = m_edges.end(); iter != end; ++iter)
    {
      int w0, w1, ch;
      fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attrib;
      fastuidraw::c_array<fastuidraw::PainterIndex> dst_index;

      if(iter->count() > 0)
        {
          w0 = iter->winding(0);
          w1 = iter->winding(1);
        }
      else
        {
          w0 = w1 = 0;
        }
      ch = fastuidraw::FilledPath::Subset::chunk_for_aa_fuzz(w0, w1);

      dst_attrib = attrib_chunks[ch].sub_array(4 * tmp[ch], 4).const_cast_pointer();
      dst_index = index_chunks[ch].sub_array(6 * tmp[ch], 6).const_cast_pointer();

      pack_attribute(iter->edge(), dst_attrib);

      dst_index[0] = 4 * tmp[ch] + 0;
      dst_index[1] = 4 * tmp[ch] + 1;
      dst_index[2] = 4 * tmp[ch] + 2;
      dst_index[3] = 4 * tmp[ch] + 1;
      dst_index[4] = 4 * tmp[ch] + 2;
      dst_index[5] = 4 * tmp[ch] + 3;
      ++tmp[ch];
    }
}


void
EdgeAttributeDataFiller::
pack_attribute(const Edge &edge,
               fastuidraw::c_array<fastuidraw::PainterAttribute> dst) const
{
  assert(dst.size() == 4);
  assert(edge[0] < m_pts.size());
  assert(edge[1] < m_pts.size());
}

////////////////////////////////////
// AttributeDataFiller methods
void
AttributeDataFiller::
compute_sizes(unsigned int &number_attributes,
              unsigned int &number_indices,
              unsigned int &number_attribute_chunks,
              unsigned int &number_index_chunks,
              unsigned int &number_z_increments) const
{
  using namespace fastuidraw;

  number_z_increments = 0;
  if(m_per_fill.empty())
    {
      number_attributes = 0;
      number_indices = 0;
      number_attribute_chunks = 0;
      number_index_chunks = 0;
      return;
    }
  number_attributes = m_points.size();
  number_attribute_chunks = 1;

  number_indices = m_odd_winding_indices.size()
    + m_nonzero_winding_indices.size()
    + m_even_winding_indices.size()
    + m_zero_winding_indices.size();

  for(std::map<int, const_c_array<unsigned int> >::const_iterator
        iter = m_per_fill.begin(), end = m_per_fill.end();
      iter != end; ++iter)
    {
      if(iter->first != 0) //winding number 0 is by complement_nonzero_fill_rule
        {
          number_indices += iter->second.size();
        }
    }

  /* now get how big the index_chunks really needs to be
   */
  int smallest_winding(m_per_fill.begin()->first);
  int largest_winding(m_per_fill.rbegin()->first);
  unsigned int largest_winding_idx(FilledPath::Subset::chunk_from_winding_number(largest_winding));
  unsigned int smallest_winding_idx(FilledPath::Subset::chunk_from_winding_number(smallest_winding));
  number_index_chunks = 1 + std::max(largest_winding_idx, smallest_winding_idx);
}

void
AttributeDataFiller::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attrib_chunks,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<unsigned int> zincrements,
          fastuidraw::c_array<int> index_adjusts) const
{
  using namespace fastuidraw;

  if(m_per_fill.empty())
    {
      return;
    }
  assert(attributes.size() == m_points.size());
  assert(attrib_chunks.size() == 1);
  assert(zincrements.empty());
  FASTUIDRAWunused(zincrements);

  /* generate attribute data
   */
  std::transform(m_points.begin(), m_points.end(), attributes.begin(),
                 AttributeDataFiller::generate_attribute);
  attrib_chunks[0] = attributes;
  std::fill(index_adjusts.begin(), index_adjusts.end(), 0);

  unsigned int current(0);

#define GRAB_MACRO(enum_name, member_name) do {                     \
    c_array<PainterIndex> dst;                                      \
    dst = index_data.sub_array(current, member_name.size());        \
    std::copy(member_name.begin(),                                  \
              member_name.end(), dst.begin());                      \
    index_chunks[PainterEnums::enum_name] = dst;                    \
    current += dst.size();                                          \
  } while(0)

  GRAB_MACRO(odd_even_fill_rule, m_odd_winding_indices);
  GRAB_MACRO(nonzero_fill_rule, m_nonzero_winding_indices);
  GRAB_MACRO(complement_odd_even_fill_rule, m_even_winding_indices);
  GRAB_MACRO(complement_nonzero_fill_rule, m_zero_winding_indices);

#undef GRAB_MACRO

  for(std::map<int, const_c_array<unsigned int> >::const_iterator
        iter = m_per_fill.begin(), end = m_per_fill.end();
      iter != end; ++iter)
    {
      if(iter->first != 0) //winding number 0 is by complement_nonzero_fill_rule
        {
          c_array<PainterIndex> dst;
          const_c_array<unsigned int> src;
          unsigned int idx;

          idx = FilledPath::Subset::chunk_from_winding_number(iter->first);

          src = iter->second;
          dst = index_data.sub_array(current, src.size());
          assert(dst.size() == src.size());

          std::copy(src.begin(), src.end(), dst.begin());

          index_chunks[idx] = dst;
          current += dst.size();
        }
    }
}

/////////////////////////////////
// SubsetPrivate methods
SubsetPrivate::
SubsetPrivate(SubPath *Q, int max_recursion,
              std::vector<SubsetPrivate*> &out_values):
  m_ID(out_values.size()),
  m_bounds(Q->bounds()),
  m_painter_data(NULL),
  m_fuzz_painter_data(NULL),
  m_sizes_ready(false),
  m_sub_path(Q),
  m_children(NULL, NULL),
  m_splitting_coordinate(-1),
  m_have_prev_neighbor(false, false),
  m_have_next_neighbor(false, false)
{
  out_values.push_back(this);
  if(max_recursion > 0 && m_sub_path->total_points() > SubsetConstants::points_per_subset)
    {
      fastuidraw::vecN<SubPath*, 2> C;

      C = Q->split(m_splitting_coordinate);
      if(C[0]->total_points() < m_sub_path->total_points() || C[1]->total_points() < m_sub_path->total_points())
        {
          m_children[0] = FASTUIDRAWnew SubsetPrivate(C[0], max_recursion - 1, out_values);
          m_children[1] = FASTUIDRAWnew SubsetPrivate(C[1], max_recursion - 1, out_values);
          FASTUIDRAWdelete(m_sub_path);
          m_sub_path = NULL;
        }
      else
        {
          FASTUIDRAWdelete(C[0]);
          FASTUIDRAWdelete(C[1]);
        }
    }
}

SubsetPrivate::
~SubsetPrivate(void)
{
  if(m_sub_path != NULL)
    {
      assert(m_painter_data == NULL);
      assert(m_fuzz_painter_data == NULL);
      assert(m_children[0] == NULL);
      assert(m_children[1] == NULL);
      FASTUIDRAWdelete(m_sub_path);
    }

  if(m_painter_data != NULL)
    {
      assert(m_sub_path == NULL);
      assert(m_fuzz_painter_data != NULL);
      FASTUIDRAWdelete(m_painter_data);
      FASTUIDRAWdelete(m_fuzz_painter_data);
    }

  if(m_children[0] != NULL)
    {
      assert(m_sub_path == NULL);
      assert(m_children[1] != NULL);
      FASTUIDRAWdelete(m_children[0]);
      FASTUIDRAWdelete(m_children[1]);
    }
}

SubsetPrivate*
SubsetPrivate::
create_root_subset(SubPath *P, std::vector<SubsetPrivate*> &out_values)
{
  SubsetPrivate *root;
  root = FASTUIDRAWnew SubsetPrivate(P, SubsetConstants::recursion_depth, out_values);

  /* assign m_have_neighbor_prev and m_have_neighbor_next
   */
  if(root->m_splitting_coordinate != -1)
    {
      assert(root->m_children[0] != NULL);
      assert(root->m_children[1] != NULL);
      root->m_children[0]->assign_neighbor_values(root, 0);
      root->m_children[1]->assign_neighbor_values(root, 1);
    }

  return root;
}

void
SubsetPrivate::
assign_neighbor_values(SubsetPrivate *parent, int child_id)
{
  assert(parent != NULL);
  assert(parent->m_splitting_coordinate == 0 || parent->m_splitting_coordinate == 1);
  assert(child_id == 0 || child_id == 1);
  assert(parent->m_children[child_id] == this);

  m_have_prev_neighbor = parent->m_have_prev_neighbor;
  m_have_next_neighbor = parent->m_have_next_neighbor;
  if(child_id == 0)
    {
      m_have_next_neighbor[parent->m_splitting_coordinate] = true;
    }
  else
    {
      m_have_prev_neighbor[parent->m_splitting_coordinate] = true;
    }
}

unsigned int
SubsetPrivate::
select_subsets(ScratchSpacePrivate &scratch,
               fastuidraw::const_c_array<fastuidraw::vec3> clip_equations,
               const fastuidraw::float3x3 &clip_matrix_local,
               unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
               fastuidraw::c_array<unsigned int> dst)
{
  unsigned int return_value(0u);

  scratch.m_adjusted_clip_eqs.resize(clip_equations.size());
  for(unsigned int i = 0; i < clip_equations.size(); ++i)
    {
      /* transform clip equations from clip coordinates to
         local coordinates.
       */
      scratch.m_adjusted_clip_eqs[i] = clip_equations[i] * clip_matrix_local;
    }

  select_subsets_implement(scratch, dst, max_attribute_cnt, max_index_cnt, return_value);
  return return_value;
}

void
SubsetPrivate::
select_subsets_implement(ScratchSpacePrivate &scratch,
                         fastuidraw::c_array<unsigned int> dst,
                         unsigned int max_attribute_cnt,
                         unsigned int max_index_cnt,
                         unsigned int &current)
{
  using namespace fastuidraw;
  using namespace fastuidraw::detail;

  vecN<vec2, 4> bb;
  bool unclipped;

  m_bounds.inflated_polygon(bb, 0.0f);
  unclipped = clip_against_planes(make_c_array(scratch.m_adjusted_clip_eqs),
                                  bb, scratch.m_clipped_rect,
                                  scratch.m_clip_scratch_floats,
                                  scratch.m_clip_scratch_vec2s);

  //completely clipped
  if(scratch.m_clipped_rect.empty())
    {
      return;
    }

  //completely unclipped or no children
  assert((m_children[0] == NULL) == (m_children[1] == NULL));
  if(unclipped || m_children[0] == NULL)
    {
      select_subsets_all_unculled(dst, max_attribute_cnt, max_index_cnt, current);
      return;
    }

  m_children[0]->select_subsets_implement(scratch, dst, max_attribute_cnt, max_index_cnt, current);
  m_children[1]->select_subsets_implement(scratch, dst, max_attribute_cnt, max_index_cnt, current);
}

void
SubsetPrivate::
select_subsets_all_unculled(fastuidraw::c_array<unsigned int> dst,
                            unsigned int max_attribute_cnt,
                            unsigned int max_index_cnt,
                            unsigned int &current)
{
  if(!m_sizes_ready && m_children[0] == NULL && m_sub_path != NULL)
    {
      /* we are going to need the attributes because
         the element will be selected.
       */
      make_ready_from_sub_path();
      assert(m_painter_data != NULL);
    }

  if(m_sizes_ready
     && m_num_attributes <= max_attribute_cnt
     && m_largest_index_block <= max_index_cnt
     && 4 * m_aa_edge_list_counter.largest_edge_count() <= max_attribute_cnt
     && 6 * m_aa_edge_list_counter.largest_edge_count() <= max_index_cnt)
    {
      dst[current] = m_ID;
      ++current;
    }
  else if(m_children[0] != NULL)
    {
      m_children[0]->select_subsets_all_unculled(dst, max_attribute_cnt, max_index_cnt, current);
      m_children[1]->select_subsets_all_unculled(dst, max_attribute_cnt, max_index_cnt, current);
      if(!m_sizes_ready)
        {
          m_sizes_ready = true;
          assert(m_children[0]->m_sizes_ready);
          assert(m_children[1]->m_sizes_ready);
          m_num_attributes = m_children[0]->m_num_attributes + m_children[1]->m_num_attributes;
          /* TODO: the actual value for m_largest_index_block might be smaller;
             this happens if the largest index block of m_children[0] and m_children[1]
             come from different index sets.
          */
          m_largest_index_block = m_children[0]->m_largest_index_block + m_children[1]->m_largest_index_block;
          m_aa_edge_list_counter.add_counts(m_children[0]->m_aa_edge_list_counter);
          m_aa_edge_list_counter.add_counts(m_children[1]->m_aa_edge_list_counter);
        }
    }
  else
    {
      assert(m_sizes_ready);
      assert(!"Childless FilledPath::Subset has too many attributes or indices");
    }
}

void
SubsetPrivate::
make_ready(void)
{
  if(m_painter_data == NULL)
    {
      if(m_sub_path != NULL)
        {
          make_ready_from_sub_path();
        }
      else
        {
          make_ready_from_children();
        }
    }
}

void
SubsetPrivate::
make_ready_from_children(void)
{
  assert(m_children[0] != NULL);
  assert(m_children[1] != NULL);
  assert(m_sub_path == NULL);
  assert(m_painter_data == NULL);

  m_children[0]->make_ready();
  m_children[1]->make_ready();

  AttributeDataMerger merger(m_children[0]->painter_data(),
                             m_children[1]->painter_data(),
                             true);
  std::set<int> wnd;

  m_painter_data = FASTUIDRAWnew fastuidraw::PainterAttributeData();
  m_painter_data->set_data(merger);

  std::copy(m_children[0]->winding_numbers().begin(),
            m_children[0]->winding_numbers().end(),
            std::inserter(wnd, wnd.begin()));
  std::copy(m_children[1]->winding_numbers().begin(),
            m_children[1]->winding_numbers().end(),
            std::inserter(wnd, wnd.begin()));
  m_winding_numbers.resize(wnd.size());
  std::copy(wnd.begin(), wnd.end(), m_winding_numbers.begin());

  if(!m_sizes_ready)
    {
      m_sizes_ready = true;
      assert(m_children[0]->m_sizes_ready);
      assert(m_children[1]->m_sizes_ready);
      m_num_attributes = m_children[0]->m_num_attributes + m_children[1]->m_num_attributes;
      /* TODO: the actual value for m_largest_index_block might be smaller;
         this happens if the largest index block of m_children[0] and m_children[1]
         come from different index sets.
       */
      m_largest_index_block = m_children[0]->m_largest_index_block + m_children[1]->m_largest_index_block;
      m_aa_edge_list_counter.add_counts(m_children[0]->m_aa_edge_list_counter);
      m_aa_edge_list_counter.add_counts(m_children[1]->m_aa_edge_list_counter);
    }

  m_fuzz_painter_data = FASTUIDRAWnew fastuidraw::PainterAttributeData();
  AttributeDataMerger fuzz_merger(m_children[0]->fuzz_painter_data(),
                                  m_children[1]->fuzz_painter_data(),
                                  false);
  m_fuzz_painter_data->set_data(fuzz_merger);
}

void
SubsetPrivate::
make_ready_from_sub_path(void)
{
  assert(m_children[0] == NULL);
  assert(m_children[1] == NULL);
  assert(m_sub_path != NULL);
  assert(m_painter_data == NULL);
  assert(!m_sizes_ready);

  AttributeDataFiller filler;
  std::vector<AAEdge> aa_edges;
  AAEdgeList edge_list(&m_aa_edge_list_counter, &aa_edges);
  BoundaryEdgeTracker tr(&filler.m_points);
  builder B(*m_sub_path, filler.m_points,
            m_have_prev_neighbor, m_have_next_neighbor,
            tr);
  unsigned int even_non_zero_start, zero_start;
  unsigned int m1, m2;

  B.fill_indices(filler.m_indices, filler.m_per_fill, even_non_zero_start, zero_start);
  tr.create_aa_edges(edge_list);

  fastuidraw::const_c_array<unsigned int> indices_ptr;
  indices_ptr = fastuidraw::make_c_array(filler.m_indices);
  filler.m_nonzero_winding_indices = indices_ptr.sub_array(0, zero_start);
  filler.m_odd_winding_indices = indices_ptr.sub_array(0, even_non_zero_start);
  filler.m_even_winding_indices = indices_ptr.sub_array(even_non_zero_start);
  filler.m_zero_winding_indices = indices_ptr.sub_array(zero_start);

  m_sizes_ready = true;
  m1 = fastuidraw::t_max(filler.m_nonzero_winding_indices.size(),
                         filler.m_zero_winding_indices.size());
  m2 = fastuidraw::t_max(filler.m_odd_winding_indices.size(),
                         filler.m_even_winding_indices.size());
  m_largest_index_block = fastuidraw::t_max(m1, m2);
  m_num_attributes = filler.m_points.size();

  m_winding_numbers.reserve(filler.m_per_fill.size());
  for(std::map<int, fastuidraw::const_c_array<unsigned int> >::iterator
        iter = filler.m_per_fill.begin(), end = filler.m_per_fill.end();
      iter != end; ++iter)
    {
      assert(!iter->second.empty());
      m_winding_numbers.push_back(iter->first);
    }

  /* now fill m_painter_data.
   */
  m_painter_data = FASTUIDRAWnew fastuidraw::PainterAttributeData();
  m_painter_data->set_data(filler);

  /* fill m_fuzz_painter_data
   */
  EdgeAttributeDataFiller edge_filler(m_winding_numbers.front(),
                                      m_winding_numbers.back(),
                                      &filler.m_points,
                                      &aa_edges);
  m_fuzz_painter_data = FASTUIDRAWnew fastuidraw::PainterAttributeData();
  m_fuzz_painter_data->set_data(edge_filler);

  FASTUIDRAWdelete(m_sub_path);
  m_sub_path = NULL;

  #ifdef FASTUIDRAW_DEBUG
    {
      if(B.triangulation_failed())
        {
          /* On debug builds, print a warning.
           */
          std::cerr << "[" << __FILE__ << ", " << __LINE__
                    << "] Triangulation failed on tessellated path "
                    << this << "\n";
        }
    }
  #endif

}

/////////////////////////////////
// FilledPathPrivate methods
FilledPathPrivate::
FilledPathPrivate(const fastuidraw::TessellatedPath &P)
{
  SubPath *q;
  q = FASTUIDRAWnew SubPath(P);
  m_root = SubsetPrivate::create_root_subset(q, m_subsets);
}

FilledPathPrivate::
~FilledPathPrivate()
{
  FASTUIDRAWdelete(m_root);
}

///////////////////////////////
//fastuidraw::FilledPath::ScratchSpace methods
fastuidraw::FilledPath::ScratchSpace::
ScratchSpace(void)
{
  m_d = FASTUIDRAWnew ScratchSpacePrivate();
}

fastuidraw::FilledPath::ScratchSpace::
~ScratchSpace(void)
{
  ScratchSpacePrivate *d;
  d = static_cast<ScratchSpacePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

/////////////////////////////////
// fastuidraw::FilledPath::Subset methods
fastuidraw::FilledPath::Subset::
Subset(void *d):
  m_d(d)
{
}

const fastuidraw::PainterAttributeData&
fastuidraw::FilledPath::Subset::
painter_data(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  return d->painter_data();
}

fastuidraw::const_c_array<int>
fastuidraw::FilledPath::Subset::
winding_numbers(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  return d->winding_numbers();
}

unsigned int
fastuidraw::FilledPath::Subset::
chunk_from_winding_number(int winding_number)
{
  /* basic idea:
     - start counting at fill_rule_data_count
     - ordering is: 1, -1, 2, -2, ...
  */
  int value, sg;

  if(winding_number == 0)
    {
      return fastuidraw::PainterEnums::complement_nonzero_fill_rule;
    }

  value = std::abs(winding_number);
  sg = (winding_number < 0) ? 1 : 0;
  return fastuidraw::PainterEnums::fill_rule_data_count + sg + 2 * (value - 1);
}

unsigned int
fastuidraw::FilledPath::Subset::
chunk_from_fill_rule(enum PainterEnums::fill_rule_t fill_rule)
{
  assert(fill_rule < fastuidraw::PainterEnums::fill_rule_data_count);
  return fill_rule;
}

unsigned int
fastuidraw::FilledPath::Subset::
chunk_for_aa_fuzz(int winding0, int winding1)
{
  unsigned int w0, w1;
  w0 = SubsetPrivate::signed_to_unsigned(winding0);
  w1 = SubsetPrivate::signed_to_unsigned(winding1);
  return SubsetPrivate::unique_combine(w0, w1);
}

///////////////////////////////////////
// fastuidraw::FilledPath methods
fastuidraw::FilledPath::
FilledPath(const TessellatedPath &P)
{
  m_d = FASTUIDRAWnew FilledPathPrivate(P);
}

fastuidraw::FilledPath::
~FilledPath()
{
  FilledPathPrivate *d;
  d = static_cast<FilledPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

unsigned int
fastuidraw::FilledPath::
number_subsets(void) const
{
  FilledPathPrivate *d;
  d = static_cast<FilledPathPrivate*>(m_d);
  return d->m_subsets.size();
}


fastuidraw::FilledPath::Subset
fastuidraw::FilledPath::
subset(unsigned int I) const
{
  FilledPathPrivate *d;
  SubsetPrivate *p;

  d = static_cast<FilledPathPrivate*>(m_d);
  assert(I < d->m_subsets.size());
  p = d->m_subsets[I];
  p->make_ready();

  return Subset(p);
}

unsigned int
fastuidraw::FilledPath::
select_subsets(ScratchSpace &work_room,
               const_c_array<vec3> clip_equations,
               const float3x3 &clip_matrix_local,
               unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
               c_array<unsigned int> dst) const
{
  FilledPathPrivate *d;
  unsigned int return_value;

  d = static_cast<FilledPathPrivate*>(m_d);
  assert(dst.size() >= d->m_subsets.size());
  /* TODO:
       - have another method in SubsetPrivate called
         "fast_select_subsets" which ignores the requirements
         coming from max_attribute_cnt and max_index_cnt.
         By ignoring this requirement, we do NOT need
         to do call make_ready() for any SubsetPrivate
         object chosen.
       - have the fast_select_subsets() also return
         if paths needed require triangulation.
       - if there such, spawn a thread and let the
         caller decide if to wait for the thread to
         finish before proceeding or to do something
         else (like use a lower level of detail that
         is ready). Another alternatic is to return
         what Subset's need to have triangulation done
         and spawn a set of threads to do the job (!)
       - All this work means we need to make SubsetPrivate
         thread safe (with regards to the SubsetPrivate
         being made ready via make_ready()).
   */
  return_value= d->m_root->select_subsets(*static_cast<ScratchSpacePrivate*>(work_room.m_d),
                                          clip_equations, clip_matrix_local,
                                          max_attribute_cnt, max_index_cnt, dst);

  return return_value;
}
