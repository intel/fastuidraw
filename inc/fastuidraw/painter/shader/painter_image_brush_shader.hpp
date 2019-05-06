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

#include <fastuidraw/image.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter_brush_shader_data.hpp>
#include <fastuidraw/painter/painter_custom_brush.hpp>
#include <fastuidraw/painter/shader/painter_brush_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup PainterShaders
 * @{
 */

  /*!
   * \brief
   * A \ref PainterImageBrushShaderData defines the \ref
   * PainterBrushShaderData that the shaders of a \ref
   * PainterImageBrushShader consume. It specifies
   * what \ref Image and what rectangular region within
   * it from which to source image data.
   */
  class PainterImageBrushShaderData:
    public PainterBrushShaderData,
    public PainterBrushEnums
  {
  public:
    /*!
     * \brief
     * Bit packing for an uvec2 value.
     */
    enum uvec2_encoding
      {
        uvec2_x_num_bits = 16, /*!< number bits to encode x-coordinate */
        uvec2_y_num_bits = 16, /*!< number bits to encode y-coordinate */

        uvec2_x_bit0 = 0, /*!< bit where x-coordinate is encoded */
        uvec2_y_bit0 = uvec2_x_num_bits, /*!< bit where y-coordinate is encoded */
      };

    /*!
     * \brief
     * Bit packing for the master index tile of a Image
     */
    enum atlas_location_encoding
      {
        atlas_location_x_num_bits = 8,  /*!< number bits to encode Image::master_index_tile().x() */
        atlas_location_y_num_bits = 8,  /*!< number bits to encode Image::master_index_tile().y() */
        atlas_location_z_num_bits = 16, /*!< number bits to encode Image::master_index_tile().z() */

        atlas_location_x_bit0 = 0, /*!< bit where Image::master_index_tile().x() is encoded */

        /*!
         * bit where Image::master_index_tile().y() is encoded
         */
        atlas_location_y_bit0 = atlas_location_x_num_bits,

        /*!
         * bit where Image::master_index_tile().z() is encoded
         */
        atlas_location_z_bit0 = atlas_location_y_bit0 + atlas_location_y_num_bits,
      };

    /*!
     * \brief
     * Offsets for image data packing. The offsets are in
     * units of \ref generic_data, NOT units of
     * vecN<generic_data, 4>.
     */
    enum offset_t
      {
        /*!
         * Width and height of the sub-rectangle of the \ref
         * Image from which to source and encoded in a single
         * uint32_t. The bits are packed as according to \ref
         * uvec2_encoding.  If there is no valid backing image,
         * then the encoded value will be 0.
         */
        size_xy_offset,

        /*!
         * The minx-miny corener of the sub-rectangle of the \ref
         * Image from which to source and encoded in a single
         * uint32_t. The bits are packed as according to \ref
         * uvec2_encoding.
         */
        start_xy_offset,

        /*!
         * Location of image (Image::master_index_tile()) in the image
         * atlas is encoded in a single uint32. The bits are packed as
         * according to \ref atlas_location_encoding. If the image
         * is not of type Image::on_atlas, gives the high 32-bits of
         * Image::handle().
         */
        atlas_location_xyz_offset,

        /*!
         * Holds the amount the number of index looks ups, see \ref
         * Image::number_index_lookups(). If the image is not of type
         * Image::on_atlas, gives the low 32-bits of Image::handle().
         */
        number_lookups_offset,

        /*!
         * Number of elements packed for image support
         * for a brush
         */
        shader_data_size,

        /*!
         * Offset to the high 32-bits of the handle value when the
         * Image is of type Image::bindless_texture2d.
         */
        bindless_handle_hi_offset = atlas_location_xyz_offset,

        /*!
         * Offset to the low 32-bits of the handle value when the
         * Image is of type Image::bindless_texture2d.
         */
        bindless_handle_low_offset = number_lookups_offset,
      };

    /*!
     * Ctor initializes to not source from any image data
     */
    PainterImageBrushShaderData(void);

    /*!
     * Copy ctor.
     */
    PainterImageBrushShaderData(const PainterImageBrushShaderData &obj):
      m_image(obj.m_image),
      m_image_xy(obj.m_image_xy),
      m_image_wh(obj.m_image_wh)
    {}

    /*!
     * Assignment operator
     */
    PainterImageBrushShaderData&
    operator=(const PainterImageBrushShaderData &rhs)
    {
      m_image = rhs.m_image;
      m_image_xy = rhs.m_image_xy;
      m_image_wh = rhs.m_image_wh;
      return *this;
    }

    /*!
     * Set to source from a sub-rectangle of a \ref Image
     * \param im \ref Image from which to source
     * \param xy minx-miny corner of sub-rectangle
     * \param wh width and height of sub-rectangle
     */
    void
    sub_image(const reference_counted_ptr<const Image> &im,
              uvec2 xy, uvec2 wh);

    /*!
     * Set to source from the entire contents of an
     * \ref Image.
     * \param im \ref Image from which to source
     */
    void
    image(const reference_counted_ptr<const Image> &im);

    /*!
     * Returns the \ref Image from which to source
     */
    const reference_counted_ptr<const Image>&
    image(void) const
    {
      return m_image;
    }

    void
    pack_data(c_array<vecN<generic_data, 4> > dst) const override;

    unsigned int
    data_size(void) const override;

    void
    save_resources(c_array<reference_counted_ptr<const resource_base> > dst) const override;

    unsigned int
    number_resources(void) const override;

    c_array<const reference_counted_ptr<const Image> >
    bind_images(void) const override;

  private:
    reference_counted_ptr<const Image> m_image;
    uvec2 m_image_xy, m_image_wh;
  };

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
