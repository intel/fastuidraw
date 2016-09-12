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
/*!\addtogroup Painter
  @{
 */

  /*!
    A PainterItemMatrix holds the value for the
    transformation from item coordinates to the coordinates
    in which the clipping rectangle applies.
  */
  class PainterItemMatrix
  {
  public:

    /*!
      Enumeration that provides offsets for the
      item matrix from the location of that data
      (item_matrix_offset)
     */
    enum item_matrix_data_offset_t
      {
        matrix00_offset, /*!< offset of m_item_matrix(0,0) (packed as float) */
        matrix01_offset, /*!< offset of m_item_matrix(0,1) (packed as float) */
        matrix02_offset, /*!< offset of m_item_matrix(0,2) (packed as float) */
        matrix10_offset, /*!< offset of m_item_matrix(1,0) (packed as float) */
        matrix11_offset, /*!< offset of m_item_matrix(1,1) (packed as float) */
        matrix12_offset, /*!< offset of m_item_matrix(1,2) (packed as float) */
        matrix20_offset, /*!< offset of m_item_matrix(2,0) (packed as float) */
        matrix21_offset, /*!< offset of m_item_matrix(2,1) (packed as float) */
        matrix22_offset, /*!< offset of m_item_matrix(2,2) (packed as float) */

        matrix_data_size /*!< Size of the data for the item matrix */
      };

    /*!
      Ctor from a float3x3
      \param m value with which to initailize m_item_matrix
    */
    PainterItemMatrix(const float3x3 &m):
      m_item_matrix(m)
    {}

    /*!
      Ctor, initializes m_item_matrix as the
      identity matrix
    */
    PainterItemMatrix(void)
    {}

    /*!
      Returns the length of the data needed to encode the data.
      Data is padded to be multiple of alignment.
      \param alignment alignment of the data store
                       in units of generic_data, see
                       PainterBackend::ConfigurationBase::alignment()
    */
    unsigned int
    data_size(unsigned int alignment) const
    {
      return round_up_to_multiple(matrix_data_size, alignment);
    }

    /*!
      Pack the values of this PainterItemMatrix
      \param alignment alignment of the data store
      in units of generic_data, see
      PainterBackend::ConfigurationBase::alignment()
      \param dst place to which to pack data
    */
    void
    pack_data(unsigned int alignment, c_array<generic_data> dst) const;

    /*!
      The 3x3 matrix tranforming from item coordinate
      to the coordinates of the clipping rectange.
    */
    float3x3 m_item_matrix;
  };
/*! @} */

} //namespace
