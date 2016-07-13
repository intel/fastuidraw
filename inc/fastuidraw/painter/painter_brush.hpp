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
  @{
 */

  /*!
    A PainterBrush defines a brush for painting via Painter.
  */
  class PainterBrush
  {
  public:

    enum
      {
        /*!
          Number bits used to store the value of
          Image::number_index_lookups()
         */
        image_number_index_lookups_num_bits = 5,

        /*! max value storeable for Image::number_index_lookups()
         */
        image_number_index_lookups_max = FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(image_number_index_lookups_num_bits),

        /*!
          Number bits used to store the value of
          Image::slack().
         */
        image_slack_num_bits = 2,

        /*! max value storeable for Image::slack()
         */
        image_slack_max = FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(image_slack_num_bits),

        /*!
          Number of bits needed to encode filter for image,
          the value packed into the shader ID encodes both
          what filter to use and whether or not an image
          is present. A value of 0 indicates no image applied,
          a non-zero value indicates an image applied and
          the value specifies what filter via the enumeration
          image_filter.
         */
        image_filter_num_bits = 2,
      };

    /*!
      Enumeration specifying what filter to apply to an image
     */
    enum image_filter
      {
	/*!
	  Indicates to use nearest filtering (i.e
	  choose closest pixel). No requirement on
	  Image::slack() when using this filtering
	  option.
	 */
        image_filter_nearest = 1,

	/*!
	  Indicates to use bilinear filtering. Requires
	  that Image::slack() is atleast 1, otherwise
	  rendering results will be wrong.
	 */
        image_filter_linear = 2,

	/*!
	  Indicates to use bicubic filtering. Requires
	  that Image::slack() is atleast 2, otherwise
	  rendering results will be wrong.
	 */
        image_filter_cubic = 3
      };

    /*!
      Enumeration describing the roles of the bits for
      PainterBrush::shader().
     */
    enum shader_bits
      {
        image_filter_bit0, /*!< first bit for if image is present on the brush and if so, what filter */
        gradient_bit = image_filter_bit0 + image_filter_num_bits, /*!< Bit is up if a gradient is present */
        radial_gradient_bit, /*!< bit is up if gradient is present and it is radial */
        gradient_repeat_bit, /*!< bit is up if gradient is present and gradient lookup repeats outside of [0,1] */
        repeat_window_bit, /*!< Bit up if the brush has a repeat window */
        transformation_translation_bit, /*!< Bit up if transformation 2x2 matrix is present */
        transformation_matrix_bit, /*!< Bit up is translation is present */
        image_number_index_lookups_bit0, /*!< first bit used to store Image::number_index_lookups() */

        /*!
          first bit used to store Image::slack()
         */
        image_slack_bit0 = image_number_index_lookups_bit0 + image_number_index_lookups_num_bits,
      };

    /*!
      Masks generated from shader_bits, use these masks on the
      return value of PainterBrush::shader() to get what features
      are active on the brush.
     */
    enum shader_masks
      {
        /*!
          bit mask for if image is used in brush,
          the value of shader() bit wise anded
          with \ref image_mask shifted right by
          \ref image_filter_bit0 gives what filter
          to use from the values of the enumeration
          of image_filter.
         */
        image_mask = (3 << image_filter_bit0),

        /*!
          bit for if gradient is used in brush
         */
        gradient_mask = (1 << gradient_bit),

        /*!
          bit for if radial_gradient is used in brush
          (only up if gradient_mask is also up)
         */
        radial_gradient_mask = (1 << radial_gradient_bit),

        /*!
          bit for if repeat gradient is used in brush
          (only up if gradient_mask is also up)
         */
        gradient_repeat_mask = (1 << gradient_repeat_bit),

        /*!
          bit for if repeat_window is used in brush
         */
        repeat_window_mask = (1 << repeat_window_bit),

        /*!
          bit mask for if translation is used in brush
         */
        transformation_translation_mask = (1 << transformation_translation_bit),

        /*!
          bit mask for if matrix is used in brush
         */
        transformation_matrix_mask = (1 << transformation_matrix_bit),

        /*!
          bit mask for how many index lookups needed for image used in brush
         */
        image_number_index_lookups_mask = (image_number_index_lookups_max << image_number_index_lookups_bit0),

        /*!
          bit mask for how much slack for image used in brush
         */
        image_slack_mask = (image_slack_max << image_slack_bit0),
      };

    /*!
      Ctor. Initializes the brush to have no image, no gradient,
      no repeat window and no transformation.
     */
    PainterBrush(void)
    {}

    /*!
      Reset the brush to initial conditions.
     */
    void
    reset(void);

    /*!
      Set the color to the pen, default value is (1.0, 1.0, 1.0, 1.0).
     */
    PainterBrush&
    pen(const vec4 &color)
    {
      m_data.m_pen = color;
      return *this;
    }

    /*!
      Set the color to the pen, default value is (1.0, 1.0, 1.0, 1.0).
     */
    PainterBrush&
    pen(float r, float g, float b, float a = 1.0f)
    {
      return pen(vec4(r, g, b, a));
    }

    /*!
      Sets the brush to have an image.
      \param im handle to image to use. If handle is invalid,
                then sets brush to not have an image.
      \param f filter to apply to image, only has effect if im
               is non-NULL
     */
    PainterBrush&
    image(const Image::const_handle im, enum image_filter f = image_filter_nearest);

    /*!
      Set the brush to source from a sub-rectangle of an image
      \param im handle to image to use
      \param xy top-left corner of sub-rectangle of image to use
      \param wh width and height of sub-rectangle of image to use
      \param f filter to apply to image, only has effect if im
               is non-NULL
     */
    PainterBrush&
    sub_image(const Image::const_handle im, uvec2 xy, uvec2 wh,
              enum image_filter f = image_filter_nearest);

    /*!
      Sets the brush to not have an image.
     */
    PainterBrush&
    no_image(void)
    {
      return image(Image::const_handle());
    }

    /*!
      Sets the brush to have a linear gradient.
      \param cs color stops for gradient. If handle is invalid,
                then sets brush to not have a gradient.
      \param start_p start position of gradient
      \param end_p end position of gradient.
      \param repeat if true, repeats the gradient, if false then
                    clamps the gradient
     */
    PainterBrush&
    linear_gradient(const ColorStopSequenceOnAtlas::const_handle cs,
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
      Sets the brush to have a radial gradient.
      \param cs color stops for gradient. If handle is invalid,
                then sets brush to not have a gradient.
      \param start_p start position of gradient
      \param start_r starting radius of radial gradient
      \param end_p end position of gradient.
      \param end_r ending radius of radial gradient
      \param repeat if true, repeats the gradient, if false then
                    clamps the gradient
     */
    PainterBrush&
    radial_gradient(const ColorStopSequenceOnAtlas::const_handle cs,
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
      Sets the brush to not have a gradient.
     */
    PainterBrush&
    no_gradient(void)
    {
      m_data.m_cs = ColorStopSequenceOnAtlas::const_handle();
      m_data.m_shader_raw &= ~(gradient_mask | gradient_repeat_mask | radial_gradient_mask);
      return *this;
    }

    /*!
      Sets the brush to have a translation in its transformation.
      \param p translation value for brush transformation
     */
    PainterBrush&
    transformation_translate(const vec2 &p)
    {
      m_data.m_transformation_p = p;
      m_data.m_shader_raw |= transformation_translation_mask;
      return *this;
    }

    /*!
      Sets the brush to have a matrix in its transformation.
      \param m matrix value for brush transformation
     */
    PainterBrush&
    transformation_matrix(const float2x2 &m)
    {
      m_data.m_transformation_matrix = m;
      m_data.m_shader_raw |= transformation_matrix_mask;
      return *this;
    }

    /*!
      Sets the brush to have a matrix and translation in its
      transformation
      \param p translation value for brush transformation
      \param m matrix value for brush transformation
     */
    PainterBrush&
    transformation(const vec2 &p, const float2x2 &m)
    {
      transformation_translate(p);
      transformation_matrix(m);
      return *this;
    }

    /*!
      Sets the brush to have no translation in its transformation.
     */
    PainterBrush&
    no_transformation_translation(void)
    {
      m_data.m_shader_raw &= ~transformation_translation_mask;
      return *this;
    }

    /*!
      Sets the brush to have no matrix in its transformation.
     */
    PainterBrush&
    no_transformation_matrix(void)
    {
      m_data.m_shader_raw &= ~transformation_matrix_mask;
      return *this;
    }

    /*!
      Sets the brush to have no transformation.
     */
    PainterBrush&
    no_transformation(void)
    {
      no_transformation_translation();
      no_transformation_matrix();
      return *this;
    }

    /*!
      Sets the brush to have a repeat window
      \param pos location of repeat window
      \param size of repeat window
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
      Sets the brush to not have a repeat window
     */
    PainterBrush&
    no_repeat_window(void)
    {
      m_data.m_shader_raw &= ~repeat_window_mask;
      return *this;
    }

    /*!
      Returns the length of the data needed to encode the brush.
      Data is padded to be multiple of alignment and also
      sub-data of brush is padded to be along alignment
      boundaries.
      \param alignment alignment of the data store, see
                       PainterBackend::Configuration::alignment(void) const
     */
    unsigned int
    data_size(unsigned int alignment) const;

    /*!
      Encodes the data. Data is packed as according to the
      enumerations defined in PainterPacking::Brush.
      Data is padded to be multiple of alignment and also
      sub-data of brush is padded to be along alignment
      boundaries.
      \param dst location to which to encode the brush
      \param alignment alignment of the data store, see
                       PainterBackend::Configuration::alignment(void) const
     */
    void
    pack_data(unsigned int alignment, c_array<generic_data> dst) const;

    /*!
      Returns the brush shader ID which when tested against the
      bit masks from \ref shader_masks tells what features are
      active in the brush. The shader is decoded as follows:

      - The value given by unpack_bits(image_filter_bit0, image_filter_num_bits, shader())
        is non-zero if an image is present and when is non-zero the value's meaning
        is enumerated by image_filter
      - If shader() & \ref gradient_mask is non-zero, then a gradient is applied.
        The gradient is a linear gradient if shader() & \ref radial_gradient_mask
        is zero and a radial gradient otherwise.
      - If shader() & \ref radial_gradient_mask is non-zero, then a radial
        gradient is applied. Note that if shader() & \ref radial_gradient_mask
        is non-zero, then shader() & \ref gradient_mask is also non-zero.
      - If shader() & \ref gradient_repeat_mask then the gradient is repeated
        instead of clamped. Note that if shader() & \ref gradient_repeat_mask
        is non-zero, then shader() & \ref gradient_mask is also non-zero.
      - If shader() & \ref repeat_window_mask is non-zero, then a repeat
        window is applied to the brush.
      - If shader() & \ref transformation_translation_mask is non-zero, then a
        translation is applied to the brush.
      - If shader() & \ref transformation_matrix_mask is non-zero, then a
        2x2 matrix is applied to the brush.
      - The value given by
        \code
        unpack_bits(image_number_index_lookups_bit0, image_number_index_lookups_num_bits, shader())
        \endcode
        gives the value to Image::number_index_lookups() of the image
        applied to the brush.
      - The value given by
        \code
        unpack_bits(image_slack_bit0, image_slack_num_bits, shader())
        \endcode
        gives the value to Image::slack() of the image
        applied to the brush.
     */
    uint32_t
    shader(void) const;

    /*!
      Specialize assignment operator.
     */
    PainterBrush&
    operator=(const PainterBrush &rhs);

    /*!
      Returns the value of the handle to the
      Image that the brush is set to use.
     */
    const Image::const_handle&
    image(void) const
    {
      return m_data.m_image;
    }

    /*!
      Returns the value of the handle to the
      ColorStopSequenceOnAtlas that the
      brush is set to use.
     */
    const ColorStopSequenceOnAtlas::const_handle&
    color_stops(void) const
    {
      return m_data.m_cs;
    }

    /*!
      Returns true if and only if passed image can
      be rendered correctly with the specified filter.
      \param im handle to image
      \param f image filter to which to with which test if
               im can be rendered
     */
    static
    bool
    filter_suitable_for_image(const Image::const_handle &im,
			      enum image_filter f);

    /*!
      Returns the highest quality filter with which
      an image may be rendered.
      \param im image to which to query
     */
    static
    enum image_filter
    best_filter_for_image(const Image::const_handle &im);

    /*!
      Returns the slack requirement for an image to
      be rendered correctly under a filter.
      \param f filter to query
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
      Image::const_handle m_image;
      uvec2 m_image_size, m_image_start;
      ColorStopSequenceOnAtlas::const_handle m_cs;
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
