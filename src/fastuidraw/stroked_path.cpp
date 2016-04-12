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
#include "private/util_private.hpp"

namespace
{
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


  class EdgeDataCreator
  {
  public:
    static const float sm_mag_tol = 0.000001f;

    EdgeDataCreator(const fastuidraw::TessellatedPath &P):
      m_P(P),
      m_outline_begin_cap_normal(m_P.number_outlines()),
      m_outline_end_cap_normal(m_P.number_outlines())
    {
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

    const fastuidraw::vec2&
    outline_begin_cap_normal(unsigned int outline) const
    {
      return m_outline_begin_cap_normal[outline];
    }

    const fastuidraw::vec2&
    outline_end_cap_normal(unsigned int outline) const
    {
      return m_outline_end_cap_normal[outline];
    }

  private:

    void
    compute_size(void);

    void
    init_prev_normal(unsigned int outline);

    void
    add_edge(unsigned int outline, unsigned int edge,
             fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
             unsigned int &depth,
             fastuidraw::c_array<unsigned int> indices,
             unsigned int &vert_offset, unsigned int &index_offset);

    PointIndexSize m_size;
    const fastuidraw::TessellatedPath &m_P;
    fastuidraw::vec2 m_prev_normal;
    std::vector<fastuidraw::vec2> m_outline_begin_cap_normal;
    std::vector<fastuidraw::vec2> m_outline_end_cap_normal;
  };

  class JoinCount
  {
  public:
    explicit
    JoinCount(const fastuidraw::TessellatedPath &P);

    unsigned int m_number_close_joins;
    unsigned int m_number_non_close_joins;
  };

  class CommonJoinData
  {
  public:
    CommonJoinData(const fastuidraw::TessellatedPath::point &p0,
                   const fastuidraw::TessellatedPath::point &p1);

    float m_det, m_lambda;
    fastuidraw::vec2 m_p0, m_v0, m_n0;
    fastuidraw::vec2 m_p1, m_v1, m_n1;
    float m_distance_from_edge_start;
    float m_distance_from_outline_start;
  };

  class JoinCreatorBase
  {
  public:
    explicit
    JoinCreatorBase(const fastuidraw::TessellatedPath &P):
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
              unsigned int &pre_close_depth, unsigned int &close_depth);

  private:

    virtual
    void
    add_join(unsigned int join_id,
             const fastuidraw::TessellatedPath &path,
             unsigned int outline, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count) = 0;


    void
    fill_join(unsigned int join_id,
              unsigned int outline, unsigned int edge,
              fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
              unsigned int &depth,
              fastuidraw::c_array<unsigned int> indices,
              unsigned int &vertex_offset, unsigned int &index_offset);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const fastuidraw::TessellatedPath &path,
                        unsigned int outline, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) = 0;

    void
    compute_size(void);

    const fastuidraw::TessellatedPath &m_P;
    PointIndexSize m_size;
    bool m_size_ready;
  };


  class RoundedJoinCreator:public JoinCreatorBase
  {
  public:
    RoundedJoinCreator(const fastuidraw::TessellatedPath &P);

  private:

    class PerJoinData:public CommonJoinData
    {
    public:
      PerJoinData(const fastuidraw::TessellatedPath::point &p0,
                  const fastuidraw::TessellatedPath::point &p1,
                  float curve_tessellation);

      void
      add_data(fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
               unsigned int &vertex_offset,
               fastuidraw::c_array<unsigned int> indices,
               unsigned int &index_offset) const;

      float m_delta_theta;
      unsigned int m_num_arc_points;
      std::complex<float> m_arc_start;
    };

    virtual
    void
    add_join(unsigned int join_id,
             const fastuidraw::TessellatedPath &path,
             unsigned int outline, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const fastuidraw::TessellatedPath &path,
                        unsigned int outline, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset);

    std::vector<PerJoinData> m_per_join_data;
  };

  class BevelJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    BevelJoinCreator(const fastuidraw::TessellatedPath &P);

  private:

    virtual
    void
    add_join(unsigned int join_id,
             const fastuidraw::TessellatedPath &path,
             unsigned int outline, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const fastuidraw::TessellatedPath &path,
                        unsigned int outline, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset);

  };

  class CommonCapData
  {
  public:
    CommonCapData(bool is_start_cap,
                  const fastuidraw::TessellatedPath::point &src_pt,
                  const fastuidraw::vec2 &normal_from_stroking):
      m_is_start_cap(is_start_cap),
      m_p(src_pt.m_p),
      m_v(src_pt.m_p_t)
    {
      //caps at the start are on the "other side"
      float lambda((is_start_cap) ? -1.0f : 1.0f);
      float mag;

      mag = m_v.magnitude();
      if(mag < EdgeDataCreator::sm_mag_tol)
        {
          m_n = lambda * normal_from_stroking;
          m_v = fastuidraw::vec2(m_n.y(), -m_n.x());
        }
      else
        {
          m_v *= (lambda / mag);
          m_n = fastuidraw::vec2(-m_v.y(), m_v.x());
        }
    }

    bool m_is_start_cap;
    float m_lambda;
    fastuidraw::vec2 m_p, m_n, m_v;
  };

  class CapCreatorBase
  {
  public:
    CapCreatorBase(const fastuidraw::TessellatedPath &P,
                   PointIndexCapSize sz,
                   enum fastuidraw::StrokedPath::point_type_t tp):
      m_P(P),
      m_size(sz),
      m_tp(tp)
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
    fill_data(const EdgeDataCreator &edge_creator,
              fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
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

    const fastuidraw::TessellatedPath &m_P;
    PointIndexCapSize m_size;
    enum fastuidraw::StrokedPath::point_type_t m_tp;

  };

  class RoundedCapCreator:public CapCreatorBase
  {
  public:
    RoundedCapCreator(const fastuidraw::TessellatedPath &P):
      CapCreatorBase(P, compute_size(P),
                     fastuidraw::StrokedPath::rounded_cap_point)
    {
    }

  private:

    PointIndexCapSize
    compute_size(const fastuidraw::TessellatedPath &P);

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
    SquareCapCreator(const fastuidraw::TessellatedPath &P):
      CapCreatorBase(P, compute_size(P),
                     fastuidraw::StrokedPath::square_cap_point)
    {
    }

  private:

    PointIndexCapSize
    compute_size(const fastuidraw::TessellatedPath &P);

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
    MiterJoinCreator(const fastuidraw::TessellatedPath &P);

  private:
    virtual
    void
    add_join(unsigned int join_id,
             const fastuidraw::TessellatedPath &path,
             unsigned int outline, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const fastuidraw::TessellatedPath &path,
                        unsigned int outline, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset);
  };

  void
  set_point_type_to_edge_point(fastuidraw::c_array<fastuidraw::StrokedPath::point> data)
  {
    fastuidraw::c_array<fastuidraw::StrokedPath::point>::iterator iter;

    for(iter = data.begin(); iter != data.end(); ++iter)
      {
        iter->m_point_type = fastuidraw::StrokedPath::edge_point;
      }
  }


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

    unsigned int&
    number_depth(bool with_closing_edge)
    {
      return m_number_depth[with_closing_edge];
    }

    unsigned int
    number_depth(bool with_closing_edge) const
    {
      return m_number_depth[with_closing_edge];
    }
  };

  class CapData
  {
  public:
    std::vector<fastuidraw::StrokedPath::point> m_points;
    std::vector<unsigned int> m_indices;

    void
    resize(fastuidraw::uvec2 szs)
    {
      m_points.resize(szs[0]);
      m_indices.resize(szs[1]);
    }
  };

  class StrokedPathPrivate
  {
  public:
    explicit
    StrokedPathPrivate(const fastuidraw::TessellatedPath &P);
    ~StrokedPathPrivate();

    Data<fastuidraw::StrokedPath::point> m_edges;
    Data<fastuidraw::StrokedPath::point> m_rounded_joins;
    Data<fastuidraw::StrokedPath::point> m_bevel_joins;
    Data<fastuidraw::StrokedPath::point> m_miter_joins;

    CapData m_rounded_cap;
    CapData m_square_cap;
    unsigned int m_cap_number_depth;

    fastuidraw::PainterAttributeData *m_attribute_data;
  };

}

//////////////////////////////////////////////
// JoinCount methods
JoinCount::
JoinCount(const fastuidraw::TessellatedPath &P):
  m_number_close_joins(0),
  m_number_non_close_joins(0)
{
  for(unsigned int o = 0; o < P.number_outlines(); ++o)
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
CommonJoinData(const fastuidraw::TessellatedPath::point &p0,
               const fastuidraw::TessellatedPath::point &p1):
  m_distance_from_edge_start(p0.m_distance_from_edge_start),
  m_distance_from_outline_start(p1.m_distance_from_outline_start)
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

  m_p0 = p0.m_p;
  m_v0 = p0.m_p_t;
  m_p1 = p1.m_p;
  m_v1 = p1.m_p_t;

  m_v0.normalize();
  m_v1.normalize();

  m_n0 = fastuidraw::vec2(-m_v0.y(), m_v0.x());
  m_n1 = fastuidraw::vec2(-m_v1.y(), m_v1.x());

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

///////////////////////////////////////////////////
// EdgeDataCreator methods
void
EdgeDataCreator::
compute_size(void)
{
  for(unsigned int o = 0; o < m_P.number_outlines(); ++o)
    {
      for(unsigned int e = 0; e < m_P.number_edges(o); ++e)
        {
          fastuidraw::range_type<unsigned int> R;

          R = m_P.edge_range(o, e);
          assert(R.m_end > R.m_begin);

          /* Each sub-edge of the tessellation produces -6- points:
             (p,n), (p,-n), (p, 0),
             (p_next, n_next), (p_next, -n_next), (p_next, n_next), (p_next, 0)

             Also each edge introduces one extra vertex pair for
             the bevel join (this can be optimized to one vertex
             actually).

             Each sub-edge has a 2 quads (12 indices) and one
             triangle for a total of 15 indices
          */
          if(e + 1 == m_P.number_edges(o))
            {
              m_size.close_verts() += 2 + 6 * ((R.m_end - R.m_begin) - 1);
              m_size.close_indices() += 15 * ((R.m_end - R.m_begin) - 1);
            }
          else
            {
              m_size.pre_close_verts() += 2 + 6 * ((R.m_end - R.m_begin) - 1);
              m_size.pre_close_indices() += 15 * ((R.m_end - R.m_begin) - 1);
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

  R = m_P.edge_range(o, e);
  assert(R.m_end > R.m_begin);

  for(unsigned int i = R.m_begin, s = 0; i < R.m_end; ++i, ++s, ++depth)
    {
      fastuidraw::vec2 normal;
      float normal_mag;

      normal = fastuidraw::vec2(src_pts[i].m_p_t.y(), -src_pts[i].m_p_t.x());
      normal_mag = normal.magnitude();
      if(normal_mag < sm_mag_tol)
        {
          normal = m_prev_normal;
        }
      else
        {
          normal /= normal_mag;
        }

      if(e == 0 && i == R.m_begin)
        {
          m_outline_begin_cap_normal[o] = normal;
        }

      if(e + 1 == m_P.number_edges(o) && (i+1) == R.m_end)
        {
          m_outline_end_cap_normal[o] = normal;
        }

      /* the last vertex of an edge does not make a quad.
       */
      if(i + 1 != R.m_end)
        {
          for(unsigned int k = 0; k < 3; ++k)
            {
              pts[vert_offset + k].m_position = src_pts[i].m_p;
              pts[vert_offset + k].m_distance_from_edge_start = src_pts[i].m_distance_from_edge_start;
              pts[vert_offset + k].m_distance_from_outline_start = src_pts[i].m_distance_from_outline_start;
              pts[vert_offset + k].m_depth = depth;
              pts[vert_offset + k].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);

              pts[vert_offset + k + 3].m_position = src_pts[i+1].m_p;
              pts[vert_offset + k + 3].m_distance_from_edge_start = src_pts[i+1].m_distance_from_edge_start;
              pts[vert_offset + k + 3].m_distance_from_outline_start = src_pts[i+1].m_distance_from_outline_start;
              pts[vert_offset + k + 3].m_depth = depth;
              pts[vert_offset + k + 3].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
            }

          pts[vert_offset + 0].m_pre_offset = normal;
          pts[vert_offset + 1].m_pre_offset = -normal;
          pts[vert_offset + 2].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);

          if(i + 2 != R.m_end)
            {
              pts[vert_offset + 3].m_pre_offset = normal;
              pts[vert_offset + 4].m_pre_offset = -normal;
            }
          else
            {
              fastuidraw::vec2 n;
              float n_mag;

              n = fastuidraw::vec2(src_pts[i+1].m_p_t.y(), -src_pts[i+1].m_p_t.x());
              n_mag = n.magnitude();
              if(n_mag < sm_mag_tol)
                {
                  n = normal;
                }
              else
                {
                  n /= n_mag;
                }

              pts[vert_offset + 3].m_pre_offset = n;
              pts[vert_offset + 4].m_pre_offset = -n;
            }
          pts[vert_offset + 5].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);

          pts[vert_offset + 0].m_on_boundary = 1.0f;
          pts[vert_offset + 1].m_on_boundary = -1.0f;
          pts[vert_offset + 2].m_on_boundary = 0.0f;
          pts[vert_offset + 3].m_on_boundary = 1.0f;
          pts[vert_offset + 4].m_on_boundary = -1.0f;
          pts[vert_offset + 5].m_on_boundary = 0.0f;


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

          /* we need to figure out which vertex from the
             next to take for the bevel, this is the
             same computation as needed for doing bevel
             joins.
           */
          CommonJoinData CJ(src_pts[i], src_pts[i+1]);
          unsigned int offset;
          if(CJ.m_lambda < 0.0f)
            {
              //take side with n as negative
              offset = 0;
            }
          else
            {
              //take the side with n as positive
              offset = 1;
            }

          indices[index_offset + 12] = vert_offset + 5;
          indices[index_offset + 13] = vert_offset + offset + 3;
          indices[index_offset + 14] = vert_offset + offset + 6;

          index_offset += 15;
          vert_offset += 6;
        }
      else
        {
          for(unsigned int k = 0; k < 2; ++k)
            {
              pts[vert_offset + k].m_position = src_pts[i].m_p;
              pts[vert_offset + k].m_distance_from_edge_start = src_pts[i].m_distance_from_edge_start;
              pts[vert_offset + k].m_distance_from_outline_start = src_pts[i].m_distance_from_outline_start;
              pts[vert_offset + k].m_depth = depth;
              pts[vert_offset + k].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
            }

          pts[vert_offset + 0].m_pre_offset = normal;
          pts[vert_offset + 1].m_pre_offset = -normal;

          pts[vert_offset + 0].m_on_boundary = 1.0f;
          pts[vert_offset + 1].m_on_boundary = -1.0f;

          vert_offset += 2;
        }
      m_prev_normal = normal;
    }
}

void
EdgeDataCreator::
init_prev_normal(unsigned int outline)
{
  m_prev_normal = fastuidraw::vec2(1.0f, 0.0f);

  /* we need to initialize m_prev_normal
     intelligently for the named outline.
     Basic idea: walk backwars through
     the outline until a "large" enough
     tangent vector is found
   */
  for(fastuidraw::const_c_array<fastuidraw::TessellatedPath::point>::const_reverse_iterator
        iter = m_P.outline_point_data(outline).rbegin(),
        end = m_P.outline_point_data(outline).rend();
      iter != end; ++iter)
    {
      float mag;
      mag = iter->m_p_t.magnitude();
      if(mag >= sm_mag_tol)
        {
          m_prev_normal = iter->m_p_t / mag;
          return;
        }
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

  set_point_type_to_edge_point(pts);
  for(unsigned int o = 0; o < m_P.number_outlines(); ++o)
    {
      init_prev_normal(o);
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
      assert(pts[v].m_depth < close_depth);
      pts[v].m_depth = total - pts[v].m_depth  - 1;
    }

  for(unsigned int v = 0, endv = m_size.pre_close_verts(); v < endv; ++v)
    {
      assert(pts[v].m_depth < pre_close_depth);
      pts[v].m_depth = total - (pts[v].m_depth + close_depth) - 1;
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
      for(unsigned int o = 0, join_id = 0; o < m_P.number_outlines(); ++o)
        {
          for(unsigned int e = 1; e + 1 < m_P.number_edges(o); ++e, ++join_id)
            {
              add_join(join_id, m_P, o, e, m_size.pre_close_verts(), m_size.pre_close_indices());
            }

          if(m_P.number_edges(o) >= 2)
            {
              add_join(join_id, m_P, o, m_P.number_edges(o) - 1,
                       m_size.close_verts(), m_size.close_indices());

              add_join(join_id + 1, m_P, o, m_P.number_edges(o),
                       m_size.close_verts(), m_size.close_indices());

              join_id += 2;
            }
        }
    }
}

void
JoinCreatorBase::
fill_join(unsigned int join_id,
          unsigned int outline, unsigned int edge,
          fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
          unsigned int &depth,
          fastuidraw::c_array<unsigned int> indices,
          unsigned int &vertex_offset, unsigned int &index_offset)
{
  unsigned int v(vertex_offset);
  fill_join_implement(join_id, m_P, outline, edge, pts, indices, vertex_offset, index_offset);
  for(; v < vertex_offset; ++v)
    {
      pts[v].m_depth = depth;
    }
  ++depth;
}

void
JoinCreatorBase::
fill_data(fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
          fastuidraw::c_array<unsigned int> indices,
          unsigned int &pre_close_depth, unsigned int &close_depth)
{
  unsigned int pre_close_vertex(0), pre_close_index(m_size.close_indices());
  unsigned int close_vertex(m_size.pre_close_verts()), close_index(0);
  unsigned int total;

  pre_close_depth = 0;
  close_depth = 0;

  for(unsigned int o = 0, join_id = 0; o < m_P.number_outlines(); ++o)
    {
      for(unsigned int e = 1; e + 1 < m_P.number_edges(o); ++e, ++join_id)
        {
          fill_join(join_id, o, e, pts, pre_close_depth, indices, pre_close_vertex, pre_close_index);
        }

      if(m_P.number_edges(o) >= 2)
        {
          fill_join(join_id, o, m_P.number_edges(o) - 1, pts, close_depth, indices, close_vertex, close_index);
          fill_join(join_id + 1, o, m_P.number_edges(o), pts, close_depth, indices, close_vertex, close_index);
          join_id += 2;
        }
    }

  total = close_depth + pre_close_depth;

  /* the vertices of the closing edge are drawn first,
     so they should have the largest depth value
   */
  for(unsigned int i = m_size.pre_close_verts(); i < pts.size(); ++i)
    {
      assert(pts[i].m_depth < close_depth);
      pts[i].m_depth = total - pts[i].m_depth - 1;
    }
  for(unsigned int i = 0; i < m_size.pre_close_verts(); ++i)
    {
      assert(pts[i].m_depth < pre_close_depth);
      pts[i].m_depth = total - (pts[i].m_depth + close_depth) - 1;
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
            float curve_tessellation):
  CommonJoinData(p0, p1)
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
  std::complex<float> n1z_times_conj_n0z(n1z * std::conj(n0z) );

  m_arc_start = n0z;
  m_delta_theta = std::atan2(n1z_times_conj_n0z.imag(), n1z_times_conj_n0z.real());

  m_num_arc_points = static_cast<unsigned int>(std::abs(m_delta_theta) / curve_tessellation);
  m_num_arc_points = std::max(m_num_arc_points, 3u);

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
  pts[vertex_offset].m_distance_from_outline_start = m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 0.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  pts[vertex_offset].m_position = m_p0;
  pts[vertex_offset].m_pre_offset = m_lambda * m_n0;
  pts[vertex_offset].m_distance_from_edge_start = m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  for(i = 1, theta = m_delta_theta; i < m_num_arc_points - 1; ++i, theta += m_delta_theta, ++vertex_offset)
    {
      float s, c;
      std::complex<float> cs_as_complex;

      s = std::sin(theta);
      c = std::cos(theta);
      cs_as_complex = std::complex<float>(c, s) * m_arc_start;

      pts[vertex_offset].m_position = m_p0;
      pts[vertex_offset].m_pre_offset = fastuidraw::vec2(cs_as_complex.real(), cs_as_complex.imag());
      pts[vertex_offset].m_distance_from_edge_start = m_distance_from_edge_start;
      pts[vertex_offset].m_distance_from_outline_start = m_distance_from_outline_start;
      pts[vertex_offset].m_on_boundary = 1.0f;
      pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
      pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::rounded_join_point;
    }

  pts[vertex_offset].m_position = m_p1;
  pts[vertex_offset].m_pre_offset = m_lambda * m_n1;
  pts[vertex_offset].m_distance_from_edge_start = m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  for(i = first + 1; i < vertex_offset - 1; ++i, index_offset+=3)
    {
      indices[index_offset + 0] = first;
      indices[index_offset + 1] = i;
      indices[index_offset + 2] = i + 1;
    }

}

///////////////////////////////////////////////////
// RoundedJoinCreator methods
RoundedJoinCreator::
RoundedJoinCreator(const fastuidraw::TessellatedPath &P):
  JoinCreatorBase(P)
{
  JoinCount J(P);
  m_per_join_data.reserve(J.m_number_close_joins + J.m_number_non_close_joins);
}

void
RoundedJoinCreator::
add_join(unsigned int join_id,
         const fastuidraw::TessellatedPath &path,
         unsigned int outline, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count)
{
  fastuidraw::range_type<unsigned int> R0, R1;
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(path.point_data());

  (void)join_id;

  assert(edge >= 1);

  R0 = path.edge_range(outline, edge - 1); //end of previous edge
  if(edge != path.number_edges(outline))
    {
      R1 = path.edge_range(outline, edge);
    }
  else
    {
      R1 = path.edge_range(outline, 0);
    }

  const fastuidraw::TessellatedPath::point p0(src_pts[R0.m_end - 1]);
  const fastuidraw::TessellatedPath::point p1(src_pts[R1.m_begin]);
  PerJoinData J(p0, p1, path.tessellation_parameters().m_curve_tessellation);

  assert(m_per_join_data.size() == join_id);
  m_per_join_data.push_back(J);

  /* a triangle fan centered at p0=p1 with
     m_num_arc_points along an edge
  */
  vert_count += (1 + J.m_num_arc_points);
  index_count += 3 * (J.m_num_arc_points - 1);
}


void
RoundedJoinCreator::
fill_join_implement(unsigned int join_id,
                    const fastuidraw::TessellatedPath &path,
                    unsigned int outline, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset)
{
  (void)outline;
  (void)edge;
  (void)path;

  assert(join_id < m_per_join_data.size());
  m_per_join_data[join_id].add_data(pts, vertex_offset, indices, index_offset);
}



///////////////////////////////////////////////////
// BevelJoinCreator methods
BevelJoinCreator::
BevelJoinCreator(const fastuidraw::TessellatedPath &P):
  JoinCreatorBase(P)
{}

void
BevelJoinCreator::
add_join(unsigned int join_id,
         const fastuidraw::TessellatedPath &path,
         unsigned int outline, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count)
{
  (void)join_id;
  (void)path;
  (void)outline;
  (void)edge;

  /* one triangle per bevel join
   */
  vert_count += 3;
  index_count += 3;
}

void
BevelJoinCreator::
fill_join_implement(unsigned int join_id,
                    const fastuidraw::TessellatedPath &path,
                    unsigned int outline, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset)
{
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(path.point_data());
  unsigned int i0, i1;
  fastuidraw::range_type<unsigned int> R0, R1;

  (void)join_id;

  R0 = path.edge_range(outline, edge - 1);
  if(edge != path.number_edges(outline))
    {
      R1 = path.edge_range(outline, edge); //start of current edge
    }
  else
    {
      R1 = path.edge_range(outline, 0);
    }

  i0 = R0.m_end - 1;
  i1 = R1.m_begin;

  CommonJoinData J(src_pts[i0], src_pts[i1]);

  pts[vertex_offset + 0].m_position = J.m_p0;
  pts[vertex_offset + 0].m_pre_offset = J.m_lambda * J.m_n0;
  pts[vertex_offset + 0].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset + 0].m_distance_from_outline_start = J.m_distance_from_outline_start;
  pts[vertex_offset + 0].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;

  pts[vertex_offset + 1].m_position = J.m_p0;
  pts[vertex_offset + 1].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset + 1].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset + 1].m_distance_from_outline_start = J.m_distance_from_outline_start;
  pts[vertex_offset + 1].m_on_boundary = 0.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;

  pts[vertex_offset + 2].m_position = J.m_p1;
  pts[vertex_offset + 2].m_pre_offset = J.m_lambda * J.m_n1;
  pts[vertex_offset + 2].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset + 2].m_distance_from_outline_start = J.m_distance_from_outline_start;
  pts[vertex_offset + 2].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;

  indices[index_offset + 0] = vertex_offset;
  indices[index_offset + 1] = vertex_offset + 1;
  indices[index_offset + 2] = vertex_offset + 2;

  vertex_offset += 3;
  index_offset += 3;
}


///////////////////////////////////////////////
// CapCreatorBase methods
unsigned int
CapCreatorBase::
fill_data(const EdgeDataCreator &edge_creator,
          fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
          fastuidraw::c_array<unsigned int> indices) const
{
  unsigned int vertex_offset(0), index_offset(0), v(0), depth(0);

  for(unsigned int o = 0; o < m_P.number_outlines(); ++o)
    {
      add_cap(edge_creator.outline_begin_cap_normal(o),
              true, m_P.unclosed_outline_point_data(o).front(),
              pts, indices,
              vertex_offset, index_offset);

      for(; v < vertex_offset; ++v)
        {
          pts[v].m_depth = depth;
          ++depth;
        }

      add_cap(edge_creator.outline_end_cap_normal(o),
              false, m_P.unclosed_outline_point_data(o).back(),
              pts, indices,
              vertex_offset, index_offset);

      assert(depth >= 1);
      for(; v < vertex_offset; ++v)
        {
          pts[v].m_depth = depth;
          ++depth;
        }
    }

  assert(vertex_offset == m_size.verts());
  assert(index_offset == m_size.indices());

  for(unsigned int i = 0, endi = pts.size(); i < endi; ++i)
    {
      assert(pts[i].m_depth < depth);
      pts[i].m_depth = depth - pts[i].m_depth - 1;
    }

  return depth;
}

///////////////////////////////////////////////////
// RoundedCapCreator methods
PointIndexCapSize
RoundedCapCreator::
compute_size(const fastuidraw::TessellatedPath &P)
{
  float tc(P.tessellation_parameters().m_curve_tessellation);
  unsigned int num_caps;
  PointIndexCapSize return_value;

  m_num_arc_points_per_cap = static_cast<unsigned int>(static_cast<float>(M_PI) / tc);
  m_num_arc_points_per_cap = std::max(m_num_arc_points_per_cap, 3u);

  m_delta_theta = static_cast<float>(M_PI) / static_cast<float>(m_num_arc_points_per_cap - 1);

  /* each cap is a triangle fan centered at the cap point.
   */
  num_caps = 2 * P.number_outlines();
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
  CommonCapData C(is_starting_cap, p, normal_from_stroking);
  unsigned int i, first;
  float theta;

  first = vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = p.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 0.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = p.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  for(i = 1, theta = m_delta_theta; i < m_num_arc_points_per_cap - 1; ++i, theta += m_delta_theta, ++vertex_offset)
    {
      float s, c;

      s = std::sin(theta);
      c = std::cos(theta);
      pts[vertex_offset].m_position = C.m_p;
      pts[vertex_offset].m_pre_offset = c * C.m_n + s * C.m_v;
      pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
      pts[vertex_offset].m_distance_from_outline_start = p.m_distance_from_outline_start;
      pts[vertex_offset].m_on_boundary = 1.0f;
      pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
      pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::rounded_cap_point;
    }

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = -C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = p.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  for(i = first + 1; i < vertex_offset - 1; ++i, index_offset+=3)
    {
      indices[index_offset + 0] = first;
      indices[index_offset + 1] = i;
      indices[index_offset + 2] = i + 1;
    }
}


///////////////////////////////////////////////////
// SquareCapCreator methods
PointIndexCapSize
SquareCapCreator::
compute_size(const fastuidraw::TessellatedPath &P)
{
  PointIndexCapSize return_value;
  unsigned int num_caps;

  /* each square cap generates 5 new points
     and 3 triangles (= 9 indices)
   */
  num_caps = 2 * P.number_outlines();
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
  CommonCapData C(is_starting_cap, p, normal_from_stroking);
  unsigned int first, i;

  first = vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = p.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 0.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = p.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = p.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = C.m_v;
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::square_cap_point;
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = -C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = p.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = C.m_v;
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::square_cap_point;
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = -C.m_n;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = p.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  for(i = first + 1; i < vertex_offset - 1; ++i, index_offset+=3)
    {
      indices[index_offset + 0] = first;
      indices[index_offset + 1] = i;
      indices[index_offset + 2] = i + 1;
    }
}


///////////////////////////////////////////////////
// MiterJoinCreator methods
MiterJoinCreator::
MiterJoinCreator(const fastuidraw::TessellatedPath &P):
  JoinCreatorBase(P)
{
}

void
MiterJoinCreator::
add_join(unsigned int join_id,
         const fastuidraw::TessellatedPath &path,
         unsigned int outline, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count)
{
  /* Each join is a triangle fan from 5 points
     (thus 3 triangles, which is 9 indices)
   */
  (void)join_id;
  (void)path;
  (void)outline;
  (void)edge;

  vert_count += 5;
  index_count += 9;
}


void
MiterJoinCreator::
fill_join_implement(unsigned int join_id,
                    const fastuidraw::TessellatedPath &path,
                    unsigned int outline, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset)
{
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(path.point_data());
  unsigned int i, i0, i1, first;
  fastuidraw::range_type<unsigned int> R0, R1;

  (void)join_id;

  R0 = path.edge_range(outline, edge - 1);
  if(edge != path.number_edges(outline))
    {
      R1 = path.edge_range(outline, edge); //start of current edge
    }
  else
    {
      R1 = path.edge_range(outline, 0);
    }

  i0 = R0.m_end - 1;
  i1 = R1.m_begin;

  CommonJoinData J(src_pts[i0], src_pts[i1]);

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
  pts[vertex_offset].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = J.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 0.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  // join point from curve into join
  pts[vertex_offset].m_position = J.m_p0;
  pts[vertex_offset].m_pre_offset = J.m_lambda * J.m_n0;
  pts[vertex_offset].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = J.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  // miter point A
  pts[vertex_offset].m_position = J.m_p0;
  pts[vertex_offset].m_pre_offset = J.m_n0;
  pts[vertex_offset].m_auxilary_offset = J.m_n1;
  pts[vertex_offset].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = J.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::miter_join_point;
  ++vertex_offset;

  // miter point B
  pts[vertex_offset].m_position = J.m_p1;
  pts[vertex_offset].m_pre_offset = J.m_n0;
  pts[vertex_offset].m_auxilary_offset = J.m_n1;
  pts[vertex_offset].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = J.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::miter_join_point;
  ++vertex_offset;

  // join point from curve out from join
  pts[vertex_offset].m_position = J.m_p1;
  pts[vertex_offset].m_pre_offset = J.m_lambda * J.m_n1;
  pts[vertex_offset].m_distance_from_edge_start = J.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_outline_start = J.m_distance_from_outline_start;
  pts[vertex_offset].m_on_boundary = 1.0f;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_point_type = fastuidraw::StrokedPath::edge_point;
  ++vertex_offset;

  for(i = first + 1; i < vertex_offset - 1; ++i, index_offset+=3)
    {
      indices[index_offset + 0] = first;
      indices[index_offset + 1] = i;
      indices[index_offset + 2] = i + 1;
    }

}

/////////////////////////////////////////////
// StrokedPathPrivate methods
StrokedPathPrivate::
StrokedPathPrivate(const fastuidraw::TessellatedPath &P):
  m_attribute_data(NULL)
{
  if(P.number_outlines() == 0)
    {
      return;
    }

  EdgeDataCreator e(P);
  m_edges.resize(e.sizes());
  e.fill_data(m_edges.m_points.data(true),
              m_edges.m_indices.data(true),
              m_edges.number_depth(false),
              m_edges.number_depth(true));

  RoundedJoinCreator r(P);
  m_rounded_joins.resize(r.sizes());
  r.fill_data(m_rounded_joins.m_points.data(true),
              m_rounded_joins.m_indices.data(true),
              m_rounded_joins.number_depth(false),
              m_rounded_joins.number_depth(true));

  BevelJoinCreator b(P);
  m_bevel_joins.resize(b.sizes());
  b.fill_data(m_bevel_joins.m_points.data(true),
              m_bevel_joins.m_indices.data(true),
              m_bevel_joins.number_depth(false),
              m_bevel_joins.number_depth(true));

  MiterJoinCreator m(P);
  m_miter_joins.resize(m.sizes());
  m.fill_data(m_miter_joins.m_points.data(true),
              m_miter_joins.m_indices.data(true),
              m_miter_joins.number_depth(false),
              m_miter_joins.number_depth(true));

  RoundedCapCreator rc(P);
  m_rounded_cap.resize(rc.sizes());
  m_cap_number_depth = rc.fill_data(e,
                                    fastuidraw::make_c_array(m_rounded_cap.m_points),
                                    fastuidraw::make_c_array(m_rounded_cap.m_indices));

  SquareCapCreator sc(P);
  m_square_cap.resize(sc.sizes());
  sc.fill_data(e,
               fastuidraw::make_c_array(m_square_cap.m_points),
               fastuidraw::make_c_array(m_square_cap.m_indices));

}

StrokedPathPrivate::
~StrokedPathPrivate()
{
  if(m_attribute_data != NULL)
    {
      FASTUIDRAWdelete(m_attribute_data);
    }
}

//////////////////////////////////////
// fastuidraw::StrokedPath::point methods
float
fastuidraw::StrokedPath::point::
miter_distance(void) const
{
  if(m_point_type == miter_join_point)
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

//////////////////////////////////////////////////////////////
// fastuidraw::StrokedPath methods
fastuidraw::StrokedPath::
StrokedPath(const fastuidraw::TessellatedPath &P)
{
  m_d = FASTUIDRAWnew StrokedPathPrivate(P);
}

fastuidraw::StrokedPath::
~StrokedPath()
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
painter_data(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  /* Painful note: the reference count is initialized as 0.
     If a handle is made at ctor, the reference count is made
     to be 1, and then when the handle goes out of scope
     it is zero, triggering delete. In particular making a
     handle at ctor time is very bad. This is one of the reasons
     why it must be made lazily and not at ctor.
   */
  if(d->m_attribute_data == NULL)
    {
      d->m_attribute_data = FASTUIDRAWnew PainterAttributeData();
      d->m_attribute_data->set_data(this);
    }
  return *d->m_attribute_data;
}

fastuidraw::const_c_array<fastuidraw::StrokedPath::point>
fastuidraw::StrokedPath::
edge_points(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_edges.m_points.data(including_closing_edge);
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::
edge_indices(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_edges.m_indices.data(including_closing_edge);
}

unsigned int
fastuidraw::StrokedPath::
edge_number_depth(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_edges.number_depth(including_closing_edge);
}

fastuidraw::const_c_array<fastuidraw::StrokedPath::point>
fastuidraw::StrokedPath::
rounded_joins_points(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_rounded_joins.m_points.data(including_closing_edge);
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::
rounded_joins_indices(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_rounded_joins.m_indices.data(including_closing_edge);
}

unsigned int
fastuidraw::StrokedPath::
rounded_join_number_depth(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_rounded_joins.number_depth(including_closing_edge);
}

fastuidraw::const_c_array<fastuidraw::StrokedPath::point>
fastuidraw::StrokedPath::
bevel_joins_points(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_bevel_joins.m_points.data(including_closing_edge);
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::
bevel_joins_indices(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_bevel_joins.m_indices.data(including_closing_edge);
}

unsigned int
fastuidraw::StrokedPath::
bevel_join_number_depth(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_bevel_joins.number_depth(including_closing_edge);
}

fastuidraw::const_c_array<fastuidraw::StrokedPath::point>
fastuidraw::StrokedPath::
miter_joins_points(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_miter_joins.m_points.data(including_closing_edge);
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::
miter_joins_indices(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_miter_joins.m_indices.data(including_closing_edge);
}

unsigned int
fastuidraw::StrokedPath::
miter_join_number_depth(bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_miter_joins.number_depth(including_closing_edge);
}

fastuidraw::const_c_array<fastuidraw::StrokedPath::point>
fastuidraw::StrokedPath::
rounded_cap_points(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return make_c_array(d->m_rounded_cap.m_points);
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::
rounded_cap_indices(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return make_c_array(d->m_rounded_cap.m_indices);
}

fastuidraw::const_c_array<fastuidraw::StrokedPath::point>
fastuidraw::StrokedPath::
square_cap_points(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return make_c_array(d->m_square_cap.m_points);
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::
square_cap_indices(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return make_c_array(d->m_square_cap.m_indices);
}

unsigned int
fastuidraw::StrokedPath::
cap_number_depth(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->m_cap_number_depth;
}
