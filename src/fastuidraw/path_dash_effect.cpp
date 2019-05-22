/*!
 * \file path_dash_effect.cpp
 * \brief file path_dash_effect.cpp
 *
 * Copyright 2019 by Intel.
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
#include <fastuidraw/path_dash_effect.hpp>
#include <private/util_private.hpp>

namespace
{
  class DashElement
  {
  public:
    float m_start_distance, m_end_distance;
    float m_draw_length, m_skip_length;
  };

  class DashElementProxy
  {
  public:
    DashElementProxy(unsigned int idx,
                     unsigned int cycle,
                     const DashElement &d,
                     float increase_by):
      m_idx(idx),
      m_cycle(cycle),
      m_start_distance(increase_by + d.m_start_distance),
      m_end_distance(increase_by + d.m_end_distance),
      m_draw_length(d.m_draw_length),
      m_skip_length(d.m_skip_length)
    {}

    float
    end_draw(void) const
    {
      return m_start_distance + m_draw_length;
    }

    unsigned int m_idx, m_cycle;
    float m_start_distance, m_end_distance;
    float m_draw_length, m_skip_length;
  };

  inline
  bool
  compare_dash_element_against_distance(const DashElement &lhs, float rhs)
  {
    return lhs.m_end_distance < rhs;
  }

  void
  add_cap(const fastuidraw::TessellatedPath::segment &S,
          float length_from_start_S,
          bool is_start_cap,
          fastuidraw::PathEffect::Storage &dst)
  {
    fastuidraw::TessellatedPath::cap C;
    float s, t, n;

    t = length_from_start_S / S.m_length;
    s = 1.0f - t;
    n = (is_start_cap) ? -1.0f : +1.0f;
    if (S.m_type == fastuidraw::TessellatedPath::line_segment)
      {
        C.m_position = s * S.m_start_pt + t * S.m_end_pt;
        C.m_unit_vector = n * S.m_enter_segment_unit_vector;
      }
    else
      {
        float angle(s * S.m_arc_angle.m_begin + t * S.m_arc_angle.m_end);

        fastuidraw::vec2 cs(fastuidraw::t_cos(angle), fastuidraw::t_sin(angle));
        C.m_position = S.m_center + S.m_radius * cs;
        C.m_unit_vector = n * fastuidraw::vec2(-cs.y(), cs.x());
      }
    C.m_contour_length = S.m_contour_length;
    C.m_edge_length = S.m_edge_length;
    C.m_is_starting_cap = false;
    C.m_contour_id = S.m_contour_id;
    dst.add_cap(C);
  }

  class PathDashEffectPrivate
  {
  public:
    PathDashEffectPrivate(void):
      m_dash_offset(0.0f)
    {}

    float
    current_length(void) const
    {
      return (m_lengths.empty()) ?
        0.0f:
        m_lengths.back().m_end_distance;
    }

    void
    process_segment(const fastuidraw::TessellatedPath::segment &S,
                    DashElementProxy &P,
                    fastuidraw::PathEffect::Storage &dst);

    DashElementProxy
    proxy(unsigned int I) const;

    /* TODO: add parameters to PathDashEffect to select if distance
     * value starts at contour, since the last join or a distance
     * since the last join.
     */
    float
    distance(const fastuidraw::TessellatedPath::segment &S)
    {
      return S.m_distance_from_contour_start;
    }

    float
    distance(const fastuidraw::TessellatedPath::join &J)
    {
      return J.m_distance_from_contour_start;
    }

    float
    distance(const fastuidraw::TessellatedPath::cap &C)
    {
      return (C.m_is_starting_cap) ?
        0.0f:
        C.m_contour_length;
    }

    unsigned int
    find_index(float f) const;

    std::vector<DashElement>::const_iterator
    find_iterator(float &f, int &N) const;

    bool
    point_inside_draw_interval(float f) const;

    float m_dash_offset;
    std::vector<DashElement> m_lengths;
  };
}

////////////////////////////////
// PathDashEffectPrivate methods
DashElementProxy
PathDashEffectPrivate::
proxy(unsigned int I) const
{
  unsigned int N(I / m_lengths.size());
  unsigned int srcI(I - N * m_lengths.size());
  const DashElement &src(m_lengths[srcI]);
  float fN(N);
  DashElementProxy return_value(I, N, src, fN * current_length());

  return return_value;
}

std::vector<DashElement>::const_iterator
PathDashEffectPrivate::
find_iterator(float &f, int &N) const
{
  float c(current_length());
  std::vector<DashElement>::const_iterator iter;

  if (f >= c)
    {
      float fN, ff(f / c);

      ff = ::modff(ff, &fN);
      f = c * ff;
      N = fN;
    }
  else
    {
      N = 0;
    }

  return std::lower_bound(m_lengths.begin(), m_lengths.end(),
                          f, compare_dash_element_against_distance);
}

unsigned int
PathDashEffectPrivate::
find_index(float f) const
{
  std::vector<DashElement>::const_iterator iter;
  int N;

  iter = find_iterator(f, N);
  return std::distance(m_lengths.begin(), iter) + m_lengths.size() * N;
}

bool
PathDashEffectPrivate::
point_inside_draw_interval(float f) const
{
  /* We shall interpret an empty-dash pattern as no dashing at all */
  if (m_lengths.empty())
    {
      return true;
    }

  std::vector<DashElement>::const_iterator iter;
  int N;

  iter = find_iterator(f, N);
  return iter == m_lengths.end()
    || iter->m_start_distance + iter->m_draw_length >= f;
}

void
PathDashEffectPrivate::
process_segment(const fastuidraw::TessellatedPath::segment &pS,
                DashElementProxy &P,
                fastuidraw::PathEffect::Storage &dst)
{
  fastuidraw::TessellatedPath::segment S(pS);
  float t, startS(distance(S)), endS(startS + S.m_length);
  bool need_to_begin_chain(true);
  fastuidraw::TessellatedPath::segment A, B;

  for (;;)
    {
      FASTUIDRAWassert(distance(S) >= P.m_start_distance);

      if (P.end_draw() >= startS && P.end_draw() <= endS)
        {
          /* Draw interval of P ends within S, so add an ending cap. */
          add_cap(S, P.end_draw() - startS, false, dst);
        }

      if (P.end_draw() >= startS && P.m_draw_length > 0.0f)
        {
          if (P.end_draw() < endS)
            {
              t = (P.end_draw() - startS) / S.m_length;

              /* add the draw element of S on [0, t] */
              S.split_segment(t, &A, &B);
              dst.add_segment(A);

              /* continue with the S on [t, 1], note that we
               * need to start a new chain
               */
              S = B;
              need_to_begin_chain = true;
            }
          else
            {
              /* The draw interval extends past the ending of S,
               * so we are done.
               */
              dst.add_segment(S);
              return;
            }
        }

      if (P.m_end_distance <= endS)
        {
          /* The skip interval ends within S, add a starting cap for
           * the next draw interval.
           */
          add_cap(S, P.m_end_distance - startS, true, dst);
        }
      else
        {
          /* the skip interval extends past S, so we are done with S */
          return;
        }

      /* S extends past the skip interval, take the portion of S that does */
      t = (P.m_end_distance - startS) / S.m_length;
      S.split_segment(t, &A, &B);
      S = B;
      startS = distance(S);

      P = proxy(P.m_idx + 1);
      if (need_to_begin_chain)
        {
          dst.begin_chain(nullptr);
          need_to_begin_chain = false;
        }
    }
}

//////////////////////////////////////
// fastuidraw::PathDashEffect methods
fastuidraw::PathDashEffect::
PathDashEffect(void)
{
  m_d = FASTUIDRAWnew PathDashEffectPrivate();
}

fastuidraw::PathDashEffect::
~PathDashEffect(void)
{
  PathDashEffectPrivate *d;
  d = static_cast<PathDashEffectPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::PathDashEffect&
fastuidraw::PathDashEffect::
dash_offset(float v)
{
  PathDashEffectPrivate *d;
  d = static_cast<PathDashEffectPrivate*>(m_d);
  d->m_dash_offset = v;
  return *this;
}

fastuidraw::PathDashEffect&
fastuidraw::PathDashEffect::
clear(void)
{
  PathDashEffectPrivate *d;
  d = static_cast<PathDashEffectPrivate*>(m_d);
  d->m_lengths.clear();
  return *this;
}

fastuidraw::PathDashEffect&
fastuidraw::PathDashEffect::
add_dash(float draw, float skip)
{
  PathDashEffectPrivate *d;
  d = static_cast<PathDashEffectPrivate*>(m_d);

  if (draw < 0.0f || skip < 0.0f)
    {
      FASTUIDRAWmessaged_assert(draw >= 0.0f, "Bad draw length passed to add_dash");
      FASTUIDRAWmessaged_assert(skip >= 0.0f, "Bad skip length passed to add_dash");
      return *this;
    }

  if (!d->m_lengths.empty() && d->m_lengths.back().m_skip_length == 0.0f)
    {
      d->m_lengths.back().m_draw_length += draw;
      d->m_lengths.back().m_skip_length += skip;
      d->m_lengths.back().m_end_distance += draw + skip;
      return *this;
    }

  DashElement E;

  E.m_draw_length = draw;
  E.m_skip_length = skip;
  E.m_start_distance = d->current_length();
  E.m_end_distance = E.m_start_distance + draw + skip;

  d->m_lengths.push_back(E);
  return *this;
}

void
fastuidraw::PathDashEffect::
process_join(const TessellatedPath::join &join, Storage &dst) const
{
  PathDashEffectPrivate *d;
  d = static_cast<PathDashEffectPrivate*>(m_d);

  float dist(d->distance(join));
  if (d->point_inside_draw_interval(dist))
    {
      dst.add_join(join);
    }
}

void
fastuidraw::PathDashEffect::
process_cap(const TessellatedPath::cap &cap, Storage &dst) const
{
  PathDashEffectPrivate *d;
  d = static_cast<PathDashEffectPrivate*>(m_d);

  /* We shall interpret an empty-dash pattern as no dashing at all */
  float dist(d->distance(cap));
  if (d->point_inside_draw_interval(dist))
    {
      dst.add_cap(cap);
    }
}

void
fastuidraw::PathDashEffect::
process_chain(const segment_chain &chain, Storage &dst) const
{
  PathDashEffectPrivate *d;
  d = static_cast<PathDashEffectPrivate*>(m_d);

  if (chain.m_segments.empty())
    {
      return;
    }

  const TessellatedPath::segment *prev_segment(chain.m_prev_to_start);
  const TessellatedPath::segment &front(chain.m_segments.front());
  float dist(d->distance(front));
  DashElementProxy P(d->proxy(d->find_index(dist)));

  dst.begin_chain(prev_segment);

  /* check if we need to give a starting cap to the
   * first segment of the chain; this is only needed
   * for the first segment of the first edge.
   */
  if (front.m_edge_id == 0 && front.m_first_segment_of_edge)
    {
      add_cap(*prev_segment, 0.0f, true, dst);
    }

  for (const auto &S : chain.m_segments)
    {
      d->process_segment(S, P, dst);
    }
}
