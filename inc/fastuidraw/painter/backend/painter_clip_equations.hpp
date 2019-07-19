/*!
 * \file painter_clip_equations.hpp
 * \brief file painter_clip_equations.hpp
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


#ifndef FASTUIDRAW_PAINTER_CLIP_EQUATIONS_HPP
#define FASTUIDRAW_PAINTER_CLIP_EQUATIONS_HPP

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */

  /*!
   * \brief
   * A PainterClipEquations stores the clip equation for
   * Painter.
   *
   * Each vec3 gives a clip equation in 3D API clip coordinats
   * (i.e. after PainterItemMatrix transformation is applied) as
   * dot(clip_vector, p) >= 0.
   */
  class PainterClipEquations
  {
  public:
    /*!
     * \brief
     * Enumeration that provides offsets for the
     * elements of the clip equation offsets
     * in units of uint32_t.
     */
    enum clip_equations_data_offset_t
      {
        clip0_coeff_x, /*!< offset to x-coefficient for clip equation 0 (i.e. m_clip_equations[0].x) */
        clip0_coeff_y, /*!< offset to y-coefficient for clip equation 0 (i.e. m_clip_equations[0].y) */
        clip0_coeff_w, /*!< offset to w-coefficient for clip equation 0 (i.e. m_clip_equations[0].z) */

        clip1_coeff_x, /*!< offset to x-coefficient for clip equation 1 (i.e. m_clip_equations[1].x) */
        clip1_coeff_y, /*!< offset to y-coefficient for clip equation 1 (i.e. m_clip_equations[1].y) */
        clip1_coeff_w, /*!< offset to w-coefficient for clip equation 1 (i.e. m_clip_equations[1].z) */

        clip2_coeff_x, /*!< offset to x-coefficient for clip equation 2 (i.e. m_clip_equations[2].x) */
        clip2_coeff_y, /*!< offset to y-coefficient for clip equation 2 (i.e. m_clip_equations[2].y) */
        clip2_coeff_w, /*!< offset to w-coefficient for clip equation 2 (i.e. m_clip_equations[2].z) */

        clip3_coeff_x, /*!< offset to x-coefficient for clip equation 3 (i.e. m_clip_equations[3].x) */
        clip3_coeff_y, /*!< offset to y-coefficient for clip equation 3 (i.e. m_clip_equations[3].y) */
        clip3_coeff_w, /*!< offset to w-coefficient for clip equation 3 (i.e. m_clip_equations[3].z) */

        clip_data_size /*!< number of elements for clip equations */
      };

    /*!
     * Ctor, initializes all clip equations as \f$ z \geq 0\f$
     */
    PainterClipEquations(void):
      m_clip_equations(vec3(0.0f, 0.0f, 1.0f))
    {}

    /*!
     * Pack the values as according to \ref clip_equations_data_offset_t
     * \param dst place to which to pack data
     */
    void
    pack_data(c_array<uvec4> dst) const;

    /*!
     * Returns the length of the data needed to encode the data.
     * Data is padded to be multiple of 4.
     */
    unsigned int
    data_size(void) const
    {
      return FASTUIDRAW_NUMBER_BLOCK4_NEEDED(clip_data_size);
    }

    /*!
     * Each element of m_clip_equations specifies a
     * clipping plane in 3D API clip-space as
     * \code
     * dot(m_clip_equations[i], p) >= 0
     * \endcode
     */
    vecN<vec3, 4> m_clip_equations;
  };

/*! @} */

} //namespace fastuidraw

#endif
