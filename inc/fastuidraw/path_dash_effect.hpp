/*!
 * \file path_dash_effect.hpp
 * \brief file path_dash_effect.hpp
 *
 * Copyright 2019 by Intel.
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

#ifndef FASTUIDRAW_PATH_DASH_EFFECT_HPP
#define FASTUIDRAW_PATH_DASH_EFFECT_HPP

#include <fastuidraw/path_effect.hpp>

namespace fastuidraw
{
/*!\addtogroup Paths
 * @{
 */

  /*!
   * A \ref PathDashEffect implemented \ref PathEffect to
   * apply a dash pattern to a path. The output data of a
   * \ref PathDashEffect -keeps- the distance values of
   * how far along an ege or countour, i.e. the values of
   * \ref TessellatedPath::cap::m_contour_length,
   * \ref TessellatedPath::cap::m_edge_length,
   * \ref TessellatedPath::cap::m_distance_from_edge_start,
   * \ref TessellatedPath::cap::m_distance_from_contour_start,
   * \ref TessellatedPath::join::m_distance_from_previous_join,
   * \ref TessellatedPath::join::m_distance_from_contour_start,
   * \ref TessellatedPath::join::m_contour_length,
   * \ref TessellatedPath::segment::m_distance_from_edge_start,
   * \ref TessellatedPath::segment::m_distance_from_contour_start,
   * \ref TessellatedPath::segment::m_contour_length and
   * \ref TessellatedPath::segment::m_edge_length are copied
   * directly from the source data. Only the value \ref
   * TessellatedPath::segment::m_length is adjusted (because
   * a given segment might be split into multiple segments).
   */
  class PathDashEffect:public PathEffect
  {
  public:
    PathDashEffect(void);
    ~PathDashEffect();

    /*!
     * Clear the dash pattern.
     */
    PathDashEffect&
    clear(void);

    /*!
     * Add an element to the dash pattern. An element in the
     * dash pattern is a length to draw following by a length
     * to skip.
     * \param draw length of draw in element
     * \param skip length of skip in element
     */
    PathDashEffect&
    add_dash(float draw, float skip);

    /*!
     * Distance values from the elements of the path have
     * this value added to them before having the dash-pattern
     * applied.
     */
    PathDashEffect&
    dash_offset(float v);

    virtual
    void
    process_chain(const segment_chain &chain, Storage &dst) const override;

    virtual
    void
    process_join(const TessellatedPath::join &join, Storage &dst) const override;

    virtual
    void
    process_cap(const TessellatedPath::cap &cap, Storage &dst) const override;

  private:
    void *m_d;
  };

/*! @} */
}

#endif
