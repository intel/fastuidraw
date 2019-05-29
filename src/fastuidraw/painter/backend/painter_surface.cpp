/*!
 * \file painter_surface.cpp
 * \brief file painter_surface.cpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#include <fastuidraw/painter/backend/painter_surface.hpp>

/////////////////////////////////////////////////////////
// fastuidraw::PainterSurface::Viewport methods
void
fastuidraw::PainterSurface::Viewport::
compute_clip_equations(ivec2 surface_dims,
                       vecN<vec3, 4> *out_clip_equations) const
{
  Rect Rndc;

  compute_normalized_clip_rect(surface_dims, &Rndc);

  /* The clip equations are:
   *
   *  Rndc.m_min_point.x() <= x <= Rndc.m_max_point.x()
   *  Rndc.m_min_point.y() <= y <= Rndc.m_max_point.y()
   *
   * Which converts to
   *
   *  +1.0 * x - Rndc.m_min_point.x() * w >= 0
   *  +1.0 * y - Rndc.m_min_point.y() * w >= 0
   *  -1.0 * x + Rndc.m_max_point.x() * w >= 0
   *  -1.0 * y + Rndc.m_max_point.y() * w >= 0
   */
  (*out_clip_equations)[0] = vec3(+1.0f, 0.0f, -Rndc.m_min_point.x());
  (*out_clip_equations)[1] = vec3(0.0f, +1.0f, -Rndc.m_min_point.y());
  (*out_clip_equations)[2] = vec3(-1.0f, 0.0f, +Rndc.m_max_point.x());
  (*out_clip_equations)[3] = vec3(0.0f, -1.0f, +Rndc.m_max_point.y());
}

void
fastuidraw::PainterSurface::Viewport::
compute_normalized_clip_rect(ivec2 surface_dims,
                             Rect *out_rect) const
{
  out_rect->m_min_point = compute_normalized_device_coords(vec2(0.0f, 0.0f));
  out_rect->m_max_point = compute_normalized_device_coords(vec2(surface_dims));

  out_rect->m_min_point.x() = t_max(-1.0f, out_rect->m_min_point.x());
  out_rect->m_max_point.x() = t_min(+1.0f, out_rect->m_max_point.x());

  out_rect->m_min_point.y() = t_max(-1.0f, out_rect->m_min_point.y());
  out_rect->m_max_point.y() = t_min(+1.0f, out_rect->m_max_point.y());
}
