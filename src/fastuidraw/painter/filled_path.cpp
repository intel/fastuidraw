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
#include <map>
#include <list>
#include <algorithm>

#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/filled_path.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler_path_fill.hpp>
#include "../private/util_private.hpp"
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

  class tesser:fastuidraw::noncopyable
  {
  public:
    /* points is arranged as follows:
         - first all points from the source Path
         - bounding box points (4 points)
         - added points from tessellation
     */
    explicit
    tesser(std::vector<fastuidraw::vec2> &points);

    virtual
    ~tesser(void);

    void
    start(void);

    void
    stop(void);

    void
    add_path(const fastuidraw::TessellatedPath &P);

    void
    add_path_boundary(const fastuidraw::TessellatedPath &P);

    virtual
    void
    on_begin_polygon(int winding_number) = 0;

    virtual
    void
    add_vertex_to_polygon(unsigned int vertex) = 0;

    virtual
    FASTUIDRAW_GLUboolean
    fill_region(int winding_number) = 0;

    const fastuidraw::vec2&
    point(unsigned int i) const
    {
      return m_points[i];
    }

  private:

    static
    void
    begin_callBack(FASTUIDRAW_GLUenum type, int winding_number, void *tess);

    static
    void
    vertex_callBack(unsigned int vertex_data, void *tess);

    static
    void
    combine_callback(double x, double y, unsigned int data[4],
                     float weight[4],  unsigned int *outData,
                     void *tess);

    static
    FASTUIDRAW_GLUboolean
    winding_callBack(int winding_number, void *tess);

    unsigned int
    add_point_to_store(float x, float y);

    fastuidraw_GLUtesselator *m_tess;
    std::vector<fastuidraw::vec2> &m_points;
    fastuidraw::vecN<unsigned int, 3> m_temp_verts;
    unsigned int m_temp_vert_count;
  };

  class non_zero_tesser:private tesser
  {
  public:

    static
    void
    execute_path(std::vector<fastuidraw::vec2> &points,
                 const fastuidraw::TessellatedPath &P,
                 winding_index_hoard &hoard)
    {
      non_zero_tesser NZ(points, P, hoard);
    }

  private:
    non_zero_tesser(std::vector<fastuidraw::vec2> &points,
                    const fastuidraw::TessellatedPath &P,
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
    void
    execute_path(std::vector<fastuidraw::vec2> &points,
                 const fastuidraw::TessellatedPath &P,
                 winding_index_hoard &hoard)
    {
      zero_tesser Z(points, P, hoard);
    }

  private:

    zero_tesser(std::vector<fastuidraw::vec2> &points,
                const fastuidraw::TessellatedPath &P,
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

  private:

    void
    init_points(const fastuidraw::TessellatedPath &P);

    winding_index_hoard m_hoard;
    std::vector<fastuidraw::vec2> &m_points;
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

////////////////////////////////////////
// tesser methods
tesser::
tesser(std::vector<fastuidraw::vec2> &points):
  m_points(points)
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
add_path(const fastuidraw::TessellatedPath &input)
{
  for(unsigned int o = 0, endo = input.number_contours(); o < endo; ++o)
    {
      fastuidraw_gluTessBeginContour(m_tess);
      for(unsigned int e = 0, ende = input.number_edges(o); e < ende; ++e)
        {
          fastuidraw::range_type<unsigned int> R(input.edge_range(o, e));
          for(unsigned int v = R.m_begin; v + 1 < R.m_end; ++v)
            {
              fastuidraw_gluTessVertex(m_tess, m_points[v].x(), m_points[v].y(), v);
            }
        }
      fastuidraw_gluTessEndContour(m_tess);
    }
}

unsigned int
tesser::
add_point_to_store(float x, float y)
{
  unsigned int return_value(m_points.size());
  m_points.push_back(fastuidraw::vec2(x, y));

  //std::cout << "add point " << m_points.back() << "@"  << m_ids_of_added_points.back() << "\n";

  return return_value;
}

void
tesser::
add_path_boundary(const fastuidraw::TessellatedPath &P)
{
  fastuidraw_gluTessBeginContour(m_tess);
  for(unsigned int i = 0, v = P.point_data().size(); i < 4; ++i, ++v)
    {
      fastuidraw_gluTessVertex(m_tess, m_points[v].x(), m_points[v].y(), v);
    }
  fastuidraw_gluTessEndContour(m_tess);
}


void
tesser::
begin_callBack(FASTUIDRAW_GLUenum type, int winding_number, void *tess)
{
  tesser *p;
  p = static_cast<tesser*>(tess);
  assert(FASTUIDRAW_GLU_TRIANGLES == type);
  FASTUIDRAWunused(type);

  //std::cout << "begin_callBack(" << winding_number << ")\n";
  p->m_temp_vert_count = 0;
  p->on_begin_polygon(winding_number);
}

void
tesser::
vertex_callBack(unsigned int vertex_id, void *tess)
{
  tesser *p;
  p = static_cast<tesser*>(tess);
  // std::cout << "add vertex [" << *data << "] = " << p->m_points[*data] << "\n";

  /* Cache added vertices in groups of 3 (triangles),
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
         && p->m_temp_verts[2] != FASTUIDRAW_GLU_NULL_CLIENT_ID)
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
                 float weight[4],  unsigned int *outData,
                 void *tess)
{
  (void)weight;
  (void)data;

  /* TODO:
      Because we run the tessellator twice, we will likely
      add the same combine points twice. We should make a
      tracking to avoid that.
   */

  tesser *p;
  unsigned int v;
  p = static_cast<tesser*>(tess);
  v = p->add_point_to_store(x, y);
  *outData = v;

  //std::cout << "combine: " << p->m_points[*v] << " @" << *v << "\n";
}

FASTUIDRAW_GLUboolean
tesser::
winding_callBack(int winding_number, void *tess)
{
  tesser *p;
  FASTUIDRAW_GLUboolean return_value;

  p = static_cast<tesser*>(tess);
  return_value = p->fill_region(winding_number);
  // std::cout << "winding_callBack(" << winding_number << "):" << int(return_value) << "\n";

  return return_value;
}

///////////////////////////////////
// non_zero_tesser methods
non_zero_tesser::
non_zero_tesser(std::vector<fastuidraw::vec2> &points,
                const fastuidraw::TessellatedPath &P,
                winding_index_hoard &hoard):
  tesser(points),
  m_hoard(hoard),
  m_current_winding(0)
{
  start();
  add_path(P);
  stop();
}

void
non_zero_tesser::
on_begin_polygon(int winding_number)
{
  // std::cout << "nonzero_tesser::on_begin_polygon(" << winding_number << ")\n";
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
  // std::cout << "\tadd " << point(vertex) << "@" << vertex << "\n";
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
zero_tesser(std::vector<fastuidraw::vec2> &points,
            const fastuidraw::TessellatedPath &P,
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
  add_path_boundary(P);
  stop();
}

void
zero_tesser::
on_begin_polygon(int winding_number)
{
  // std::cout << "zero_tesser::on_begin_polygon(" << winding_number << ")\n";
  assert(winding_number == -1);
  FASTUIDRAWunused(winding_number);
}

void
zero_tesser::
add_vertex_to_polygon(unsigned int vertex)
{
  // std::cout << "\tadd " << point(vertex) << "@" << vertex << "\n";
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
  m_points(points)
{
  init_points(P);

  // std::cout << "Non-zero building\n";
  non_zero_tesser::execute_path(m_points, P, m_hoard);

  // std::cout << "Zero building\n";
  zero_tesser::execute_path(m_points, P, m_hoard);
}

builder::
~builder()
{
}

void
builder::
init_points(const fastuidraw::TessellatedPath &P)
{
  m_points.resize(P.point_data().size() + 4);
  for(unsigned int i = 0, endi = P.point_data().size(); i < endi; ++i)
    {
      m_points[i] = P.point_data()[i].m_p;
    }

  fastuidraw::vec2 pmin, pmax, pdelta;
  float tiny(1e-6);

  pmin = P.bounding_box_min();
  pmax = P.bounding_box_max();
  pdelta = (pmax - pmin) * tiny;

  pmin -= pdelta;
  pmax += pdelta;

  m_points[P.point_data().size() + 0] = fastuidraw::vec2(pmin.x(), pmin.y());
  m_points[P.point_data().size() + 1] = fastuidraw::vec2(pmin.x(), pmax.y());
  m_points[P.point_data().size() + 2] = fastuidraw::vec2(pmax.x(), pmax.y());
  m_points[P.point_data().size() + 3] = fastuidraw::vec2(pmax.x(), pmin.y());
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

      // std::cout << "Winding=" << iter->first << " has " << cnt << " indices\n";

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

  /*
  std::cout << "NonZero range= " << range_type<unsigned int>(0, zero_start) << "\n"
            << "Odd-Even range=" << range_type<unsigned int>(0, even_non_zero_start) << "\n"
            << "Even range=" << range_type<unsigned int>(even_non_zero_start, indices_ptr.size()) << "\n"
            << "Zero range=" << range_type<unsigned int>(zero_start, indices_ptr.size()) << "\n";
  */
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
