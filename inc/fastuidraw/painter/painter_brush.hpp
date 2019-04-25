/*!
 * \file painter_brush.hpp
 * \brief file painter_brush.hpp
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

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/math.hpp>

#include <fastuidraw/image.hpp>
#include <fastuidraw/colorstop_atlas.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter_shader_data.hpp>
#include <fastuidraw/painter/painter_brush_shader_data.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A PainterBrush defines a brush for painting via Painter.
   *
   * The brush applies, in the following order:
   *  -# a constant color, specified by \ref color()
   *  -# optionally modulates by an image, specified by \ref
   *     image() or \ref sub_image() and \ref no_image()
   *  -# optionally modulates by a gradient, see \ref
   *     linear_gradient(), \ref radial_gradient(),
   *     sweep_gradient() and \ref no_gradient().
   *
   *  An item shader's vertex stage provides the coordinate
   *  fed to the brush. That coordinate is procesed in the
   *  following order before it is fed to the image and gradient:
   *  -# an optional 2x2 matrix is applied, see \ref
   *     transformation_matrix(), \ref apply_matrix()
   *     \ref apply_shear(), \ref apply_rotate()
   *     and \ref no_transformation_matrix()
   *  -# an optional translation is applied specified, see
   *     \ref transformation_translate(), apply_matrix()
   *     \ref apply_translate() and \ref no_transformation_translation()
   *  -# an optional repeat window is applied, see \ref
   *     repeat_window() and \ref no_repeat_window().
   */
  class PainterBrush:public PainterBrushShaderData
  {
  public:
    /*!
     * \brief
     * Enumeration specifying what filter to apply to an image
     */
    enum image_filter
      {
        /*!
         * Indicates to use nearest filtering (i.e
         * choose closest pixel).
         */
        image_filter_nearest = 1,

        /*!
         * Indicates to use bilinear filtering.
         */
        image_filter_linear = 2,

        /*!
         * Indicates to use bicubic filtering.
         */
        image_filter_cubic = 3
      };

    /*!
     * \brief
     * Enumeration to specify what kind of gradient is applied
     */
    enum gradient_type_t
      {
        /*!
         * Indicates that no gradient is present
         */
        no_gradient_type = 0,

        /*!
         * Indicates that a linear gradient is present;
         * a linear gradient is defined by two points
         * p0 and p1 where the interpolate at a point p
         * is the value t when p = p0 + (p1 - p0) * t
         */
        linear_gradient_type = 1,

        /*!
         * Indicates that a radial gradient is present;
         * a radial gradient is defined by two circles
         * C0 = Circle(p0, r0), C1 = Circle(p1, r1)
         * where the interpolate at a point p is the
         * time t when p is on the circle C(t) where
         * C(t) = Circle(p(t), r(t)) where
         * p(t) = p0 + (p1 - p0) * t
         * r(t) = r0 + (r1 - r0) * t
         */
        radial_gradient_type = 2,

        /*!
         * A sweep gradient is defined by a single point C,
         * an angle theta (in radians), a sign S and a factor F.
         * The angle theta represents at what angle the gradient
         * starts, the point C is the center point of the
         * sweep, the sign of S represents the angle orientation
         * and the factor F reprsents how many times the
         * gradient is to be repated. Precisely, the interpolate
         * at a point p is defined as t_interpolate where
         * \code
         * vec2 d = p - C;
         * float theta, v;
         * theta = S * atan(p.y, p.x);
         * if (theta < alpha )
         *   {
         *    theta  += 2 * PI;
         *   }
         * theta -= alpha;
         * v = (theta - angle) / (2 * PI);
         * t_interpolate = (S < 0.0) ? F * (1.0 - v) : F * v;
         * \endcode
         */
        sweep_gradient_type = 3,
      };

    /*!
     * Enumeration to specify how a value is interpreted
     * outside of its natural range. For gradients the
     * range is [0, 1] acting on its interpolate.
     */
    enum spread_type_t
      {
        /*!
         * Clamp the value to its range, i.e.
         * for a value t on a range [A, B] the
         * value is clamp(r, A, B).
         */
        spread_clamp,

        /*!
         * Mirror the value across the start of
         * its range, i.e. for a value t on a
         * range [A, B] the value is
         * clamp(A + abs(t - A), A, B)
         */
        spread_mirror,

        /*!
         * Repeat the value to its range, i.e.
         * for a value t on a range [A, B] the
         * value is A + mod(t - A, B - A)
         */
        spread_repeat,

        /*!
         * Mirror repeat the  value across the start
         * of its range, i.e. for a value t on a
         * range [A, B] the value is
         * B - abs(mod(t - A, 2 * (B - A)) - (B - A))
         */
        spread_mirror_repeat,

        number_spread_types
      };

    /*!
     * enumeration to specify mipmapping on an image
     */
    enum mipmap_t
      {
        /*!
         * Indicates to apply mipmap filtering
         */
        apply_mipmapping,

        /*!
         * Indicates to not apply mipmap filtering
         */
        dont_apply_mipmapping
      };

    /*!
     * \brief
     * Enumeration describing the roles of the bits for
     * \ref features().
     */
    enum feature_bits
      {
        /*!
         * Number of bits needed to encode filter for image,
         * the value packed into the \ref features() encodes
         * both what filter to use and whether or not an image
         * is present. A value of 0 indicates no image applied,
         * a non-zero value indicates an image applied and
         * the value specifies what filter via the enumeration
         * image_filter.
         */
        image_filter_num_bits = 2,

        /*!
         * Number of bits needed to encode the image type
         * (when an image is present). The possible values
         * are given by the enumeration \ref Image::type_t.
         */
        image_type_num_bits = 4,

        /*!
         * Number of bits needed to encode the value of
         * Image::format().
         */
        image_format_num_bits = 1,

        /*!
         * Number of bits used to encode number of mipmap
         * levels (when an image is present).
         */
        image_mipmap_num_bits = 7,

        /*!
         * Number of bits used to encode the gradient type,
         * see \ref gradient_type_t
         */
        gradient_type_num_bits = 2,

        /*!
         * Number of bits used to encode a \ref spread_type_t
         */
        spread_type_num_bits = 2,

        /*!
         * first bit for if image is present on the brush and if so, what filter
         */
        image_filter_bit0 = 0,

        /*!
         * first bit to indicate maximum mipmap level to use
         */
        image_mipmap_bit0 = image_filter_bit0 + image_filter_num_bits,

        /*!
         * first bit used to encode the \ref gradient_type_t
         */
        gradient_type_bit0 = image_mipmap_bit0 + image_mipmap_num_bits,

        /*!
         * first bit used to encode the \ref gradient_spread_type
         */
        gradient_spread_type_bit0 = gradient_type_bit0 + gradient_type_num_bits,

        /*!
         * Bit up if the brush has a repeat window
         */
        repeat_window_bit = gradient_spread_type_bit0 + spread_type_num_bits,

        /* First bit used to encore the spread mode for the x-coordinate
         * if a repeat window is active
         */
        repeat_window_x_spread_type_bit0,

        /* First bit used to encore the spread mode for the x-coordinate
         * if a repeat window is active
         */
        repeat_window_y_spread_type_bit0 = repeat_window_x_spread_type_bit0 + spread_type_num_bits,

        /*!
         * Bit up if transformation 2x2 matrix is present
         */
        transformation_translation_bit = repeat_window_y_spread_type_bit0 + spread_type_num_bits,

        /*!
         * Bit up is translation is present
         */
        transformation_matrix_bit,

        /*!
         * First bit to hold the type of image present if an image is present;
         * the value is the enumeration in \ref Image::type_t
         */
        image_type_bit0,

        /*!
         * First bit to encode \ref Image::format_t
         */
        image_format_bit0 = image_type_bit0 + image_type_num_bits,

        /*!
         * Must be last enum, gives number of bits needed to hold feature bits
         * of a PainterBrush.
         */
        number_feature_bits = image_format_bit0 + image_format_num_bits,
      };

    /*!
     * \brief
     * Masks generated from \ref feature_bits, use these masks on the
     * return value of \ref features() to get what features
     * are active on the brush.
     */
    enum feature_masks
      {
        /*!
         * mask generated from \ref image_filter_bit0 and \ref image_filter_num_bits
         */
        image_mask = FASTUIDRAW_MASK(image_filter_bit0, image_filter_num_bits),

        /*!
         * mask generated from \ref image_mipmap_bit0 and \ref image_mipmap_num_bits
         */
        image_mipmap_mask = FASTUIDRAW_MASK(image_mipmap_bit0, image_mipmap_num_bits),

        /*!
         * mask generated from \ref gradient_type_bit0 and \ref gradient_type_num_bits
         */
        gradient_type_mask = FASTUIDRAW_MASK(gradient_type_bit0, gradient_type_num_bits),

        /*!
         * mask generated from \ref gradient_spread_type_bit0 and \ref spread_type_num_bits
         */
        gradient_spread_type_mask = FASTUIDRAW_MASK(gradient_spread_type_bit0, spread_type_num_bits),

        /*!
         * mask generated from \ref repeat_window_bit
         */
        repeat_window_mask = FASTUIDRAW_MASK(repeat_window_bit, 1),

        /*!
         * mask generated from \ref repeat_window_x_spread_type
         * and \ref spread_type_num_bits
         */
        repeat_window_x_spread_type_mask = FASTUIDRAW_MASK(repeat_window_x_spread_type_bit0,
                                                           spread_type_num_bits),

        /*!
         * mask generated from \ref repeat_window_y_spread_type
         * and \ref spread_type_num_bits
         */
        repeat_window_y_spread_type_mask = FASTUIDRAW_MASK(repeat_window_y_spread_type_bit0,
                                                           spread_type_num_bits),

        /*!
         * mask of \ref repeat_window_x_spread_type_mask and \ref
         * repeat_window_y_spread_type bitwise or'd together
         */
        repeat_window_spread_type_mask = repeat_window_x_spread_type_mask | repeat_window_y_spread_type_mask,

        /*!
         * mask generated from \ref transformation_translation_bit
         */
        transformation_translation_mask = FASTUIDRAW_MASK(transformation_translation_bit, 1),

        /*!
         * mask generated from \ref transformation_matrix_bit
         */
        transformation_matrix_mask = FASTUIDRAW_MASK(transformation_matrix_bit, 1),

        /*!
         * mask generated from \ref image_type_bit0 and \ref image_type_num_bits
         */
        image_type_mask = FASTUIDRAW_MASK(image_type_bit0, image_type_num_bits),

        /*!
         * mask generated from \ref image_format_bit0 and \ref image_format_num_bits
         */
        image_format_mask = FASTUIDRAW_MASK(image_format_bit0, image_format_num_bits),
      };

    /*!
     * \brief
     * Enumeration giving the packing order for data of a brush.
     * Each enumeration is an entry and when data is packed each
     * entry starts on a multiple of the 4.
     */
    enum packing_order_t
      {
        /*!
         * Color packed first, see \ref header_offset_t
         * for the offsets for the individual fields
         */
        header_packing,

        /*!
         * image packing, see \ref image_offset_t
         * for the offsets for the individual fields
         */
        image_packing,

        /*!
         * gradient packing, see \ref gradient_offset_t
         * for the offsets from the start of gradient packing
         * for individual fields
         */
        gradient_packing,

        /*!
         * repeat window packing, see \ref
         * repeat_window_offset_t for the offsets
         * for the individual fields
         */
        repeat_window_packing,

        /*!
         * transformation_translation, see \ref
         * transformation_translation_offset_t for the
         * offsets of the individual fields
         */
        transformation_translation_packing,

        /*!
         * transformation_matrix, see \ref
         * transformation_matrix_offset_t for the
         * offsets of the individual fields
         */
        transformation_matrix_packing,
      };

    /*!
     * \brief
     * Bit packing for the master index tile of a Image
     */
    enum image_atlas_location_encoding
      {
        image_atlas_location_x_num_bits = 8,  /*!< number bits to encode Image::master_index_tile().x() */
        image_atlas_location_y_num_bits = 8,  /*!< number bits to encode Image::master_index_tile().y() */
        image_atlas_location_z_num_bits = 16, /*!< number bits to encode Image::master_index_tile().z() */

        image_atlas_location_x_bit0 = 0, /*!< bit where Image::master_index_tile().x() is encoded */

        /*!
         * bit where Image::master_index_tile().y() is encoded
         */
        image_atlas_location_y_bit0 = image_atlas_location_x_num_bits,

        /*!
         * bit where Image::master_index_tile().z() is encoded
         */
        image_atlas_location_z_bit0 = image_atlas_location_y_bit0 + image_atlas_location_y_num_bits,
      };

    /*!
     * \brief
     * Bit packing for size of the image, Image::dimensions()
     */
    enum image_size_encoding
      {
        image_size_x_num_bits = 16, /*!< number bits to encode Image::dimensions().x() */
        image_size_y_num_bits = 16, /*!< number bits to encode Image::dimensions().y() */

        image_size_x_bit0 = 0, /*!< bit where Image::dimensions().x() is encoded */
        image_size_y_bit0 = image_size_x_num_bits, /*!< bit where Image::dimensions().y() is encoded */
      };

    /*!
     * \brief
     * enumerations for offsets to color values
     */
    enum header_offset_t
      {
        features_offset, /*!< offset to value of features() (packed as uint) */
        header_red_green_offset, /*!< offset for color red and green value (packed as fp16 pair) */
        header_blue_alpha_offset, /*!< offset for color blue and alpha value (packed as fp16 pair) */

        header_data_size /*!< number of elements to pack color */
      };

    /*!
     * \brief
     * Offsets for image data packing.
     *
     * The number of index look ups is recorded in
     * PainterBrush::features(). The ratio of the size of the
     * image to the size of the master index is given by
     * pow(I, Image::number_index_lookups). where I is given
     * by ImageAtlas::index_tile_size().
     */
    enum image_offset_t
      {
        /*!
         * Width and height of the image (Image::dimensions())
         * encoded in a single uint32. The bits are packed as according
         * to \ref image_size_encoding
         */
        image_size_xy_offset,

        /*!
         * top left corner of start of image to use (for example
         * using the entire image would be (0,0)). Both x and y
         * start values are encoded into a single uint32. Encoding
         * is the same as \ref image_size_xy_offset, see \ref
         * image_size_encoding
         */
        image_start_xy_offset,

        /*!
         * Location of image (Image::master_index_tile()) in the image
         * atlas is encoded in a single uint32. The bits are packed as
         * according to \ref image_atlas_location_encoding. If the image
         * is not of type Image::on_atlas, gives the high 32-bits of
         * Image::handle().
         */
        image_atlas_location_xyz_offset,

        /*!
         * Offset to the high 32-bits of the handle value when the
         * Image is of type Image::bindless_texture2d.
         */
        image_bindless_handle_hi_offset = image_atlas_location_xyz_offset,

        /*!
         * Holds the amount the number of index looks ups, see \ref
         * Image::number_index_lookups(). If the image is not of type
         * Image::on_atlas, gives the low 32-bits of Image::handle().
         */
        image_number_lookups_offset,

        /*!
         * Offset to the low 32-bits of the handle value when the
         * Image is of type Image::bindless_texture2d.
         */
        image_bindless_handle_low_offset = image_number_lookups_offset,

        /*!
         * Number of elements packed for image support
         * for a brush.
         */
        image_data_size
      };

    /*!
     * \brief
     * Bit encoding for packing ColorStopSequenceOnAtlas::texel_location()
     */
    enum gradient_color_stop_xy_encoding
      {
        gradient_color_stop_x_num_bits = 16, /*!< number bits to encode ColorStopSequenceOnAtlas::texel_location().x() */
        gradient_color_stop_y_num_bits = 16, /*!< number bits to encode ColorStopSequenceOnAtlas::texel_location().y() */

        gradient_color_stop_x_bit0 = 0, /*!< where ColorStopSequenceOnAtlas::texel_location().x() is encoded */
        gradient_color_stop_y_bit0 = gradient_color_stop_x_num_bits /*!< where ColorStopSequenceOnAtlas::texel_location().y() is encoded */
      };

    /*!
     * \brief
     * Enumeration that provides offset from the start of
     * gradient packing to data for linear or radial gradients.
     */
    enum gradient_offset_t
      {
        /*!
         * Offset to x-coordinate of starting point of both
         * linear and radial gradients (packed at float)
         */
        gradient_p0_x_offset = 0,

        /*!
         * Offset to y-coordinate of starting point of both
         * linear and radial gradients (packed at float)
         */
        gradient_p0_y_offset,

        /*!
         * Offset to x-coordinate of ending point of both
         * linear and radial gradients (packed at float)
         */
        gradient_p1_x_offset,

        /*!
         * Offset to y-coordinate of ending point of both
         * linear and radial gradients (packed at float)
         */
        gradient_p1_y_offset,

        /*!
         * Offset to the x and y-location of the color stops.
         * The offset is stored as a uint32 packed as according
         * in the enumeration \ref gradient_color_stop_xy_encoding
         */
        gradient_color_stop_xy_offset,

        /*!
         * Offset to the length of the color stop in -texels-, i.e.
         * ColorStopSequenceOnAtlas::width(), packed as a uint32
         */
        gradient_color_stop_length_offset,

        /*!
         * Size of the data for linear gradients.
         */
        linear_gradient_data_size,

        /*!
         * Offset to starting radius of gradient
         * (packed at float) (radial gradient only)
         */
        gradient_start_radius_offset = linear_gradient_data_size,

        /*!
         * Offset to ending radius of gradient
         * (packed at float) (radial gradient only)
         */
        gradient_end_radius_offset,

        /*!
         * Size of the data for radial gradients.
         */
        radial_gradient_data_size,

        /*!
         * Offset to the x-coordinate of the point
         * of a sweep gradient.
         */
        sweep_gradient_p_x_offset = gradient_p0_x_offset,

        /*!
         * Offset to the y-coordinate of the point
         * of a sweep gradient.
         */
        sweep_gradient_p_y_offset = gradient_p0_y_offset,

        /*!
         * Offset to the angle of a sweep gradient.
         */
        sweep_gradient_angle_offset = gradient_p1_x_offset,

        /*!
         * Offset to the sign-factor of the sweep gradient.
         * The sign of the value is the sign of the sweep
         * gradient and the magnitude is the repeat factor
         * of the gradient.
         */
        sweep_gradient_sign_factor_offset = gradient_p1_y_offset,

        /*!
         * Size of the data for sweep gradients.
         */
        sweep_gradient_data_size = linear_gradient_data_size,
      };

    /*!
     * \brief
     * Enumeration that provides offset from the start of
     * repeat window packing to data for repeat window data
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
     * \brief
     * Enumeration that provides offset from the start of
     * repeat transformation matrix to data for the transformation
     * matrix data
     */
    enum transformation_matrix_offset_t
      {
        transformation_matrix_row0_col0_offset, /*!< offset for float2x2(0, 0) (packed at float) */
        transformation_matrix_row0_col1_offset, /*!< offset for float2x2(0, 1) (packed at float) */
        transformation_matrix_row1_col0_offset, /*!< offset for float2x2(1, 0) (packed at float) */
        transformation_matrix_row1_col1_offset, /*!< offset for float2x2(1, 1) (packed at float) */
        transformation_matrix_data_size, /*!< size of data for transformation matrix */

        transformation_matrix_col0_row0_offset = transformation_matrix_row0_col0_offset, /*!< alias of \ref transformation_matrix_row0_col0_offset */
        transformation_matrix_col0_row1_offset = transformation_matrix_row1_col0_offset, /*!< alias of \ref transformation_matrix_row1_col0_offset */
        transformation_matrix_col1_row0_offset = transformation_matrix_row0_col1_offset, /*!< alias of \ref transformation_matrix_row0_col1_offset */
        transformation_matrix_col1_row1_offset = transformation_matrix_row1_col1_offset, /*!< alias of \ref transformation_matrix_row1_col1_offset */
      };

    /*!
     * \brief
     * Enumeration that provides offset from the start of
     * repeat transformation translation to data for the
     * transformation translation data
     */
    enum transformation_translation_offset_t
      {
        transformation_translation_x_offset, /*!< offset for x-coordinate of translation (packed at float) */
        transformation_translation_y_offset, /*!< offset for y-coordinate of translation (packed at float) */
        transformation_translation_data_size /*!< size of data for transformation translation (packed at float) */
      };

    /*!
     * Ctor. Initializes the brush to have no image, no gradient,
     * no repeat window and no transformation with the color as
     * (1.0, 1.0, 1.0, 1.0) which is solid white.
     */
    PainterBrush(void)
    {}

    /*!
     * Copy ctor.
     */
    PainterBrush(const PainterBrush &obj):
      m_data(obj.m_data)
    {}

    /*!
     * Ctor. Initializes the brush to have no image, no gradient,
     * no repeat window and no transformation with the given
     * color color.
     * \param pcolor inital color
     */
    PainterBrush(const vec4 &pcolor)
    {
      m_data.m_color = pcolor;
    }

    /*!
     * Assignment operator.
     * \param obj value from which to copy
     */
    PainterBrush&
    operator=(const PainterBrush &obj)
    {
      m_data = obj.m_data;
      return *this;
    }

    /*!
     * Reset the brush to initial conditions.
     */
    PainterBrush&
    reset(void);

    /*!
     * Set the color to the color, default value is (1.0, 1.0, 1.0, 1.0).
     */
    PainterBrush&
    color(const vec4 &color)
    {
      m_data.m_color = color;
      return *this;
    }

    /*!
     * Set the color to the color, default value is (1.0, 1.0, 1.0, 1.0).
     */
    PainterBrush&
    color(float r, float g, float b, float a = 1.0f)
    {
      return color(vec4(r, g, b, a));
    }

    /*!
     * Returns the current color-color.
     */
    const vec4&
    color(void) const
    {
      return m_data.m_color;
    }

    /*!
     * Sets the brush to have an image.
     * \param im handle to image to use. If handle is invalid,
     *           then sets brush to not have an image.
     * \param f filter to apply to image, only has effect if im
     *          is non-nullptr
     * \param mipmap_filtering specifies if to apply mipmap filtering
     */
    PainterBrush&
    image(const reference_counted_ptr<const Image> &im,
          enum image_filter f = image_filter_nearest,
          enum mipmap_t mipmap_filtering = apply_mipmapping);

    /*!
     * Set the brush to source from a sub-rectangle of an image
     * \param im handle to image to use
     * \param xy min-corner of sub-rectangle of image to use
     * \param wh width and height of sub-rectangle of image to use
     * \param f filter to apply to image, only has effect if im
     *          is non-nullptr
     * \param mipmap_filtering specifies if to apply mipmap filtering
     */
    PainterBrush&
    sub_image(const reference_counted_ptr<const Image> &im, uvec2 xy, uvec2 wh,
              enum image_filter f = image_filter_nearest,
              enum mipmap_t mipmap_filtering = apply_mipmapping);

    /*!
     * Sets the brush to not have an image.
     */
    PainterBrush&
    no_image(void)
    {
      return image(reference_counted_ptr<const Image>());
    }

    /*!
     * Sets the brush to have a linear gradient.
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param start_p start position of gradient
     * \param end_p end position of gradient.
     * \param spread specifies the gradient spread type
     */
    PainterBrush&
    linear_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
                    const vec2 &start_p, const vec2 &end_p,
                    enum spread_type_t spread)
    {
      uint32_t gradient_bits;

      m_data.m_cs = cs;
      m_data.m_grad_start = start_p;
      m_data.m_grad_end = end_p;
      gradient_bits = cs ?
        pack_bits(gradient_type_bit0, gradient_type_num_bits, linear_gradient_type)
        | pack_bits(gradient_spread_type_bit0, spread_type_num_bits, spread) :
        0u;
      m_data.m_features_raw &= ~(gradient_type_mask | gradient_spread_type_mask);
      m_data.m_features_raw |= gradient_bits;
      return *this;
    }

    /*!
     * Sets the brush to have a radial gradient.
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param start_p start position of gradient
     * \param start_r starting radius of radial gradient
     * \param end_p end position of gradient.
     * \param end_r ending radius of radial gradient
     * \param spread specifies the gradient spread type
     */
    PainterBrush&
    radial_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
                    const vec2 &start_p, float start_r,
                    const vec2 &end_p, float end_r,
                    enum spread_type_t spread)
    {
      uint32_t gradient_bits;

      m_data.m_cs = cs;
      m_data.m_grad_start = start_p;
      m_data.m_grad_start_r = start_r;
      m_data.m_grad_end = end_p;
      m_data.m_grad_end_r = end_r;
      gradient_bits = cs ?
        pack_bits(gradient_type_bit0, gradient_type_num_bits, radial_gradient_type)
        | pack_bits(gradient_spread_type_bit0, spread_type_num_bits, spread) :
        0u;
      m_data.m_features_raw &= ~(gradient_type_mask | gradient_spread_type_mask);
      m_data.m_features_raw |= gradient_bits;
      return *this;
    }

    /*!
     * Sets the brush to have a radial gradient. Provided as
     * a conveniance, equivalent to
     * \code
     * radial_gradient(cs, p, 0.0f, p, r, repeat);
     * \endcode
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param p start and end position of gradient
     * \param r ending radius of radial gradient
     * \param spread specifies the gradient spread type
     */
    PainterBrush&
    radial_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
                    const vec2 &p, float r, enum spread_type_t spread)
    {
      return radial_gradient(cs, p, 0.0f, p, r, spread);
    }

    /*!
     * Sets the brush to have a sweep gradient (directly).
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param p position of gradient
     * \param theta start angle of the sweep gradient, this value
     *              should be in the range [-PI, PI]
     * \param F the repeat factor applied to the interpolate, the
     *          sign of F is used to determine the sign of the
     *          sweep gradient.
     * \param spread specifies the gradient spread type
     */
    PainterBrush&
    sweep_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
                   const vec2 &p, float theta, float F,
                   enum spread_type_t spread)
    {
      uint32_t gradient_bits;

      m_data.m_cs = cs;
      m_data.m_grad_start = p;
      m_data.m_grad_end = vec2(theta, F);
      gradient_bits = cs ?
        pack_bits(gradient_type_bit0, gradient_type_num_bits, sweep_gradient_type)
        | pack_bits(gradient_spread_type_bit0, spread_type_num_bits, spread) :
        0u;
      m_data.m_features_raw &= ~(gradient_type_mask | gradient_spread_type_mask);
      m_data.m_features_raw |= gradient_bits;
      return *this;
    }

    /*!
     * Sets the brush to have a sweep gradient where the sign is
     * determined by a PainterEnums::screen_orientation and a
     * PainterEnums::rotation_orientation_t.
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param p position of gradient
     * \param theta angle of the sweep gradient, this value
     *              should be in the range [-PI, PI]
     * \param F the repeat factor applied to the interpolate,
     *          a negative reverses the orientation of the sweep.
     * \param orientation orientation of the screen
     * \param rotation_orientation orientation of the sweep
     * \param spread specifies the gradient spread type
     */
    PainterBrush&
    sweep_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
                   const vec2 &p, float theta,
                   enum PainterEnums::screen_orientation orientation,
                   enum PainterEnums::rotation_orientation_t rotation_orientation,
                   float F, enum spread_type_t spread)
    {
      float S;
      bool b1(orientation == PainterEnums::y_increases_upwards);
      bool b2(rotation_orientation == PainterEnums::counter_clockwise);

      S = (b1 == b2) ? 1.0f : -1.0f;
      return sweep_gradient(cs, p, theta, S * F, spread);
    }

    /*!
     * Sets the brush to have a sweep gradient with a repeat factor
     * of 1.0 and where the sign is determined by a
     * PainterEnums::screen_orientation and a
     * PainterEnums::rotation_orientation_t. Equivalent to
     * \code
     * sweep_gradient(cs, p, theta, orientation, rotation_orientation, 1.0f, repeat);
     * \endcode
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param p position of gradient
     * \param theta angle of the sweep gradient, this value
     *              should be in the range [-PI, PI]
     * \param orientation orientation of the screen
     * \param rotation_orientation orientation of the sweep
     * \param spread specifies the gradient spread type
     */
    PainterBrush&
    sweep_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
                   const vec2 &p, float theta,
                   enum PainterEnums::screen_orientation orientation,
                   enum PainterEnums::rotation_orientation_t rotation_orientation,
                   enum spread_type_t spread)
    {
      return sweep_gradient(cs, p, theta, orientation, rotation_orientation, 1.0f, spread);
    }

    /*!
     * Sets the brush to not have a gradient.
     */
    PainterBrush&
    no_gradient(void)
    {
      m_data.m_cs = reference_counted_ptr<const ColorStopSequenceOnAtlas>();
      m_data.m_features_raw &= ~(gradient_type_mask | gradient_spread_type_mask);
      return *this;
    }

    /*!
     * Return the gradient_type_t that the brush applies.
     */
    enum gradient_type_t
    gradient_type(void) const
    {
      uint32_t v;
      v = unpack_bits(gradient_type_bit0,
                      gradient_type_num_bits,
                      m_data.m_features_raw);
      return static_cast<enum gradient_type_t>(v);
    }

    /*!
     * Return the gradient_type_t that the brush applies.
     */
    enum spread_type_t
    gradient_spread_type(void) const
    {
      uint32_t v;
      v = unpack_bits(gradient_spread_type_bit0,
                      spread_type_num_bits,
                      m_data.m_features_raw);
      return static_cast<enum spread_type_t>(v);
    }

    /*!
     * Sets the brush to have a translation in its transformation.
     * \param p translation value for brush transformation
     */
    PainterBrush&
    transformation_translate(const vec2 &p)
    {
      m_data.m_transformation_p = p;
      m_data.m_features_raw |= transformation_translation_mask;
      return *this;
    }

    /*!
     * Returns the translation of the transformation of the brush.
     */
    const vec2&
    transformation_translate(void) const
    {
      return m_data.m_transformation_p;
    }

    /*!
     * Sets the brush to have a matrix in its transformation.
     * \param m matrix value for brush transformation
     */
    PainterBrush&
    transformation_matrix(const float2x2 &m)
    {
      m_data.m_transformation_matrix = m;
      m_data.m_features_raw |= transformation_matrix_mask;
      return *this;
    }

    /*!
     * Returns the matrix of the transformation of the brush.
     */
    const float2x2&
    transformation_matrix(void) const
    {
      return m_data.m_transformation_matrix;
    }

    /*!
     * Apply a shear to the transformation of the brush.
     * \param m matrix to which to apply
     */
    PainterBrush&
    apply_matrix(const float2x2 &m)
    {
      m_data.m_features_raw |= transformation_matrix_mask;
      m_data.m_transformation_matrix = m_data.m_transformation_matrix * m;
      return *this;
    }

    /*!
     * Apply a shear to the transformation of the brush.
     * \param sx scale factor in x-direction
     * \param sy scale factor in y-direction
     */
    PainterBrush&
    apply_shear(float sx, float sy)
    {
      m_data.m_features_raw |= transformation_matrix_mask;
      m_data.m_transformation_matrix(0, 0) *= sx;
      m_data.m_transformation_matrix(1, 0) *= sx;
      m_data.m_transformation_matrix(0, 1) *= sy;
      m_data.m_transformation_matrix(1, 1) *= sy;
      return *this;
    }

    /*!
     * Apply a rotation to the transformation of the brush.
     * \param angle in radians by which to rotate
     */
    PainterBrush&
    apply_rotate(float angle)
    {
      float s, c;
      float2x2 tr;

      s = t_sin(angle);
      c = t_cos(angle);
      tr(0, 0) = c;
      tr(1, 0) = s;
      tr(0, 1) = -s;
      tr(1, 1) = c;
      apply_matrix(tr);
      return *this;
    }

    /*!
     * Apply a translation to the transformation of the brush.
     */
    PainterBrush&
    apply_translate(const vec2 &p)
    {
      m_data.m_transformation_p += m_data.m_transformation_matrix * p;
      m_data.m_features_raw |= transformation_translation_mask;
      return *this;
    }

    /*!
     * Sets the brush to have a matrix and translation in its
     * transformation
     * \param p translation value for brush transformation
     * \param m matrix value for brush transformation
     */
    PainterBrush&
    transformation(const vec2 &p, const float2x2 &m)
    {
      transformation_translate(p);
      transformation_matrix(m);
      return *this;
    }

    /*!
     * Sets the brush to have no translation in its transformation.
     */
    PainterBrush&
    no_transformation_translation(void)
    {
      m_data.m_features_raw &= ~transformation_translation_mask;
      m_data.m_transformation_p = vec2(0.0f, 0.0f);
      return *this;
    }

    /*!
     * Sets the brush to have no matrix in its transformation.
     */
    PainterBrush&
    no_transformation_matrix(void)
    {
      m_data.m_features_raw &= ~transformation_matrix_mask;
      m_data.m_transformation_matrix = float2x2();
      return *this;
    }

    /*!
     * Sets the brush to have no transformation.
     */
    PainterBrush&
    no_transformation(void)
    {
      no_transformation_translation();
      no_transformation_matrix();
      return *this;
    }

    /*!
     * Sets the brush to have a repeat window
     * \param pos location of repeat window
     * \param size of repeat window
     * \param x_mode spread mode for x-coordinate
     * \param y_mode spread mode for y-coordinate
     */
    PainterBrush&
    repeat_window(const vec2 &pos, const vec2 &size,
                  enum spread_type_t x_mode = spread_repeat,
                  enum spread_type_t y_mode = spread_repeat)
    {
      m_data.m_window_position = pos;
      m_data.m_window_size = size;
      m_data.m_features_raw |= repeat_window_mask;
      m_data.m_features_raw &= ~repeat_window_spread_type_mask;
      m_data.m_features_raw |= pack_bits(repeat_window_x_spread_type_bit0,
                                       spread_type_num_bits,
                                       x_mode);
      m_data.m_features_raw |= pack_bits(repeat_window_y_spread_type_bit0,
                                       spread_type_num_bits,
                                       y_mode);
      return *this;
    }

    /*!
     * Returns true if a repeat window is applied to the
     * brush and writes out the position and size of
     * the repeat window as well.
     */
    bool
    repeat_window(vec2 *pos, vec2 *size) const
    {
      *pos = m_data.m_window_position;
      *size = m_data.m_window_size;
      return m_data.m_features_raw & repeat_window_mask;
    }

    /*!
     * Returns the x-coordinate spread type. Return
     * value is undefined if the repeat_window() is
     * not active.
     */
    enum spread_type_t
    repeat_window_x_spread_type(void) const
    {
      uint32_t v;
      v = unpack_bits(repeat_window_x_spread_type_bit0,
                      spread_type_num_bits,
                      m_data.m_features_raw);
      return static_cast<enum spread_type_t>(v);
    }

    /*!
     * Returns the y-coordinate spread type. Return
     * value is undefined if the repeat_window() is
     * not active.
     */
    enum spread_type_t
    repeat_window_y_spread_type(void) const
    {
      uint32_t v;
      v = unpack_bits(repeat_window_y_spread_type_bit0,
                      spread_type_num_bits,
                      m_data.m_features_raw);
      return static_cast<enum spread_type_t>(v);
    }

    /*!
     * Sets the brush to not have a repeat window
     */
    PainterBrush&
    no_repeat_window(void)
    {
      m_data.m_features_raw &= ~(repeat_window_mask | repeat_window_spread_type_mask);
      return *this;
    }

    /*!
     * Returns the brush features which when tested against the
     * bit masks from \ref feature_masks tells what features are
     * active in the brush; \ref features() is decoded as follows:
     *
     * - The value given by
     *   \code
     *   unpack_bits(image_filter_bit0, image_filter_num_bits, features())
     *   \endcode
     *   is non-zero if an image is present and when is non-zero the value's meaning
     *   is enumerated by image_filter
     * - The value given by
     *   \code
     *   unpack_bits(gradient_type_bit0, gradient_type_num_bits, features())
     *   \endcode
     *   gives what gradient (if any) the brush applies as according to
     *   \ref gradient_type_t
     * - The value given by
     *   \code
     *   unpack_bits(gradient_spread_type_bit0, gradient_spread_type_num_bits, features())
     *   \endcode
     *   gives the gradient spread pattern (if any) the brush applies as
     *   according to \ref spread_type_t
     * - If features() & \ref repeat_window_mask is non-zero, then a repeat
     *   window is applied to the brush.
     * - If features() & \ref transformation_translation_mask is non-zero, then a
     *   translation is applied to the brush.
     * - If features() & \ref transformation_matrix_mask is non-zero, then a
     *   2x2 matrix is applied to the brush.
     */
    uint32_t
    features(void) const;

    /*!
     * Returns true if the brush sources from an Image whose
     * Image::type() is \ref Image::context_texture2d.
     */
    bool
    image_requires_binding(void) const
    {
      return image()
        && image()->type() == Image::context_texture2d;
    }

    /*!
     * Returns a reference to the Image that the brush is set
     * to use; if there is no such image, returns nullptr.
     */
    const reference_counted_ptr<const Image>&
    image(void) const
    {
      return m_data.m_image;
    }

    /*!
     * Returns a pointer to the Image that the brush is
     * set to use together with the sub-image of the image
     * used.
     * \param start location to which to write the min-corner
     *              of the image that is used
     * \param size location to which to write the size of
     *             the portion of the image used.
     */
    const reference_counted_ptr<const Image>&
    image(uvec2 *start, uvec2 *size) const
    {
      *start = m_data.m_image_start;
      *size = m_data.m_image_size;
      return m_data.m_image;
    }

    /*!
     * Returns the value of the handle to the
     * ColorStopSequenceOnAtlas that the
     * brush is set to use.
     */
    const reference_counted_ptr<const ColorStopSequenceOnAtlas>&
    color_stops(void) const
    {
      return m_data.m_cs;
    }

    unsigned int
    data_size(void) const override;

    void
    pack_data(c_array<generic_data> dst) const override;

    void
    save_resources(c_array<reference_counted_ptr<const resource_base> > dst) const override
    {
      dst[0] = m_data.m_image;
      dst[1] = m_data.m_cs;
    }

    unsigned int
    number_resources(void) const override
    {
      return 2;
    }

    c_array<const reference_counted_ptr<const Image> >
    bind_images(void) const override
    {
      return (image_requires_binding()) ?
        c_array<const reference_counted_ptr<const Image> >(&m_data.m_image, 1) :
        c_array<const reference_counted_ptr<const Image> >();
    }

  private:
    class brush_data
    {
    public:
      brush_data(void):
        m_features_raw(0),
        m_color(1.0f, 1.0f, 1.0f, 1.0f),
        m_image_size(0, 0),
        m_image_start(0, 0),
        m_grad_start(0.0f, 0.0f),
        m_grad_end(1.0f, 1.0f),
        m_grad_start_r(0.0f),
        m_grad_end_r(1.0f),
        m_window_position(0.0f, 0.0f),
        m_window_size(1.0f, 1.0f),
        m_transformation_matrix(),
        m_transformation_p(0.0f, 0.0f)
      {}

      uint32_t m_features_raw;
      vec4 m_color;
      reference_counted_ptr<const Image> m_image;
      uvec2 m_image_size, m_image_start;
      reference_counted_ptr<const ColorStopSequenceOnAtlas> m_cs;
      vec2 m_grad_start, m_grad_end;
      float m_grad_start_r, m_grad_end_r;
      vec2 m_window_position, m_window_size;
      float2x2 m_transformation_matrix;
      vec2 m_transformation_p;

      /*
       * m_grad_start holds point for sweep gradient
       * m_grad_end.x() holds the sweep angle of the
       * m_grad_end.y() holds the recirpocal of the
       * sweep angle
       */
    };

    brush_data m_data;
  };
/*! @} */
}
