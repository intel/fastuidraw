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
#include <algorithm>
#include <math.h>

#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/filled_path.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler_path_fill.hpp>
#include "../private/util_private.hpp"
#include "../private/util_private_ostream.hpp"
#include "../private/bounding_box.hpp"
#include "../../3rd_party/glu-tess/glu-tess.hpp"

namespace
{
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

  /* Coordinate converter's purpose is to remap
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

     TODO: Incrementing the amount by which to apply
     fudge is not the correct thing to do. Rather, we
     should only increment and apply fudge on overlapping
     and degenerate edges.
   */
  class coordinate_converter
  {
  public:
    enum
      {
        log2_box_dim = 24,
        negative_log2_fudge = 20,
        box_dim = (1 << log2_box_dim),
      };

    explicit
    coordinate_converter(const fastuidraw::TessellatedPath &P)
    {
      fastuidraw::vecN<double, 2> delta, pmin, pmax;

      pmin = fastuidraw::vecN<double, 2>(P.bounding_box_min());
      pmax = fastuidraw::vecN<double, 2>(P.bounding_box_max());
      delta = pmax - pmin;
      m_scale = fastuidraw::vecN<double, 2>(1.0, 1.0) / delta;
      m_scale *= static_cast<double>(box_dim);
      m_translate = pmin;
      m_delta_fudge = ::exp2(static_cast<double>(-negative_log2_fudge));
    }

    fastuidraw::vecN<double, 2>
    apply(const fastuidraw::vec2 &pt, unsigned int fudge_count) const
    {
      fastuidraw::vecN<double, 2> r;
      fastuidraw::vecN<double, 2> qt(pt);
      double fudge;

      r = m_scale * (qt - m_translate);
      fudge = static_cast<double>(fudge_count) * m_delta_fudge;
      r.x() += fudge;
      r.y() += fudge;
      return r;
    }

    double
    fudge_delta(void) const
    {
      return m_delta_fudge;
    }

  private:
    double m_delta_fudge;
    fastuidraw::vecN<double, 2> m_scale, m_translate;
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

  class point_hoard:fastuidraw::noncopyable
  {
  public:
    typedef std::pair<unsigned int, bool> ContourPoint;
    typedef std::vector<ContourPoint> Contour;
    typedef std::list<Contour> Path;
    typedef std::vector<fastuidraw::uvec4> BoundingBoxes;

    enum
      {
        boxes_per_box = 4,
        pts_per_box = 8,
      };

    explicit
    point_hoard(const fastuidraw::TessellatedPath &P,
                std::vector<fastuidraw::vec2> &pts):
      m_converter(P),
      m_pts(pts)
    {}

    unsigned int
    fetch(const fastuidraw::vec2 &pt);

    void
    generate_path(const fastuidraw::TessellatedPath &input,
                  Path &output,
                  BoundingBoxes &bounding_boxes);

    const fastuidraw::vec2&
    operator[](unsigned int v) const
    {
      assert(v < m_pts.size());
      return m_pts[v];
    }

    const coordinate_converter&
    converter(void) const
    {
      return m_converter;
    }

  private:
    void
    pre_process_boxes(std::vector<fastuidraw::BoundingBox> &boxes,
                      unsigned int cnt);

    void
    process_bounding_boxes(const std::vector<fastuidraw::BoundingBox> &boxes,
                           BoundingBoxes &bounding_boxes);

    void
    add_edge(const fastuidraw::TessellatedPath &input,
             unsigned int o, unsigned int e,
             Path &output,
             BoundingBoxes &bounding_boxes);

    coordinate_converter m_converter;
    std::map<fastuidraw::vec2, unsigned int> m_map;
    std::vector<fastuidraw::vec2> &m_pts;
  };

  class tesser:fastuidraw::noncopyable
  {
  protected:
    explicit
    tesser(point_hoard &points);

    virtual
    ~tesser(void);

    void
    start(void);

    void
    stop(void);

    void
    add_path(const point_hoard::Path &P);

    void
    add_bounding_box_path(const point_hoard::BoundingBoxes &P);

    void
    add_path_boundary(const fastuidraw::TessellatedPath &P);

    bool
    triangulation_failed(void)
    {
      return m_triangulation_failed;
    }

    virtual
    void
    on_begin_polygon(int winding_number) = 0;

    virtual
    void
    add_vertex_to_polygon(unsigned int vertex) = 0;

    virtual
    FASTUIDRAW_GLUboolean
    fill_region(int winding_number) = 0;

  private:

    void
    add_contour(const point_hoard::Contour &C);

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

    unsigned int
    add_point_to_store(const fastuidraw::vec2 &p);

    bool
    temp_verts_non_degenerate_triangle(void);

    unsigned int m_point_count;
    fastuidraw_GLUtesselator *m_tess;
    point_hoard &m_points;
    fastuidraw::vecN<unsigned int, 3> m_temp_verts;
    unsigned int m_temp_vert_count;
    bool m_triangulation_failed;
  };

  class non_zero_tesser:private tesser
  {
  public:
    static
    bool
    execute_path(point_hoard &points,
                 point_hoard::Path &P,
                 point_hoard::BoundingBoxes &bounding_box_P,
                 winding_index_hoard &hoard)
    {
      non_zero_tesser NZ(points, P, bounding_box_P, hoard);
      return NZ.triangulation_failed();
    }

  private:
    non_zero_tesser(point_hoard &points,
                    point_hoard::Path &P,
                    point_hoard::BoundingBoxes &bounding_box_P,
                    winding_index_hoard &hoard);

    virtual
    void
    on_begin_polygon(int winding_number);

    virtual
    void
    add_vertex_to_polygon(unsigned int vertex);

    virtual
    FASTUIDRAW_GLUboolean
    fill_region(int winding_number);

    winding_index_hoard &m_hoard;
    int m_current_winding;
    fastuidraw::reference_counted_ptr<per_winding_data> m_current_indices;
  };

  class zero_tesser:private tesser
  {
  public:
    static
    bool
    execute_path(point_hoard &points,
                 point_hoard::Path &P,
                 point_hoard::BoundingBoxes &bounding_box_P,
                 const fastuidraw::TessellatedPath &path,
                 winding_index_hoard &hoard)
    {
      zero_tesser Z(points, P, bounding_box_P, path, hoard);
      return Z.triangulation_failed();
    }

  private:

    zero_tesser(point_hoard &points,
                point_hoard::Path &P,
                point_hoard::BoundingBoxes &bounding_box_P,
                const fastuidraw::TessellatedPath &path,
                winding_index_hoard &hoard);

    virtual
    void
    on_begin_polygon(int winding_number);

    virtual
    void
    add_vertex_to_polygon(unsigned int vertex);

    virtual
    FASTUIDRAW_GLUboolean
    fill_region(int winding_number);

    fastuidraw::reference_counted_ptr<per_winding_data> &m_indices;
  };

  class builder:fastuidraw::noncopyable
  {
  public:
    explicit
    builder(const fastuidraw::TessellatedPath &P,
            std::vector<fastuidraw::vec2> &pts);

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
    point_hoard m_points;
    bool m_failed;
  };

  class FilledPathPrivate
  {
  public:
    explicit
    FilledPathPrivate(const fastuidraw::TessellatedPath &P);

    ~FilledPathPrivate();

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

    /* m_per_fill[w] gives the indices to the triangles
       with the winding number w. The value points into
       m_indices
    */
    std::map<int, fastuidraw::const_c_array<unsigned int> > m_per_fill;

    /* list of values V for which m_per_fill[V] entry exists
     */
    std::vector<int> m_winding_numbers;

    fastuidraw::const_c_array<unsigned int> m_nonzero_winding, m_odd_winding;
    fastuidraw::const_c_array<unsigned int> m_even_winding, m_zero_winding;

    fastuidraw::PainterAttributeData *m_attribute_data;
  };
}

//////////////////////////////////////
// point_hoard methods
unsigned int
point_hoard::
fetch(const fastuidraw::vec2 &pt)
{
  std::map<fastuidraw::vec2, unsigned int>::iterator iter;
  unsigned int return_value;

  iter = m_map.find(pt);
  if(iter != m_map.end())
    {
      return_value = iter->second;
    }
  else
    {
      /* TODO: have an error allowance and use
         lower_bound(pt - error) and upper_bound(pt + error)
         which will give a range of iterators that one
         could/should find pt.
       */
      return_value = m_pts.size();
      m_pts.push_back(pt);
      m_map[pt] = return_value;
    }
  return return_value;
}

void
point_hoard::
generate_path(const fastuidraw::TessellatedPath &input,
              Path &output, BoundingBoxes &bounding_box_path)
{
  output.clear();
  for(unsigned int o = 0, endo = input.number_contours(); o < endo; ++o)
    {
      output.push_back(Contour());
      for(unsigned int e = 0, ende = input.number_edges(o); e < ende; ++e)
        {
          add_edge(input, o, e, output, bounding_box_path);
        }
    }
}

void
point_hoard::
add_edge(const fastuidraw::TessellatedPath &input,
         unsigned int o, unsigned int e,
         Path &output,
         BoundingBoxes &bounding_box_path)
{
  fastuidraw::range_type<unsigned int> R(input.edge_range(o, e));
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> pts;
  std::vector<fastuidraw::BoundingBox> boxes(1);
  unsigned int total_cnt(0), cnt(0);

  pts = input.point_data();

  for(unsigned int v = R.m_begin; v < R.m_end; ++v, ++cnt, ++total_cnt)
    {
      fastuidraw::vec2 p;
      unsigned int I;

      p = pts[v].m_p;
      I = fetch(p);
      output.back().push_back(ContourPoint(I, true));
      boxes.back().union_point(p);

      if(cnt == pts_per_box)
        {
          cnt = 0;
          boxes.push_back(fastuidraw::BoundingBox());
        }
    }

  pre_process_boxes(boxes, cnt);
  if(total_cnt >= pts_per_box)
    {
      process_bounding_boxes(boxes, bounding_box_path);
    }
}

void
point_hoard::
pre_process_boxes(std::vector<fastuidraw::BoundingBox> &boxes, unsigned int cnt)
{
  if(cnt <= 4 && boxes.size() > 1)
    {
      fastuidraw::BoundingBox B;
      B = boxes.back();
      boxes.pop_back();
      boxes.back().union_box(B);
    }
  else if(boxes.size() == 1 && cnt <= 2)
    {
      boxes.pop_back();
    }
}

void
point_hoard::
process_bounding_boxes(const std::vector<fastuidraw::BoundingBox> &in_boxes,
                       BoundingBoxes &bounding_box_path)
{
  std::vector<fastuidraw::BoundingBox> boxes_of_boxes(1);
  unsigned int total_cnt(0), cnt(0);

  for(unsigned int i = 0, endi = in_boxes.size(); i < endi; ++i, ++cnt, ++total_cnt)
    {
      fastuidraw::vec2 sz;

      assert(!in_boxes[i].empty());

      /* get/save the positions of the box*/
      bounding_box_path.push_back(fastuidraw::uvec4());
      for(unsigned int k = 0; k < 4; ++k)
        {
          fastuidraw::vec2 pt;

          if(k & box_max_x_flag)
            {
              pt.x() = in_boxes[i].max_point().x();
            }
          else
            {
              pt.x() = in_boxes[i].min_point().x();
            }

          if(k & box_max_y_flag)
            {
              pt.y() = in_boxes[i].max_point().y();
            }
          else
            {
              pt.y() = in_boxes[i].min_point().y();
            }
          bounding_box_path.back()[k] = fetch(pt);
        }

      boxes_of_boxes.back().union_box(in_boxes[i]);
      if(cnt == boxes_per_box)
        {
          cnt = 0;
          boxes_of_boxes.push_back(fastuidraw::BoundingBox());
        }
    }

  pre_process_boxes(boxes_of_boxes, cnt);
  if(total_cnt >= boxes_per_box)
    {
      process_bounding_boxes(boxes_of_boxes, bounding_box_path);
    }
}

////////////////////////////////////////
// tesser methods
tesser::
tesser(point_hoard &points):
  m_point_count(0),
  m_points(points),
  m_triangulation_failed(false)
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
add_contour(const point_hoard::Contour &C)
{
  fastuidraw_gluTessBeginContour(m_tess);
  for(unsigned int v = 0, endv = C.size(); v < endv; ++v)
    {
      fastuidraw::vecN<double, 2> p;
      point_hoard::ContourPoint I;

      I = C[v];
      if(I.second)
        {
          p = m_points.converter().apply(m_points[I.first], m_point_count);
          ++m_point_count;
        }
      else
        {
          p = m_points.converter().apply(m_points[I.first], 0u);
        }
      fastuidraw_gluTessVertex(m_tess, p.x(), p.y(), I.first);
    }
  fastuidraw_gluTessEndContour(m_tess);
}

void
tesser::
add_path(const point_hoard::Path &P)
{
  for(point_hoard::Path::const_iterator iter = P.begin(),
        end = P.end(); iter != end; ++iter)
    {
      add_contour(*iter);
    }
}

void
tesser::
add_bounding_box_path(const point_hoard::BoundingBoxes &P)
{
  const unsigned int indices[2][4] =
    {
      {
        box_min_x_min_y,
        box_min_x_max_y,
        box_max_x_max_y,
        box_max_x_min_y,
      },

      {
        box_max_x_min_y,
        box_max_x_max_y,
        box_min_x_max_y,
        box_min_x_min_y,
      }
    };

  for(point_hoard::BoundingBoxes::const_iterator iter = P.begin(),
        end = P.end(); iter != end; ++iter)
    {
      const fastuidraw::uvec4 &box(*iter);
      /* we add the box going forward and add the box going
         backwards. We adjust the point as follows:
          - for each coordinate seperately, for max side: add m_fudge
          - for each coordinate seperately, for max side: subtract m_fudge
          - first add in one direction
          - increment m_fudge
          - then add in other direction
       */
      for(unsigned int pass = 0; pass < 2; ++pass,
            ++m_point_count)
        {
          fastuidraw_gluTessBeginContour(m_tess);
          for(unsigned int i = 0; i < 4; ++i)
            {
              unsigned int k;
              fastuidraw::vecN<double, 2> p;
              double slack;

              k = indices[pass][i];
              p = m_points.converter().apply(m_points[box[k]], 0u);
              slack = static_cast<double>(m_point_count) * m_points.converter().fudge_delta();

              if(k & box_max_x_flag)
                {
                  p.x() += slack;
                }
              else
                {
                  p.x() -= slack;
                }

              if(k & box_max_y_flag)
                {
                  p.y() += slack;
                }
              else
                {
                  p.y() -= slack;
                }
              fastuidraw_gluTessVertex(m_tess, p.x(), p.y(), box[k]);
            }
          fastuidraw_gluTessEndContour(m_tess);
        }
    }
}

void
tesser::
add_path_boundary(const fastuidraw::TessellatedPath &P)
{
  fastuidraw::vec2 pmin, pmax;
  unsigned int src[4] =
    {
      box_min_x_min_y,
      box_min_x_max_y,
      box_max_x_max_y,
      box_max_x_min_y,
    };

  pmin = P.bounding_box_min();
  pmax = P.bounding_box_max();

  fastuidraw_gluTessBeginContour(m_tess);
  for(unsigned int i = 0; i < 4; ++i)
    {
      double slack, x, y;
      unsigned int k;
      fastuidraw::vec2 p;

      slack = static_cast<double>(m_point_count) * m_points.converter().fudge_delta();
      k = src[i];
      if(k & box_max_x_flag)
        {
          x = slack + static_cast<double>(coordinate_converter::box_dim);
          p.x() = pmax.x();
        }
      else
        {
          x = -slack;
          p.x() = pmin.x();
        }

      if(k & box_max_y_flag)
        {
          y = slack + static_cast<double>(coordinate_converter::box_dim);
          p.y() = pmax.y();
        }
      else
        {
          y = -slack;
          p.y() = pmin.y();
        }
      fastuidraw_gluTessVertex(m_tess, x, y, m_points.fetch(p));
    }
  fastuidraw_gluTessEndContour(m_tess);
}

unsigned int
tesser::
add_point_to_store(const fastuidraw::vec2 &p)
{
  unsigned int return_value;
  return_value = m_points.fetch(p);
  return return_value;
}

bool
tesser::
temp_verts_non_degenerate_triangle(void)
{
  if(m_temp_verts[0] == m_temp_verts[1]
     || m_temp_verts[0] == m_temp_verts[2]
     || m_temp_verts[1] == m_temp_verts[2])
    {
      return false;
    }

  fastuidraw::vec2 p0(m_points[m_temp_verts[0]]);
  fastuidraw::vec2 p1(m_points[m_temp_verts[1]]);
  fastuidraw::vec2 p2(m_points[m_temp_verts[2]]);

  if(p0 == p1 || p0 == p2 || p1 == p2)
    {
      return false;
    }

  fastuidraw::vec2 v(p1 - p0), w(p2 - p0);
  float area;
  bool return_value;

  /* we only reject a triangle if its area to floating
     point arithematic is zero.
   */
  area = fastuidraw::t_abs(v.x() * w.y() - v.y() * w.x());
  return_value = (area > 0.0f);
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
  p->on_begin_polygon(winding_number);
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
      p->m_temp_vert_count = 0;
      /*
        if vertex_id is FASTUIDRAW_GLU_NULL_CLIENT_ID, that means
        the triangle is junked.
      */
      if(p->m_temp_verts[0] != FASTUIDRAW_GLU_NULL_CLIENT_ID
         && p->m_temp_verts[1] != FASTUIDRAW_GLU_NULL_CLIENT_ID
         && p->m_temp_verts[2] != FASTUIDRAW_GLU_NULL_CLIENT_ID
         && p->temp_verts_non_degenerate_triangle())
        {
          p->add_vertex_to_polygon(p->m_temp_verts[0]);
          p->add_vertex_to_polygon(p->m_temp_verts[1]);
          p->add_vertex_to_polygon(p->m_temp_verts[2]);
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
  fastuidraw::vec2 pt(0.0f, 0.0f);

  p = static_cast<tesser*>(tess);
  for(unsigned int i = 0; i < 4; ++i)
    {
      if(data[i] != FASTUIDRAW_GLU_NULL_CLIENT_ID)
        {
          pt += float(weight[i]) * p->m_points[data[i]];
        }
    }
  v = p->add_point_to_store(pt);
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
non_zero_tesser(point_hoard &points,
                point_hoard::Path &P,
                point_hoard::BoundingBoxes &bounding_box_P,
                winding_index_hoard &hoard):
  tesser(points),
  m_hoard(hoard),
  m_current_winding(0)
{
  start();
  add_path(P);
  add_bounding_box_path(bounding_box_P);
  stop();
}

void
non_zero_tesser::
on_begin_polygon(int winding_number)
{
  if(!m_current_indices || m_current_winding != winding_number)
    {
      fastuidraw::reference_counted_ptr<per_winding_data> &h(m_hoard[winding_number]);
      m_current_winding = winding_number;
      if(!h)
        {
          h = FASTUIDRAWnew per_winding_data();
        }
      m_current_indices = h;
    }
}

void
non_zero_tesser::
add_vertex_to_polygon(unsigned int vertex)
{
  m_current_indices->add_index(vertex);
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
zero_tesser(point_hoard &points,
            point_hoard::Path &P,
            point_hoard::BoundingBoxes &bounding_box_P,
            const fastuidraw::TessellatedPath &path,
            winding_index_hoard &hoard):
  tesser(points),
  m_indices(hoard[0])
{
  if(!m_indices)
    {
      m_indices = FASTUIDRAWnew per_winding_data();
    }

  start();
  add_path(P);
  add_bounding_box_path(bounding_box_P);
  add_path_boundary(path);
  stop();
}

void
zero_tesser::
on_begin_polygon(int winding_number)
{
  assert(winding_number == -1);
  FASTUIDRAWunused(winding_number);
}

void
zero_tesser::
add_vertex_to_polygon(unsigned int vertex)
{
  m_indices->add_index(vertex);
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
builder(const fastuidraw::TessellatedPath &P, std::vector<fastuidraw::vec2> &points):
  m_points(P, points)
{
  bool failZ, failNZ;
  point_hoard::Path path;
  point_hoard::BoundingBoxes bounding_boxes;

  m_points.generate_path(P, path, bounding_boxes);
  failNZ = non_zero_tesser::execute_path(m_points, path, bounding_boxes, m_hoard);
  failZ = zero_tesser::execute_path(m_points, path, bounding_boxes, P, m_hoard);
  m_failed= failNZ || failZ;
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

/////////////////////////////////
// FilledPathPrivate methods
FilledPathPrivate::
FilledPathPrivate(const fastuidraw::TessellatedPath &P):
  m_attribute_data(NULL)
{
  builder B(P, m_points);
  unsigned int even_non_zero_start, zero_start;

  B.fill_indices(m_indices, m_per_fill, even_non_zero_start, zero_start);

  /* copy/swap into fresh std::vector to free extra storage
   */
  if(m_indices.empty())
    {
      std::vector<fastuidraw::vec2> temp;
      temp.swap(m_points);
    }
  else
    {
      std::vector<fastuidraw::vec2> temp(m_points);
      temp.swap(m_points);
    }


  fastuidraw::const_c_array<unsigned int> indices_ptr;
  indices_ptr = fastuidraw::make_c_array(m_indices);
  m_nonzero_winding = indices_ptr.sub_array(0, zero_start);
  m_odd_winding = indices_ptr.sub_array(0, even_non_zero_start);
  m_even_winding = indices_ptr.sub_array(even_non_zero_start);
  m_zero_winding = indices_ptr.sub_array(zero_start);

  m_winding_numbers.reserve(m_per_fill.size());
  for(std::map<int, fastuidraw::const_c_array<unsigned int> >::iterator
        iter = m_per_fill.begin(), end = m_per_fill.end();
      iter != end; ++iter)
    {
      assert(!iter->second.empty());
      m_winding_numbers.push_back(iter->first);
    }

  #ifdef FASTUIDRAW_DEBUG
    {
      if(B.triangulation_failed())
        {
          /* On debug builds, print a warning.
           */
          std::cerr << "[" << __FILE__ << ", " << __LINE__
                    << "] Triangulation failed on tessellated path "
                    << &P << "\n";
        }
    }
  #endif
}

FilledPathPrivate::
~FilledPathPrivate()
{
  if(m_attribute_data != NULL)
    {
      FASTUIDRAWdelete(m_attribute_data);
    }
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
  d = reinterpret_cast<FilledPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::FilledPath::
indices(int winding_number) const
{
  FilledPathPrivate *d;
  d = reinterpret_cast<FilledPathPrivate*>(m_d);

  std::map<int, const_c_array<unsigned int> >::const_iterator iter;

  iter = d->m_per_fill.find(winding_number);
  return (iter != d->m_per_fill.end()) ?
    iter->second:
    const_c_array<unsigned int>();
}

const fastuidraw::PainterAttributeData&
fastuidraw::FilledPath::
painter_data(void) const
{
  FilledPathPrivate *d;
  d = reinterpret_cast<FilledPathPrivate*>(m_d);

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
      d->m_attribute_data->set_data(PainterAttributeDataFillerPathFill(this));
    }
  return *d->m_attribute_data;
}

fastuidraw::const_c_array<fastuidraw::vec2>
fastuidraw::FilledPath::
points(void) const
{
  FilledPathPrivate *d;
  d = reinterpret_cast<FilledPathPrivate*>(m_d);
  return make_c_array(d->m_points);
}

fastuidraw::const_c_array<int>
fastuidraw::FilledPath::
winding_numbers(void) const
{
  FilledPathPrivate *d;
  d = reinterpret_cast<FilledPathPrivate*>(m_d);
  return make_c_array(d->m_winding_numbers);
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::FilledPath::
nonzero_winding_indices(void) const
{
  FilledPathPrivate *d;
  d = reinterpret_cast<FilledPathPrivate*>(m_d);
  return d->m_nonzero_winding;
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::FilledPath::
odd_winding_indices(void) const
{
  FilledPathPrivate *d;
  d = reinterpret_cast<FilledPathPrivate*>(m_d);
  return d->m_odd_winding;
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::FilledPath::
zero_winding_indices(void) const
{
  FilledPathPrivate *d;
  d = reinterpret_cast<FilledPathPrivate*>(m_d);
  return d->m_zero_winding;
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::FilledPath::
even_winding_indices(void) const
{
  FilledPathPrivate *d;
  d = reinterpret_cast<FilledPathPrivate*>(m_d);
  return d->m_even_winding;
}
