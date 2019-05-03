/*!
 * \file painter_image_brush_shader.hpp
 * \brief file painter_image_brush_shader.hpp
 *
 * Copyright 2019 by Intel.
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
#include <fastuidraw/painter/shader/painter_brush_shader.hpp>
#include <fastuidraw/image.hpp>

namespace fastuidraw
{

/*!\addtogroup PainterShaders
 * @{
 */

  /*!
   * \brief
   * A \ref PainterImageBrushShader represents a single \ref
   * PainterBrushShader together with sub-shaders of it that
   * can be used to apply an \ref Image. The sub-shader ID is
   * used to describe the \ref Image::type(), \ref Image::format(),
   * what filtering and mipmapping to apply to the image data.
   */
  class PainterImageBrushShader
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
        image_filter_nearest = 0,

        /*!
         * Indicates to use bilinear filtering.
         */
        image_filter_linear,

        /*!
         * Indicates to use bicubic filtering.
         */
        image_filter_cubic
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
     * the sub-shader ID's.
     */
    enum sub_shader_bits
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
        filter_num_bits = 2,

        /*!
         * Number of bits used to encode number of mipmap
         * levels (when an image is present).
         */
        mipmap_num_bits = 7,

        /*!
         * Number of bits needed to encode the image type
         * (when an image is present). The possible values
         * are given by the enumeration \ref Image::type_t.
         */
        type_num_bits = 4,

        /*!
         * Number of bits needed to encode the value of
         * Image::format().
         */
        format_num_bits = 1,

        /*!
         * first bit for if image is present on the brush and if so, what filter
         */
        filter_bit0 = 0,

        /*!
         * first bit to indicate maximum mipmap level to use
         */
        mipmap_bit0 = filter_bit0 + filter_num_bits,

        /*!
         * First bit to hold the type of image present if an image is present;
         * the value is the enumeration in \ref Image::type_t
         */
        type_bit0 = mipmap_bit0 + mipmap_num_bits,

        /*!
         * First bit to encode \ref Image::format_t
         */
        format_bit0 = type_bit0 + type_num_bits,

        /*!
         * The total number of bits needed to specify the sub-shader IDs.
         */
        number_bits = format_bit0 + format_num_bits,

        /*!
         * the total number of sub-shaders
         */
        number_sub_shaders = FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(number_bits)
      };

    /*!
     * Ctor.
     * \param parent_shader the parent \ref PainterBrushShader that implements
     *                      image-brush shading and has \ref number_sub_shaders
     *                      that implement brush shading where the I'th sub-shader
     *                      implement brush shading as described by extracting
     *                      from the bits of I the values as encoded by \ref
     *                      sub_shader_bits
     */
    explicit
    PainterImageBrushShader(const reference_counted_ptr<PainterBrushShader> &parent_shader);

    ~PainterImageBrushShader();

    /*!
     * \param image \ref Image from which the brush-image
     *              will source; a nullptr value indicates
     *              no image and the brush-image will emit
     *              constant color white fully opaque
     * \param image_filter filtering to apply to image
     * \param mip_mapping mipmapping to apply to image
     */
    const reference_counted_ptr<PainterBrushShader>&
    sub_shader(const Image *image,
               enum image_filter image_filter,
               enum mipmap_t mip_mapping);

  private:
    void *m_d;
  };

/*! @} */
}
