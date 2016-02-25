/*!
 * \file painter_packing_brush.hpp
 * \brief file painter_packing_brush.hpp
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

  namespace PainterPacking
  {
    /*!
      Namespace for packing offset values for fastuidraw::PainterBrush values.
     */
    namespace Brush
    {
      /*!
        Enumeration giving the packing order for data of a brush.
        Each enumeration is an entry and when data is packed each
        entry starts on a multiple of the alignment (see
        PainterBackend::Configuration::alignment()) to the
        destination packing store.
      */
      enum packing_order_t
        {
          /*!
            Pen packed first, see \ref pen_offset_t
            for the offsets for the individual fields
           */
          pen_packing,

          /*!
            image packing, see \ref image_offset_t
            for the offsets for the individual fields
          */
          image_packing,

          /*!
            gradient packing, see \ref gradient_offset_t
            for the offsets from the start of gradient packing
            for individual fields
          */
          gradient_packing,

          /*!
            repeat window packing, see \ref
            repeat_window_offset_t for the offsets
            for the individual fields
          */
          repeat_window_packing,

          /*!
            transformation_translation, see \ref
            transformation_translation_offset_t for the
            offsets of the individual fields
          */
          transformation_translation_packing,

          /*!
            transformation_matrix, see \ref
            transformation_matrix_offset_t for the
            offsets of the individual fields
          */
          transformation_matrix_packing,
        };

      /*!
        Bit packing for the master index tile of a Image
       */
      enum image_atlas_location_encoding
        {
          image_atlas_location_x_num_bits = 10, /*!< number bits to encode Image::master_index_tile().x() */
          image_atlas_location_y_num_bits = 10, /*!< number bits to encode Image::master_index_tile().y() */
          image_atlas_location_z_num_bits = 10, /*!< number bits to encode Image::master_index_tile().z() */

          image_atlas_location_x_bit0 = 0, /*!< bit where Image::master_index_tile().x() is encoded */

          /*!
            bit where Image::master_index_tile().y() is encoded
          */
          image_atlas_location_y_bit0 = image_atlas_location_x_num_bits,

          /*!
            bit where Image::master_index_tile().z() is encoded
          */
          image_atlas_location_z_bit0 = image_atlas_location_y_bit0 + image_atlas_location_y_num_bits,
        };

      /*!
        Bit packing for size of the image, Image::dimensions()
       */
      enum image_size_encoding
        {
          image_size_x_num_bits = 16, /*!< number bits to encode Image::dimensions().x() */
          image_size_y_num_bits = 16, /*!< number bits to encode Image::dimensions().y() */

          image_size_x_bit0 = 0, /*!< bit where Image::dimensions().x() is encoded */
          image_size_y_bit0 = image_size_x_num_bits, /*!< bit where Image::dimensions().y() is encoded */
        };

      /*!
        enumerations for offsets to pen color values
       */
      enum pen_offset_t
        {
          pen_red_offset, /*!< offset for pen red value */
          pen_green_offset, /*!< offset for pen green value */
          pen_blue_offset, /*!< offset for pen blue value */
          pen_alpha_offset, /*!< offset for pen alpha value */

          pen_data_size /*!< number of elements to pack pen color */
        };

      /*!
        Offsets for image data packing; the number of index
        look ups is recorded in PainterBrush::shader().
        The ratio of the size of the image to the size of the
        master index is given by pow(I, Image::number_index_lookups).
        where I is given by ImageAtlas::index_tile_size().
        NOTE:
        - packing it into 2 elements is likely overkill since
          alignment is likely to be 4. We could split the
          atlas location over 3 full integers, or encode
          Image::master_index_tile_dims() as floats.
      */
      enum image_offset_t
        {
          /*!
            Location of image (Image::master_index_tile()) in
            the image atlas is encoded in a single uint32. The bits
            are packed as according to image_atlas_location_encoding
          */
          image_atlas_location_xyz_offset,

          /*!
            Width and height of the image (Image::dimensions())
            encoded in a single uint32. The bits are packed as according
            to image_size_encoding
          */
          image_size_xy_offset,

          /*!
            top left corner of start of image to use (for example
            using the entire image would be (0,0)). Both x and y
            start values are encoded into a simge uint32. Encoding
            is the same as image_size_xy_offset, see
            image_size_encoding
           */
          image_start_xy_offset,

          /*!
            Number of elements packed for image support
            for a brush.
          */
          image_data_size
        };

      /*!
        Bit encoding for packing ColorStopSequenceOnAtlas::texel_location()
       */
      enum gradient_color_stop_xy_encoding
        {
          gradient_color_stop_x_num_bits = 16, /*!< number bits to encode ColorStopSequenceOnAtlas::texel_location().x() */
          gradient_color_stop_y_num_bits = 16, /*!< number bits to encode ColorStopSequenceOnAtlas::texel_location().y() */

          gradient_color_stop_x_bit0 = 0, /*!< where ColorStopSequenceOnAtlas::texel_location().x() is encoded */
          gradient_color_stop_y_bit0 = gradient_color_stop_x_num_bits /*!< where ColorStopSequenceOnAtlas::texel_location().y() is encoded */
        };

      /*!
        Enumeration that provides offset from the start of
        gradient packing to data for linear or radial gradients.
      */
      enum gradient_offset_t
        {

          /*!
            Offset to x-coordinate of starting point of gradient
            (packed at float)
          */
          gradient_p0_x_offset,

          /*!
            Offset to y-coordinate of starting point of gradient
            (packed at float)
          */
          gradient_p0_y_offset,

          /*!
            Offset to x-coordinate of ending point of gradient
            (packed at float)
          */
          gradient_p1_x_offset,

          /*!
            Offset to y-coordinate of ending point of gradient
            (packed at float)
          */
          gradient_p1_y_offset,

          /*!
            Offset to the x and y-location of the color stops.
            The offset is stored as a uint32 packed as according
            in the enumeration gradient_color_stop_xy_encoding
          */
          gradient_color_stop_xy_offset,

          /*!
            Offset to the length of the color stop in -texels-, i.e.
            ColorStopSequenceOnAtlas::width(), packed as a uint32
          */
          gradient_color_stop_length_offset,

          /*!
            Size of the data for linear gradients.
          */
          linear_gradient_data_size,

          /*!
            Offset to starting radius of gradient
            (packed at float) (radial gradient only)
          */
          gradient_start_radius_offset = linear_gradient_data_size,

          /*!
            Offset to ending radius of gradient
            (packed at float) (radial gradient only)
          */
          gradient_end_radius_offset,

          /*!
            Size of the data for radial gradients.
          */
          radial_gradient_data_size
        };

      /*!
        Enumeration that provides offset from the start of
        repeat window packing to data for repeat window data
      */
      enum repeat_window_offset_t
        {
          repeat_window_x_offset, /*!< offset for the x-position of the repeat window (packed at float) */
          repeat_window_y_offset, /*!< offset for the y-position of the repeat window (packed at float) */
          repeat_window_width_offset, /*!< offset for the width of the repeat window (packed at float) */
          repeat_window_height_offset, /*!< offset for the height of the repeat window (packed at float) */
          repeat_window_data_size /*!< size of data for repeat window */
        };

      /*!
        Enumeration that provides offset from the start of
        repeat transformation matrix to data for the transformation
        matrix data
      */
      enum transformation_matrix_offset_t
        {
          transformation_matrix_m00_offset, /*!< offset for float2x2(0, 0) (packed at float) */
          transformation_matrix_m01_offset, /*!< offset for float2x2(0, 1) (packed at float) */
          transformation_matrix_m10_offset, /*!< offset for float2x2(1, 0) (packed at float) */
          transformation_matrix_m11_offset, /*!< offset for float2x2(1, 1) (packed at float) */
          transformation_matrix_data_size /*!< size of data for transformation matrix */
        };

      /*!
        Enumeration that provides offset from the start of
        repeat transformation translation to data for the
        transformation translation data
      */
      enum transformation_translation_offset_t
        {
          transformation_translation_x_offset, /*!< offset for x-coordinate of translation (packed at float) */
          transformation_translation_y_offset, /*!< offset for y-coordinate of translation (packed at float) */
          transformation_translation_data_size /*!< size of data for transformation translation (packed at float) */
        };
    } //namespace Brush
  } //namespace PainterPacking
/*! @} */

} //namespace fastuidraw
