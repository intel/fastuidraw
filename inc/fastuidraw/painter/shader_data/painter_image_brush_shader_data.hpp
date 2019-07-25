/*!
 * \file painter_image_brush_shader.hpp
 * \brief file painter_image_brush_shader.hpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef FASTUIDRAW_PAINTER_IMAGE_BRUSH_SHADER_DATA_HPP
#define FASTUIDRAW_PAINTER_IMAGE_BRUSH_SHADER_DATA_HPP

#include <fastuidraw/image.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/shader_data/painter_brush_shader_data.hpp>
#include <fastuidraw/painter/painter_custom_brush.hpp>
#include <fastuidraw/painter/shader/painter_brush_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup PainterShaderData
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
     * units of uint32_t, NOT units of \ref uvec4.
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
    pack_data(c_array<uvec4> dst) const override;

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

/*! @} */
}

#endif
