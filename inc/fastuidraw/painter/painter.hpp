/*!
 * \file painter.hpp
 * \brief file painter.hpp
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


#include <fastuidraw/painter/packing/painter_packer.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

  /*!
    Painter wraps around PainterPacker to implement a classic
    2D rendering interface:
     - stroking
     - filling
     - brush
     - single 3x3 transformation
     - save and restore state
     - clipIn against path or rect
     - clipOut against path

    The transformation of a Painter goes from local item coordinate
    to 3D API clip-coordinates (for example in GL, from item coordinates
    to gl_Position.xyw).

    One can specify the exact attribute and index data for a Painter
    to consume, see \ref draw_generic(). In addition, the class
    PainterAttributeData can be used to generate and save attribute and
    index data to be used repeatedly.
   */
  class Painter:public reference_counted<Painter>::default_base
  {
  public:
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
      Conveniance typedef to PainterState::PainterBrushState
     */
    typedef PainterState::PainterBrushState PainterBrushState;

    /*!
      Conveniance typedef to PainterState::ItemMatrixState
     */
    typedef PainterState::ItemMatrixState ItemMatrixState;

    /*!
      Conveniance typedef to PainterState::VertexShaderDataState
     */
    typedef PainterState::VertexShaderDataState VertexShaderDataState;

    /*!
      Conveniance typedef to PainterState::FragmentShaderDataState
     */
    typedef PainterState::FragmentShaderDataState FragmentShaderDataState;

    /*!
      Ctor.
     */
    explicit
    Painter(PainterBackend::handle backend);

    ~Painter(void);

    /*!
      Returns a handle to the GlyphAtlas of this
      Painter. All glyphs used by this
      Painter must live on glyph_atlas().
     */
    const GlyphAtlas::handle&
    glyph_atlas(void) const;

    /*!
      Returns a handle to the ImageAtlas of this
      Painter. All images used by all brushes of
      this Painter must live on image_atlas().
     */
    const ImageAtlas::handle&
    image_atlas(void) const;

    /*!
      Returns a handle to the ColorStopAtlas of this
      Painter. All color stops used by all brushes
      of this Painter must live on colorstop_atlas().
     */
    const ColorStopAtlas::handle&
    colorstop_atlas(void) const;

    /*!
      Informs the Painter what the resolution of
      the target surface is.
      \param w width of target resolution
      \param h height of target resolution
     */
    void
    target_resolution(int w, int h);

    /*!
      Indicate to start drawing with methods of this Painter.
      Drawing commands sent to 3D hardware are buffered and not
      sent to hardware until end() is called.
      All draw commands must be between a begin()/end() pair.
     */
    void
    begin(bool reset_z = true);

    /*!
      Indicate to end drawing with methods of this Painter.
      Drawing commands sent to 3D hardware are buffered and not
      sent to hardware until end is called.
      All draw commands must be between a begin()/end() pair.
     */
    void
    end(void);

    /*!
      Concats the current transformation matrix
      by a given matrix.
      \param tr transformation by which to concat
     */
    void
    concat(const float3x3 &tr);

    /*!
      Sets the transformation matrix
      \param m new value for transformation matrix
     */
    void
    transformation(const float3x3 &m);

    /*!
      Sets the transformation matrix
      \param m new value for transformation matrix
     */
    void
    transformation(const ItemMatrix &m)
    {
      transformation(m.m_item_matrix);
    }

    /*!
      Concats the current transformation matrix
      with a translation
      \param p translation by which to translate
     */
    void
    translate(const vec2 &p);

    /*!
      Concats the current transformation matrix
      with a scaleing.
      \param s scaling factor by which to scale
     */
    void
    scale(float s);

    /*!
      Concats the current transformation matrix
      with a rotation.
      \param angle angle by which to rotate in radians.
     */
    void
    rotate(float angle);

    /*!
      Concats the current transformation matrix
      with a shear.
      \param sx scaling factor in x-direction to apply
      \param sy scaling factor in y-direction to apply
     */
    void
    shear(float sx, float sy);

    /*!
      Returns a handle to current state of the 3x3
      transformation that can be re-used by passing it to
      transformation_state(const ItemMatrixState&)
     */
    const ItemMatrixState&
    transformation_state(void);

    /*!
      Set the transformation state from a transformation state handle.
      \param h handle to transformation state.
     */
    void
    transformation_state(const ItemMatrixState &h);

    /*!
      Set clipping to the intersection of the current
      clipping with a rectangle.
      \param xy location of rectangle
      \param wh width and height of rectange
     */
    void
    clipInRect(const vec2 &xy, const vec2 &wh);

    /*!
      Clip-out by a path, i.e. set the clipping to be
      the intersection of the current clipping against
      the -complement- of the fill of a path.
      \param path path by which to clip out
      \param fill_rule fill rule to apply to path
     */
    void
    clipOutPath(const Path &path, enum PainterEnums::fill_rule_t fill_rule);

    /*!
      Clip-in by a path, i.e. set the clipping to be
      the intersection of the current clipping against
      the the fill of a path.
      \param path path by which to clip out
      \param fill_rule fill rule to apply to path
     */
    void
    clipInPath(const Path &path, enum PainterEnums::fill_rule_t fill_rule);

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
    brush_state(void) const;

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
      Sets the blend shader. It is a crashing error for
      h to be NULL.
      \param h blend shader to use for blending.
     */
    void
    blend_shader(const PainterShader::const_handle &h);

    /*!
      Equivalent to
      \code
      blend_shader(default_shaders().blend_shaders().shader(m))
      \endcode
      It is a crashing error if default_shaders() does not support
      the named blend mode.
      \param m Blend mode to use
     */
    void
    blend_shader(enum PainterEnums::blend_mode_t m)
    {
      blend_shader(default_shaders().blend_shaders().shader(m));
    }

    /*!
      Provided as a conveniance to return if a blend
      mode is supported by the default shaders, equivalent
      to
      \code
      default_shaders().blend_shaders().shader(m)
      \endcode
      \param m blend mode to check.
     */
    bool
    blend_mode_supported(enum PainterEnums::blend_mode_t m)
    {
      return default_shaders().blend_shaders().shader(m);
    }

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
    vertex_shader_data(void) const;

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
    fragment_shader_data(void) const;

    /*!
      Save the current state of this Painter onto the save state stack.
      The state is restored (and the stack popped) by called restore().
      The state saved is:
      - transformation state
      - clip state
      - brush state
      - vertex shader data state
      - fragment shader data state
     */
    void
    save(void);

    /*!
      Restore the state of this Painter to the state
      it had from the last call to save().
     */
    void
    restore(void);

    /*!
      Return the default shaders for common drawing types.
     */
    const PainterShaderSet&
    default_shaders(void) const;

    /*!
      Draw glyphs. Note: the value of current_z() is incremented on each glyph.
      \param data attribute and index data with which to draw the glyphs.
      \param shader with which to draw the glyphs
     */
    void
    draw_glyphs(const PainterAttributeData &data, const PainterGlyphShader &shader);

    /*!
      Draw glyphs. Note: the value of current_z() is incremented on each glyph.
      \param data attribute and index data with which to draw the glyphs
      \param use_anistopic_antialias if true, use default_shaders().glyph_shader_anisotropic()
                                     otherwise use default_shaders().glyph_shader()
     */
    void
    draw_glyphs(const PainterAttributeData &data, bool use_anistopic_antialias = false);

    /*!
      Stroke a path.
      \param data attribute and index data with which to stroke a path
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param cp cap style
      \param js join style
      \param shader shader with which to stroke the attribute data.
     */
    void
    stroke_path(const PainterAttributeData &data,
                enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                bool with_anti_aliasing, const PainterStrokeShader &shader);

    /*!
      Stroke a path.
      \param path path to stroke
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param cp cap style
      \param js join style
      \param shader shader with which to stroke the attribute data.
     */
    void
    stroke_path(const Path &path,
                enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                bool with_anti_aliasing, const PainterStrokeShader &shader)
    {
      stroke_path(path.tessellation()->stroked()->painter_data(),
                  cp, js, with_anti_aliasing, shader);
    }

    /*!
      Stroke a path using the default stroking shader of default_shaders().
      \param path path to stroke
      \param cp cap style
      \param js join style
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
     */
    void
    stroke_path(const Path &path,
                enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                bool with_anti_aliasing)
    {
      stroke_path(path, cp, js, with_anti_aliasing, default_shaders().stroke_shader());
    }

    /*!
      Fill a path.
      \param data attribute and index data with which to fill a path
      \param fill_rule fill rule with which to fill the path
      \param shader shader with which to fill the attribute data
     */
    void
    fill_path(const PainterAttributeData &data,
              enum PainterEnums::fill_rule_t fill_rule,
              const PainterItemShader &shader)
    {
      draw_generic(data.attribute_data_chunk(fill_rule),
                   data.index_data_chunk(fill_rule),
                   shader);
    }

    /*!
      Fill a path.
      \param path to fill
      \param fill_rule fill rule with which to fill the path
      \param shader shader with which to fill the attribute data
     */
    void
    fill_path(const Path &path, enum PainterEnums::fill_rule_t fill_rule,
              const PainterItemShader &shader)
    {
      fill_path(path.tessellation()->filled()->painter_data(), fill_rule, shader);
    }

    /*!
      Fill a path using the default shader to draw the fill.
      \param path path to fill
      \param fill_rule fill rule with which to fill the path
     */
    void
    fill_path(const Path &path, enum PainterEnums::fill_rule_t fill_rule)
    {
      fill_path(path, fill_rule, default_shaders().fill_shader());
    }

    /*!
      Draw a rect.
      \param p min-corner of rect
      \param wh width and height of rect
      \param call_back handle to PainterPacker::DataCallBack for the draw
     */
    void
    draw_rect(const vec2 &p, const vec2 &wh,
              const PainterPacker::DataCallBack::handle &call_back = PainterPacker::DataCallBack::handle());

    /*!
      Draw a quad
      \param p0 first point of quad, shares an edge with p3
      \param p1 point after p0, shares an edge with p0
      \param p2 point after p1, shares an edge with p1
      \param p3 point after p2, shares an edge with p2
      \param call_back handle to PainterPacker::DataCallBack for the draw
     */
    void
    draw_quad(const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3,
              const PainterPacker::DataCallBack::handle &call_back = PainterPacker::DataCallBack::handle());

    /*!
      Draw generic attribute data
      \param attrib_chunk attribute data to draw
      \param index_chunk indx data into attrib_chunk
      \param shader shader with which to draw data
      \param call_back handle to PainterPacker::DataCallBack for the draw
     */
    void
    draw_generic(const_c_array<PainterAttribute> attrib_chunk,
                 const_c_array<PainterIndex> index_chunk,
                 const PainterItemShader &shader,
                 const PainterPacker::DataCallBack::handle &call_back = PainterPacker::DataCallBack::handle())
    {
      vecN<const_c_array<PainterAttribute>, 1> aa(attrib_chunk);
      vecN<const_c_array<PainterIndex>, 1> ii(index_chunk);
      draw_generic(aa, ii, shader, call_back);
    }

    /*!
      Draw generic attribute data
      \param attrib_chunks attribute data to draw
      \param index_chunks the i'th element is index data into attrib_chunks[i]
      \param shader shader with which to draw data
      \param call_back handle to PainterPacker::DataCallBack for the draw
     */
    void
    draw_generic(const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
                 const_c_array<const_c_array<PainterIndex> > index_chunks,
                 const PainterItemShader &shader,
                 const PainterPacker::DataCallBack::handle &call_back = PainterPacker::DataCallBack::handle());

    /*!
      Return the z-depth value that the next item will have
     */
    unsigned int
    current_z(void) const;

   /*!
     Increment the value of current_z(void) const.
     \param amount amount by which to increment current_z(void) const
     */
    void
    increment_z(int amount = 1);

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

  private:
    void *m_d;
  };
/*! @} */
}
