/*!
 * \file painter_packer.hpp
 * \brief file painter_packer.hpp
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

#include <fastuidraw/text/glyph_atlas.hpp>
#include <fastuidraw/colorstop_atlas.hpp>
#include <fastuidraw/image.hpp>

#include <fastuidraw/painter/painter_shader_set.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter_header.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/packing/painter_draw.hpp>
#include <fastuidraw/painter/packing/painter_backend.hpp>
#include <fastuidraw/painter/packing/painter_packer_data.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterPacking
 * @{
 */

  /*!
   * \brief
   * A PainterPacker packs data created by a Painter
   * to be fed to a PainterBackend to draw.
   */
  class PainterPacker:public reference_counted<PainterPacker>::default_base
  {
  public:
    /*!
     * \brief
     * A DataCallBack represents a functor call back
     * from PainterPacker called whenever a header is
     * added or when a new PainterDraw is
     * taken into use.
     */
    class DataCallBack:public reference_counted<DataCallBack>::default_base
    {
    public:
      /*!
       * To be implemented by a derived class to note the current
       * PainterDraw being filled by the PainterPacker.
       * \param h handle to active PainterDraw
       */
      virtual
      void
      current_draw(const reference_counted_ptr<const PainterDraw> &h) = 0;

      /*!
       * To be implemented by a derived class to note when a header
       * was added.
       * \param original_value header values written to PainterDraw::m_store
       * \param mapped_location sub-array into PainterDraw::m_store where header is written
       */
      virtual
      void
      header_added(const PainterHeader &original_value, c_array<generic_data> mapped_location) = 0;
    };

    /*!
     * \brief
     * A provides an interface to write attribute and index data into a PainterDraw
     * for the cases where a simple copy is not sufficient.
     */
    class DataWriter
    {
    public:
      virtual
      ~DataWriter()
      {}

      /*!
       * To be implemented by a derived class to return
       * the number of attribute chunks of the DataWriter.
       */
      virtual
      unsigned int
      number_attribute_chunks(void) const = 0;

      /*!
       * To be implemented by a derived class to return
       * the number of attribute of an attribute chunk
       * of the DataWriter.
       * \param attribute_chunk which chunk of attributes
       */
      virtual
      unsigned int
      number_attributes(unsigned int attribute_chunk) const = 0;

      /*!
       * To be implemented by a derived class to return
       * the number of index chunks of the DataWriter.
       */
      virtual
      unsigned int
      number_index_chunks(void) const = 0;

      /*!
       * To be implemented by a derived class to return
       * the number of indices of an index chunk
       * of the DataWriter.
       * \param index_chunk which chunk of attributes
       */
      virtual
      unsigned int
      number_indices(unsigned int index_chunk) const = 0;

      /*!
       * To be implemented by a derived class to return
       * what attribute chunk to use for a given index
       * chunk.
       * \param index_chunk index chunk with 0 <= index_chunk <= number_index_chunks()
       */
      virtual
      unsigned int
      attribute_chunk_selection(unsigned int index_chunk) const = 0;

      /*!
       * To be implemented by a derived class to write indices.
       * \param dst location to which to write indices
       * \param index_offset_value value by which to increment the index
       *                           values written
       * \param index_chunk which chunk of indices to write
       */
      virtual
      void
      write_indices(c_array<PainterIndex> dst,
                    unsigned int index_offset_value,
                    unsigned int index_chunk) const = 0;

      /*!
       * To be implemented by a derived class to write attributes.
       * \param dst location to which to write indices
       * \param attribute_chunk which chunk of attributes to write
       */
      virtual
      void
      write_attributes(c_array<PainterAttribute> dst,
                       unsigned int attribute_chunk) const = 0;
    };

    /*!
     * \brief
     * Enumeration to query the statistics of how
     * much data has been packed
     */
    enum stats_t
      {
        /*!
         * Offset to how many attributes processed
         */
        num_attributes,

        /*!
         * Offset to how many indices processed
         */
        num_indices,

        /*!
         * Offset to how many generic_data values placed
         * onto store buffer(s).
         */
        num_generic_datas,

        /*!
         * Offset to how many PainterDraw objects sent
         */
        num_draws,

        /*!
         * Offset to how many painter headers packed.
         */
        num_headers,

        /*!
         * Number of stats.
         */
        num_stats,
      };

    /*!
     * Ctor.
     * \param backend handle to PainterBackend for the constructed PainterPacker
     */
    explicit
    PainterPacker(reference_counted_ptr<PainterBackend> backend);

    virtual
    ~PainterPacker();

    /*!
     * Returns a handle to the GlyphAtlas of this
     * PainterPacker. All glyphs used by this
     * PainterPacker must live on glyph_atlas().
     */
    const reference_counted_ptr<GlyphAtlas>&
    glyph_atlas(void) const;

    /*!
     * Returns a handle to the ImageAtlas of this
     * PainterPacker. All images used by all brushes
     * of this PainterPacker must live on image_atlas().
     */
    const reference_counted_ptr<ImageAtlas>&
    image_atlas(void) const;

    /*!
     * Returns a handle to the ColorStopAtlas of this
     * PainterPacker. All color stops used by all brushes
     * of this PainterPacker must live on colorstop_atlas().
     */
    const reference_counted_ptr<ColorStopAtlas>&
    colorstop_atlas(void) const;

    /*!
     * Returns the PainterShaderRegistrar of the PainterBackend
     * that was used to create this PainterPacker object. Use this
     * return value to add custom shaders. NOTE: shaders added
     * within a thread are not useable within that thread until
     * the next call to begin().
     */
    reference_counted_ptr<PainterShaderRegistrar>
    painter_shader_registrar(void) const;

    /*!
     * Returns the PainterPackedValuePool used to construct
     * PainterPackedValue objects.
     */
    PainterPackedValuePool&
    packed_value_pool(void);

    /*!
     * Returns the active composite shader
     */
    const reference_counted_ptr<PainterCompositeShader>&
    composite_shader(void) const;

    /*!
     * Returns the active 3D API composite mode packed
     * as in BlendMode::packed().
     */
    BlendMode::packed_value
    composite_mode(void) const;

    /*!
     * Sets the active composite shader.
     * \param h composite shader to use for compositing.
     * \param packed_composite_mode 3D API composite mode packed via BlendMode::packed().
     */
    void
    composite_shader(const reference_counted_ptr<PainterCompositeShader> &h,
                     BlendMode::packed_value packed_composite_mode);

    /*!
     * Returns the active blend shader
     */
    const reference_counted_ptr<PainterBlendShader>&
    blend_shader(void) const;

    /*!
     * Sets the active blend shader.
     * \param h blend shader to use for blending.
     */
    void
    blend_shader(const reference_counted_ptr<PainterBlendShader> &h);

    /*!
     * Indicate to start drawing. Commands are buffered and not
     * set to the backend until end() or flush() is called.
     * All draw commands must be between a begin() / end() pair.
     * \param surface the \ref PainterBackend::Surface to which
     *                 to render content
     * \param clear_color_buffer if true, clear the color buffer
     *                           on the viewport of the surface.
     */
    void
    begin(const reference_counted_ptr<PainterBackend::Surface> &surface,
          bool clear_color_buffer);

    /*!
     * Indicate to end drawing. Commands are buffered and not
     * sent to the backend until end() is called.
     * All draw commands must be between a begin() / end() pair.
     */
    void
    end(void);

    /*!
     * Add a draw break to execute an action.
     * \param action action to execute on draw break
     */
    void
    draw_break(const reference_counted_ptr<const PainterDraw::Action> &action);

    /*!
     * Return the default shaders for common drawing types.
     */
    const PainterShaderSet&
    default_shaders(void) const;

    /*!
     * Draw generic attribute data
     * \param shader shader with which to draw data
     * \param data data for how to draw
     * \param attrib_chunks attribute data to draw
     * \param index_chunks the i'th element is index data into attrib_chunks[i]
     * \param index_adjusts if non-empty, the i'th element is the value by which
     *                      to adjust all of index_chunks[i]; if empty the index
     *                      values are not adjusted.
     * \param z z-value z value placed into the header
     * \param call_back if non-nullptr handle, call back called when attribute data
     *                  is added.
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterPackerData &data,
                 c_array<const c_array<const PainterAttribute> > attrib_chunks,
                 c_array<const c_array<const PainterIndex> > index_chunks,
                 c_array<const int> index_adjusts,
                 int z,
                 const reference_counted_ptr<DataCallBack> &call_back = reference_counted_ptr<DataCallBack>());

    /*!
     * Draw generic attribute data
     * \param shader shader with which to draw data
     * \param data data for how to draw
     * \param attrib_chunks attribute data to draw
     * \param index_chunks the i'th element is index data into attrib_chunks[K]
     *                     where K = attrib_chunk_selector[i]
     * \param index_adjusts if non-empty, the i'th element is the value by which
     *                      to adjust all of index_chunks[i]; if empty the index
     *                      values are not adjusted.
     * \param attrib_chunk_selector selects which attribute chunk to use for
     *        each index chunk
     * \param z z-value z value placed into the header
     * \param call_back if non-nullptr handle, call back called when attribute data
     *                  is added.
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterPackerData &data,
                 c_array<const c_array<const PainterAttribute> > attrib_chunks,
                 c_array<const c_array<const PainterIndex> > index_chunks,
                 c_array<const int> index_adjusts,
                 c_array<const unsigned int> attrib_chunk_selector,
                 int z,
                 const reference_counted_ptr<DataCallBack> &call_back = reference_counted_ptr<DataCallBack>());
    /*!
     * Draw generic attribute data
     * \param shader shader with which to draw data
     * \param data data for how to draw
     * \param src DrawWriter to use to write attribute and index data
     * \param z z-value z value placed into the header
     * \param call_back if non-nullptr handle, call back called when attribute data
     *                  is added.
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterPackerData &data,
                 const DataWriter &src,
                 int z,
                 const reference_counted_ptr<DataCallBack> &call_back = reference_counted_ptr<DataCallBack>());
    /*!
     * Returns a stat on how much data the PainterPacker has
     * handled since the last call to begin().
     * \param st stat to query
     */
    unsigned int
    query_stat(enum stats_t st) const;

    /*!
     * Returns the PainterBackend::PerformanceHints of the underlying
     * PainterBackend of this PainterPacker.
     */
    const PainterBackend::PerformanceHints&
    hints(void);

  private:
    void *m_d;
  };
/*! @} */

}
