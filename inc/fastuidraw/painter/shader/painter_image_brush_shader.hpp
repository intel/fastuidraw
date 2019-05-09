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

#include <fastuidraw/painter/painter_custom_brush.hpp>
#include <fastuidraw/painter/data/painter_image_brush_shader_data.hpp>
#include <fastuidraw/painter/shader/painter_brush_shader.hpp>

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
  class PainterImageBrushShader:
    public reference_counted<PainterImageBrushShader>::concurrent,
    public PainterBrushEnums
  {
  public:
    /*!
     * \brief
     * Enumeration describing the roles of the bits for
     * the sub-shader ID's.
     */
    enum sub_shader_bits
      {
        /*!
         * Number of bits needed to encode filter for image.
         * A value of 0 indicates no image applied, a non-zero
         * value indicates an image applied and the value
         * specifies what filter via the enumeration \ref
         * PainterBrushEnums::filter_t.
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
        number_sub_shaders = 1u << number_bits
      };

    /*!
     * Various bit-mask values derived from \ref sub_shader_bits
     */
    enum sub_shader_masks
      {
        /*!
         * mask generated from \ref filter_bit0 and \ref filter_num_bits
         */
        filter_mask = FASTUIDRAW_MASK(filter_bit0, filter_num_bits),

        /*!
         * mask generated from \ref mipmap_bit0 and \ref mipmap_num_bits
         */
        mipmap_mask = FASTUIDRAW_MASK(mipmap_bit0, mipmap_num_bits),

        /*!
         * mask generated from \ref type_bit0 and \ref type_num_bits
         */
        type_mask = FASTUIDRAW_MASK(type_bit0, type_num_bits),

        /*!
         * mask generated from \ref format_bit0 and \ref format_num_bits
         */
        format_mask = FASTUIDRAW_MASK(format_bit0, format_num_bits),
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
               enum filter_t image_filter,
               enum mipmap_t mip_mapping) const;

    /*!
     * returns all the sub-shaders of the \ref PainterImageBrushShader
     */
    c_array<const reference_counted_ptr<PainterBrushShader> >
    sub_shaders(void) const;

    /*!
     * Create a \ref PainterCustomBrush with packed data to brush
     * by sourcing from a sub-rectangle of an Image.
     * \param pool \ref PainterPackedValuePool used to create the packed value
     * \param image \ref Image from which to source
     * \param xy minx-miny corner of sub-rectangle
     * \param wh width and height of sub-rectangle
     * \param image_filter filtering to apply to image
     * \param mip_mapping mipmapping to apply to image
     */
    PainterCustomBrush
    create_brush(PainterPackedValuePool &pool,
                 const reference_counted_ptr<const Image> &image,
                 uvec2 xy, uvec2 wh,
                 enum filter_t image_filter = filter_linear,
                 enum mipmap_t mip_mapping = apply_mipmapping) const;

    /*!
     * Create a \ref PainterCustomBrush with packed data to brush
     * by sourcing from the entirity of an Image.
     * \param pool \ref PainterPackedValuePool used to create the packed value
     * \param image \ref Image from which to source
     * \param image_filter filtering to apply to image
     * \param mip_mapping mipmapping to apply to image
     */
    PainterCustomBrush
    create_brush(PainterPackedValuePool &pool,
                 const reference_counted_ptr<const Image> &image,
                 enum filter_t image_filter = filter_linear,
                 enum mipmap_t mip_mapping = apply_mipmapping) const;

    /*!
     * Produce the sub-shader ID from what \ref Image
     * and how to sample from the image.
     * \param image \ref Image from which the brush-image
     *              will source; a nullptr value indicates
     *              no image and the brush-image will emit
     *              constant color white fully opaque
     * \param image_filter filtering to apply to image
     * \param mip_mapping mipmapping to apply to image
     */
    static
    uint32_t
    sub_shader_id(const Image *image,
                  enum filter_t image_filter,
                  enum mipmap_t mip_mapping);

  private:
    void *m_d;
  };

/*! @} */
}
