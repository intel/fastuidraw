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
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/glyph_selector.hpp>

#include <fastuidraw/image.hpp>
#include <fastuidraw/colorstop_atlas.hpp>
#include <fastuidraw/path.hpp>

#include <fastuidraw/painter/painter_shader.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter_state.hpp>
#include <fastuidraw/painter/painter_save_state.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/packing/painter_draw.hpp>
#include <fastuidraw/painter/packing/painter_packing_enums.hpp>
#include <fastuidraw/painter/packing/painter_backend.hpp>

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
      Conveniance typedef to PainterState::ClipEquations
     */
    typedef PainterState::ClipEquations ClipEquations;

    /*!
      Conveniance typedef to PainterState::ItemMatrix
     */
    typedef PainterState::ItemMatrix ItemMatrix;

    /*!
      Conveniance typedef to PainterState::VertexShaderData
     */
    typedef PainterState::VertexShaderData VertexShaderData;

    /*!
      Conveniance typedef to PainterState::FragmentShaderData
     */
    typedef PainterState::FragmentShaderData FragmentShaderData;

    /*!
      Conveniance typedef to PainterState::ClipEquationsState
     */
    typedef PainterState::ClipEquationsState ClipEquationsState;

    /*!
      Conveniance typedef to PainterState::ItemMatrixState
     */
    typedef PainterState::ItemMatrixState ItemMatrixState;

    /*!
      Conveniance typedef to PainterState::PainterBrushState
     */
    typedef PainterState::PainterBrushState PainterBrushState;

    /*!
      Conveniance typedef to PainterState::VertexShaderDataState
     */
    typedef PainterState::VertexShaderDataState VertexShaderDataState;

    /*!
      Conveniance typedef to PainterState::FragmentShaderDataState
     */
    typedef PainterState::FragmentShaderDataState FragmentShaderDataState;

    /*!
      A DataCallBack represents a functor call back
      from PainterPacker called whenever attribute data
      is generated or when a new PainterDrawCommand is
      taken into use.
     */
    class DataCallBack:public reference_counted<DataCallBack>::default_base
    {
    public:
      /*!
        To be implemented by a derived class to note the current
        PainterDrawCommand being filled by the PainterPacker.
        \param h handle to active PainterDrawCommand
       */
      virtual
      void
      current_draw_command(const PainterDrawCommand::const_handle &h) = 0;

      /*!
        To be implemented by a derived class to note when a header
        was added.
        \param original_value values written to PainterDrawCommand::m_store for the header, read access is ok
        \param mapped_location sub-array into PainterDrawCommand::m_store where header is located
       */
      virtual
      void
      header_added(const_c_array<generic_data> original_value, c_array<generic_data> mapped_location) = 0;
    };

    /*!
      Ctor.
      \param backend handle to PainterBackend for the constructed PainterPacker
     */
    explicit
    PainterPacker(PainterBackend::handle backend);

    virtual
    ~PainterPacker();

    /*!
      Returns a handle to the GlyphAtlas of this
      PainterPacker. All glyphs used by this
      PainterPacker must live on glyph_atlas().
     */
    const GlyphAtlas::handle&
    glyph_atlas(void) const;

    /*!
      Returns a handle to the ImageAtlas of this
      PainterPacker. All images used by all brushes
      of this PainterPacker must live on image_atlas().
     */
    const ImageAtlas::handle&
    image_atlas(void) const;

    /*!
      Returns a handle to the ColorStopAtlas of this
      PainterPacker. All color stops used by all brushes
      of this PainterPacker must live on colorstop_atlas().
     */
    const ColorStopAtlas::handle&
    colorstop_atlas(void) const;

    /*!
      Returns a reference to the current PainterBrush for
      the purpose of changing the properties of the current brush.
     */
    PainterBrush&
    brush(void);

    /*!
      Returns a const reference to the current PainterBrush for
      the purpose of querying the properties of the current brush.
     */
    const PainterBrush&
    cbrush(void) const;

    /*!
      Returns a handle to brush state that can re-used by passing it to
      brush_state(const PainterBrushState&).
     */
    const PainterBrushState&
    brush_state(void);

    /*!
      Set the brush state from a brush state handle.
      \param h handle to brush state.
     */
    void
    brush_state(const PainterBrushState &h);

    /*!
      Returns the active blend shader
     */
    const PainterShader::const_handle&
    blend_shader(void) const;

    /*!
      Sets the active blend shader. It is a crashing error for
      h to be NULL.
      \param h blend shader to use for blending.
     */
    void
    blend_shader(const PainterShader::const_handle &h);

    /*!
      Returns the current value of the ItemMatrix state.
     */
    const ItemMatrix&
    item_matrix(void) const;

    /*!
      Set the current value of the ItemMatrix state.
      \param v value to which to assign the ItemMatrix
     */
    void
    item_matrix(const ItemMatrix &v);

    /*!
      Returns a handle to current ItemMatrix (see
      item_matrix(void) ) that can re-used by passing it to
      image_matrix_state(const ItemMatrixState &).
     */
    const ItemMatrixState&
    item_matrix_state(void);

    /*!
      Set the ItemMatrix state from a state handle.
      \param h handle to ItemMatrix state
     */
    void
    item_matrix_state(const ItemMatrixState &h);

    /*!
      Return the current ClipEquations state value
     */
    const ClipEquations&
    clip_equations(void) const;

    /*!
      Set the current ClipEquations state value
      \param v value to which to set the ClipEquations state
     */
    void
    clip_equations(const ClipEquations &v);

    /*!
      Returns a handle to current ClipEquationsState that can re-used
      by passing it to clip_equations(const ClipEquationsState &).
     */
    const ClipEquationsState&
    clip_equations_state(void);

    /*!
      Set the ClipEquations state from a state handle.
      \param h handle to ClipEquationsState
     */
    void
    clip_equations_state(const ClipEquationsState &h);

    /*!
      Set the VertexShaderData state.
      \param shader_data values to which to pass to vertex shader
     */
    void
    vertex_shader_data(const VertexShaderData &shader_data);

    /*!
      Set the VertexShaderData state from a state handle
      \param h handle to values to which to pass to vertex shader
     */
    void
    vertex_shader_data(const VertexShaderDataState &h);

    /*!
      Returns a handle to current VertexShaderData state than can be reused
      by passing it to vertex_shader_data(const VertexShaderDataState&).
     */
    const VertexShaderDataState&
    vertex_shader_data(void);

    /*!
      Set the FragmentShaderData state.
      \param shader_data values to which to pass to fragment shader
     */
    void
    fragment_shader_data(const FragmentShaderData &shader_data);

    /*!
      Set the FragmentShaderData state from a state handle
      \param h handle to values to which to pass to fragment shader
     */
    void
    fragment_shader_data(const FragmentShaderDataState &h);

    /*!
      Returns a handle to current FragmentShaderDataState state than can be reused
      by passing it to fragment_shader_data(const FragmentShaderDataState&).
     */
    const FragmentShaderDataState&
    fragment_shader_data(void);

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
      \param attrib_chunks attribute data to draw
      \param index_chunks the i'th element is index data into attrib_chunks[i]
      \param shader shader with which to draw data
      \param z z-value z value placed into the header
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_generic(const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
                 const_c_array<const_c_array<PainterIndex> > index_chunks,
                 const PainterItemShader &shader, unsigned int z,
                 const DataCallBack::handle &call_back = DataCallBack::handle());

    /*!
      Draw generic attribute data
      \param attrib_chunks attribute data to draw
      \param index_chunks the i'th element is index data into attrib_chunks[K]
                          where K = attrib_chunk_selector[i]
      \param attrib_chunk_selector selects which attribute chunk to use for
             each index chunk
      \param shader shader with which to draw data
      \param z z-value z value placed into the header
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_generic(const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
                 const_c_array<const_c_array<PainterIndex> > index_chunks,
                 const_c_array<unsigned int> attrib_chunk_selector,
                 const PainterItemShader &shader, unsigned int z,
                 const DataCallBack::handle &call_back = DataCallBack::handle());

    /*!
      Registers a shader for use. Must not be called within a
      begin() / end() pair.
     */
    void
    register_vert_shader(const PainterShader::handle &shader);

    /*!
      Registers a shader for use. Must not be called within
      a begin() / end() pair.
    */
    void
    register_frag_shader(const PainterShader::handle &shader);

    /*!
      Registers a shader for use. Must not be called within
      a begin() / end() pair.
    */
    void
    register_blend_shader(const PainterShader::handle &shader);

    /*!
      Register an item shader for use. Must not be called within
      a begin() / end() pair.
      \param p PainterItemShader hold shaders to register
     */
    void
    register_shader(const PainterItemShader &p);

    /*!
      Registers a stroke shader for use. Must not be called within
      a begin() / end() pair.
      \param p PainterStrokeShader hold shaders to register
     */
    void
    register_shader(const PainterStrokeShader &p);

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
