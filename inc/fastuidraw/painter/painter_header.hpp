/*!
 * \file painter_header.hpp
 * \brief file painter_header.hpp
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
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterPacking
 * @{
 */

  /*!
   * \brief
   * A PainterHeader represents the values of the header for
   * the shaders to read to draw data. The header holds data
   * that is common for all vertices and fragments for an
   * item to draw, including with what shader and the data
   * for the shader.
   */
  class PainterHeader
  {
  public:
    /*!
     * Enumerations specifying how the contents of a PainterHeader
     * are packed into a data store buffer (PainterDraw::m_store).
     */
    enum offset_t
      {
        clip_equations_location_offset, /*!< offset to \ref m_clip_equations_location */
        item_matrix_location_offset, /*!< offset to \ref m_item_matrix_location */
        brush_shader_data_location_offset, /*!< offset to \ref m_brush_shader_data_location */
        item_shader_data_location_offset, /*!< offset to \ref m_item_shader_data_location */
        composite_shader_data_location_offset, /*!< offset to \ref m_composite_shader_data_location */
        blend_shader_data_location_offset, /*!< offset to \ref m_blend_shader_data_location */
        item_shader_offset, /*!< offset to \ref m_item_shader */
        brush_shader_offset, /*!< offset to \ref m_brush_shader */
        composite_shader_offset, /*!< offset to \ref m_composite_shader */
        blend_shader_offset, /*!< offset to \ref m_blend_shader */
        z_offset, /*!< offset to \ref m_z */

        header_size /*!< size of header */
      };

    /*!
     * The offset, in units of vecN<generic_data, 4> tuples, to the
     * location in the data store buffer (PainterDraw::m_store) for the
     * clip equations. I.e. the PainterClipEquations value is stored
     * (packed) at the location \code
     * PainterDraw::m_store[m_clip_equations_location * 4]
     * \endcode
     */
    uint32_t m_clip_equations_location;

    /*!
     * The location, in units of vecN<generic_data, 4> tuples, to the
     * location in the data store buffer (PainterDraw::m_store) for the
     * item matrix. I.e. the PainterItemMatrix value is stored (packed)
     * at the location
     * \code
     * PainterDraw::m_store[m_item_matrix_location * 4]
     * \endcode
     */
    uint32_t m_item_matrix_location;

    /*!
     * The location, in units of vecN<generic_data, 4> tuples, to the
     * location in the data store buffer (PainterDraw::m_store) for the
     * brush shader data. I.e. the data for a brush is stored (packed)
     * at the location
     * \code
     * PainterDraw::m_store[m_brush_shader_data_location * 4]
     * \endcode
     */
    uint32_t m_brush_shader_data_location;

    /*!
     * The location, in units of vecN<generic_data, 4> tuples, to the
     * location in the data store buffer (PainterDraw::m_store) for the
     * item shader data. I.e. the PainterItemShaderData value is stored
     * (packed) at the location
     * \code
     * PainterDraw::m_store[m_item_shader_data_location * 4]
     * \endcode
     */
    uint32_t m_item_shader_data_location;

    /*!
     * The location, in units of vecN<generic_data, 4> tuples, to the
     * location in the data store buffer (PainterDraw::m_store) for the
     * composite shader data. I.e. the PainterCompositeShaderData value
     * is stored (packed) at the location
     * \code
     * PainterDraw::m_store[m_composite_shader_data_location * 4]
     * \endcode
     */
    uint32_t m_composite_shader_data_location;

    /*!
     * The location, in units of vecN<generic_data, 4> tuples, to the
     * location in the data store buffer (PainterDraw::m_store) for the
     * blend shader data. I.e. the PainterBlendShaderData value is stored
     * (packed) at the location
     * \code
     * PainterDraw::m_store[m_blend_shader_data_location * 4]
     * \endcode
     */
    uint32_t m_blend_shader_data_location;

    /*!
     * The ID of the item shader (i.e. PainterItemShader::ID()).
     */
    uint32_t m_item_shader;

    /*!
     * The brush shader, i.e. the value of PainterBrush::shader().
     */
    uint32_t m_brush_shader;

    /*!
     * The ID of the composite shader (i.e. PainterCompositeShader::ID()).
     */
    uint32_t m_composite_shader;

    /*!
     * The ID of the blend shader (i.e. PainterBlendShader::ID()).
     */
    uint32_t m_blend_shader;

    /*!
     * The z-value to use for the item. The z-value is used
     * by Painter to implement clipping.
     */
    int32_t m_z;

    /*!
     * Pack the values of this PainterHeader
     * \param dst place to which to pack data
     */
    void
    pack_data(c_array<generic_data> dst) const;

    /*!
     * Returns the length of the data needed to encode the data.
     * Data is padded to be multiple of 4.
     */
    static
    unsigned int
    data_size(void)
    {
      return FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(header_size);
    }
  };

/*! @} */
}
