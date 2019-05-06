/*!
 * \file painter_item_matrix.hpp
 * \brief file painter_item_matrix.hpp
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


#pragma once

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */

  /*!
   * \brief
   * A PainterItemMatrix holds the value for the
   * transformation from item coordinates to the coordinates
   * in which the clipping rectangle applies.
   */
  class PainterItemMatrix
  {
  public:

    /*!
     * \brief
     * Enumeration that provides offsets for the
     * item matrix from the location of that data
     * (item_matrix_offset) in units of \ref
     * generic_data.
     */
    enum item_matrix_data_offset_t
      {
        matrix_row0_col0_offset, /*!< offset of m_item_matrix(0, 0) (packed as float) */
        matrix_row0_col1_offset, /*!< offset of m_item_matrix(0, 1) (packed as float) */
        matrix_row0_col2_offset, /*!< offset of m_item_matrix(0, 2) (packed as float) */
        matrix_row1_col0_offset, /*!< offset of m_item_matrix(1, 0) (packed as float) */
        matrix_row1_col1_offset, /*!< offset of m_item_matrix(1, 1) (packed as float) */
        matrix_row1_col2_offset, /*!< offset of m_item_matrix(1, 2) (packed as float) */
        matrix_row2_col0_offset, /*!< offset of m_item_matrix(2, 0) (packed as float) */
        matrix_row2_col1_offset, /*!< offset of m_item_matrix(2, 1) (packed as float) */
        matrix_row2_col2_offset, /*!< offset of m_item_matrix(2, 2) (packed as float) */

        normalized_translate_x,  /*!< offset of m_normalized_translate.x() */
        normalized_translate_y,  /*!< offset of m_normalized_translate.y() */

        matrix_data_size, /*!< Size of the data for the item matrix */

        matrix_col0_row0_offset = matrix_row0_col0_offset, /*!< alias of \ref matrix_row0_col0_offset */
        matrix_col0_row1_offset = matrix_row1_col0_offset, /*!< alias of \ref matrix_row1_col0_offset */
        matrix_col0_row2_offset = matrix_row2_col0_offset, /*!< alias of \ref matrix_row2_col0_offset */
        matrix_col1_row0_offset = matrix_row0_col1_offset, /*!< alias of \ref matrix_row0_col1_offset */
        matrix_col1_row1_offset = matrix_row1_col1_offset, /*!< alias of \ref matrix_row1_col1_offset */
        matrix_col1_row2_offset = matrix_row2_col1_offset, /*!< alias of \ref matrix_row2_col1_offset */
        matrix_col2_row0_offset = matrix_row0_col2_offset, /*!< alias of \ref matrix_row0_col2_offset */
        matrix_col2_row1_offset = matrix_row1_col2_offset, /*!< alias of \ref matrix_row1_col2_offset */
        matrix_col2_row2_offset = matrix_row2_col2_offset, /*!< alias of \ref matrix_row2_col2_offset */
      };

    /*!
     * Ctor from a float3x3
     * \param m value with which to initailize \ref m_item_matrix
     * \param t value with which to initailize \ref m_normalized_translate
     */
    explicit
    PainterItemMatrix(const float3x3 &m, const vec2 &t = vec2(0.0f, 0.0f)):
      m_item_matrix(m),
      m_normalized_translate(t)
    {}

    /*!
     * Ctor, initializes \ref m_item_matrix as the identity matrix
     * \param t value with which to initailize \ref m_normalized_translate
     */
    PainterItemMatrix(const vec2 &t = vec2(0.0f, 0.0f)):
      m_normalized_translate(t)
    {}

    /*!
     * Returns the length of the data needed to encode the data.
     * Data is padded to be multiple of 4.
     */
    unsigned int
    data_size(void) const
    {
      return FASTUIDRAW_NUMBER_BLOCK4_NEEDED(matrix_data_size);
    }

    /*!
     * Pack the values of this PainterItemMatrix
     * \param dst place to which to pack data
     */
    void
    pack_data(c_array<vecN<generic_data, 4> > dst) const;

    /*!
     * The 3x3 matrix tranforming from item coordinate
     * to the coordinates of the clipping rectange.
     */
    float3x3 m_item_matrix;

    /*!
     * The translation in normalized device coordinates
     * to apply to all vertices. For various internal
     * implementation details, it is more efficient
     * to have them seperate here instead of concating
     * it to \ref m_item_matrix
     */
    vec2 m_normalized_translate;
  };
/*! @} */

} //namespace
