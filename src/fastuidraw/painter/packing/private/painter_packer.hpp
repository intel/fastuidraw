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
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/painter_attribute_writer.hpp>
#include <fastuidraw/painter/packing/painter_draw.hpp>
#include <fastuidraw/painter/packing/painter_backend.hpp>
#include <fastuidraw/painter/packing/painter_header.hpp>

#include "painter_packer_data.hpp"

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
     * from any of the PainterPacker::draw_generic()
     * methods called whenever a header is added.
     */
    class DataCallBack:public reference_counted<DataCallBack>::default_base
    {
    public:
      /*!
       * Ctor.
       */
      DataCallBack(void);

      ~DataCallBack();

      /*!
       * Returns true if this DataCallBack is already active on a
       * PainterPacker object.
       */
      bool
      active(void) const;

      /*!
       * To be implemented by a derived class to implement the call back
       * issues whenever a \ref PainterHeader value is added.
       * \param h handle to active PainterDraw
       * \param original_value header values written to PainterDraw::m_store
       * \param mapped_location sub-array into PainterDraw::m_store where header is written
       */
      virtual
      void
      header_added(const reference_counted_ptr<const PainterDraw> &h,
                   const PainterHeader &original_value,
                   c_array<generic_data> mapped_location) = 0;

    private:
      friend class PainterPacker;
      void *m_d;
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
     * Returns the active 3D API blending mode.
     */
    BlendMode
    composite_mode(void) const;

    /*!
     * Sets the active composite shader.
     * \param h composite shader to use for compositing.
     * \param blend_mode 3D API blend mode.
     */
    void
    composite_shader(const reference_counted_ptr<PainterCompositeShader> &h,
                     BlendMode blend_mode);

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
     * Add a \ref DataCallBack to this PainterPacker. A fixed DataCallBack
     * can only be active on one PainterPacker, but a single PainterPacker
     * can have multiple objects active on it. Callback objects are called
     * in REVERSE ordered there are added (thus the most recent callback
     * objects are called first).
     */
    void
    add_callback(const reference_counted_ptr<DataCallBack> &callback);

    /*!
     * Remove a \ref DataCallBack from this PainterPacker.
     */
    void
    remove_callback(const reference_counted_ptr<DataCallBack> &callback);

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
     * Returns the PainterBackend::Surface to which the Painter
     * is drawing. If there is no active surface, then returns
     * a null reference.
     */
    const reference_counted_ptr<PainterBackend::Surface>&
    surface(void) const;

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
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterPackerData &data,
                 c_array<const c_array<const PainterAttribute> > attrib_chunks,
                 c_array<const c_array<const PainterIndex> > index_chunks,
                 c_array<const int> index_adjusts,
                 int z);

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
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterPackerData &data,
                 c_array<const c_array<const PainterAttribute> > attrib_chunks,
                 c_array<const c_array<const PainterIndex> > index_chunks,
                 c_array<const int> index_adjusts,
                 c_array<const unsigned int> attrib_chunk_selector,
                 int z);
    /*!
     * Draw generic attribute data
     * \param shader shader with which to draw data
     * \param data data for how to draw
     * \param src DrawWriter to use to write attribute and index data
     * \param z z-value z value placed into the header
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterPackerData &data,
                 const PainterAttributeWriter &src,
                 int z);
    /*!
     * Returns a stat on how much data the PainterPacker has
     * handled since the last call to begin().
     * \param st stat to query
     */
    unsigned int
    query_stat(enum PainterEnums::query_stats_t st) const;

    /*!
     * Returns the PainterBackend::PerformanceHints of the underlying
     * PainterBackend of this PainterPacker.
     */
    const PainterBackend::PerformanceHints&
    hints(void);

    /*
     * PainterPacker sources are compiled private, but they contain
     * the implmentation to PainterPackedValuePool, so we implement
     * them as static methods here that PainterPackedValuePool
     * methods will call.
     */
    static
    void*
    create_packed_value(void *d, const PainterBrush &value);

    static
    void*
    create_packed_value(void *d, const PainterItemShaderData &value);

    static
    void*
    create_packed_value(void *d, const PainterCompositeShaderData &value);

    static
    void*
    create_packed_value(void *d, const PainterBlendShaderData &value);

    static
    void*
    create_packed_value(void *d, const PainterClipEquations &value);

    static
    void*
    create_packed_value(void *d, const PainterItemMatrix &value);

    static
    void*
    create_painter_packed_value_pool_d(void);

    static
    void
    delete_painter_packed_value_pool_d(void*);

    static
    void
    acquire_packed_value(void *md);

    static
    void
    release_packed_value(void *md);

    static
    const void*
    raw_data_of_packed_value(void *md);

    /* The data behind a PainterShaderGroup is also defined privately
     * within PainterPacker implementation, so to implement the
     * PainterShaderGroup methods, we implement them here and have
     * the actual implementation call them.
     */
    static
    uint32_t
    composite_group(const PainterShaderGroup *md);

    static
    uint32_t
    blend_group(const PainterShaderGroup *md);

    static
    uint32_t
    item_group(const PainterShaderGroup *md);

    static
    uint32_t
    brush(const PainterShaderGroup *md);

    static
    BlendMode
    composite_mode(const PainterShaderGroup *md);

  private:
    void *m_d;
  };
/*! @} */

}
