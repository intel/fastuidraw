/*!
 * \file fastuidraw_painter_util.geom.glsl.resource_string
 * \brief file fastuidraw_painter_util.geom.glsl.resource_string
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

using namespace fastuidraw;

template<int I>
class StaticIndex
{
public:
  enum { value = I };
};

class fastuidraw_clipper_data_vertex
{
public:
  fastuidraw_clipper_data_vertex(void):
    skip_vertex(true)
  {}
  
  vec3 barycentric;
  bool skip_vertex;
};

template<int N>
class fastuidraw_clipper_data
{
public:
  enum { no_added_vertex = N + 1 };
  
  fastuidraw_clipper_data_vertex v[N - 1];
  vec3 last_vertex, added_vertex;
  int added_vertex_at, count;

  fastuidraw_clipper_data(void):
    last_vertex(0.0f),
    added_vertex_at(no_added_vertex),
    count(0)
  {}

  bool
  has_added_vertex(void) const
  {
    return added_vertex_at < N;
  }

  void
  dump_padded_barycentrics(std::vector<vec3> &dst) const
  {
    dst.clear();
    vec3 prev_vertex(last_vertex);
    dump_padded_barycentrics_upto(StaticIndex<N>(), prev_vertex, dst);
  }

  void
  dump_unpadded_barycentrics(std::vector<vec3> &dst) const
  {
    dst.clear();
    dump_unpadded_barycentrics_upto(StaticIndex<N>(), dst);
  }
  
  void
  print_raw(std::ostream &str) const
  {
    str << "[count = " << count << ", ";
    for (int i = 0; i < N - 1; ++i)
      {
        if (!v[i].skip_vertex)
          {
            str << v[i].barycentric;
          }
        else
          {
            str << "*";
          }
        str << ", ";
      }

    if (added_vertex_at < N)
      {
        str << "insert = " << added_vertex << "@"
            << added_vertex_at << ", ";
      }
    str  << "last_vertex = " << last_vertex << "]";
  }

  void
  print_unpadded(std::ostream &str) const
  {
    print_unpadded_impl(StaticIndex<N>(), str);
  }

  template<int I>
  void
  print_unpadded_impl(StaticIndex<I>, std::ostream &str) const
  {
    vec3 p;
    print_unpadded_impl(StaticIndex<I - 1>(), str);
    if (!read_vertex(StaticIndex<I - 1>(), p, &str))
      {
        str << p << "(" << I - 1 << "), ";
      }
  }

  void
  print_unpadded_impl(StaticIndex<0>, std::ostream&) const
  {
  }
  
  template<int I>
  bool
  read_vertex(StaticIndex<I>, vec3 &dst, std::ostream *str = nullptr) const
  {
    FASTUIDRAWassert(I < N);
    FASTUIDRAWassert(I >= 0);
    if (I < added_vertex_at)
      {
        dst = v[I].barycentric;
        if (str && !v[I].skip_vertex) *str << "{P" << I << "}";
        return v[I].skip_vertex;
      }
    else if (I > added_vertex_at)
      {
        dst = v[I - 1].barycentric;
        if (str && !v[I - 1].skip_vertex) *str << "{Q" << I - 1 << "}";
        return v[I - 1].skip_vertex;
      }
    else
      {
        dst = added_vertex;
        if (str) *str << "{A}";
        return false;
      }
  }

  bool
  read_vertex(StaticIndex<0>, vec3 &dst, std::ostream *str = nullptr) const
  {
    if (added_vertex_at == 0)
      {
        dst = added_vertex;
        if (str && count > 0) *str << "{A}";
        return count == 0;
      }
    else
      {
        dst = v[0].barycentric;
        if (str && !v[0].skip_vertex) *str << "{0}";
        return v[0].skip_vertex;
      }
  }

  bool
  read_vertex(StaticIndex<N - 1>, vec3 &dst, std::ostream *str = nullptr) const
  {
    if (N - 1 < added_vertex_at)
      {
        return true;
      }
    else if (N - 1 > added_vertex_at)
      {
        dst = v[N - 2].barycentric;
        if (str && !v[N - 2].skip_vertex) *str << "{" << N - 2 << "}";
        return v[N - 2].skip_vertex;
      }
    else
      {
        dst = added_vertex;
        if (str && added_vertex_at == N - 1) *str << "{A}";
        return added_vertex_at != N - 1;
      }
  }

private:
  template<int I>
  void
  dump_padded_barycentrics_upto(StaticIndex<I>, vec3 &prev_vertex, std::vector<vec3> &dst) const
  {
    vec3 p;
    dump_padded_barycentrics_upto(StaticIndex<I - 1>(), prev_vertex, dst);
    if (!read_vertex(StaticIndex<I - 1>(), p))
      {
        prev_vertex = p;
      }
    dst.push_back(prev_vertex);
  }

  void
  dump_padded_barycentrics_upto(StaticIndex<0>, vec3&, std::vector<vec3>&) const
  {
  }

  template<int I>
  void
  dump_unpadded_barycentrics_upto(StaticIndex<I>, std::vector<vec3> &dst) const
  {
    vec3 p;
    dump_unpadded_barycentrics_upto(StaticIndex<I - 1>(), dst);
    if (!read_vertex(StaticIndex<I - 1>(), p))
      {
        dst.push_back(p);
      }
  }

  void
  dump_unpadded_barycentrics_upto(StaticIndex<0>, std::vector<vec3>&) const
  {
  }
};

template<>
class fastuidraw_clipper_data<3>
{
public:
  fastuidraw_clipper_data():
    last_vertex(v[2]),
    count(3)
  {}
  
  vec3 v[3];
  vec3 &last_vertex;
  int count;

  template<int I>
  bool
  read_vertex(StaticIndex<I>, vec3 &dst) const
  {
    dst = v[I];
    return false;
  }
};

vec3
fastuidraw_compute_intersection(vec3 p0, float c0,
                                vec3 p1, float c1)
{
  float t;

  t = c0 / (c0 - c1);
  return (1.0f - t) * p0 +  t * p1;
}

template<int N>
class fastuidraw_clip_polygon_impl
{
public:
  fastuidraw_clip_polygon_impl(const fastuidraw_clipper_data<N> &psrc,
                               fastuidraw_clipper_data<N + 1> &pdst,
                               vec3 pclip_values):
    src(psrc),
    dst(pdst),
    clip_values(pclip_values),
    prev_vert(src.last_vertex),
    prev_d(pclip_values.dot(src.last_vertex))
  {}
  
  const fastuidraw_clipper_data<N> &src;
  fastuidraw_clipper_data<N + 1> &dst;
  vec3 clip_values;

  vec3 current_vert, prev_vert;
  float current_d, prev_d;

  void
  clip_polygon(void)
  {
    clip_polygon_upto_step(StaticIndex<N>());
  }

private:
  template<int I>
  void
  clip_polygon_upto_step(StaticIndex<I>)
  {
    clip_polygon_upto_step(StaticIndex<I - 1>());
    clip_polygon_one_step<I - 1>();
  }

  void
  clip_polygon_upto_step(StaticIndex<0>)
  {
  }

  template<int I>
  void
  clip_polygon_one_step(void)
  {
    if (!src.read_vertex(StaticIndex<I>(), current_vert))
      {
        current_d = clip_values.dot(current_vert);
        if (current_d >= 0.0f)
          {
            if (prev_d < 0.0f)
              {
                vec3 p;

                FASTUIDRAWassert(!dst.has_added_vertex());

                p = fastuidraw_compute_intersection(prev_vert, prev_d, current_vert, current_d);
                dst.added_vertex_at = I;
                dst.added_vertex = p;
                ++dst.count;

                FASTUIDRAWassert(dst.has_added_vertex());
              }
            dst.v[I].barycentric = current_vert;
            dst.v[I].skip_vertex = false;
            ++dst.count;
            dst.last_vertex = current_vert;
          }
        else if (prev_d >= 0.0f)
          {
            vec3 p;
            p = fastuidraw_compute_intersection(prev_vert, prev_d, current_vert, current_d);
            dst.v[I].barycentric = p;
            dst.v[I].skip_vertex = false;
            ++dst.count;
            dst.last_vertex = p;
          }
        prev_d = current_d;
        prev_vert = current_vert;
      }
  }
};

template<int N>
void
fastuidraw_clip_polygon(const fastuidraw_clipper_data<N> &src,
                        vec3 clip_values,
                        fastuidraw_clipper_data<N + 1> &dst)
{
  if (src.count >= 3)
    {
      fastuidraw_clip_polygon_impl<N> clipper(src, dst, clip_values);
      clipper.clip_polygon();
    }
}

class fastuidraw_per_vertex_data
{
public:
  vec4 fastuidraw_clip_planes;
};

void
dump_positions_from_barycentrics(const vecN<vec2, 3> &triangle,
                                 const std::vector<vec3> &barycentrics,
                                 std::vector<vec2> &dst)
{
  dst.clear();
  for (const vec3 &p : barycentrics)
    {
      vec2 pt;
      pt = p.x() * triangle[0]
        + p.y() * triangle[1]
        + p.z() * triangle[2];
      dst.push_back(pt);
    }
}

void
usual_clip_triangle(const vecN<vec2, 3> &triangle,
                    const vecN<fastuidraw_per_vertex_data, 3> &fastuidraw_in,
                    std::vector<vec2> &out_unpadded_verts)
{
  std::vector<vec3> src, dst;

  dst.resize(3);
  dst[0] = vec3(1.0f, 0.0f, 0.0f);
  dst[1] = vec3(0.0f, 1.0f, 0.0f);
  dst[2] = vec3(0.0f, 0.0f, 1.0f);

  std::cout << "\t\tusual_clip_triangle\n";
  
  for (int clip = 0; clip < 4 && !dst.empty(); ++clip)
    {
      vec3 clip_eq;
      float current_d, prev_d;
      vec3 p, current_vert, prev_vert;

      clip_eq.x() = fastuidraw_in[0].fastuidraw_clip_planes[clip];
      clip_eq.y() = fastuidraw_in[1].fastuidraw_clip_planes[clip];
      clip_eq.z() = fastuidraw_in[2].fastuidraw_clip_planes[clip];

      std::swap(src, dst);
      dst.clear();

      prev_vert = src.back();
      prev_d = prev_vert.dot(clip_eq);

      for (unsigned int v = 0; v < src.size(); ++v)
        {
          current_vert = src[v];
          current_d = current_vert.dot(clip_eq);

          if (current_d >= 0.0f)
            {
              if (prev_d < 0.0f)
                {
                  p = fastuidraw_compute_intersection(prev_vert, prev_d, current_vert, current_d);
                  dst.push_back(p);
                }
              dst.push_back(current_vert);
            }
          else if (prev_d >= 0.0f)
            {
              p = fastuidraw_compute_intersection(prev_vert, prev_d, current_vert, current_d);
              dst.push_back(p);
            }

          prev_d = current_d;
          prev_vert = current_vert;
        }
      std::cout << "\t\t\tA" << clip + 1 << ": [count = " << dst.size()
                << ", " << dst << "]\n";
    }

  dump_positions_from_barycentrics(triangle, dst, out_unpadded_verts);
}

bool
close_enough(const std::vector<vec2> &A,
             const std::vector<vec2> &B)
{
  if (A.size() != B.size())
    return false;

  for (unsigned int i = 0; i < A.size(); ++i)
    {
      float dx, dy;

      dx = t_abs(A[i].x() - B[i].x());
      dy = t_abs(A[i].y() - B[i].y());

      if (dx > 0.0f || dy > 0.0f)
        return false;      
    }
  return true;
}

void
inplace_clip_triangle(const vecN<vec2, 3> &triangle,
                      const vecN<fastuidraw_per_vertex_data, 3> &fastuidraw_in,
                      std::vector<vec2> &out_padded_verts)
{
  fastuidraw_clipper_data<3> A0;
  fastuidraw_clipper_data<4> A1;
  fastuidraw_clipper_data<5> A2;
  fastuidraw_clipper_data<6> A3;
  fastuidraw_clipper_data<7> A4;
  vec3 C;

  A0.v[0] = vec3(1.0, 0.0, 0.0);
  A0.v[1] = vec3(0.0, 1.0, 0.0);
  A0.v[2] = vec3(0.0, 0.0, 1.0);
  
  C.x() = fastuidraw_in[0].fastuidraw_clip_planes.x();
  C.y() = fastuidraw_in[1].fastuidraw_clip_planes.x();
  C.z() = fastuidraw_in[2].fastuidraw_clip_planes.x();
  fastuidraw_clip_polygon<3>(A0, C, A1);

  C.x() = fastuidraw_in[0].fastuidraw_clip_planes.y();
  C.y() = fastuidraw_in[1].fastuidraw_clip_planes.y();
  C.z() = fastuidraw_in[2].fastuidraw_clip_planes.y();
  fastuidraw_clip_polygon<4>(A1, C, A2);

  C.x() = fastuidraw_in[0].fastuidraw_clip_planes.z();
  C.y() = fastuidraw_in[1].fastuidraw_clip_planes.z();
  C.z() = fastuidraw_in[2].fastuidraw_clip_planes.z();
  fastuidraw_clip_polygon<5>(A2, C, A3);

  C.x() = fastuidraw_in[0].fastuidraw_clip_planes.w();
  C.y() = fastuidraw_in[1].fastuidraw_clip_planes.w();
  C.z() = fastuidraw_in[2].fastuidraw_clip_planes.w();
  fastuidraw_clip_polygon<6>(A3, C, A4);
  
  std::vector<vec3> tmp;

  A4.dump_padded_barycentrics(tmp);
  dump_positions_from_barycentrics(triangle, tmp, out_padded_verts);
}
