/*!
 * \file stroked_path.cpp
 * \brief file stroked_path.cpp
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
#include <complex>

#include <fastuidraw/stroked_path.hpp>
#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler_path_stroked.hpp>
#include "private/util_private.hpp"
#include "private/path_util_private.hpp"

namespace
{
  uint32_t
  pack_data(int on_boundary,
            enum fastuidraw::StrokedPath::offset_type_t pt,
            uint32_t depth = 0u)
  {
    assert(on_boundary == 0 || on_boundary == 1);

    uint32_t bb(on_boundary), pp(pt);
    return fastuidraw::pack_bits(fastuidraw::StrokedPath::offset_type_bit0, fastuidraw::StrokedPath::offset_type_num_bits, pp)
      | fastuidraw::pack_bits(fastuidraw::StrokedPath::boundary_bit, 1u, bb)
      | fastuidraw::pack_bits(fastuidraw::StrokedPath::depth_bit0, fastuidraw::StrokedPath::depth_num_bits, depth);
  }

  void
  assign_depth(fastuidraw::StrokedPath::point &pt, uint32_t depth)
  {
    uint32_t dd;
    dd = fastuidraw::pack_bits(fastuidraw::StrokedPath::depth_bit0, fastuidraw::StrokedPath::depth_num_bits, depth);
    pt.m_packed_data &= ~FASTUIDRAW_MASK(fastuidraw::StrokedPath::depth_bit0, fastuidraw::StrokedPath::depth_num_bits);
    pt.m_packed_data |= dd;
  }

  void
  add_triangle_fan(unsigned int begin, unsigned int end,
                   fastuidraw::c_array<unsigned int> indices,
                   unsigned int &index_offset)
  {
    for(unsigned int i = begin + 1; i < end - 1; ++i, index_offset += 3)
      {
        indices[index_offset + 0] = begin;
        indices[index_offset + 1] = i;
        indices[index_offset + 2] = i + 1;
      }
  }

  class Location
  {
  public:
    Location(void):
      m_attribs(0, 0),
      m_indices(0, 0)
    {}
    fastuidraw::range_type<unsigned int> m_attribs, m_indices;
  };

  class PointIndexSize
  {
  public:
    PointIndexSize(void):
      m_data(0, 0, 0, 0)
    {}

    unsigned int
    pre_close_verts(void) const { return m_data[0]; }

    unsigned int&
    pre_close_verts(void) { return m_data[0]; }

    unsigned int
    close_verts(void) const { return m_data[1]; }

    unsigned int&
    close_verts(void) { return m_data[1]; }

    unsigned int
    pre_close_indices(void) const { return m_data[2]; }

    unsigned int&
    pre_close_indices(void) { return m_data[2]; }

    unsigned int
    close_indices(void) const { return m_data[3]; }

    unsigned int&
    close_indices(void) { return m_data[3]; }


    fastuidraw::uvec4 m_data;
  };

  class PointIndexCapSize
  {
  public:
    PointIndexCapSize(void):
      m_data(0, 0)
    {}

    unsigned int
    verts(void) const { return m_data[0]; }

    unsigned int&
    verts(void) { return m_data[0]; }

    unsigned int
    indices(void) const { return m_data[1]; }

    unsigned int&
    indices(void) { return m_data[1]; }

    fastuidraw::uvec2 m_data;
  };

  class PerEdgeData
  {
  public:
    fastuidraw::vec2 m_begin_normal, m_end_normal;
    fastuidraw::TessellatedPath::point m_start_pt, m_end_pt;
  };

  class PerContourData
  {
  public:
    const PerEdgeData&
    edge_data(unsigned int E) const
    {
      return (E == m_edge_data_store.size()) ?
        m_edge_data_store[0]:
        m_edge_data_store[E];
    }

    PerEdgeData&
    write_edge_data(unsigned int E)
    {
      assert(E < m_edge_data_store.size());
      return m_edge_data_store[E];
    }

    fastuidraw::vec2 m_begin_cap_normal, m_end_cap_normal;
    fastuidraw::TessellatedPath::point m_start_contour_pt, m_end_contour_pt;
    std::vector<PerEdgeData> m_edge_data_store;
  };

  class PathData:fastuidraw::noncopyable
  {
  public:
    unsigned int
    number_contours(void) const
    {
      return m_per_contour_data.size();
    }

    unsigned int
    number_edges(unsigned int C) const
    {
      assert(C < m_per_contour_data.size());
      return m_per_contour_data[C].m_edge_data_store.size();
    }

    std::vector<PerContourData> m_per_contour_data;
  };

  class EdgeDataCreator
  {
  public:
    static const float sm_mag_tol;

    EdgeDataCreator(const fastuidraw::TessellatedPath &P,
                    PathData &path_data):
      m_P(P),
      m_path_data(path_data)
    {
      m_path_data.m_per_contour_data.resize(m_P.number_contours());
      compute_size();
    }

    fastuidraw::uvec4
    sizes(void) const
    {
      return m_size.m_data;
    }

    void
    fill_data(fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
              fastuidraw::c_array<unsigned int> indices,
              unsigned int &pre_close_depth, unsigned int &close_depth);

  private:
    enum
      {
        points_per_segment = 6,
        triangles_per_segment = points_per_segment - 2,
        indices_per_segment_without_bevel = 3 * triangles_per_segment,
      };

    void
    compute_size(void);

    void
    init_prev_normal(unsigned int contour);

    void
    add_edge(unsigned int contour, unsigned int edge,
             fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
             unsigned int &depth,
             fastuidraw::c_array<unsigned int> indices,
             unsigned int &vert_offset, unsigned int &index_offset);

    PointIndexSize m_size;
    const fastuidraw::TessellatedPath &m_P;
    PathData &m_path_data;
    fastuidraw::vec2 m_prev_normal;
    std::vector<PerContourData> m_per_contour_data;
  };

  class JoinCount
  {
  public:
    explicit
    JoinCount(const PathData &P);

    unsigned int m_number_close_joins;
    unsigned int m_number_non_close_joins;
  };

  class CommonJoinData
  {
  public:
    CommonJoinData(const fastuidraw::vec2 &p0,
                   const fastuidraw::vec2 &n0,
                   const fastuidraw::vec2 &p1,
                   const fastuidraw::vec2 &n1,
                   float distance_from_edge_start,
                   float distance_from_contour_start,
                   float edge_length,
                   float open_contour_length,
                   float closed_contour_length);

    float m_det, m_lambda;
    fastuidraw::vec2 m_p0, m_v0, m_n0;
    fastuidraw::vec2 m_p1, m_v1, m_n1;
    float m_distance_from_edge_start;
    float m_distance_from_contour_start;
    float m_edge_length;
    float m_open_contour_length;
    float m_closed_contour_length;

    static
    float
    compute_lambda(const fastuidraw::vec2 &n0, const fastuidraw::vec2 &n1);
  };

  class JoinCreatorBase
  {
  public:
    explicit
    JoinCreatorBase(const PathData &P):
      m_P(P),
      m_size_ready(false)
    {}

    virtual
    ~JoinCreatorBase() {}

    fastuidraw::uvec4
    sizes(void)
    {
      compute_size();
      return m_size.m_data;
    }

    void
    fill_data(fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
              fastuidraw::c_array<unsigned int> indices,
              unsigned int &pre_close_depth, unsigned int &close_depth,
              std::vector<std::vector<Location> > &locations);

  private:

    virtual
    void
    post_assign_depth(unsigned int &depth,
                      fastuidraw::c_array<fastuidraw::StrokedPath::point> pts);

    virtual
    void
    add_join(unsigned int join_id,
             const PathData &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count) = 0;


    void
    fill_join(unsigned int join_id,
              unsigned int contour, unsigned int edge,
              fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
              unsigned int &depth,
              fastuidraw::c_array<unsigned int> indices,
              unsigned int &vertex_offset, unsigned int &index_offset,
              Location &loc);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PathData &path,
                        unsigned int contour, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) = 0;

    void
    compute_size(void);

    const PathData &m_P;
    PointIndexSize m_size;
    bool m_size_ready;
  };


  class RoundedJoinCreator:public JoinCreatorBase
  {
  public:
    RoundedJoinCreator(const PathData &P, float thresh);

  private:

    class PerJoinData:public CommonJoinData
    {
    public:
      PerJoinData(const fastuidraw::TessellatedPath::point &p0,
                  const fastuidraw::TessellatedPath::point &p1,
                  const fastuidraw::vec2 &n0_from_stroking,
                  const fastuidraw::vec2 &n1_from_stroking,
                  float thresh);

      void
      add_data(fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
               unsigned int &vertex_offset,
               fastuidraw::c_array<unsigned int> indices,
               unsigned int &index_offset) const;

      std::complex<float> m_arc_start;
      float m_delta_theta;
      unsigned int m_num_arc_points;
    };

    virtual
    void
    add_join(unsigned int join_id,
             const PathData &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PathData &path,
                        unsigned int contour, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset);

    float m_thresh;
    std::vector<PerJoinData> m_per_join_data;
  };

  class BevelJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    BevelJoinCreator(const PathData &P);

  private:

    virtual
    void
    add_join(unsigned int join_id,
             const PathData &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PathData &path,
                        unsigned int contour, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset);

    std::vector<fastuidraw::vec2> m_n0, m_n1;
  };

  class CommonCapData
  {
  public:
    CommonCapData(bool is_start_cap,
                  const fastuidraw::vec2 &src_pt,
                  const fastuidraw::vec2 &normal_from_stroking):
      m_is_start_cap(is_start_cap),
      m_lambda((is_start_cap) ? -1.0f : 1.0f),
      m_p(src_pt),
      m_n(normal_from_stroking)
    {
      //caps at the start are on the "other side"
      m_v = fastuidraw::vec2(m_n.y(), -m_n.x());
      m_v *= m_lambda;
      m_n *= m_lambda;
    }

    bool m_is_start_cap;
    float m_lambda;
    fastuidraw::vec2 m_p, m_n, m_v;
  };

  class CapCreatorBase
  {
  public:
    CapCreatorBase(const PathData &P, PointIndexCapSize sz):
      m_P(P),
      m_size(sz)
    {}

    virtual
    ~CapCreatorBase()
    {}

    fastuidraw::uvec2
    sizes(void) const
    {
      return m_size.m_data;
    }

    unsigned int
    fill_data(fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
              fastuidraw::c_array<unsigned int> indices) const;

  private:

    virtual
    void
    add_cap(const fastuidraw::vec2 &normal_from_stroking,
            bool is_starting_cap,
            const fastuidraw::TessellatedPath::point &p0,
            fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const = 0;

    const PathData &m_P;
    PointIndexCapSize m_size;
  };

  class RoundedCapCreator:public CapCreatorBase
  {
  public:
    RoundedCapCreator(const PathData &P, float thresh):
      CapCreatorBase(P, compute_size(P, thresh))
    {
    }

  private:

    PointIndexCapSize
    compute_size(const PathData &P, float thresh);

    void
    add_cap(const fastuidraw::vec2 &normal_from_stroking,
            bool is_starting_cap,
            const fastuidraw::TessellatedPath::point &p0,
            fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const;

    float m_delta_theta;
    unsigned int m_num_arc_points_per_cap;
  };

  class SquareCapCreator:public CapCreatorBase
  {
  public:
    SquareCapCreator(const PathData &P):
      CapCreatorBase(P, compute_size(P))
    {
    }

  private:

    PointIndexCapSize
    compute_size(const PathData &P);

    void
    add_cap(const fastuidraw::vec2 &normal_from_stroking,
            bool is_starting_cap,
            const fastuidraw::TessellatedPath::point &p0,
            fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const;
  };

  class AdjustableCapCreator:public CapCreatorBase
  {
  public:
    AdjustableCapCreator(const PathData &P):
      CapCreatorBase(P, compute_size(P))
    {
    }

  private:
    enum
      {
        number_points_per_fan = 6,
        number_triangles_per_fan = number_points_per_fan - 2,
        number_indices_per_fan = 3 * number_triangles_per_fan,
        number_points_per_join = 2 * number_points_per_fan,
        number_indices_per_join = 2 * number_indices_per_fan
      };

    static
    void
    pack_fan(bool entering_contour,
             enum fastuidraw::StrokedPath::offset_type_t type,
             const fastuidraw::TessellatedPath::point &edge_pt,
             const fastuidraw::vec2 &stroking_normal,
             fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
             unsigned int &vertex_offset,
             fastuidraw::c_array<unsigned int> indices,
             unsigned int &index_offset);

    PointIndexCapSize
    compute_size(const PathData &P);

    void
    add_cap(const fastuidraw::vec2 &normal_from_stroking,
            bool is_starting_cap,
            const fastuidraw::TessellatedPath::point &p0,
            fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const;
  };

  class MiterJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    MiterJoinCreator(const PathData &P);

  private:
    virtual
    void
    add_join(unsigned int join_id,
             const PathData &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PathData &path,
                        unsigned int contour, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset);

    std::vector<fastuidraw::vec2> m_n0, m_n1;
  };

  template<typename T>
  class PartitionedArray
  {
  public:
    typedef typename std::vector<T>::size_type size_type;

    void
    resize(size_type number_elements_not_including_closing_edge,
           size_type number_elements_closing_edge,
           bool closing_edge_data_at_end)
    {
      m_data.resize(number_elements_not_including_closing_edge + number_elements_closing_edge);
      m_with_closing_edge = fastuidraw::make_c_array(m_data);
      if(closing_edge_data_at_end)
        {
          m_without_closing_edge = m_with_closing_edge.sub_array(0, number_elements_not_including_closing_edge);
        }
      else
        {
          m_without_closing_edge = m_with_closing_edge.sub_array(number_elements_closing_edge);
        }
    }

    fastuidraw::const_c_array<T>
    data(bool including_closing_edge) const
    {
      return including_closing_edge ?
        m_with_closing_edge :
        m_without_closing_edge;
    }

    fastuidraw::c_array<T>
    data(bool including_closing_edge)
    {
      return including_closing_edge ?
        m_with_closing_edge :
        m_without_closing_edge;
    }

  private:
    std::vector<T> m_data;
    fastuidraw::c_array<T> m_with_closing_edge;
    fastuidraw::c_array<T> m_without_closing_edge;
  };

  class DataAsCArrays
  {
  public:
    fastuidraw::const_c_array<fastuidraw::StrokedPath::point> m_points;
    fastuidraw::const_c_array<unsigned int> m_indices;
    unsigned int m_number_depth;
  };

  typedef fastuidraw::vecN<DataAsCArrays, 2> DataAsCArraysPair;

  template<typename T>
  class Data
  {
  public:
    PartitionedArray<T> m_points;
    PartitionedArray<unsigned int> m_indices;
    fastuidraw::vecN<unsigned int, 2> m_number_depth;

    /* szs[0] --> number of geometry points not including closing
       szs[1] --> number of geometry points for closing
       szs[0] --> number of indices not including closing
       szs[1] --> number of indices for closing

       Note that the points come at the end, but the
       indices come at the front.
     */
    void
    resize(fastuidraw::uvec4 szs)
    {
      m_points.resize(szs[0], szs[1], true);
      m_indices.resize(szs[2], szs[3], false);
    }

    void
    compute_conveniance(DataAsCArraysPair &out_value)
    {
      out_value[false].m_points = m_points.data(false);
      out_value[true].m_points  = m_points.data(true);

      out_value[false].m_indices = m_indices.data(false);
      out_value[true].m_indices  = m_indices.data(true);

      out_value[false].m_number_depth = m_number_depth[false];
      out_value[true].m_number_depth  = m_number_depth[true];
    }
  };

  class CapsPrivate
  {
  public:
    CapsPrivate(void):
      m_attribute_data(NULL)
    {}

    ~CapsPrivate()
    {
      if(m_attribute_data != NULL)
        {
          FASTUIDRAWdelete(m_attribute_data);
        }
    }

    void
    resize(fastuidraw::uvec2 szs)
    {
      m_points.resize(szs[0]);
      m_indices.resize(szs[1]);
    }

    std::vector<fastuidraw::StrokedPath::point> m_points;
    std::vector<unsigned int> m_indices;
    unsigned int m_number_depth;
    fastuidraw::PainterAttributeData *m_attribute_data;
  };

  class GenericDataPrivate
  {
  public:
    GenericDataPrivate(void);
    ~GenericDataPrivate();

    Data<fastuidraw::StrokedPath::point> m_data;
    DataAsCArraysPair m_return_values;
    fastuidraw::PainterAttributeData *m_attribute_data;
  };

  class JoinsPrivate:public GenericDataPrivate
  {
  public:
    void
    resize_locations(const PathData &P);

    std::vector<std::vector<Location> > m_locations;
  };

  template<typename T>
  class ThreshWith
  {
  public:
    ThreshWith(void):
      m_data(NULL),
      m_thresh(-1)
    {}

    ThreshWith(T *d, float t):
      m_data(d), m_thresh(t)
    {}

    static
    bool
    reverse_compare_against_thresh(const ThreshWith &lhs, float rhs)
    {
      return lhs.m_thresh > rhs;
    }

    T *m_data;
    float m_thresh;
  };

  typedef ThreshWith<fastuidraw::StrokedPath::Joins> ThreshWithJoins;
  typedef ThreshWith<fastuidraw::StrokedPath::Caps> ThreshWithCaps;

  class StrokedPathPrivate
  {
  public:
    explicit
    StrokedPathPrivate(void);
    ~StrokedPathPrivate();

    static
    void
    create_edges(PathData &e, const fastuidraw::TessellatedPath &P, void *out_edges);

    template<typename T>
    static
    void
    create_joins(const PathData &e, void *out_data);

    template<typename T>
    static
    void
    create_joins(const PathData &e, float thresh, void *out_data);

    template<typename T>
    static
    void
    create_caps(const PathData &e, void *out_data);

    template<typename T>
    static
    void
    create_caps(const PathData &e, float thresh, void *out_data);

    fastuidraw::StrokedPath::Edges m_edges;
    fastuidraw::StrokedPath::Joins m_bevel_joins, m_miter_joins;
    fastuidraw::StrokedPath::Caps m_square_caps, m_adjustable_caps;
    PathData m_path_data;

    std::vector<ThreshWithJoins> m_rounded_joins;
    std::vector<ThreshWithCaps> m_rounded_caps;

    float m_effective_curve_distance_threshhold;
  };

}


//////////////////////////////////////////////
// JoinCount methods
JoinCount::
JoinCount(const PathData &P):
  m_number_close_joins(0),
  m_number_non_close_joins(0)
{
  for(unsigned int o = 0; o < P.number_contours(); ++o)
    {
      if(P.number_edges(o) >= 2)
        {
          m_number_non_close_joins += P.number_edges(o) - 2;
          m_number_close_joins += 2;
        }
    }
}

////////////////////////////////////////////////
// CommonJoinData methods
CommonJoinData::
CommonJoinData(const fastuidraw::vec2 &p0,
               const fastuidraw::vec2 &n0,
               const fastuidraw::vec2 &p1,
               const fastuidraw::vec2 &n1,
               float distance_from_edge_start,
               float distance_from_contour_start,
               float edge_length,
               float open_contour_length,
               float closed_contour_length):
  m_distance_from_edge_start(distance_from_edge_start),
  m_distance_from_contour_start(distance_from_contour_start),
  m_edge_length(edge_length),
  m_open_contour_length(open_contour_length),
  m_closed_contour_length(closed_contour_length)
{
  /* Explanation:
      We have two curves, a(t) and b(t) with a(1) = b(0)
      The point p0 represents the end of a(t) and the
      point p1 represents the start of b(t).

      When stroking we have four auxilary curves:
        a0(t) = a(t) + w * a_n(t)
        a1(t) = a(t) - w * a_n(t)
        b0(t) = b(t) + w * b_n(t)
        b1(t) = b(t) - w * b_n(t)
      where
        w = width of stroking
        a_n(t) = J( a'(t) ) / || a'(t) ||
        b_n(t) = J( b'(t) ) / || b'(t) ||
      when
        J(x, y) = (-y, x).

      A Bevel join is a triangle that connects
      consists of p, A and B where p is a(1)=b(0),
      A is one of a0(1) or a1(1) and B is one
      of b0(0) or b1(0). Now if we use a0(1) for
      A then we will use b0(0) for B because
      the normals are generated the same way for
      a(t) and b(t). Then, the questions comes
      down to, do we wish to add or subtract the
      normal. That value is represented by m_lambda.

      Now to figure out m_lambda. Let q0 be a point
      on a(t) before p=a(1). The q0 is given by

        q0 = p - s * m_v0

      and let q1 be a point on b(t) after p=b(0),

        q1 = p + t * m_v1

      where both s, t are positive. Let

        z = (q0+q1) / 2

      the point z is then on the side of the join
      of the acute angle of the join.

      With this in mind, if either of <z-p, m_n0>
      or <z-p, m_n1> is positive then we want
      to add by -w * n rather than  w * n.

      Note that:

      <z-p, m_n1> = 0.5 * < -s * m_v0 + t * m_v1, m_n1 >
                  = -0.5 * s * <m_v0, m_n1> + 0.5 * t * <m_v1, m_n1>
                  = -0.5 * s * <m_v0, m_n1>
                  = -0.5 * s * <m_v0, J(m_v1) >

      and

      <z-p, m_n0> = 0.5 * < -s * m_v0 + t * m_v1, m_n0 >
                  = -0.5 * s * <m_v0, m_n0> + 0.5 * t * <m_v1, m_n0>
                  = 0.5 * t * <m_v1, m_n0>
                  = 0.5 * t * <m_v1, J(m_v0) >
                  = -0.5 * t * <J(m_v1), m_v0>

      (the last line because transpose(J) = -J). Notice
      that the sign of <z-p, m_n1> and the sign of <z-p, m_n0>
      is then the same.

      thus m_lambda is positive if <m_v1, m_n0> is negative.
   */
  m_p0 = p0;
  m_n0 = n0;
  m_v0 = fastuidraw::vec2(m_n0.y(), -m_n0.x());

  m_p1 = p1;
  m_n1 = n1;
  m_v1 = fastuidraw::vec2(m_n1.y(), -m_n1.x());

  m_det = fastuidraw::dot(m_v1, m_n0);
  if(m_det > 0.0f)
    {
      m_lambda = -1.0f;
    }
  else
    {
      m_lambda = 1.0f;
    }
}

float
CommonJoinData::
compute_lambda(const fastuidraw::vec2 &n0, const fastuidraw::vec2 &n1)
{
  fastuidraw::vec2 v1;
  float d;

  v1 = fastuidraw::vec2(n1.y(), -n1.x());
  d = fastuidraw::dot(v1, n0);
  if(d > 0.0f)
    {
      return -1.0f;
    }
  else
    {
      return 1.0f;
    }
}

///////////////////////////////////////////////////
// EdgeDataCreator methods
const float EdgeDataCreator::sm_mag_tol = 0.000001f;

void
EdgeDataCreator::
compute_size(void)
{
  for(unsigned int o = 0; o < m_P.number_contours(); ++o)
    {
      m_path_data.m_per_contour_data[o].m_edge_data_store.resize(m_P.number_edges(o));
      m_path_data.m_per_contour_data[o].m_start_contour_pt = m_P.unclosed_contour_point_data(o).front();
      m_path_data.m_per_contour_data[o].m_end_contour_pt = m_P.unclosed_contour_point_data(o).back();
      for(unsigned int e = 0; e < m_P.number_edges(o); ++e)
        {
          fastuidraw::range_type<unsigned int> R;
          unsigned int number_segments, number_verts, number_indices;

          R = m_P.edge_range(o, e);
          assert(R.m_end > R.m_begin);

          /* Each sub-edge induces points_per_segment vertices
             and indices_per_segment_without_bevel indices.
             In addition, between each sub-edge, there is a
             bevel-join triangle that adds 3 more indices
             and 3 more vertex
           */
          number_segments = R.m_end - R.m_begin - 1;
          number_verts = points_per_segment * number_segments;
          number_indices = indices_per_segment_without_bevel * number_segments;
          if(number_segments >= 1)
            {
              number_verts += 3 * (number_segments - 1);
              number_indices += 3 * (number_segments - 1);
            }
          if(e + 1 == m_P.number_edges(o))
            {
              m_size.close_verts() += number_verts;
              m_size.close_indices() += number_indices;
            }
          else
            {
              m_size.pre_close_verts() += number_verts;
              m_size.pre_close_indices() += number_indices;
            }
        }
    }
}

void
EdgeDataCreator::
add_edge(unsigned int o, unsigned int e,
         fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
         unsigned int &depth,
         fastuidraw::c_array<unsigned int> indices,
         unsigned int &vert_offset, unsigned int &index_offset)
{
  fastuidraw::range_type<unsigned int> R;
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(m_P.point_data());
  fastuidraw::vec2 normal(1.0f, 0.0f), last_normal(1.0f, 0.0f);
  unsigned int last_edge_middle, last_edge_positive_normal, last_edge_negative_normal;

  R = m_P.edge_range(o, e);
  assert(R.m_end > R.m_begin);

  for(unsigned int i = R.m_begin; i + 1 < R.m_end; ++i, ++depth)
    {
      /* for the edge connecting src_pts[i] to src_pts[i+1]
       */
      fastuidraw::vec2 delta;
      float delta_magnitude;

      delta = src_pts[i+1].m_p - src_pts[i].m_p;
      delta_magnitude = delta.magnitude();
      if(delta.magnitude() >= sm_mag_tol)
        {
          normal = fastuidraw::vec2(-delta.y(), delta.x()) / delta_magnitude;
        }
      else
        {
          delta_magnitude = 0.0;
          if(src_pts[i].m_p_t.magnitudeSq() >= sm_mag_tol * sm_mag_tol)
            {
              normal = fastuidraw::vec2(-src_pts[i].m_p_t.y(), src_pts[i].m_p_t.x());
              normal.normalize();
            }
        }

      if(i == R.m_begin)
        {
          m_path_data.m_per_contour_data[o].write_edge_data(e).m_begin_normal = normal;
          m_path_data.m_per_contour_data[o].write_edge_data(e).m_start_pt = src_pts[i];
          if(e == 0)
            {
              m_path_data.m_per_contour_data[o].m_begin_cap_normal = normal;
            }
        }
      else
        {
          float lambda;

          /*! add a join from the last sub-edge to this sub-edge
           */
          lambda = CommonJoinData::compute_lambda(last_normal, normal);

          indices[index_offset + 0] = vert_offset + 0;
          indices[index_offset + 1] = vert_offset + 1;
          indices[index_offset + 2] = vert_offset + 2;
          index_offset += 3;

          pts[vert_offset + 0] = pts[last_edge_middle];
          if(lambda < 0.0f)
            {
              pts[vert_offset + 1] = pts[last_edge_negative_normal];
            }
          else
            {
              pts[vert_offset + 1] = pts[last_edge_positive_normal];
            }
          pts[vert_offset + 2].m_position = src_pts[i].m_p;
          pts[vert_offset + 2].m_distance_from_edge_start = src_pts[i].m_distance_from_edge_start;
          pts[vert_offset + 2].m_distance_from_contour_start = src_pts[i].m_distance_from_contour_start;
          pts[vert_offset + 2].m_edge_length = src_pts[i].m_edge_length;
          pts[vert_offset + 2].m_open_contour_length = src_pts[i].m_open_contour_length;
          pts[vert_offset + 2].m_closed_contour_length = src_pts[i].m_closed_contour_length;
          pts[vert_offset + 2].m_pre_offset = lambda * normal;
          pts[vert_offset + 2].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_start_sub_edge, depth);

          for(unsigned int k = 0; k < 3; ++k)
            {
              pts[vert_offset + k].m_packed_data |= fastuidraw::StrokedPath::bevel_edge_mask;
            }
          vert_offset += 3;
        }

      int boundary_values[3] =
        {
          1,
          1,
          0,
        };

      float normal_sign[3] =
        {
          1.0f,
          -1.0f,
          0.0f,
        };

      /* The quad is:
         (p, n, delta,  1),
         (p,-n, delta,  1),
         (p, 0,     0,  0),
         (p_next,  n, -delta, 1),
         (p_next, -n, -delta, 1),
         (p_next,  0, 0)

         Notice that we are encoding if it is
         start or end of edge from the sign of
         m_on_boundary.
       */
      for(unsigned int k = 0; k < 3; ++k)
        {
          pts[vert_offset + k].m_position = src_pts[i].m_p;
          pts[vert_offset + k].m_distance_from_edge_start = src_pts[i].m_distance_from_edge_start;
          pts[vert_offset + k].m_distance_from_contour_start = src_pts[i].m_distance_from_contour_start;
          pts[vert_offset + k].m_edge_length = src_pts[i].m_edge_length;
          pts[vert_offset + k].m_open_contour_length = src_pts[i].m_open_contour_length;
          pts[vert_offset + k].m_closed_contour_length = src_pts[i].m_closed_contour_length;
          pts[vert_offset + k].m_pre_offset = normal_sign[k] * normal;
          pts[vert_offset + k].m_auxilary_offset = delta;
          pts[vert_offset + k].m_packed_data = pack_data(boundary_values[k],
                                                         fastuidraw::StrokedPath::offset_start_sub_edge,
                                                         depth);

          pts[vert_offset + k + 3].m_position = src_pts[i + 1].m_p;
          pts[vert_offset + k + 3].m_distance_from_edge_start = src_pts[i + 1].m_distance_from_edge_start;
          pts[vert_offset + k + 3].m_distance_from_contour_start = src_pts[i + 1].m_distance_from_contour_start;
          pts[vert_offset + k + 3].m_edge_length = src_pts[i + 1].m_edge_length;
          pts[vert_offset + k + 3].m_open_contour_length = src_pts[i + 1].m_open_contour_length;
          pts[vert_offset + k + 3].m_closed_contour_length = src_pts[i + 1].m_closed_contour_length;
          pts[vert_offset + k + 3].m_pre_offset = normal_sign[k] * normal;
          pts[vert_offset + k + 3].m_auxilary_offset = -delta;
          pts[vert_offset + k + 3].m_packed_data = pack_data(boundary_values[k],
                                                             fastuidraw::StrokedPath::offset_end_sub_edge,
                                                             depth);
        }

      last_normal = normal;
      last_edge_middle = vert_offset + 5;
      last_edge_positive_normal = vert_offset + 3;
      last_edge_negative_normal = vert_offset + 4;

      indices[index_offset + 0] = vert_offset + 0;
      indices[index_offset + 1] = vert_offset + 2;
      indices[index_offset + 2] = vert_offset + 5;
      indices[index_offset + 3] = vert_offset + 0;
      indices[index_offset + 4] = vert_offset + 5;
      indices[index_offset + 5] = vert_offset + 3;

      indices[index_offset + 6] = vert_offset + 2;
      indices[index_offset + 7] = vert_offset + 1;
      indices[index_offset + 8] = vert_offset + 4;
      indices[index_offset + 9] = vert_offset + 2;
      indices[index_offset + 10] = vert_offset + 4;
      indices[index_offset + 11] = vert_offset + 5;

      index_offset += indices_per_segment_without_bevel;
      vert_offset += points_per_segment;
    }

  if(R.m_begin + 1 >= R.m_end)
    {
      normal = fastuidraw::vec2(-src_pts[R.m_begin].m_p_t.y(), src_pts[R.m_begin].m_p_t.x());
      normal.normalize();
      m_path_data.m_per_contour_data[o].write_edge_data(e).m_begin_normal = normal;
      m_path_data.m_per_contour_data[o].write_edge_data(e).m_start_pt = src_pts[R.m_begin];
      if(e == 0)
        {
          m_path_data.m_per_contour_data[o].m_begin_cap_normal = normal;
        }
    }

  m_path_data.m_per_contour_data[o].write_edge_data(e).m_end_normal = normal;
  m_path_data.m_per_contour_data[o].write_edge_data(e).m_end_pt = src_pts[R.m_end - 1];
  if(e + 2 == m_P.number_edges(o))
    {
      m_path_data.m_per_contour_data[o].m_end_cap_normal = normal;
    }
}


void
EdgeDataCreator::
fill_data(fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
          fastuidraw::c_array<unsigned int> indices,
          unsigned int &pre_close_depth, unsigned int &close_depth)
{
  unsigned int pre_close_vertex(0), pre_close_index(m_size.close_indices());
  unsigned int close_vertex(m_size.pre_close_verts()), close_index(0);
  unsigned int total;

  pre_close_depth = 0;
  close_depth = 0;

  for(unsigned int o = 0; o < m_P.number_contours(); ++o)
    {
      for(unsigned int e = 0; e < m_P.number_edges(o); ++e)
        {
          if(e + 1 == m_P.number_edges(o))
            {
              add_edge(o, e, pts, close_depth, indices,
                       close_vertex, close_index);
            }
          else
            {
              add_edge(o, e, pts, pre_close_depth, indices,
                       pre_close_vertex, pre_close_index);
            }
        }
    }

  assert(pre_close_vertex == m_size.pre_close_verts());
  assert(pre_close_index == m_size.close_indices() + m_size.pre_close_indices());
  assert(close_vertex == pre_close_vertex + m_size.close_verts());
  assert(close_index ==  m_size.close_indices());

  /* the vertices of the closing edge are drawn first,
     so they should have the largest depth value
   */
  total = close_depth + pre_close_depth;
  for(unsigned int v = m_size.pre_close_verts(), endv = pts.size(); v < endv; ++v)
    {
      assert(pts[v].depth() < close_depth);
      assign_depth(pts[v], total - pts[v].depth()  - 1);
    }

  for(unsigned int v = 0, endv = m_size.pre_close_verts(); v < endv; ++v)
    {
      assert(pts[v].depth() < pre_close_depth);
      assign_depth(pts[v], total - (pts[v].depth() + close_depth) - 1);
    }
  close_depth = total;
}

/////////////////////////////////////////////////
// JoinCreatorBase methods
void
JoinCreatorBase::
compute_size(void)
{
  if(!m_size_ready)
    {
      m_size_ready = true;
      for(unsigned int o = 0, join_id = 0; o < m_P.number_contours(); ++o)
        {
          for(unsigned int e = 1; e + 1 < m_P.number_edges(o); ++e, ++join_id)
            {
              add_join(join_id, m_P,
                       m_P.m_per_contour_data[o].edge_data(e - 1).m_end_normal,
                       m_P.m_per_contour_data[o].edge_data(e).m_begin_normal,
                       o, e, m_size.pre_close_verts(), m_size.pre_close_indices());
            }

          if(m_P.number_edges(o) >= 2)
            {
              add_join(join_id, m_P,
                       m_P.m_per_contour_data[o].edge_data(m_P.number_edges(o) - 2).m_end_normal,
                       m_P.m_per_contour_data[o].edge_data(m_P.number_edges(o) - 1).m_begin_normal,
                       o, m_P.number_edges(o) - 1,
                       m_size.close_verts(), m_size.close_indices());

              add_join(join_id + 1, m_P,
                       m_P.m_per_contour_data[o].m_edge_data_store.back().m_end_normal,
                       m_P.m_per_contour_data[o].m_edge_data_store.front().m_begin_normal,
                       o, m_P.number_edges(o),
                       m_size.close_verts(), m_size.close_indices());

              join_id += 2;
            }
        }
    }
}

void
JoinCreatorBase::
fill_join(unsigned int join_id,
          unsigned int contour, unsigned int edge,
          fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
          unsigned int &depth,
          fastuidraw::c_array<unsigned int> indices,
          unsigned int &vertex_offset, unsigned int &index_offset,
          Location &loc)
{
  unsigned int v(vertex_offset);

  loc.m_attribs.m_begin = vertex_offset;
  loc.m_indices.m_begin = index_offset;
  fill_join_implement(join_id, m_P, contour, edge, pts, indices, vertex_offset, index_offset);
  loc.m_attribs.m_end = vertex_offset;
  loc.m_indices.m_end = index_offset;

  post_assign_depth(depth, pts.sub_array(v, vertex_offset - v));
}

void
JoinCreatorBase::
post_assign_depth(unsigned int &depth,
                  fastuidraw::c_array<fastuidraw::StrokedPath::point> pts)
{
  for(unsigned int v = 0; v < pts.size(); ++v)
    {
      assign_depth(pts[v], depth);
      pts[v].m_packed_data |= fastuidraw::StrokedPath::join_mask;
    }
  ++depth;
}

void
JoinCreatorBase::
fill_data(fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
          fastuidraw::c_array<unsigned int> indices,
          unsigned int &pre_close_depth, unsigned int &close_depth,
          std::vector<std::vector<Location> > &locations)
{
  unsigned int pre_close_vertex(0), pre_close_index(m_size.close_indices());
  unsigned int close_vertex(m_size.pre_close_verts()), close_index(0);
  unsigned int total;

  pre_close_depth = 0;
  close_depth = 0;

  for(unsigned int o = 0, join_id = 0; o < m_P.number_contours(); ++o)
    {
      for(unsigned int e = 1; e + 1 < m_P.number_edges(o); ++e, ++join_id)
        {
          fill_join(join_id, o, e, pts, pre_close_depth, indices,
                    pre_close_vertex, pre_close_index,
                    locations[o][e - 1]);
        }

      if(m_P.number_edges(o) >= 2)
        {
          fill_join(join_id, o, m_P.number_edges(o) - 1, pts, close_depth, indices,
                    close_vertex, close_index,
                    locations[o][m_P.number_edges(o) - 2]);

          fill_join(join_id + 1, o, m_P.number_edges(o), pts, close_depth, indices,
                    close_vertex, close_index,
                    locations[o][m_P.number_edges(o) - 1]);

          join_id += 2;
        }
    }

  total = close_depth + pre_close_depth;

  /* the vertices of the closing edge are drawn first,
     so they should have the largest depth value
   */
  for(unsigned int i = m_size.pre_close_verts(); i < pts.size(); ++i)
    {
      assert(pts[i].depth() < close_depth);
      assign_depth(pts[i], total - pts[i].depth() - 1);
    }
  for(unsigned int i = 0; i < m_size.pre_close_verts(); ++i)
    {
      assert(pts[i].depth() < pre_close_depth);
      assign_depth(pts[i], total - (pts[i].depth() + close_depth) - 1);
    }

  assert(pre_close_vertex == m_size.pre_close_verts());
  assert(pre_close_index == m_size.pre_close_indices() + m_size.close_indices());
  assert(close_vertex == pre_close_vertex + m_size.close_verts());
  assert(close_index == m_size.close_indices());

  close_depth = total;
}


/////////////////////////////////////////////////
// RoundedJoinCreator::PerJoinData methods
RoundedJoinCreator::PerJoinData::
PerJoinData(const fastuidraw::TessellatedPath::point &p0,
            const fastuidraw::TessellatedPath::point &p1,
            const fastuidraw::vec2 &n0_from_stroking,
            const fastuidraw::vec2 &n1_from_stroking,
            float thresh):
  CommonJoinData(p0.m_p, n0_from_stroking, p1.m_p, n1_from_stroking,
                 p0.m_distance_from_edge_start, p0.m_distance_from_contour_start,
                 p0.m_edge_length, p0.m_open_contour_length, p0.m_closed_contour_length)
{
  /* n0z represents the start point of the rounded join in the complex plane
     as if the join was at the origin, n1z represents the end point of the
     rounded join in the complex plane as if the join was at the origin.
  */
  std::complex<float> n0z(m_lambda * m_n0.x(), m_lambda * m_n0.y());
  std::complex<float> n1z(m_lambda * m_n1.x(), m_lambda * m_n1.y());

  /* n1z_times_conj_n0z satisfies:
     n1z = n1z_times_conj_n0z * n0z
     i.e. it represents the arc-movement from n0z to n1z
  */
  std::complex<float> n1z_times_conj_n0z(n1z * std::conj(n0z));

  m_arc_start = n0z;
  m_delta_theta = std::atan2(n1z_times_conj_n0z.imag(), n1z_times_conj_n0z.real());
  m_num_arc_points = fastuidraw::detail::number_segments_for_tessellation(m_delta_theta, thresh);
  m_delta_theta /= static_cast<float>(m_num_arc_points - 1);
}

void
RoundedJoinCreator::PerJoinData::
add_data(fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
         unsigned int &vertex_offset,
         fastuidraw::c_array<unsigned int> indices,
         unsigned int &index_offset) const
{
  unsigned int i, first;
  float theta;

  first = vertex_offset;

  pts[vertex_offset].m_position = m_p0;
  pts[vertex_offset].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_distance_from_edge_start = m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = m_edge_length;
  pts[vertex_offset].m_open_contour_length = m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = m_closed_contour_length;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_packed_data = pack_data(0, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  pts[vertex_offset].m_position = m_p0;
  pts[vertex_offset].m_pre_offset = m_lambda * m_n0;
  pts[vertex_offset].m_distance_from_edge_start = m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = m_edge_length;
  pts[vertex_offset].m_open_contour_length = m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = m_closed_contour_length;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  for(i = 1, theta = m_delta_theta; i < m_num_arc_points - 1; ++i, theta += m_delta_theta, ++vertex_offset)
    {
      float t, c, s;
      std::complex<float> cs_as_complex;

      t = static_cast<float>(i) / static_cast<float>(m_num_arc_points - 1);
      c = std::cos(theta);
      s = std::sin(theta);
      cs_as_complex = std::complex<float>(c, s) * m_arc_start;

      pts[vertex_offset].m_position = m_p0;
      pts[vertex_offset].m_pre_offset = m_lambda * fastuidraw::vec2(m_n0.x(), m_n1.x());
      pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(t, cs_as_complex.real());
      pts[vertex_offset].m_distance_from_edge_start = m_distance_from_edge_start;
      pts[vertex_offset].m_distance_from_contour_start = m_distance_from_contour_start;
      pts[vertex_offset].m_edge_length = m_edge_length;
      pts[vertex_offset].m_open_contour_length = m_open_contour_length;
      pts[vertex_offset].m_closed_contour_length = m_closed_contour_length;
      pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_rounded_join);

      if(m_lambda * m_n0.y() < 0.0f)
        {
          pts[vertex_offset].m_packed_data |= fastuidraw::StrokedPath::normal0_y_sign_mask;
        }

      if(m_lambda * m_n1.y() < 0.0f)
        {
          pts[vertex_offset].m_packed_data |= fastuidraw::StrokedPath::normal1_y_sign_mask;
        }

      if(cs_as_complex.imag() < 0.0f)
        {
          pts[vertex_offset].m_packed_data |= fastuidraw::StrokedPath::sin_sign_mask;
        }
    }

  pts[vertex_offset].m_position = m_p1;
  pts[vertex_offset].m_pre_offset = m_lambda * m_n1;
  pts[vertex_offset].m_distance_from_edge_start = m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = m_edge_length;
  pts[vertex_offset].m_open_contour_length = m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = m_closed_contour_length;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////////////
// RoundedJoinCreator methods
RoundedJoinCreator::
RoundedJoinCreator(const PathData &P, float thresh):
  JoinCreatorBase(P),
  m_thresh(thresh)
{
  JoinCount J(P);
  m_per_join_data.reserve(J.m_number_close_joins + J.m_number_non_close_joins);
}

void
RoundedJoinCreator::
add_join(unsigned int join_id,
         const PathData &path,
         const fastuidraw::vec2 &n0_from_stroking,
         const fastuidraw::vec2 &n1_from_stroking,
         unsigned int contour, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count)
{
  (void)join_id;
  PerJoinData J(path.m_per_contour_data[contour].edge_data(edge - 1).m_end_pt,
                path.m_per_contour_data[contour].edge_data(edge).m_start_pt,
                n0_from_stroking, n1_from_stroking, m_thresh);

  m_per_join_data.push_back(J);

  /* a triangle fan centered at p0 = p1 with
     m_num_arc_points along an edge
  */
  vert_count += (1 + J.m_num_arc_points);
  index_count += 3 * (J.m_num_arc_points - 1);
}


void
RoundedJoinCreator::
fill_join_implement(unsigned int join_id,
                    const PathData &path,
                    unsigned int contour, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset)
{
  (void)contour;
  (void)edge;
  (void)path;

  assert(join_id < m_per_join_data.size());
  m_per_join_data[join_id].add_data(pts, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////////////
// BevelJoinCreator methods
BevelJoinCreator::
BevelJoinCreator(const PathData &P):
  JoinCreatorBase(P)
{}

void
BevelJoinCreator::
add_join(unsigned int join_id,
         const PathData &path,
         const fastuidraw::vec2 &n0_from_stroking,
         const fastuidraw::vec2 &n1_from_stroking,
         unsigned int contour, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count)
{
  (void)join_id;
  (void)path;
  (void)contour;
  (void)edge;

  /* one triangle per bevel join
   */
  vert_count += 3;
  index_count += 3;

  m_n0.push_back(n0_from_stroking);
  m_n1.push_back(n1_from_stroking);
}

void
BevelJoinCreator::
fill_join_implement(unsigned int join_id,
                    const PathData &path,
                    unsigned int contour, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset)
{
  const fastuidraw::TessellatedPath::point &prev_pt(path.m_per_contour_data[contour].edge_data(edge - 1).m_end_pt);
  const fastuidraw::TessellatedPath::point &next_pt(path.m_per_contour_data[contour].edge_data(edge).m_start_pt);

  CommonJoinData J(prev_pt.m_p, m_n0[join_id],
                   next_pt.m_p, m_n1[join_id],
                   prev_pt.m_distance_from_edge_start,
                   prev_pt.m_distance_from_contour_start,
                   //using p0 to decide the edge length, as
                   //we think of the join as ending an edge.
                   prev_pt.m_edge_length,
                   prev_pt.m_open_contour_length,
                   prev_pt.m_closed_contour_length);

  pts[vertex_offset + 0].m_position = J.m_p0;
  pts[vertex_offset + 0].m_pre_offset = J.m_lambda * J.m_n0;
  pts[vertex_offset + 0].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset + 0].m_distance_from_contour_start = J.m_distance_from_contour_start;
  pts[vertex_offset + 0].m_edge_length = J.m_edge_length;
  pts[vertex_offset + 0].m_open_contour_length = J.m_open_contour_length;
  pts[vertex_offset + 0].m_closed_contour_length = J.m_closed_contour_length;
  pts[vertex_offset + 0].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset + 0].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);

  pts[vertex_offset + 1].m_position = J.m_p0;
  pts[vertex_offset + 1].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset + 1].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset + 1].m_distance_from_contour_start = J.m_distance_from_contour_start;
  pts[vertex_offset + 1].m_edge_length = J.m_edge_length;
  pts[vertex_offset + 1].m_open_contour_length = J.m_open_contour_length;
  pts[vertex_offset + 1].m_closed_contour_length = J.m_closed_contour_length;
  pts[vertex_offset + 1].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset + 1].m_packed_data = pack_data(0, fastuidraw::StrokedPath::offset_shared_with_edge);

  pts[vertex_offset + 2].m_position = J.m_p1;
  pts[vertex_offset + 2].m_pre_offset = J.m_lambda * J.m_n1;
  pts[vertex_offset + 2].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset + 2].m_distance_from_contour_start = J.m_distance_from_contour_start;
  pts[vertex_offset + 2].m_edge_length = J.m_edge_length;
  pts[vertex_offset + 2].m_open_contour_length = J.m_open_contour_length;
  pts[vertex_offset + 2].m_closed_contour_length = J.m_closed_contour_length;
  pts[vertex_offset + 2].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset + 2].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);

  add_triangle_fan(vertex_offset, vertex_offset + 3, indices, index_offset);
  vertex_offset += 3;
}

///////////////////////////////////////////////
// CapCreatorBase methods
unsigned int
CapCreatorBase::
fill_data(fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
          fastuidraw::c_array<unsigned int> indices) const
{
  unsigned int vertex_offset(0), index_offset(0), v(0), depth(0);

  for(unsigned int o = 0; o < m_P.number_contours(); ++o)
    {
      add_cap(m_P.m_per_contour_data[o].m_begin_cap_normal,
              true, m_P.m_per_contour_data[o].m_start_contour_pt,
              pts, indices,
              vertex_offset, index_offset);

      for(; v < vertex_offset; ++v)
        {
          assign_depth(pts[v], depth);
          ++depth;
        }

      add_cap(m_P.m_per_contour_data[o].m_end_cap_normal,
              false, m_P.m_per_contour_data[o].m_end_contour_pt,
              pts, indices,
              vertex_offset, index_offset);

      assert(depth >= 1);
      for(; v < vertex_offset; ++v)
        {
          assign_depth(pts[v], depth);
          ++depth;
        }
    }

  assert(vertex_offset == m_size.verts());
  assert(index_offset == m_size.indices());

  for(unsigned int i = 0, endi = pts.size(); i < endi; ++i)
    {
      assert(pts[i].depth() < depth);
      assign_depth(pts[i], depth - pts[i].depth() - 1);
    }

  return depth;
}

///////////////////////////////////////////////////
// RoundedCapCreator methods
PointIndexCapSize
RoundedCapCreator::
compute_size(const PathData &P, float thresh)
{
  unsigned int num_caps;
  PointIndexCapSize return_value;

  m_num_arc_points_per_cap = fastuidraw::detail::number_segments_for_tessellation(M_PI, thresh);
  m_delta_theta = static_cast<float>(M_PI) / static_cast<float>(m_num_arc_points_per_cap - 1);

  /* each cap is a triangle fan centered at the cap point.
   */
  num_caps = 2 * P.number_contours();
  return_value.verts() = (1 + m_num_arc_points_per_cap) * num_caps;
  return_value.indices() = 3 * (m_num_arc_points_per_cap - 1) * num_caps;

  return return_value;
}

void
RoundedCapCreator::
add_cap(const fastuidraw::vec2 &normal_from_stroking,
        bool is_starting_cap,
        const fastuidraw::TessellatedPath::point &p,
        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
        fastuidraw::c_array<unsigned int> indices,
        unsigned int &vertex_offset,
        unsigned int &index_offset) const
{
  CommonCapData C(is_starting_cap, p.m_p, normal_from_stroking);
  unsigned int first, i;
  float theta;

  first = vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_packed_data = pack_data(0, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  for(i = 1, theta = m_delta_theta; i < m_num_arc_points_per_cap - 1; ++i, theta += m_delta_theta, ++vertex_offset)
    {
      float s, c;

      s = std::sin(theta);
      c = std::cos(theta);
      pts[vertex_offset].m_position = C.m_p;
      pts[vertex_offset].m_pre_offset = C.m_n;
      pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(s, c);
      pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
      pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
      pts[vertex_offset].m_edge_length = p.m_edge_length;
      pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
      pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
      pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_rounded_cap);
    }

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = -C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}


///////////////////////////////////////////////////
// SquareCapCreator methods
PointIndexCapSize
SquareCapCreator::
compute_size(const PathData &P)
{
  PointIndexCapSize return_value;
  unsigned int num_caps;

  /* each square cap generates 5 new points
     and 3 triangles (= 9 indices)
   */
  num_caps = 2 * P.number_contours();
  return_value.verts() = 5 * num_caps;
  return_value.indices() = 9 * num_caps;

  return return_value;
}

void
SquareCapCreator::
add_cap(const fastuidraw::vec2 &normal_from_stroking,
        bool is_starting_cap,
        const fastuidraw::TessellatedPath::point &p,
        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
        fastuidraw::c_array<unsigned int> indices,
        unsigned int &vertex_offset,
        unsigned int &index_offset) const
{
  CommonCapData C(is_starting_cap, p.m_p, normal_from_stroking);
  unsigned int first;

  first = vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_packed_data = pack_data(0, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_auxilary_offset = C.m_v;
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_square_cap);
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = -C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_auxilary_offset = C.m_v;
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_square_cap);
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = -C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

//////////////////////////////////////
// AdjustableCapCreator methods
PointIndexCapSize
AdjustableCapCreator::
compute_size(const PathData &P)
{
  PointIndexCapSize return_value;
  unsigned int num_caps;

  num_caps = 2 * P.number_contours();
  return_value.verts() = number_points_per_fan * num_caps;
  return_value.indices() = number_indices_per_fan * num_caps;

  return return_value;
}

void
AdjustableCapCreator::
pack_fan(bool entering_contour,
         enum fastuidraw::StrokedPath::offset_type_t cp_js_type,
         const fastuidraw::TessellatedPath::point &p,
         const fastuidraw::vec2 &stroking_normal,
         fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
         unsigned int &vertex_offset,
         fastuidraw::c_array<unsigned int> indices,
         unsigned int &index_offset)
{
  CommonCapData C(entering_contour, p.m_p, stroking_normal);
  unsigned int first(vertex_offset);

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_auxilary_offset = C.m_v;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(0, cp_js_type);
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = C.m_n;
  pts[vertex_offset].m_auxilary_offset = C.m_v;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(1, cp_js_type);
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = C.m_n;
  pts[vertex_offset].m_auxilary_offset = C.m_v;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(1, cp_js_type) | fastuidraw::StrokedPath::adjustable_cap_ending_mask;
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_auxilary_offset = C.m_v;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(0, cp_js_type) | fastuidraw::StrokedPath::adjustable_cap_ending_mask;
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = -C.m_n;
  pts[vertex_offset].m_auxilary_offset = C.m_v;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(1, cp_js_type) | fastuidraw::StrokedPath::adjustable_cap_ending_mask;
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = -C.m_n;
  pts[vertex_offset].m_auxilary_offset = C.m_v;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(1, cp_js_type);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

void
AdjustableCapCreator::
add_cap(const fastuidraw::vec2 &normal_from_stroking,
        bool is_starting_cap,
        const fastuidraw::TessellatedPath::point &p0,
        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
        fastuidraw::c_array<unsigned int> indices,
        unsigned int &vertex_offset,
        unsigned int &index_offset) const
{
  enum fastuidraw::StrokedPath::offset_type_t tp;
  tp = (is_starting_cap) ?
    fastuidraw::StrokedPath::offset_adjustable_cap_contour_start :
    fastuidraw::StrokedPath::offset_adjustable_cap_contour_end;

  pack_fan(is_starting_cap, tp,
           p0, normal_from_stroking,
           pts, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////////////
// MiterJoinCreator methods
MiterJoinCreator::
MiterJoinCreator(const PathData &P):
  JoinCreatorBase(P)
{
}

void
MiterJoinCreator::
add_join(unsigned int join_id,
         const PathData &path,
         const fastuidraw::vec2 &n0_from_stroking,
         const fastuidraw::vec2 &n1_from_stroking,
         unsigned int contour, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count)
{
  /* Each join is a triangle fan from 5 points
     (thus 3 triangles, which is 9 indices)
   */
  (void)join_id;
  (void)path;
  (void)contour;
  (void)edge;

  vert_count += 5;
  index_count += 9;

  m_n0.push_back(n0_from_stroking);
  m_n1.push_back(n1_from_stroking);
}


void
MiterJoinCreator::
fill_join_implement(unsigned int join_id,
                    const PathData &path,
                    unsigned int contour, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset)
{
  FASTUIDRAWunused(join_id);

  const fastuidraw::TessellatedPath::point &prev_pt(path.m_per_contour_data[contour].edge_data(edge - 1).m_end_pt);
  const fastuidraw::TessellatedPath::point &next_pt(path.m_per_contour_data[contour].edge_data(edge).m_start_pt);
  unsigned int first;

  CommonJoinData J(prev_pt.m_p, m_n0[join_id],
                   next_pt.m_p, m_n1[join_id],
                   prev_pt.m_distance_from_edge_start,
                   prev_pt.m_distance_from_contour_start,
                   //using p0 to decide the edge length, as
                   //we think of the join as ending an edge.
                   prev_pt.m_edge_length,
                   prev_pt.m_open_contour_length,
                   prev_pt.m_closed_contour_length);

  /* The miter point is given by where the two boundary
     curves intersect. The two curves are given by:

     a(t) = J.m_p0 + stroke_width * J.m_lamba * J.m_n0 + t * J.m_v0
     b(s) = J.m_p1 + stroke_width * J.m_lamba * J.m_n1 - s * J.m_v1

    With J.m_p0 is the same value as J.m_p1, the location
    of the join.

    We need to solve a(t) = b(s) and compute that location.
    Linear algebra gives us that:

    t = - stroke_width * J.m_lamba * r
    s = - stroke_width * J.m_lamba * r
     where
    r = (<J.m_v1, J.m_v0> - 1) / <J.m_v0, J.m_n1>

    thus

    a(t) = J.m_p0 + stroke_width * ( J.m_lamba * J.m_n0 -  r * J.m_lamba * J.m_v0)
         = b(s)
         = J.m_p1 + stroke_width * ( J.m_lamba * J.m_n1 +  r * J.m_lamba * J.m_v1)
   */

  first = vertex_offset;

  // join center point.
  pts[vertex_offset].m_position = J.m_p0;
  pts[vertex_offset].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = J.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = J.m_edge_length;
  pts[vertex_offset].m_open_contour_length = J.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = J.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(0, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  // join point from curve into join
  pts[vertex_offset].m_position = J.m_p0;
  pts[vertex_offset].m_pre_offset = J.m_lambda * J.m_n0;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = J.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = J.m_edge_length;
  pts[vertex_offset].m_open_contour_length = J.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = J.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  // miter point A
  pts[vertex_offset].m_position = J.m_p0;
  pts[vertex_offset].m_pre_offset = J.m_n0;
  pts[vertex_offset].m_auxilary_offset = J.m_n1;
  pts[vertex_offset].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = J.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = J.m_edge_length;
  pts[vertex_offset].m_open_contour_length = J.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = J.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_miter_join);
  ++vertex_offset;

  // miter point B
  pts[vertex_offset].m_position = J.m_p1;
  pts[vertex_offset].m_pre_offset = J.m_n0;
  pts[vertex_offset].m_auxilary_offset = J.m_n1;
  pts[vertex_offset].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = J.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = J.m_edge_length;
  pts[vertex_offset].m_open_contour_length = J.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = J.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_miter_join);
  ++vertex_offset;

  // join point from curve out from join
  pts[vertex_offset].m_position = J.m_p1;
  pts[vertex_offset].m_pre_offset = J.m_lambda * J.m_n1;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = J.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = J.m_edge_length;
  pts[vertex_offset].m_open_contour_length = J.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = J.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

/////////////////////////////////////
// GenericDataPrivate methods
GenericDataPrivate::
GenericDataPrivate(void):
  m_attribute_data(NULL)
{}

GenericDataPrivate::
~GenericDataPrivate()
{
  if(m_attribute_data != NULL)
    {
      FASTUIDRAWdelete(m_attribute_data);
    }
}

///////////////////////////
// JoinsPrivate methods
void
JoinsPrivate::
resize_locations(const PathData &P)
{
  m_locations.resize(P.number_contours());
  for(unsigned int C = 0, endC = P.number_contours(); C < endC; ++C)
    {
      m_locations[C].resize(P.number_edges(C));
    }
}

/////////////////////////////////////////////
// StrokedPathPrivate methods
StrokedPathPrivate::
StrokedPathPrivate(void)
{
}

StrokedPathPrivate::
~StrokedPathPrivate()
{
  for(unsigned int i = 0, endi = m_rounded_joins.size(); i < endi; ++i)
    {
      FASTUIDRAWdelete(m_rounded_joins[i].m_data);
    }

  for(unsigned int i = 0, endi = m_rounded_caps.size(); i < endi; ++i)
    {
      FASTUIDRAWdelete(m_rounded_caps[i].m_data);
    }
}

void
StrokedPathPrivate::
create_edges(PathData &path_data, const fastuidraw::TessellatedPath &P, void *d)
{
  EdgeDataCreator e(P, path_data);
  GenericDataPrivate *edge_private;

  edge_private = reinterpret_cast<GenericDataPrivate*>(d);
  edge_private->m_data.resize(e.sizes());
  e.fill_data(edge_private->m_data.m_points.data(true),
              edge_private->m_data.m_indices.data(true),
              edge_private->m_data.m_number_depth[false],
              edge_private->m_data.m_number_depth[true]);
  edge_private->m_data.compute_conveniance(edge_private->m_return_values);
}

template<typename T>
void
StrokedPathPrivate::
create_joins(const PathData &e, void *d)
{
  JoinsPrivate *out_data;

  out_data = reinterpret_cast<JoinsPrivate*>(d);
  out_data->resize_locations(e);

  T creator(e);
  out_data->m_data.resize(creator.sizes());
  creator.fill_data(out_data->m_data.m_points.data(true),
                    out_data->m_data.m_indices.data(true),
                    out_data->m_data.m_number_depth[false],
                    out_data->m_data.m_number_depth[true],
                    out_data->m_locations);
  out_data->m_data.compute_conveniance(out_data->m_return_values);
}

template<typename T>
void
StrokedPathPrivate::
create_joins(const PathData &e, float thresh, void *d)
{
  JoinsPrivate *out_data;

  out_data = reinterpret_cast<JoinsPrivate*>(d);
  out_data->resize_locations(e);

  T creator(e, thresh);
  out_data->m_data.resize(creator.sizes());
  creator.fill_data(out_data->m_data.m_points.data(true),
                    out_data->m_data.m_indices.data(true),
                    out_data->m_data.m_number_depth[false],
                    out_data->m_data.m_number_depth[true],
                    out_data->m_locations);
  out_data->m_data.compute_conveniance(out_data->m_return_values);
}

template<typename T>
void
StrokedPathPrivate::
create_caps(const PathData &e, void *d)
{
  CapsPrivate *out_data;
  out_data = reinterpret_cast<CapsPrivate*>(d);

  T creator(e);
  out_data->resize(creator.sizes());
  out_data->m_number_depth = creator.fill_data(fastuidraw::make_c_array(out_data->m_points),
                                               fastuidraw::make_c_array(out_data->m_indices));
}

template<typename T>
void
StrokedPathPrivate::
create_caps(const PathData &e, float thresh, void *d)
{
  CapsPrivate *out_data;
  out_data = reinterpret_cast<CapsPrivate*>(d);

  T creator(e, thresh);
  out_data->resize(creator.sizes());
  out_data->m_number_depth = creator.fill_data(fastuidraw::make_c_array(out_data->m_points),
                                               fastuidraw::make_c_array(out_data->m_indices));
}

//////////////////////////////////////
// fastuidraw::StrokedPath::point methods
void
fastuidraw::StrokedPath::point::
pack_point(PainterAttribute *dst) const
{
  dst->m_attrib0 = pack_vec4(m_position.x(),
                             m_position.y(),
                             m_pre_offset.x(),
                             m_pre_offset.y());

  dst->m_attrib1 = pack_vec4(m_distance_from_edge_start,
                             m_distance_from_contour_start,
                             m_auxilary_offset.x(),
                             m_auxilary_offset.y());

  dst->m_attrib2 = uvec4(m_packed_data,
                         pack_float(m_edge_length),
                         pack_float(m_open_contour_length),
                         pack_float(m_closed_contour_length));
}

void
fastuidraw::StrokedPath::point::
unpack_point(point *dst, const PainterAttribute &a)
{
  dst->m_position.x() = unpack_float(a.m_attrib0.x());
  dst->m_position.y() = unpack_float(a.m_attrib0.y());

  dst->m_pre_offset.x() = unpack_float(a.m_attrib0.z());
  dst->m_pre_offset.y() = unpack_float(a.m_attrib0.w());

  dst->m_distance_from_edge_start = unpack_float(a.m_attrib1.x());
  dst->m_distance_from_contour_start = unpack_float(a.m_attrib1.y());
  dst->m_auxilary_offset.x() = unpack_float(a.m_attrib1.z());
  dst->m_auxilary_offset.y() = unpack_float(a.m_attrib1.w());

  dst->m_packed_data = a.m_attrib2.x();
  dst->m_edge_length = unpack_float(a.m_attrib2.y());
  dst->m_open_contour_length = unpack_float(a.m_attrib2.z());
  dst->m_closed_contour_length = unpack_float(a.m_attrib2.w());
}

fastuidraw::vec2
fastuidraw::StrokedPath::point::
offset_vector(void)
{
  switch(offset_type())
    {
    case offset_start_sub_edge:
    case offset_end_sub_edge:
    case offset_shared_with_edge:
      return m_pre_offset;

    case offset_square_cap:
      return m_pre_offset + m_auxilary_offset;
      break;

    case offset_miter_join:
      {
        vec2 n(m_pre_offset), v(-n.y(), n.x());
        float r, lambda;
        if(dot(v, m_auxilary_offset) > 0.0f)
          {
            lambda = -1.0f;
          }
        else
          {
            lambda = 1.0f;
          }
        r = (dot(m_pre_offset, m_auxilary_offset) - 1.0) / dot(v, m_auxilary_offset);
        return lambda * (n - r * v);
      }

    case offset_rounded_cap:
      {
        vec2 n(m_pre_offset), v(n.y(), -n.x());
        return m_auxilary_offset.x() * v + m_auxilary_offset.y() * n;
      }

    case offset_rounded_join:
      {
        vec2 cs;

        cs.x() = m_auxilary_offset.y();
        cs.y() = sqrt(1.0 - cs.x() * cs.x());
        if(m_packed_data & sin_sign_mask)
          {
            cs.y() = -cs.y();
          }
        return cs;
      }

    default:
      return vec2(0.0f, 0.0f);
    }
}

float
fastuidraw::StrokedPath::point::
miter_distance(void) const
{
  if(offset_type() == offset_miter_join)
    {
      vec2 n0(m_pre_offset), v0(-n0.y(), n0.x());
      vec2 n1(m_auxilary_offset), v1(-n1.y(), n1.x());
      float numer, denom;

      numer = dot(v1, v0) - 1.0f;
      denom = dot(v0, n1);
      return denom != 0.0f ?
        numer / denom :
        0.0f;
    }
  else
    {
      return 0.0f;
    }
}

/////////////////////////////////////////////////////
// fastuidraw::StrokedPath::Edges methods
fastuidraw::StrokedPath::Edges::
Edges(void)
{
  m_d = FASTUIDRAWnew GenericDataPrivate();
}

fastuidraw::StrokedPath::Edges::
~Edges()
{
  GenericDataPrivate *d;
  d = reinterpret_cast<GenericDataPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::const_c_array<fastuidraw::StrokedPath::point>
fastuidraw::StrokedPath::Edges::
points(bool including_closing_edge) const
{
  GenericDataPrivate *d;
  d = reinterpret_cast<GenericDataPrivate*>(m_d);
  return d->m_return_values[including_closing_edge].m_points;
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::Edges::
indices(bool including_closing_edge) const
{
  GenericDataPrivate *d;
  d = reinterpret_cast<GenericDataPrivate*>(m_d);
  return d->m_return_values[including_closing_edge].m_indices;
}

unsigned int
fastuidraw::StrokedPath::Edges::
number_depth(bool including_closing_edge) const
{
  GenericDataPrivate *d;
  d = reinterpret_cast<GenericDataPrivate*>(m_d);
  return d->m_return_values[including_closing_edge].m_number_depth;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::Edges::
painter_data(void) const
{
  GenericDataPrivate *d;
  d = reinterpret_cast<GenericDataPrivate*>(m_d);
  if(d->m_attribute_data == NULL)
    {
      d->m_attribute_data = FASTUIDRAWnew PainterAttributeData();
      d->m_attribute_data->set_data(PainterAttributeDataFillerPathEdges(*this));
    }
  return *d->m_attribute_data;
}

/////////////////////////////////////
// fastuidraw::StrokedPath::Joins methods
fastuidraw::StrokedPath::Joins::
Joins(void)
{
  m_d = FASTUIDRAWnew JoinsPrivate();
}

fastuidraw::StrokedPath::Joins::
~Joins()
{
  JoinsPrivate *d;
  d = reinterpret_cast<JoinsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::const_c_array<fastuidraw::StrokedPath::point>
fastuidraw::StrokedPath::Joins::
points(bool including_closing_edge) const
{
  JoinsPrivate *d;
  d = reinterpret_cast<JoinsPrivate*>(m_d);
  return d->m_return_values[including_closing_edge].m_points;
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::Joins::
indices(bool including_closing_edge) const
{
  JoinsPrivate *d;
  d = reinterpret_cast<JoinsPrivate*>(m_d);
  return d->m_return_values[including_closing_edge].m_indices;
}

unsigned int
fastuidraw::StrokedPath::Joins::
number_depth(bool including_closing_edge) const
{
  JoinsPrivate *d;
  d = reinterpret_cast<JoinsPrivate*>(m_d);
  return d->m_return_values[including_closing_edge].m_number_depth;
}

unsigned int
fastuidraw::StrokedPath::Joins::
number_contours(void) const
{
  JoinsPrivate *d;
  d = reinterpret_cast<JoinsPrivate*>(m_d);
  return d->m_locations.size();
}

unsigned int
fastuidraw::StrokedPath::Joins::
number_joins(unsigned int contour) const
{
  JoinsPrivate *d;
  d = reinterpret_cast<JoinsPrivate*>(m_d);
  assert(contour < d->m_locations.size());
  return d->m_locations[contour].size();
}

fastuidraw::range_type<unsigned int>
fastuidraw::StrokedPath::Joins::
points_range(unsigned int contour, unsigned int N) const
{
  JoinsPrivate *d;
  d = reinterpret_cast<JoinsPrivate*>(m_d);
  assert(contour < d->m_locations.size() && N < d->m_locations[contour].size());
  return d->m_locations[contour][N].m_attribs;
}

fastuidraw::range_type<unsigned int>
fastuidraw::StrokedPath::Joins::
indices_range(unsigned int contour, unsigned int N) const
{
  JoinsPrivate *d;
  d = reinterpret_cast<JoinsPrivate*>(m_d);
  assert(contour < d->m_locations.size() && N < d->m_locations[contour].size());
  return d->m_locations[contour][N].m_indices;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::Joins::
painter_data(void) const
{
  JoinsPrivate *d;
  d = reinterpret_cast<JoinsPrivate*>(m_d);
  if(d->m_attribute_data == NULL)
    {
      d->m_attribute_data = FASTUIDRAWnew PainterAttributeData();
      d->m_attribute_data->set_data(PainterAttributeDataFillerPathJoins(*this));
    }
  return *d->m_attribute_data;
}

//////////////////////////////////
// fastuidraw::StrokedPath::Caps methods
fastuidraw::StrokedPath::Caps::
Caps(void)
{
  m_d = FASTUIDRAWnew CapsPrivate();
}

fastuidraw::StrokedPath::Caps::
~Caps()
{
  CapsPrivate *d;
  d = reinterpret_cast<CapsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::const_c_array<fastuidraw::StrokedPath::point>
fastuidraw::StrokedPath::Caps::
points(void) const
{
  const CapsPrivate *d;
  d = reinterpret_cast<const CapsPrivate*>(m_d);
  return make_c_array(d->m_points);
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::Caps::
indices(void) const
{
  const CapsPrivate *d;
  d = reinterpret_cast<const CapsPrivate*>(m_d);
  return make_c_array(d->m_indices);
}

unsigned int
fastuidraw::StrokedPath::Caps::
number_depth(void) const
{
  const CapsPrivate *d;
  d = reinterpret_cast<const CapsPrivate*>(m_d);
  return d->m_number_depth;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::Caps::
painter_data(void) const
{
  CapsPrivate *d;
  d = reinterpret_cast<CapsPrivate*>(m_d);
  if(d->m_attribute_data == NULL)
    {
      d->m_attribute_data = FASTUIDRAWnew PainterAttributeData();
      d->m_attribute_data->set_data(PainterAttributeDataFillerPathCaps(*this));
    }
  return *d->m_attribute_data;
}

//////////////////////////////////////////////////////////////
// fastuidraw::StrokedPath methods
fastuidraw::StrokedPath::
StrokedPath(const fastuidraw::TessellatedPath &P)
{
  StrokedPathPrivate *d;

  assert(number_offset_types < FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(offset_type_num_bits));
  m_d = d = FASTUIDRAWnew StrokedPathPrivate();

  StrokedPathPrivate::create_edges(d->m_path_data, P, d->m_edges.m_d);

  StrokedPathPrivate::create_joins<BevelJoinCreator>(d->m_path_data, d->m_bevel_joins.m_d);
  StrokedPathPrivate::create_joins<MiterJoinCreator>(d->m_path_data, d->m_miter_joins.m_d);
  StrokedPathPrivate::create_caps<SquareCapCreator>(d->m_path_data, d->m_square_caps.m_d);
  StrokedPathPrivate::create_caps<AdjustableCapCreator>(d->m_path_data, d->m_adjustable_caps.m_d);

  d->m_effective_curve_distance_threshhold = P.effective_curve_distance_threshhold();
}

fastuidraw::StrokedPath::
~StrokedPath()
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

float
fastuidraw::StrokedPath::
effective_curve_distance_threshhold(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_effective_curve_distance_threshhold;
}

const fastuidraw::StrokedPath::Edges&
fastuidraw::StrokedPath::
edges(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_edges;
}

const fastuidraw::StrokedPath::Caps&
fastuidraw::StrokedPath::
square_caps(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_square_caps;
}

const fastuidraw::StrokedPath::Caps&
fastuidraw::StrokedPath::
adjustable_caps(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_adjustable_caps;
}

const fastuidraw::StrokedPath::Joins&
fastuidraw::StrokedPath::
bevel_joins(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_bevel_joins;
}

const fastuidraw::StrokedPath::Joins&
fastuidraw::StrokedPath::
miter_joins(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_miter_joins;
}

const fastuidraw::StrokedPath::Joins&
fastuidraw::StrokedPath::
rounded_joins(float thresh) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  if(d->m_rounded_joins.empty())
    {
      Joins *newJ;
      newJ = FASTUIDRAWnew Joins();
      StrokedPathPrivate::create_joins<RoundedJoinCreator>(d->m_path_data, 1.0f, newJ->m_d);
      d->m_rounded_joins.push_back(ThreshWithJoins(newJ, 1.0f));
    }

  /* we set a hard tolerance of 1e-6. Should we
     set it as a ratio of the bounding box of
     the underlying tessellated path?
   */
  thresh = t_max(thresh, float(1e-6));

  if(d->m_rounded_joins.back().m_thresh <= thresh)
    {
      std::vector<ThreshWithJoins>::const_iterator iter;
      iter = std::lower_bound(d->m_rounded_joins.begin(),
                              d->m_rounded_joins.end(),
                              thresh,
                              ThreshWithJoins::reverse_compare_against_thresh);
      assert(iter != d->m_rounded_joins.end());
      assert(iter->m_thresh <= thresh);
      assert(iter->m_data);
      return *iter->m_data;
    }
  else
    {
      float t;
      t = d->m_rounded_joins.back().m_thresh;
      while(t > thresh)
        {
          Joins *newJ;

          t *= 0.5f;
          newJ = FASTUIDRAWnew Joins();
          StrokedPathPrivate::create_joins<RoundedJoinCreator>(d->m_path_data, t, newJ->m_d);
          d->m_rounded_joins.push_back(ThreshWithJoins(newJ, t));
        }
      return *d->m_rounded_joins.back().m_data;
    }
}

const fastuidraw::StrokedPath::Caps&
fastuidraw::StrokedPath::
rounded_caps(float thresh) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  if(d->m_rounded_caps.empty())
    {
      Caps *newC;

      newC = FASTUIDRAWnew Caps();
      StrokedPathPrivate::create_caps<RoundedCapCreator>(d->m_path_data, 1.0f, newC->m_d);
      d->m_rounded_caps.push_back(ThreshWithCaps(newC, 1.0f));
    }

  /* we set a hard tolerance of 1e-6. Should we
     set it as a ratio of the bounding box of
     the underlying tessellated path?
   */
  thresh = t_max(thresh, float(1e-6));

  if(d->m_rounded_caps.back().m_thresh <= thresh)
    {
      std::vector<ThreshWithCaps>::const_iterator iter;
      iter = std::lower_bound(d->m_rounded_caps.begin(),
                              d->m_rounded_caps.end(),
                              thresh,
                              ThreshWithCaps::reverse_compare_against_thresh);
      assert(iter != d->m_rounded_caps.end());
      assert(iter->m_thresh <= thresh);
      assert(iter->m_data);
      return *iter->m_data;
    }
  else
    {
      float t;
      t = d->m_rounded_caps.back().m_thresh;
      while(t > thresh)
        {
          Caps *newC;

          t *= 0.5f;
          newC = FASTUIDRAWnew Caps();
          StrokedPathPrivate::create_caps<RoundedCapCreator>(d->m_path_data, t, newC->m_d);
          d->m_rounded_caps.push_back(ThreshWithCaps(newC, t));
        }
      return *d->m_rounded_caps.back().m_data;
    }
}
