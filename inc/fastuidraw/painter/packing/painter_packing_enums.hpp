/*!
 * \file painter_packing_enums.hpp
 * \brief file painter_packing_enums.hpp
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

namespace fastuidraw
{
/*!\addtogroup PainterPacking
  @{
 */

  /* the first part of a shader of a backend for fastuidraw
     will do is read a header from a buffer. The header
     consists offsets to data.
   */

  /*!
    Namespace encapsulating the offsets used in data packing
    by PainterPacker.
   */
  namespace PainterPacking
  {
    /*!
      Bit packing for the vert-frag shader field of header
      (located at vert_frag_shader_offset)
     */
    enum vert_frag_shader_encoding
      {
        /*!
          Number of bits used to store the vertex shader ID.
         */
        vert_shader_num_bits = 16,

        /*!
          Number of bits to store the fragment shader ID.
         */
        frag_shader_num_bits = 16,

        /*!
          First bit used to store the vertex shader ID
         */
        vert_shader_bit0 = 0,

        /*!
          First bit used to store the frag shader ID
         */
        frag_shader_bit0 = vert_shader_num_bits,
      };

    /*!
      Bit packing for the z-blend-shader field of the header
      (located at z_blend_shader_offset)
     */
    enum z_blend_shader_encoding
      {
        /*!
          Number of bits used for the z-value
         */
        z_num_bits = 23,

        /*!
          Number of bits used for the blend shader ID
         */
        blend_shader_num_bits = 32 - z_num_bits,

        /*!
          First bit used to store the z value
         */
        z_bit0 = 0,

        /*!
          First bit used to store the blend shader ID
         */
        blend_shader_bit0 = z_num_bits,
      };

    /*!
      A header is a set of uint32_t values. A headers is
      shared by all vertices of an invocation of a draw
      method of PainterPacker. The enumeration item_header_offset_t
      gives the offsets from the header location for the
      values of the header. All offsets are packed as uint32_t
      values. NOTE: the values packed into a header
      give location into the data store buffer in uint32_t units
      of alignment (see PainterBackend::Configuration::alignment()).
      In contrast, the offsets in item_header_offset_t that
      specifies the offsets in the header are in uint32_t units.
     */
    enum item_header_offset_t
      {
        /*!
          Location in item header for offset to
          clipping equations. The values at
          the offset are packed as according
          to the enumeration clip_equations_data_offset_t.
         */
        clip_equations_offset,

        /*!
          Location in the item header for the
          offset to the 3x3 matrix transforming
          (x,y,w) coordinates of the item to
          3D API clip coordinates. The values at
          the offset are packed as according
          to the enumeration item_matrix_data_offset_t.
         */
        item_matrix_offset,

        /*!
          Location in item header for offset of the
          brush data used by shaders in fastuidraw.
          The data represents the data
          for the fastuidraw::PainterBrush with
          which to draw. The brush data is a collection
          of packed objects, with what is packed determined
          by the brush shader ID value (packed at offset
          \ref brush_shader_offset). To determine what is packed
          examine the brush shader bit-wised anded with
          the masks defined in fastuidraw::PainterBrush::shader_masks.
          The ordering of what is packed is set by the enumeration
          PainterPacking::Brush::packing_order_t.
         */
        brush_shader_data_offset,

        /*!
          Location in item header for offset of the
          data that is vertex shader specific
         */
        vert_shader_data_offset,

        /*!
          Location in item header for offset of the
          data that is fragment shader specific
         */
        frag_shader_data_offset,

        /*!
          Location in item header for the vertex
          and fragment shader ID. The enumerants
          \ref vert_shader_num_bits, \ref frag_shader_num_bits,
          \ref vert_shader_bit0 and \ref frag_shader_bit0
          describe how to unpack the value.
         */
        vert_frag_shader_offset,

        /*!
          Location in item header for the brush
          shader value (PainterBrush::shader()).
         */
        brush_shader_offset,

        /*!
          Location in item header for the z
          value and blend shader ID. The enumerants
          \ref z_num_bits, \ref blend_shader_num_bits,
          \ref z_bit0 and \ref blend_shader_bit0
          describe how to unpack the value.
         */
        z_blend_shader_offset,

        /*!
          Size of the header.
         */
        header_size
      };

    /*!
      Enumeration that provides offsets for the
      elements of the clip equation offsets
      (clip_equations_offset)
     */
    enum clip_equations_data_offset_t
      {
        clip0_coeff_x, /*!< offset to x-coefficient for clip equation 0 */
        clip0_coeff_y, /*!< offset to y-coefficient for clip equation 0 */
        clip0_coeff_w, /*!< offset to w-coefficient for clip equation 0 */

        clip1_coeff_x, /*!< offset to x-coefficient for clip equation 1 */
        clip1_coeff_y, /*!< offset to y-coefficient for clip equation 1 */
        clip1_coeff_w, /*!< offset to w-coefficient for clip equation 1 */

        clip2_coeff_x, /*!< offset to x-coefficient for clip equation 2 */
        clip2_coeff_y, /*!< offset to y-coefficient for clip equation 2 */
        clip2_coeff_w, /*!< offset to w-coefficient for clip equation 2 */

        clip3_coeff_x, /*!< offset to x-coefficient for clip equation 3 */
        clip3_coeff_y, /*!< offset to y-coefficient for clip equation 3 */
        clip3_coeff_w, /*!< offset to w-coefficient for clip equation 3 */

        clip_equations_data_size /*!< number of elements for clip equations */
      };

    /*!
      Enumeration that provides offsets for the
      item matrix from the location of that data
      (item_matrix_offset)
     */
    enum item_matrix_data_offset_t
      {
        item_matrix_m00_offset, /*!< offset of item matrix float3x3(0,0) (packed as float) */
        item_matrix_m01_offset, /*!< offset of item matrix float3x3(0,1) (packed as float) */
        item_matrix_m02_offset, /*!< offset of item matrix float3x3(0,2) (packed as float) */
        item_matrix_m10_offset, /*!< offset of item matrix float3x3(1,0) (packed as float) */
        item_matrix_m11_offset, /*!< offset of item matrix float3x3(1,1) (packed as float) */
        item_matrix_m12_offset, /*!< offset of item matrix float3x3(1,2) (packed as float) */
        item_matrix_m20_offset, /*!< offset of item matrix float3x3(2,0) (packed as float) */
        item_matrix_m21_offset, /*!< offset of item matrix float3x3(2,1) (packed as float) */
        item_matrix_m22_offset, /*!< offset of item matrix float3x3(2,2) (packed as float) */

        item_matrix_data_size /*!< Size of the data for the item matrix */
      };

    /*!
      Enumeration that provides offsets for the stroking
      parameters. These values are realized as vertex
      shader data (Painter::vert_shader_data()).
     */
    enum stroke_data_offset_t
      {
        stroke_width_offset, /*!< offset to stroke width (packed as float) */
        stroke_miter_limit_offset, /*!< offset to stroke miter limit (packed as float) */

        stroke_data_size /*!< size of data for stroking*/
      };

  }
/*! @} */
}
