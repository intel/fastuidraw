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

#include <fastuidraw/image.hpp>
#include <fastuidraw/colorstop_atlas.hpp>

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
   *  -# a constant color, specified by \ref pen()
   *  -# optionally applies an image, specified by \ref image() or
   *     \ref sub_image() also see \ref no_image()
   *  -# optionally applies a linear or radial gradient, see \ref
   *     linear_gradient() and \ref radial_gradient() also see
   *     \ref no_gradient().
   *
   *  An item shader's vertex stage provides the coordinate
   *  fed to the brush. That coordinate is procesed in the
   *  following order before it is fed to the image and gradient:
   *  -# an optional 2x2 matrix is applied specified by \ref
   *     transformation_matrix(), also see \ref
   *     no_transformation_matrix() and \ref
   *     transformation().
   *  -# an optional translation is applied specified by \ref
   *     transformation_translate(), also see \ref
   *     no_transformation_translation() and \ref
   *     transformation().
   *  -# an optional repeat window is applied specified by \ref
   *     repeat_window(), also see no_repeat_window().
   */
  class PainterBrush
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
         * choose closest pixel). No requirement on
         * Image::slack() when using this filtering
         * option.
         */
        image_filter_nearest = 1,

        /*!
         * Indicates to use bilinear filtering. Requires
         * that Image::slack() is atleast 1, otherwise
         * rendering results will be wrong.
         */
        image_filter_linear = 2,

        /*!
         * Indicates to use bicubic filtering. Requires
         * that Image::slack() is atleast 2, otherwise
         * rendering results will be wrong.
         */
        image_filter_cubic = 3
      };

    /*!
     * \brief
     * Enumeration describing the roles of the bits for
     * PainterBrush::shader().
     */
    enum shader_bits
      {
        /*!
         * Number of bits needed to encode filter for image,
         * the value packed into the shader ID encodes both
         * what filter to use and whether or not an image
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
         * first bit for if image is present on the brush and if so, what filter
         */
        image_filter_bit0 = 0,

        /*!
         * Bit is up if a gradient is present
         */
        gradient_bit = image_filter_bit0 + image_filter_num_bits,

        /*!
         * bit is up if gradient is present and it is radial
         */
        radial_gradient_bit,

        /*!
         * bit is up if gradient is present and gradient lookup repeats outside of [0,1]
         */
        gradient_repeat_bit,

        /*!
         * Bit up if the brush has a repeat window
         */
        repeat_window_bit,

        /*!
         * Bit up if transformation 2x2 matrix is present
         */
        transformation_translation_bit,

        /*!
         * Bit up is translation is present
         */
        transformation_matrix_bit,

        /*!
         * First bit to hold the type of image present if an image is present;
         * the value is the enumeration in \ref Image::type_t
         */
        image_type_bit0,
      };

    /*!
     * \brief
     * Masks generated from shader_bits, use these masks on the
     * return value of PainterBrush::shader() to get what features
     * are active on the brush.
     */
    enum shader_masks
      {
        /*!
         * mask generated from \ref image_filter_bit0 and \ref image_filter_num_bits
         */
        image_mask = FASTUIDRAW_MASK(image_filter_bit0, image_filter_num_bits),

        /*!
         * mask generated from \ref gradient_bit
         */
        gradient_mask = FASTUIDRAW_MASK(gradient_bit, 1),

        /*!
         * mask generated from \ref radial_gradient_bit
         */
        radial_gradient_mask = FASTUIDRAW_MASK(radial_gradient_bit, 1),

        /*!
         * mask generated from \ref gradient_repeat_bit
         */
        gradient_repeat_mask = FASTUIDRAW_MASK(gradient_repeat_bit, 1),

        /*!
         * mask generated from \ref repeat_window_bit
         */
        repeat_window_mask = FASTUIDRAW_MASK(repeat_window_bit, 1),

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
      };

    /*!
     * \brief
     * Enumeration giving the packing order for data of a brush.
     * Each enumeration is an entry and when data is packed each
     * entry starts on a multiple of the alignment (see
     * PainterBackend::ConfigurationBase::alignment()) to the
     * destination packing store.
     */
    enum packing_order_t
      {
        /*!
         * Pen packed first, see \ref pen_offset_t
         * for the offsets for the individual fields
         */
        pen_packing,

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
     * Encoding for bits to specify Image::number_index_lookups()
     * and Image::number_index_lookups().
     */
    enum image_slack_number_lookups_encoding
      {
        /*!
         * Number bits used to store the value of
         * Image::slack().
         */
        image_slack_num_bits = 16,

        /*!
         * Number bits used to store the value of
         * Image::number_index_lookups()
         */
        image_number_index_lookups_num_bits = 16,

        /*!
         * first bit used to store Image::number_index_lookups()
         */
        image_number_index_lookups_bit0 = 0,

        /*!
         * first bit used to store Image::slack()
         */
        image_slack_bit0 = image_number_index_lookups_bit0 + image_number_index_lookups_num_bits,
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
     * enumerations for offsets to pen color values
     */
    enum pen_offset_t
      {
        pen_red_offset, /*!< offset for pen red value (packed as float) */
        pen_green_offset, /*!< offset for pen green value (packed as float) */
        pen_blue_offset, /*!< offset for pen blue value (packed as float) */
        pen_alpha_offset, /*!< offset for pen alpha value (packed as float) */

        pen_data_size /*!< number of elements to pack pen color */
      };

    /*!
     * \brief
     * Offsets for image data packing.
     *
     * The number of index look ups is recorded in
     * PainterBrush::shader(). The ratio of the size of the
     * image to the size of the master index is given by
     * pow(I, Image::number_index_lookups). where I is given
     * by ImageAtlas::index_tile_size().
     * NOTE:
     * - packing it into 2 elements is likely overkill since
     *   alignment is likely to be 4. We could split the
     *   atlas location over 3 full integers, or encode
     *   Image::master_index_tile_dims() as floats.
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
         * Alias for image_atlas_location_xyz_offset to be used
         * when packing an image whose type is not Image::on_atlas.
         */
        image_bindless_handle_hi_offset = image_atlas_location_xyz_offset,

        /*!
         * Holds the amount of slack in the image (see Image::slack())
         * and the number of index looks ups (Image::number_index_lookups())
         * with bits packed as according to \ref image_slack_number_lookups_encoding.
         * If the image is not of type Image::on_atlas, gives the low 32-bits
         * of Image::handle().
         */
        image_slack_number_lookups_offset,

        /*!
         * Alias for image_slack_number_lookups_offset to be used
         * when packing an image whose type is not Image::on_atlas.
         */
        image_bindless_handle_low_offset = image_slack_number_lookups_offset,

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
         * Offset to x-coordinate of starting point of gradient
         * (packed at float)
         */
        gradient_p0_x_offset,

        /*!
         * Offset to y-coordinate of starting point of gradient
         * (packed at float)
         */
        gradient_p0_y_offset,

        /*!
         * Offset to x-coordinate of ending point of gradient
         * (packed at float)
         */
        gradient_p1_x_offset,

        /*!
         * Offset to y-coordinate of ending point of gradient
         * (packed at float)
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
        radial_gradient_data_size
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
        transformation_matrix_m00_offset, /*!< offset for float2x2(0, 0) (packed at float) */
        transformation_matrix_m01_offset, /*!< offset for float2x2(0, 1) (packed at float) */
        transformation_matrix_m10_offset, /*!< offset for float2x2(1, 0) (packed at float) */
        transformation_matrix_m11_offset, /*!< offset for float2x2(1, 1) (packed at float) */
        transformation_matrix_data_size /*!< size of data for transformation matrix */
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
     * no repeat window and no transformation with the pen color
     * as (1.0, 1.0, 1.0, 1.0) which is solid white.
     */
    PainterBrush(void)
    {}

    /*!
     * Ctor. Initializes the brush to have no image, no gradient,
     * no repeat window and no transformation with the given
     * pen color.
     * \param ppen_color inital pen color
     */
    PainterBrush(const vec4 &ppen_color)
    {
      m_data.m_pen = ppen_color;
    }

    /*!
     * Reset the brush to initial conditions.
     */
    void
    reset(void);

    /*!
     * Set the color to the pen, default value is (1.0, 1.0, 1.0, 1.0).
     */
    PainterBrush&
    pen(const vec4 &color)
    {
      m_data.m_pen = color;
      return *this;
    }

    /*!
     * Set the color to the pen, default value is (1.0, 1.0, 1.0, 1.0).
     */
    PainterBrush&
    pen(float r, float g, float b, float a = 1.0f)
    {
      return pen(vec4(r, g, b, a));
    }

    /*!
     * Sets the brush to have an image.
     * \param im handle to image to use. If handle is invalid,
     *           then sets brush to not have an image.
     * \param f filter to apply to image, only has effect if im
     *          is non-nullptr
     */
    PainterBrush&
    image(const reference_counted_ptr<const Image> &im, enum image_filter f = image_filter_nearest);

    /*!
     * Set the brush to source from a sub-rectangle of an image
     * \param im handle to image to use
     * \param xy top-left corner of sub-rectangle of image to use
     * \param wh width and height of sub-rectangle of image to use
     * \param f filter to apply to image, only has effect if im
     *          is non-nullptr
     */
    PainterBrush&
    sub_image(const reference_counted_ptr<const Image> &im, uvec2 xy, uvec2 wh,
              enum image_filter f = image_filter_nearest);

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
     * \param repeat if true, repeats the gradient, if false then
     *               clamps the gradient
     */
    PainterBrush&
    linear_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
                    const vec2 &start_p, const vec2 &end_p, bool repeat)
    {
      m_data.m_cs = cs;
      m_data.m_grad_start = start_p;
      m_data.m_grad_end = end_p;
      m_data.m_shader_raw = apply_bit_flag(m_data.m_shader_raw, cs, gradient_mask);
      m_data.m_shader_raw = apply_bit_flag(m_data.m_shader_raw, cs && repeat, gradient_repeat_mask);
      m_data.m_shader_raw &= ~radial_gradient_mask;
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
     * \param repeat if true, repeats the gradient, if false then
     *               clamps the gradient
     */
    PainterBrush&
    radial_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
                    const vec2 &start_p, float start_r,
                    const vec2 &end_p, float end_r, bool repeat)
    {
      m_data.m_cs = cs;
      m_data.m_grad_start = start_p;
      m_data.m_grad_start_r = start_r;
      m_data.m_grad_end = end_p;
      m_data.m_grad_end_r = end_r;
      m_data.m_shader_raw = apply_bit_flag(m_data.m_shader_raw, cs, gradient_mask);
      m_data.m_shader_raw = apply_bit_flag(m_data.m_shader_raw, cs && repeat, gradient_repeat_mask);
      m_data.m_shader_raw = apply_bit_flag(m_data.m_shader_raw, cs, radial_gradient_mask);
      return *this;
    }

    /*!
     * Sets the brush to not have a gradient.
     */
    PainterBrush&
    no_gradient(void)
    {
      m_data.m_cs = reference_counted_ptr<const ColorStopSequenceOnAtlas>();
      m_data.m_shader_raw &= ~(gradient_mask | gradient_repeat_mask | radial_gradient_mask);
      return *this;
    }

    /*!
     * Sets the brush to have a translation in its transformation.
     * \param p translation value for brush transformation
     */
    PainterBrush&
    transformation_translate(const vec2 &p)
    {
      m_data.m_transformation_p = p;
      m_data.m_shader_raw |= transformation_translation_mask;
      return *this;
    }

    /*!
     * Sets the brush to have a matrix in its transformation.
     * \param m matrix value for brush transformation
     */
    PainterBrush&
    transformation_matrix(const float2x2 &m)
    {
      m_data.m_transformation_matrix = m;
      m_data.m_shader_raw |= transformation_matrix_mask;
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
      m_data.m_shader_raw &= ~transformation_translation_mask;
      return *this;
    }

    /*!
     * Sets the brush to have no matrix in its transformation.
     */
    PainterBrush&
    no_transformation_matrix(void)
    {
      m_data.m_shader_raw &= ~transformation_matrix_mask;
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
     */
    PainterBrush&
    repeat_window(const vec2 &pos, const vec2 &size)
    {
      m_data.m_window_position = pos;
      m_data.m_window_size = size;
      m_data.m_shader_raw |= repeat_window_mask;
      return *this;
    }

    /*!
     * Sets the brush to not have a repeat window
     */
    PainterBrush&
    no_repeat_window(void)
    {
      m_data.m_shader_raw &= ~repeat_window_mask;
      return *this;
    }

    /*!
     * Returns the length of the data needed to encode the brush.
     * Data is padded to be multiple of alignment and also
     * sub-data of brush is padded to be along alignment
     * boundaries.
     * \param alignment alignment of the data store, see
     *                  PainterBackend::ConfigurationBase::alignment(void) const
     */
    unsigned int
    data_size(unsigned int alignment) const;

    /*!
     * Encodes the data. Data is packed in the order
     * specified by \ref packing_order_t.
     * Data is padded to be multiple of alignment and also
     * sub-data of brush is padded to be along alignment
     * boundaries.
     * \param dst location to which to encode the brush
     * \param alignment alignment of the data store, see
     *                  PainterBackend::ConfigurationBase::alignment(void) const
     */
    void
    pack_data(unsigned int alignment, c_array<generic_data> dst) const;

    /*!
     * Returns the brush shader ID which when tested against the
     * bit masks from \ref shader_masks tells what features are
     * active in the brush. The shader is decoded as follows:
     *
     * - The value given by
     *   \code
     *   unpack_bits(image_filter_bit0, image_filter_num_bits, shader())
     *   \endcode
     *   is non-zero if an image is present and when is non-zero the value's meaning
     *   is enumerated by image_filter
     * - If shader() & \ref gradient_mask is non-zero, then a gradient is applied.
     *   The gradient is a linear gradient if shader() & \ref radial_gradient_mask
     *   is zero and a radial gradient otherwise.
     * - If shader() & \ref radial_gradient_mask is non-zero, then a radial
     *   gradient is applied. Note that if shader() & \ref radial_gradient_mask
     *   is non-zero, then shader() & \ref gradient_mask is also non-zero.
     * - If shader() & \ref gradient_repeat_mask then the gradient is repeated
     *   instead of clamped. Note that if shader() & \ref gradient_repeat_mask
     *   is non-zero, then shader() & \ref gradient_mask is also non-zero.
     * - If shader() & \ref repeat_window_mask is non-zero, then a repeat
     *   window is applied to the brush.
     * - If shader() & \ref transformation_translation_mask is non-zero, then a
     *   translation is applied to the brush.
     * - If shader() & \ref transformation_matrix_mask is non-zero, then a
     *   2x2 matrix is applied to the brush.
     */
    uint32_t
    shader(void) const;

    /*!
     * Returns the value of the handle to the
     * Image that the brush is set to use.
     */
    const reference_counted_ptr<const Image>&
    image(void) const
    {
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

    /*!
     * Returns true if and only if passed image can
     * be rendered correctly with the specified filter.
     * \param im handle to image
     * \param f image filter to which to with which test if
     *          im can be rendered
     */
    static
    bool
    filter_suitable_for_image(const reference_counted_ptr<const Image> &im,
                              enum image_filter f);

    /*!
     * Returns the highest quality filter with which
     * an image may be rendered.
     * \param im image to which to query
     */
    static
    enum image_filter
    best_filter_for_image(const reference_counted_ptr<const Image> &im);

    /*!
     * Returns the slack requirement for an image to
     * be rendered correctly under a filter.
     * \param f filter to query
     */
    static
    int
    slack_requirement(enum image_filter f);

  private:

    class brush_data
    {
    public:
      brush_data(void):
        m_shader_raw(0),
        m_pen(1.0f, 1.0f, 1.0f, 1.0f),
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

      uint32_t m_shader_raw;
      vec4 m_pen;
      reference_counted_ptr<const Image> m_image;
      uvec2 m_image_size, m_image_start;
      reference_counted_ptr<const ColorStopSequenceOnAtlas> m_cs;
      vec2 m_grad_start, m_grad_end;
      float m_grad_start_r, m_grad_end_r;
      vec2 m_window_position, m_window_size;
      float2x2 m_transformation_matrix;
      vec2 m_transformation_p;
    };

    brush_data m_data;
  };
/*! @} */
}
