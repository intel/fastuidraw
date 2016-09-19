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

namespace
{
  enum joint_type_t
    {
      rounded_join = 0,
      miter_join,
      bevel_join,
      cap_join,

      joint_type_count
    };

  enum cap_type_t
    {
      rounded_cap = 0,
      square_cap,

      cap_type_count
    };

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

  class Location
  {
  public:
    Location(void):
      m_attribs(0, 0),
      m_indices(0, 0)
    {}
    fastuidraw::range_type<unsigned int> m_attribs, m_indices;
  };

  class LocationsOfJoins
  {
  public:
    fastuidraw::vecN<Location, joint_type_count + 1> m_values;
  };

  class LocationsOfCaps
  {
  public:
    /* 0 -> front, 1 -> back
     */
    fastuidraw::vecN<Location, 2> m_values;
  };

  class LocationsOfCapsAndJoins
  {
  public:
    std::vector<LocationsOfJoins> m_joins;
    fastuidraw::vecN<LocationsOfCaps, cap_type_count> m_caps;

    const Location&
    fetch(enum fastuidraw::StrokedPath::point_set_t tp, unsigned int J);

    static
    enum joint_type_t
    get_join_type_t(enum fastuidraw::StrokedPath::point_set_t tp);

    static
    enum cap_type_t
    get_cap_type_t(enum fastuidraw::StrokedPath::point_set_t tp);
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

  class PerContourData
  {
  public:
    std::vector<fastuidraw::vec2> m_edge_begin_normal, m_edge_end_normal;
    fastuidraw::vec2 m_begin_cap_normal, m_end_cap_normal;
  };

  class EdgeDataCreator
  {
  public:
    static const float sm_mag_tol;

    EdgeDataCreator(const fastuidraw::TessellatedPath &P):
      m_P(P),
      m_per_contour_data(m_P.number_contours())
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

    const PerContourData&
    per_contour_data(unsigned int contour) const
    {
      return m_per_contour_data[contour];
    }

  private:

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
    fastuidraw::vec2 m_prev_normal;
    std::vector<PerContourData> m_per_contour_data;
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
    JoinCreatorBase(const fastuidraw::TessellatedPath &P,
                    const EdgeDataCreator &e):
      m_P(P),
      m_e(e),
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
              std::vector<LocationsOfCapsAndJoins> &locations,
              enum joint_type_t Jtype);

  private:

    virtual
    void
    post_assign_depth(unsigned int &depth,
                      fastuidraw::c_array<fastuidraw::StrokedPath::point> pts);

    virtual
    void
    add_join(unsigned int join_id,
             const fastuidraw::TessellatedPath &path,
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
                        const fastuidraw::TessellatedPath &path,
                        unsigned int contour, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) = 0;

    void
    compute_size(void);

    const fastuidraw::TessellatedPath &m_P;
    const EdgeDataCreator &m_e;
    PointIndexSize m_size;
    bool m_size_ready;
  };


  class RoundedJoinCreator:public JoinCreatorBase
  {
  public:
    RoundedJoinCreator(const fastuidraw::TessellatedPath &P,
                       const EdgeDataCreator &e);

  private:

    class PerJoinData:public CommonJoinData
    {
    public:
      PerJoinData(const fastuidraw::TessellatedPath::point &p0,
                  const fastuidraw::TessellatedPath::point &p1,
                  const fastuidraw::vec2 &n0_from_stroking,
                  const fastuidraw::vec2 &n1_from_stroking,
                  float curve_tessellation);

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
             const fastuidraw::TessellatedPath &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const fastuidraw::TessellatedPath &path,
                        unsigned int contour, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset);

    std::vector<PerJoinData> m_per_join_data;
  };

  class BevelJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    BevelJoinCreator(const fastuidraw::TessellatedPath &P,
                     const EdgeDataCreator &e);

  private:

    virtual
    void
    add_join(unsigned int join_id,
             const fastuidraw::TessellatedPath &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const fastuidraw::TessellatedPath &path,
                        unsigned int contour, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset);

    std::vector<fastuidraw::vec2> m_n0, m_n1;
  };

  class CapJoinCreator:public JoinCreatorBase
  {
  public:
    enum
      {
        number_points_per_fan = 6,
        number_triangles_per_fan = number_points_per_fan - 2,
        number_indices_per_fan = 3 * number_triangles_per_fan,
        number_points_per_join = 2 * number_points_per_fan,
        number_indices_per_join = 2 * number_indices_per_fan
      };

    explicit
    CapJoinCreator(const fastuidraw::TessellatedPath &P,
                   const EdgeDataCreator &e);

  private:

    virtual
    void
    post_assign_depth(unsigned int &depth,
                      fastuidraw::c_array<fastuidraw::StrokedPath::point> pts);

    virtual
    void
    add_join(unsigned int join_id,
             const fastuidraw::TessellatedPath &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const fastuidraw::TessellatedPath &path,
                        unsigned int contour, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset);
    void
    pack_fan(bool leaving_join,
             const fastuidraw::TessellatedPath::point &edge_pt,
             const fastuidraw::vec2 &stroking_normal,
             fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
             unsigned int &vertex_offset,
             fastuidraw::c_array<unsigned int> indices,
             unsigned int &index_offset);

    std::vector<fastuidraw::vec2> m_n0, m_n1;
  };

  class CommonCapData
  {
  public:
    CommonCapData(bool is_start_cap,
                  const fastuidraw::TessellatedPath::point &src_pt,
                  const fastuidraw::vec2 &normal_from_stroking):
      m_is_start_cap(is_start_cap),
      m_lambda((is_start_cap) ? -1.0f : 1.0f),
      m_p(src_pt.m_p),
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
    CapCreatorBase(const fastuidraw::TessellatedPath &P,
                   PointIndexCapSize sz):
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
    fill_data(const EdgeDataCreator &edge_creator,
              fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
              fastuidraw::c_array<unsigned int> indices,
              std::vector<LocationsOfCapsAndJoins> &locations,
              enum cap_type_t cp) const;

  private:

    void
    add_cap_and_set_location(const fastuidraw::vec2 &normal_from_stroking,
                             bool is_starting_cap,
                             const fastuidraw::TessellatedPath::point &p0,
                             fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                             fastuidraw::c_array<unsigned int> indices,
                             unsigned int &vertex_offset,
                             unsigned int &index_offset,
                             Location &location) const;

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

  };

  class RoundedCapCreator:public CapCreatorBase
  {
  public:
    RoundedCapCreator(const fastuidraw::TessellatedPath &P):
      CapCreatorBase(P, compute_size(P))
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
      CapCreatorBase(P, compute_size(P))
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
    MiterJoinCreator(const fastuidraw::TessellatedPath &P,
                     const EdgeDataCreator &e);

  private:
    virtual
    void
    add_join(unsigned int join_id,
             const fastuidraw::TessellatedPath &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count);

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const fastuidraw::TessellatedPath &path,
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

  class CapData
  {
  public:
    std::vector<fastuidraw::StrokedPath::point> m_points;
    std::vector<unsigned int> m_indices;
    unsigned int m_number_depth;

    void
    resize(fastuidraw::uvec2 szs)
    {
      m_points.resize(szs[0]);
      m_indices.resize(szs[1]);
    }

    void
    compute_conveniance(DataAsCArraysPair &out_value)
    {
      //Have that CapData is for both closing and non-closing data
      out_value[true].m_points  = out_value[false].m_points = fastuidraw::make_c_array(m_points);
      out_value[true].m_indices = out_value[false].m_indices = fastuidraw::make_c_array(m_indices);
      out_value[true].m_number_depth = out_value[false].m_number_depth = m_number_depth;
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
    Data<fastuidraw::StrokedPath::point> m_cap_joins;

    CapData m_rounded_cap;
    CapData m_square_cap;
    std::vector<LocationsOfCapsAndJoins> m_locations;

    fastuidraw::vecN<DataAsCArraysPair, fastuidraw::StrokedPath::number_point_set_types> m_return_values;
    fastuidraw::PainterAttributeData *m_attribute_data;
  };

}

//////////////////////////////////
// LocationsOfCapsAndJoins methods
enum joint_type_t
LocationsOfCapsAndJoins::
get_join_type_t(enum fastuidraw::StrokedPath::point_set_t tp)
{
  switch(tp)
    {
    case fastuidraw::StrokedPath::bevel_join_point_set:
      return bevel_join;

    case fastuidraw::StrokedPath::rounded_join_point_set:
      return rounded_join;

    case fastuidraw::StrokedPath::miter_join_point_set:
      return miter_join;

    case fastuidraw::StrokedPath::cap_join_point_set:
      return cap_join;

    default:
      assert(!"Passed a non-joint type to get_join_type_t");
      return joint_type_count;
    }
}

enum cap_type_t
LocationsOfCapsAndJoins::
get_cap_type_t(enum fastuidraw::StrokedPath::point_set_t tp)
{
  switch(tp)
    {
    case fastuidraw::StrokedPath::square_cap_point_set:
      return square_cap;

    case fastuidraw::StrokedPath::rounded_cap_point_set:
      return rounded_cap;

    default:
      assert(!"Passed a non-cap type to get_cap_type_t");
      return cap_type_count;
    }
}

const Location&
LocationsOfCapsAndJoins::
fetch(enum fastuidraw::StrokedPath::point_set_t tp, unsigned int N)
{
  switch(tp)
    {
    case fastuidraw::StrokedPath::square_cap_point_set:
    case fastuidraw::StrokedPath::rounded_cap_point_set:
      {
        // NOTE: m_caps is indexed by cap_type_t
        // and m_values holds the front and back
        // cap locations.
        enum cap_type_t C;
        C = get_cap_type_t(tp);
        return m_caps[C].m_values[N != 0];
      }
    default:
      {
        // NOTE: m_joins is indexed by which join
        // and m_values is indexed by joint_type_t
        enum joint_type_t J;
        J = get_join_type_t(tp);
        return m_joins[N].m_values[J];
      }
    }
}

//////////////////////////////////////////////
// JoinCount methods
JoinCount::
JoinCount(const fastuidraw::TessellatedPath &P):
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
      m_per_contour_data[o].m_edge_begin_normal.resize(m_P.number_edges(o), fastuidraw::vec2(999,999));
      m_per_contour_data[o].m_edge_end_normal.resize(m_P.number_edges(o), fastuidraw::vec2(111,111));
      for(unsigned int e = 0; e < m_P.number_edges(o); ++e)
        {
          fastuidraw::range_type<unsigned int> R;

          R = m_P.edge_range(o, e);
          assert(R.m_end > R.m_begin);

          /* Each sub-edge of the tessellation produces -6- points:
             (p,n), (p,-n), (p, 0),
             (p_next, -n), (p_next, n), (p_next, 0)

             where n = J(v), v = (p - p_next) / || p-p_next ||

             There are 5 triangles:
               4 triangles for the 2 quads formed from the 6 points
               1 triangle for the bevel-like join between the two quads
               However, the last triangle is not present on the last
               edge segment.
          */
          if(e + 1 == m_P.number_edges(o))
            {
              m_size.close_verts() += 6 * ((R.m_end - R.m_begin) - 1);
              m_size.close_indices() += 15 * ((R.m_end - R.m_begin) - 1) - 3;
            }
          else
            {
              m_size.pre_close_verts() += 6 * ((R.m_end - R.m_begin) - 1);
              m_size.pre_close_indices() += 15 * ((R.m_end - R.m_begin) - 1) - 3;
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
  fastuidraw::vec2 normal(1.0f, 0.0f);

  R = m_P.edge_range(o, e);
  assert(R.m_end > R.m_begin);

  for(unsigned int i = R.m_begin; i + 1 < R.m_end; ++i, ++depth)
    {
      /* for the edge connecting src_pts[i] to src_pts[i+1]
       */
      fastuidraw::vec2 delta;

      delta = src_pts[i+1].m_p - src_pts[i].m_p;
      if(delta.magnitudeSq() >= sm_mag_tol * sm_mag_tol)
        {
          normal = fastuidraw::vec2(-delta.y(), delta.x());
        }
      else
        {
          if(src_pts[i].m_p_t.magnitudeSq() >= sm_mag_tol * sm_mag_tol)
            {
              normal = fastuidraw::vec2(-src_pts[i].m_p_t.y(), src_pts[i].m_p_t.x());
            }
        }
      normal.normalize();

      if(i == R.m_begin)
        {
          m_per_contour_data[o].m_edge_begin_normal[e] = normal;
          if(e == 0)
            {
              m_per_contour_data[o].m_begin_cap_normal = normal;
            }
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
          pts[vert_offset + k].m_packed_data = pack_data(boundary_values[k], fastuidraw::StrokedPath::offset_edge, depth);

          pts[vert_offset + k + 3].m_position = src_pts[i + 1].m_p;
          pts[vert_offset + k + 3].m_distance_from_edge_start = src_pts[i + 1].m_distance_from_edge_start;
          pts[vert_offset + k + 3].m_distance_from_contour_start = src_pts[i + 1].m_distance_from_contour_start;
          pts[vert_offset + k + 3].m_edge_length = src_pts[i + 1].m_edge_length;
          pts[vert_offset + k + 3].m_open_contour_length = src_pts[i + 1].m_open_contour_length;
          pts[vert_offset + k + 3].m_closed_contour_length = src_pts[i + 1].m_closed_contour_length;
          pts[vert_offset + k + 3].m_pre_offset = normal_sign[k] * normal;
          pts[vert_offset + k + 3].m_auxilary_offset = -delta;
          pts[vert_offset + k + 3].m_packed_data = pack_data(boundary_values[k], fastuidraw::StrokedPath::offset_next_edge, depth);
        }

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
      index_offset += 12;

      if(i + 2 != R.m_end)
        {
          unsigned int offset;
          float lambda;
          lambda = CommonJoinData::compute_lambda(src_pts[i].m_p_t, src_pts[i+1].m_p_t);
          if(lambda > 0.0f)
            {
              //take side with n as negative
              offset = 0;
            }
          else
            {
              //take the side with n as positive
              offset = 1;
            }

          indices[index_offset + 0] = vert_offset + 5;
          indices[index_offset + 1] = vert_offset + offset + 3;
          indices[index_offset + 2] = vert_offset + offset + 6;
          index_offset += 3;
        }
      vert_offset += 6;
    }

  if(R.m_begin + 1 >= R.m_end)
    {
      normal = fastuidraw::vec2(-src_pts[R.m_begin].m_p_t.y(), src_pts[R.m_begin].m_p_t.x());
      normal.normalize();
      m_per_contour_data[o].m_edge_begin_normal[e] = normal;
      if(e == 0)
        {
          m_per_contour_data[o].m_begin_cap_normal = normal;
        }
    }

  m_per_contour_data[o].m_edge_end_normal[e] = normal;
  if(e + 2 == m_P.number_edges(o))
    {
      m_per_contour_data[o].m_end_cap_normal = normal;
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
                       m_e.per_contour_data(o).m_edge_end_normal[e - 1],
                       m_e.per_contour_data(o).m_edge_begin_normal[e],
                       o, e, m_size.pre_close_verts(), m_size.pre_close_indices());
            }

          if(m_P.number_edges(o) >= 2)
            {
              add_join(join_id, m_P,
                       m_e.per_contour_data(o).m_edge_end_normal[m_P.number_edges(o) - 2],
                       m_e.per_contour_data(o).m_edge_begin_normal[m_P.number_edges(o) - 1],
                       o, m_P.number_edges(o) - 1,
                       m_size.close_verts(), m_size.close_indices());

              add_join(join_id + 1, m_P,
                       m_e.per_contour_data(o).m_edge_end_normal.back(),
                       m_e.per_contour_data(o).m_edge_begin_normal.front(),
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
  uint32_t ss;
  ss = static_cast<uint32_t>(fastuidraw::StrokedPath::skip_dash_computation_mask)
    | fastuidraw::StrokedPath::join_mask;

  for(unsigned int v = 0; v < pts.size(); ++v)
    {
      assign_depth(pts[v], depth);
      pts[v].m_packed_data |= ss;
    }
  ++depth;
}

void
JoinCreatorBase::
fill_data(fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
          fastuidraw::c_array<unsigned int> indices,
          unsigned int &pre_close_depth, unsigned int &close_depth,
          std::vector<LocationsOfCapsAndJoins> &locations,
          enum joint_type_t Jtype)
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
                    locations[o].m_joins[e - 1].m_values[Jtype]);
        }

      if(m_P.number_edges(o) >= 2)
        {
          fill_join(join_id, o, m_P.number_edges(o) - 1, pts, close_depth, indices,
                    close_vertex, close_index,
                    locations[o].m_joins[m_P.number_edges(o) - 2].m_values[Jtype]);

          fill_join(join_id + 1, o, m_P.number_edges(o), pts, close_depth, indices,
                    close_vertex, close_index,
                    locations[o].m_joins[m_P.number_edges(o) - 1].m_values[Jtype]);

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
            float curve_tessellation):
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

  for(i = first + 1; i < vertex_offset - 1; ++i, index_offset += 3)
    {
      indices[index_offset + 0] = first;
      indices[index_offset + 1] = i;
      indices[index_offset + 2] = i + 1;
    }

}

///////////////////////////////////////////////////
// RoundedJoinCreator methods
RoundedJoinCreator::
RoundedJoinCreator(const fastuidraw::TessellatedPath &P,
                   const EdgeDataCreator &e):
  JoinCreatorBase(P, e)
{
  JoinCount J(P);
  m_per_join_data.reserve(J.m_number_close_joins + J.m_number_non_close_joins);
}

void
RoundedJoinCreator::
add_join(unsigned int join_id,
         const fastuidraw::TessellatedPath &path,
         const fastuidraw::vec2 &n0_from_stroking,
         const fastuidraw::vec2 &n1_from_stroking,
         unsigned int contour, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count)
{
  fastuidraw::range_type<unsigned int> R0, R1;
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(path.point_data());

  (void)join_id;

  assert(edge >= 1);

  R0 = path.edge_range(contour, edge - 1); //end of previous edge
  if(edge != path.number_edges(contour))
    {
      R1 = path.edge_range(contour, edge);
    }
  else
    {
      R1 = path.edge_range(contour, 0);
    }

  PerJoinData J(src_pts[R0.m_end - 1], src_pts[R1.m_begin],
        n0_from_stroking, n1_from_stroking,
        path.tessellation_parameters().m_curve_tessellation);

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
BevelJoinCreator(const fastuidraw::TessellatedPath &P,
                 const EdgeDataCreator &e):
  JoinCreatorBase(P, e)
{}

void
BevelJoinCreator::
add_join(unsigned int join_id,
         const fastuidraw::TessellatedPath &path,
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
                    const fastuidraw::TessellatedPath &path,
                    unsigned int contour, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset)
{
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(path.point_data());
  unsigned int i0, i1;
  fastuidraw::range_type<unsigned int> R0, R1;

  (void)join_id;

  R0 = path.edge_range(contour, edge - 1);
  if(edge != path.number_edges(contour))
    {
      R1 = path.edge_range(contour, edge); //start of current edge
    }
  else
    {
      R1 = path.edge_range(contour, 0);
    }

  i0 = R0.m_end - 1;
  i1 = R1.m_begin;

  CommonJoinData J(src_pts[i0].m_p, m_n0[join_id],
                   src_pts[i1].m_p, m_n1[join_id],
                   src_pts[i0].m_distance_from_edge_start,
                   src_pts[i0].m_distance_from_contour_start,
                   //using p0 to decide the edge length, as
                   //we think of the join as ending an edge.
                   src_pts[i0].m_edge_length,
                   src_pts[i0].m_open_contour_length,
                   src_pts[i0].m_closed_contour_length);

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

  indices[index_offset + 0] = vertex_offset;
  indices[index_offset + 1] = vertex_offset + 1;
  indices[index_offset + 2] = vertex_offset + 2;

  vertex_offset += 3;
  index_offset += 3;
}

///////////////////////////////////////////////
// CapJoinCreator methods
CapJoinCreator::
CapJoinCreator(const fastuidraw::TessellatedPath &P,
               const EdgeDataCreator &e):
  JoinCreatorBase(P, e)
{}


void
CapJoinCreator::
post_assign_depth(unsigned int &depth,
                  fastuidraw::c_array<fastuidraw::StrokedPath::point> pts)
{
  assert(pts.size() == number_points_per_join);
  for(unsigned int v = 0; v < number_points_per_fan; ++v)
    {
      assign_depth(pts[v], depth);
    }
  for(unsigned int v = number_points_per_fan; v < pts.size(); ++v)
    {
      assign_depth(pts[v], depth + 1u);
    }
  depth += 2u;
}

void
CapJoinCreator::
add_join(unsigned int join_id,
         const fastuidraw::TessellatedPath &path,
         const fastuidraw::vec2 &n0_from_stroking,
         const fastuidraw::vec2 &n1_from_stroking,
         unsigned int contour, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count)
{
  (void)join_id;
  (void)path;
  (void)contour;
  (void)edge;

  vert_count += number_points_per_join;
  index_count += number_indices_per_join;

  m_n0.push_back(n0_from_stroking);
  m_n1.push_back(n1_from_stroking);
}

void
CapJoinCreator::
fill_join_implement(unsigned int join_id,
                    const fastuidraw::TessellatedPath &path,
                    unsigned int contour, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset)
{
  /* a cap join is two quads. Each quad is extending an
     edge of the join. The offset data is just like an
     edge. The main catch is that fastuidraw::Painter
     will be copying the data and modifying it before
     sending it to PainterPacker; the modification is
     to change the length of the join so that caps
     induced from dashed stroking are correctly handled.
   */
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(path.point_data());
  unsigned int i0, i1;
  fastuidraw::range_type<unsigned int> R0, R1;

  /* R0 : the range into src_pts for the edge into the join
     R1 : the range into src_pts for the edge out of the join
   */
  R0 = path.edge_range(contour, edge - 1);
  if(edge != path.number_edges(contour))
    {
      R1 = path.edge_range(contour, edge);
    }
  else
    {
      R1 = path.edge_range(contour, 0);
    }
  i0 = R0.m_end - 1;
  i1 = R1.m_begin;

  pack_fan(false, src_pts[i0], m_n0[join_id], pts, vertex_offset, indices, index_offset);
  pack_fan(true , src_pts[i1], m_n1[join_id], pts, vertex_offset, indices, index_offset);
}

void
CapJoinCreator::
pack_fan(bool leaving_join,
         const fastuidraw::TessellatedPath::point &p,
         const fastuidraw::vec2 &stroking_normal,
         fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
         unsigned int &vertex_offset,
         fastuidraw::c_array<unsigned int> indices,
         unsigned int &index_offset)
{
  CommonCapData C(leaving_join, p, stroking_normal);
  unsigned int first(vertex_offset), i;
  enum fastuidraw::StrokedPath::offset_type_t cp_js_type;

  cp_js_type = leaving_join ?
    fastuidraw::StrokedPath::offset_cap_leaving_join :
    fastuidraw::StrokedPath::offset_cap_entering_join;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(0, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = C.m_n;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);
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
  pts[vertex_offset].m_pre_offset = -C.m_n;
  pts[vertex_offset].m_auxilary_offset = C.m_v;
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(1, cp_js_type);
  ++vertex_offset;

  pts[vertex_offset].m_position = C.m_p;
  pts[vertex_offset].m_pre_offset = -C.m_n;
  pts[vertex_offset].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pts[vertex_offset].m_distance_from_edge_start = p.m_distance_from_edge_start;
  pts[vertex_offset].m_distance_from_contour_start = p.m_distance_from_contour_start;
  pts[vertex_offset].m_edge_length = p.m_edge_length;
  pts[vertex_offset].m_open_contour_length = p.m_open_contour_length;
  pts[vertex_offset].m_closed_contour_length = p.m_closed_contour_length;
  pts[vertex_offset].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge);
  ++vertex_offset;

  for(i = first + 1; i < vertex_offset - 1; ++i, index_offset += 3)
    {
      indices[index_offset + 0] = first;
      indices[index_offset + 1] = i;
      indices[index_offset + 2] = i + 1;
    }
}

///////////////////////////////////////////////
// CapCreatorBase methods
void
CapCreatorBase::
add_cap_and_set_location(const fastuidraw::vec2 &normal_from_stroking,
                         bool is_starting_cap,
                         const fastuidraw::TessellatedPath::point &p0,
                         fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                         fastuidraw::c_array<unsigned int> indices,
                         unsigned int &vertex_offset,
                         unsigned int &index_offset,
                         Location &location) const
{
  location.m_attribs.m_begin = vertex_offset;
  location.m_indices.m_begin = index_offset;

  add_cap(normal_from_stroking, is_starting_cap, p0,
          pts, indices, vertex_offset, index_offset);

  location.m_attribs.m_end = vertex_offset;
  location.m_indices.m_end = index_offset;
}

unsigned int
CapCreatorBase::
fill_data(const EdgeDataCreator &edge_creator,
          fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
          fastuidraw::c_array<unsigned int> indices,
          std::vector<LocationsOfCapsAndJoins> &locations,
          enum cap_type_t cp) const
{
  unsigned int vertex_offset(0), index_offset(0), v(0), depth(0);

  for(unsigned int o = 0; o < m_P.number_contours(); ++o)
    {
      add_cap_and_set_location(edge_creator.per_contour_data(o).m_begin_cap_normal,
                               true, m_P.unclosed_contour_point_data(o).front(),
                               pts, indices,
                               vertex_offset, index_offset,
                               locations[o].m_caps[cp].m_values[0]);

      for(; v < vertex_offset; ++v)
        {
          assign_depth(pts[v], depth);
          ++depth;
        }

      add_cap_and_set_location(edge_creator.per_contour_data(o).m_end_cap_normal,
                               false, m_P.unclosed_contour_point_data(o).back(),
                               pts, indices,
                               vertex_offset, index_offset,
                               locations[o].m_caps[cp].m_values[1]);

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
  CommonCapData C(is_starting_cap, p, normal_from_stroking);
  unsigned int i, first;
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

  for(i = first + 1; i < vertex_offset - 1; ++i, index_offset += 3)
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
  CommonCapData C(is_starting_cap, p, normal_from_stroking);
  unsigned int first, i;

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

  for(i = first + 1; i < vertex_offset - 1; ++i, index_offset += 3)
    {
      indices[index_offset + 0] = first;
      indices[index_offset + 1] = i;
      indices[index_offset + 2] = i + 1;
    }
}


///////////////////////////////////////////////////
// MiterJoinCreator methods
MiterJoinCreator::
MiterJoinCreator(const fastuidraw::TessellatedPath &P,
                 const EdgeDataCreator &e):
  JoinCreatorBase(P, e)
{
}

void
MiterJoinCreator::
add_join(unsigned int join_id,
         const fastuidraw::TessellatedPath &path,
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
                    const fastuidraw::TessellatedPath &path,
                    unsigned int contour, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::StrokedPath::point> pts,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset)
{
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(path.point_data());
  unsigned int i, i0, i1, first;
  fastuidraw::range_type<unsigned int> R0, R1;

  FASTUIDRAWunused(join_id);

  /* R0 : the range into src_pts for the edge into the join
     R1 : the range into src_pts for the edge out of the join
   */
  R0 = path.edge_range(contour, edge - 1);
  if(edge != path.number_edges(contour))
    {
      R1 = path.edge_range(contour, edge);
    }
  else
    {
      R1 = path.edge_range(contour, 0);
    }
  i0 = R0.m_end - 1;
  i1 = R1.m_begin;

  CommonJoinData J(src_pts[i0].m_p, m_n0[join_id],
                   src_pts[i1].m_p, m_n1[join_id],
                   src_pts[i0].m_distance_from_edge_start,
                   src_pts[i0].m_distance_from_contour_start,
                   //using p0 to decide the edge length, as
                   //we think of the join as ending an edge.
                   src_pts[i0].m_edge_length,
                   src_pts[i0].m_open_contour_length,
                   src_pts[i0].m_closed_contour_length);

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

  for(i = first + 1; i < vertex_offset - 1; ++i, index_offset += 3)
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
  if(P.number_contours() == 0)
    {
      return;
    }

  m_locations.resize(P.number_contours());
  for(unsigned int C = 0, endC = P.number_contours(); C < endC; ++C)
    {
      m_locations[C].m_joins.resize(P.number_edges(C));
    }

  EdgeDataCreator e(P);
  m_edges.resize(e.sizes());
  e.fill_data(m_edges.m_points.data(true),
              m_edges.m_indices.data(true),
              m_edges.m_number_depth[false],
              m_edges.m_number_depth[true]);

  RoundedJoinCreator r(P, e);
  m_rounded_joins.resize(r.sizes());
  r.fill_data(m_rounded_joins.m_points.data(true),
              m_rounded_joins.m_indices.data(true),
              m_rounded_joins.m_number_depth[false],
              m_rounded_joins.m_number_depth[true],
              m_locations, rounded_join);

  BevelJoinCreator b(P, e);
  m_bevel_joins.resize(b.sizes());
  b.fill_data(m_bevel_joins.m_points.data(true),
              m_bevel_joins.m_indices.data(true),
              m_bevel_joins.m_number_depth[false],
              m_bevel_joins.m_number_depth[true],
              m_locations, bevel_join);

  MiterJoinCreator m(P, e);
  m_miter_joins.resize(m.sizes());
  m.fill_data(m_miter_joins.m_points.data(true),
              m_miter_joins.m_indices.data(true),
              m_miter_joins.m_number_depth[false],
              m_miter_joins.m_number_depth[true],
              m_locations, miter_join);

  CapJoinCreator cj(P, e);
  m_cap_joins.resize(cj.sizes());
  cj.fill_data(m_cap_joins.m_points.data(true),
               m_cap_joins.m_indices.data(true),
               m_cap_joins.m_number_depth[false],
               m_cap_joins.m_number_depth[true],
               m_locations, cap_join);

  RoundedCapCreator rc(P);
  m_rounded_cap.resize(rc.sizes());
  m_rounded_cap.m_number_depth = rc.fill_data(e,
                                              fastuidraw::make_c_array(m_rounded_cap.m_points),
                                              fastuidraw::make_c_array(m_rounded_cap.m_indices),
                                              m_locations, rounded_cap);

  SquareCapCreator sc(P);
  m_square_cap.resize(sc.sizes());
  m_square_cap.m_number_depth = sc.fill_data(e,
                                             fastuidraw::make_c_array(m_square_cap.m_points),
                                             fastuidraw::make_c_array(m_square_cap.m_indices),
                                             m_locations, square_cap);

  m_bevel_joins.compute_conveniance(m_return_values[fastuidraw::StrokedPath::bevel_join_point_set]);
  m_rounded_joins.compute_conveniance(m_return_values[fastuidraw::StrokedPath::rounded_join_point_set]);
  m_miter_joins.compute_conveniance(m_return_values[fastuidraw::StrokedPath::miter_join_point_set]);
  m_cap_joins.compute_conveniance(m_return_values[fastuidraw::StrokedPath::cap_join_point_set]);

  m_rounded_cap.compute_conveniance(m_return_values[fastuidraw::StrokedPath::rounded_cap_point_set]);
  m_square_cap.compute_conveniance(m_return_values[fastuidraw::StrokedPath::square_cap_point_set]);

  m_edges.compute_conveniance(m_return_values[fastuidraw::StrokedPath::edge_point_set]);
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
fastuidraw::vec2
fastuidraw::StrokedPath::point::
offset_vector(void)
{
  switch(offset_type())
    {
    case offset_edge:
    case offset_next_edge:
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

//////////////////////////////////////////////////////////////
// fastuidraw::StrokedPath methods
fastuidraw::StrokedPath::
StrokedPath(const fastuidraw::TessellatedPath &P)
{
  assert(number_offset_types < FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(offset_type_num_bits));
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
      d->m_attribute_data->set_data(PainterAttributeDataFillerPathStroked(this));
    }
  return *d->m_attribute_data;
}

fastuidraw::const_c_array<fastuidraw::StrokedPath::point>
fastuidraw::StrokedPath::
points(enum point_set_t tp, bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  assert(tp < number_point_set_types);
  return d->m_return_values[tp][including_closing_edge].m_points;
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::
indices(enum point_set_t tp, bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  assert(tp < number_point_set_types);
  return d->m_return_values[tp][including_closing_edge].m_indices;
}

unsigned int
fastuidraw::StrokedPath::
number_depth(enum point_set_t tp, bool including_closing_edge) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  assert(tp < number_point_set_types);
  return d->m_return_values[tp][including_closing_edge].m_number_depth;
}

unsigned int
fastuidraw::StrokedPath::
number_contours(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_locations.size();
}

unsigned int
fastuidraw::StrokedPath::
number_joins(unsigned int contour) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  assert(contour < d->m_locations.size());
  return d->m_locations[contour].m_joins.size();
}

fastuidraw::range_type<unsigned int>
fastuidraw::StrokedPath::
points_range(enum point_set_t tp, unsigned int contour, unsigned int N) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  assert(contour < d->m_locations.size());
  return d->m_locations[contour].fetch(tp, N).m_attribs;
}

fastuidraw::range_type<unsigned int>
fastuidraw::StrokedPath::
indices_range(enum point_set_t tp, unsigned int contour, unsigned int N) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  assert(contour < d->m_locations.size());
  return d->m_locations[contour].fetch(tp, N).m_indices;
}
