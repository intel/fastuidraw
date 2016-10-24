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
  @{
 */

  /*!
    A PainterPacker packs data created by a Painter
    to be fed to a PainterBackend to draw.
   */
  class PainterPacker:public reference_counted<PainterPacker>::default_base
  {
  public:
    /*!
      A DataCallBack represents a functor call back
      from PainterPacker called whenever a header is
      added or when a new PainterDraw is
      taken into use.
     */
    class DataCallBack:public reference_counted<DataCallBack>::default_base
    {
    public:
      /*!
        To be implemented by a derived class to note the current
        PainterDraw being filled by the PainterPacker.
        \param h handle to active PainterDraw
       */
      virtual
      void
      current_draw(const reference_counted_ptr<const PainterDraw> &h) = 0;

      /*!
        To be implemented by a derived class to note when a header
        was added.
        \param original_value header values written to PainterDraw::m_store
        \param mapped_location sub-array into PainterDraw::m_store where header is written
       */
      virtual
      void
      header_added(const PainterHeader &original_value, c_array<generic_data> mapped_location) = 0;
    };

    /*!
      Enumeration to query the statistics of how
      much data has been packed
    */
    enum stats_t
      {
        /*!
          Offset to how many attributes processed
        */
        num_attributes,

        /*!
          Offset to how many indices processed
        */
        num_indices,

        /*!
          Offset to how many generic_data values placed
          onto store buffer(s).
        */
        num_generic_datas,

        /*!
          Offset to how many PainterDraw objects sent
        */
        num_draws,

        /*!
          Offset to how many painter headers packed.
        */
        num_headers,

        /*!
          Number of stats.
         */
        num_stats,
      };

    /*!
      Ctor.
      \param backend handle to PainterBackend for the constructed PainterPacker
     */
    explicit
    PainterPacker(reference_counted_ptr<PainterBackend> backend);

    virtual
    ~PainterPacker();

    /*!
      Returns a handle to the GlyphAtlas of this
      PainterPacker. All glyphs used by this
      PainterPacker must live on glyph_atlas().
     */
    const reference_counted_ptr<GlyphAtlas>&
    glyph_atlas(void) const;

    /*!
      Returns a handle to the ImageAtlas of this
      PainterPacker. All images used by all brushes
      of this PainterPacker must live on image_atlas().
     */
    const reference_counted_ptr<ImageAtlas>&
    image_atlas(void) const;

    /*!
      Returns a handle to the ColorStopAtlas of this
      PainterPacker. All color stops used by all brushes
      of this PainterPacker must live on colorstop_atlas().
     */
    const reference_counted_ptr<ColorStopAtlas>&
    colorstop_atlas(void) const;

    /*!
      Returns the active blend shader
     */
    const reference_counted_ptr<PainterBlendShader>&
    blend_shader(void) const;

    /*!
      Returns the active 3D API blend mode packed
      as in BlendMode::packed().
     */
    BlendMode::packed_value
    blend_mode(void) const;

    /*!
      Sets the active blend shader. It is a crashing error for
      h to be NULL.
      \param h blend shader to use for blending.
      \param packed_blend_mode 3D API blend mode packed via BlendMode::packed().
     */
    void
    blend_shader(const reference_counted_ptr<PainterBlendShader> &h,
                 BlendMode::packed_value packed_blend_mode);

    /*!
      Indicate to start drawing. Commands are buffered and not
      set to the backend until end() or flush() is called.
      All draw commands must be between a begin() / end() pair.
     */
    void
    begin(void);

    /*!
      Indicate to end drawing. Commands are buffered and not
      set to the backend until end() or flush() is called.
      All draw commands must be between a begin() / end() pair.
     */
    void
    end(void);

    /*!
      Flush all buffered rendering commands.
     */
    void
    flush(void);

    /*!
      Return the default shaders for common drawing types.
     */
    const PainterShaderSet&
    default_shaders(void) const;

    /*!
      Draw generic attribute data
      \param data data for how to draw
      \param attrib_chunks attribute data to draw
      \param index_chunks the i'th element is index data into attrib_chunks[i]
      \param index_adjusts the i'th element is the value by which to adjust all of index_chunks[i]
      \param shader shader with which to draw data
      \param z z-value z value placed into the header
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterPackerData &data,
                 const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
                 const_c_array<const_c_array<PainterIndex> > index_chunks,
                 const_c_array<int> index_adjusts,
                 unsigned int z,
                 const reference_counted_ptr<DataCallBack> &call_back = reference_counted_ptr<DataCallBack>());

    /*!
      Draw generic attribute data
      \param data data for how to draw
      \param attrib_chunks attribute data to draw
      \param index_chunks the i'th element is index data into attrib_chunks[K]
                          where K = attrib_chunk_selector[i]
      \param index_adjusts the i'th element is the value by which to adjust all of index_chunks[i]
      \param attrib_chunk_selector selects which attribute chunk to use for
             each index chunk
      \param shader shader with which to draw data
      \param z z-value z value placed into the header
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterPackerData &data,
                 const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
                 const_c_array<const_c_array<PainterIndex> > index_chunks,
                 const_c_array<int> index_adjusts,
                 const_c_array<unsigned int> attrib_chunk_selector,
                 unsigned int z,
                 const reference_counted_ptr<DataCallBack> &call_back = reference_counted_ptr<DataCallBack>());
    /*!
      Returns a stat on how much data the PainterPacker has
      handled since the last call to begin().
      \param st stat to query
     */
    unsigned int
    query_stat(enum stats_t st) const;

    /*!
      Returns the PainterBackend::PerformanceHints of the underlying
      PainterBackend of this PainterPacker.
     */
    const PainterBackend::PerformanceHints&
    hints(void);

    /*!
      Registers a shader for use. Must not be called within a
      begin() / end() pair.
     */
    void
    register_shader(const reference_counted_ptr<PainterItemShader> &shader);

    /*!
      Registers a shader for use. Must not be called within
      a begin() / end() pair.
    */
    void
    register_shader(const reference_counted_ptr<PainterBlendShader> &shader);

    /*!
      Registers a stroke shader for use. Must not be called within
      a begin() / end() pair.
      \param p PainterStrokeShader hold shaders to register
     */
    void
    register_shader(const PainterStrokeShader &p);

    /*!
      Registers a stroke shader for use. Must not be called within
      a begin() / end() pair.
      \param p PainterFillShader hold shaders to register
     */
    void
    register_shader(const PainterFillShader &p);

    /*!
      Registers a dashed stroke shader for use. Must not be called within
      a begin() / end() pair.
      \param p PainterDashedStrokeShaderSet hold shaders to register
     */
    void
    register_shader(const PainterDashedStrokeShaderSet &p);

    /*!
      Registers a glyph shader for use. Must not be called within
      a begin() / end() pair.
     */
    void
    register_shader(const PainterGlyphShader &p);

    /*!
      Registers a shader set for use. Must not be called within
      a begin() / end() pair.
     */
    void
    register_shader(const PainterShaderSet &p);

    /*!
      Informs the PainterPacker what the resolution of
      the target surface is.
      \param w width of target resolution
      \param h height of target resolution
     */
    void
    target_resolution(int w, int h);

  private:
    void *m_d;
  };
/*! @} */

}
