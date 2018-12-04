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

#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/stroking_style.hpp>
#include <fastuidraw/painter/rounded_rect.hpp>
#include <fastuidraw/painter/glyph_sequence.hpp>
#include <fastuidraw/painter/glyph_run.hpp>
#include <fastuidraw/painter/stroked_path.hpp>
#include <fastuidraw/painter/filled_path.hpp>
#include <fastuidraw/painter/fill_rule.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>
#include <fastuidraw/painter/painter_data.hpp>
#include <fastuidraw/painter/packing/painter_packer.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * Painter wraps around PainterPacker to implement a classic
   * 2D rendering interface.
   *
   * Painter implements:
   *  - stroking
   *  - filling
   *  - applying a brush (see PainterBrush)
   *  - single 3x3 transformation
   *  - save and restore state
   *  - clipIn against Path or rectangle
   *  - clipOut against Path
   *
   * The transformation of a Painter goes from local item coordinate
   * to 3D API clip-coordinates (for example in GL, from item coordinates
   * to gl_Position.xyw). FastUIDraw follows the convention that
   * the top of the window is at normalized y-coordinate -1 and the
   * bottom of the window is at normalized y-coordinate +1.
   *
   * One can specify the exact attribute and index data for a Painter
   * to consume, see \ref draw_generic(). In addition, the class
   * PainterAttributeData can be used to generate and save attribute and
   * index data to be used repeatedly.
   */
  class Painter:
    public PainterEnums,
    public reference_counted<Painter>::default_base
  {
  public:
    /*!
     * Ctor.
     */
    explicit
    Painter(reference_counted_ptr<PainterBackend> backend);

    ~Painter(void);

    /*!
     * Returns a handle to the GlyphAtlas of this
     * Painter. All glyphs used by this
     * Painter must live on glyph_atlas().
     */
    const reference_counted_ptr<GlyphAtlas>&
    glyph_atlas(void) const;

    /*!
     * Returns a handle to the ImageAtlas of this
     * Painter. All images used by all brushes of
     * this Painter must live on image_atlas().
     */
    const reference_counted_ptr<ImageAtlas>&
    image_atlas(void) const;

    /*!
     * Returns a handle to the ColorStopAtlas of this
     * Painter. All color stops used by all brushes
     * of this Painter must live on colorstop_atlas().
     */
    const reference_counted_ptr<ColorStopAtlas>&
    colorstop_atlas(void) const;

    /*!
     * Returns the PainterShaderRegistrar of the PainterBackend
     * that was used to create this Painter object. Use this
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
     * Returns the active 3D API blend mode
     */
    BlendMode
    composite_mode(void) const;

    /*!
     * Sets the composite shader.
     * \param h composite shader to use for compositing.
     * \param blend_mode 3D API blend mode
     */
    void
    composite_shader(const reference_counted_ptr<PainterCompositeShader> &h,
                     BlendMode blend_mode);

    /*!
     * Equivalent to
     * \code
     * composite_shader(shader_set.shader(m),
     *              shader_set.composite_mode(m))
     * \endcode
     * It is a crashing error if shader_set does not support
     * the named composite mode.
     * \param shader_set PainterCompositeShaderSet from which to take composite shader
     * \param m Composite mode to use
     */
    void
    composite_shader(const PainterCompositeShaderSet &shader_set,
                     enum composite_mode_t m)
    {
      composite_shader(shader_set.shader(m), shader_set.composite_mode(m));
    }

    /*!
     * Equivalent to
     * \code
     * composite_shader(default_shaders().composite_shaders(), m)
     * \endcode
     * \param m Composite mode to use
     */
    void
    composite_shader(enum composite_mode_t m)
    {
      composite_shader(default_shaders().composite_shaders(), m);
    }

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
     * Equivalent to
     * \code
     * blend_shader(default_shaders().blend_shaders().shader(m))
     * \endcode
     * \param m blend mode to use
     */
    void
    blend_shader(enum blend_w3c_mode_t m)
    {
      blend_shader(default_shaders().blend_shaders().shader(m));
    }

    /*!
     * Indicate to start drawing with methods of this Painter. The
     * ransformation matrix will be intialized with a projection
     * matrix derived from the passed screen_orientation
     * and the viewort of the passed PainterBackend::Surface. Drawing
     * commands sent to 3D hardware are buffered and not sent to the
     * hardware until end() is called. All draw commands must be between
     * a begin()/end() pair.
     * \param surface the \ref PainterBackend::Surface to which to render content
     * \param orientation orientation convention with which to initialize the
     *                    transformation
     * \param clear_color_buffer if true, clear the color buffer on the viewport
     *                           of the surface.
     */
    void
    begin(const reference_counted_ptr<PainterBackend::Surface> &surface,
          enum screen_orientation orientation,
          bool clear_color_buffer = true);

    /*!
     * Indicate to end drawing with methods of this Painter.
     * Drawing commands sent to 3D hardware are buffered and not
     * sent to hardware until end() is called.
     * All draw commands must be between a begin()/end() pair.
     */
    void
    end(void);

    /*!
     * Concats the current transformation matrix
     * by a given matrix.
     * \param tr transformation by which to concat
     */
    void
    concat(const float3x3 &tr);

    /*!
     * Sets the transformation matrix
     * \param m new value for transformation matrix
     */
    void
    transformation(const float3x3 &m);

    /*!
     * Sets the transformation matrix
     * \param m new value for transformation matrix
     */
    void
    transformation(const PainterItemMatrix &m)
    {
      transformation(m.m_item_matrix);
    }

    /*!
     * Concats the current transformation matrix
     * with a translation
     * \param p translation by which to translate
     */
    void
    translate(const vec2 &p);

    /*!
     * Concats the current transformation matrix
     * with a scaleing.
     * \param s scaling factor by which to scale
     */
    void
    scale(float s);

    /*!
     * Concats the current transformation matrix
     * with a rotation.
     * \param angle angle by which to rotate in radians.
     */
    void
    rotate(float angle);

    /*!
     * Concats the current transformation matrix
     * with a shear.
     * \param sx scaling factor in x-direction to apply
     * \param sy scaling factor in y-direction to apply
     */
    void
    shear(float sx, float sy);

    /*!
     * Returns the value of the current transformation.
     */
    const PainterItemMatrix&
    transformation(void);

    /*!
     * Set clipping to the intersection of the current
     * clipping with a rectangle.
     * \param xy location of rectangle
     * \param wh width and height of rectange
     */
    void
    clip_in_rect(const vec2 &xy, const vec2 &wh);

    /*!
     * Set clipping to the intersection of the current
     * clipping with a rounded rectangle.
     * \param R rounded rectangle
     */
    void
    clip_in_rounded_rect(const RoundedRect &R);

    /*!
     * Clip-out by a path, i.e. set the clipping to be
     * the intersection of the current clipping against
     * the -complement- of the fill of a path.
     * \param path path by which to clip out
     * \param fill_rule fill rule to apply to path
     */
    void
    clip_out_path(const Path &path, enum fill_rule_t fill_rule);

    /*!
     * Clip-out by a path, i.e. set the clipping to be
     * the intersection of the current clipping against
     * the -complement- of the fill of a path.
     * \param path path by which to clip out
     * \param fill_rule fill rule to apply to path
     */
    void
    clip_out_path(const FilledPath &path, enum fill_rule_t fill_rule);

    /*!
     * Clip-in by a path, i.e. set the clipping to be
     * the intersection of the current clipping against
     * the the fill of a path.
     * \param path path by which to clip out
     * \param fill_rule fill rule to apply to path
     */
    void
    clip_in_path(const Path &path, enum fill_rule_t fill_rule);

    /*!
     * Clip-in by a path, i.e. set the clipping to be
     * the intersection of the current clipping against
     * the the fill of a path.
     * \param path path by which to clip out
     * \param fill_rule fill rule to apply to path
     */
    void
    clip_in_path(const FilledPath &path, enum fill_rule_t fill_rule);

    /*!
     * Clip-out by a path, i.e. set the clipping to be
     * the intersection of the current clipping against
     * the -complement- of the fill of a path.
     * \param path path by which to clip out
     * \param fill_rule custom fill rule to apply to path
     */
    void
    clip_out_path(const Path &path, const CustomFillRuleBase &fill_rule);

    /*!
     * Clip-out by a path, i.e. set the clipping to be
     * the intersection of the current clipping against
     * the -complement- of the fill of a path.
     * \param path path by which to clip out
     * \param fill_rule custom fill rule to apply to path
     */
    void
    clip_out_path(const FilledPath &path, const CustomFillRuleBase &fill_rule);

    /*!
     * Clip-in by a path, i.e. set the clipping to be
     * the intersection of the current clipping against
     * the the fill of a path.
     * \param path path by which to clip out
     * \param fill_rule custom fill rule to apply to path
     */
    void
    clip_in_path(const Path &path, const CustomFillRuleBase &fill_rule);

    /*!
     * Clip-in by a path, i.e. set the clipping to be
     * the intersection of the current clipping against
     * the the fill of a path.
     * \param path path by which to clip out
     * \param fill_rule custom fill rule to apply to path
     */
    void
    clip_in_path(const FilledPath &path, const CustomFillRuleBase &fill_rule);

    /*!
     * Clipout by custom data.
     * \param shader shader with which to draw the attribute/index data
     * \param shader_data shader data to pass to shader
     * \param attrib_chunks attribute data to draw
     * \param index_chunks the i'th element is index data into attrib_chunks[i]
     * \param index_adjusts if non-empty, the i'th element is the value by which
     *                      to adjust all of index_chunks[i]; if empty the index
     *                      values are not adjusted.
     * \param attrib_chunk_selector selects which attribute chunk to use for
     *                              each index chunk
     */
    void
    clipOutCustom(const reference_counted_ptr<PainterItemShader> &shader,
                  const PainterData::value<PainterItemShaderData> &shader_data,
                  c_array<const c_array<const PainterAttribute> > attrib_chunks,
                  c_array<const c_array<const PainterIndex> > index_chunks,
                  c_array<const int> index_adjusts,
                  c_array<const unsigned int> attrib_chunk_selector);

    /*!
     * Clipout by custom data.
     * \param shader shader with which to draw the attribute/index data
     * \param shader_data shader data to pass to shader
     * \param attrib_chunks attribute data to draw
     * \param index_chunks the i'th element is index data into attrib_chunks[i]
     * \param index_adjusts if non-empty, the i'th element is the value by which
     *                      to adjust all of index_chunks[i]; if empty the index
     *                      values are not adjusted.
     */
    void
    clipOutCustom(const reference_counted_ptr<PainterItemShader> &shader,
                  const PainterData::value<PainterItemShaderData> &shader_data,
                  c_array<const c_array<const PainterAttribute> > attrib_chunks,
                  c_array<const c_array<const PainterIndex> > index_chunks,
                  c_array<const int> index_adjusts)
    {
      clipOutCustom(shader, shader_data,
                    attrib_chunks, index_chunks, index_adjusts,
                    c_array<const unsigned int>());
    }

    /*!
     * Clipout by custom data.
     * \param shader shader with which to draw the attribute/index data
     * \param shader_data shader data to pass to shader
     * \param attrib_chunk attribute data to draw
     * \param index_chunk index data into attrib_chunk
     * \param index_adjust amount by which to adjust the values in index_chunk
     */
    void
    clipOutCustom(const reference_counted_ptr<PainterItemShader> &shader,
                  const PainterData::value<PainterItemShaderData> &shader_data,
                  c_array<const PainterAttribute> attrib_chunk,
                  c_array<const PainterIndex> index_chunk,
                  int index_adjust)
    {
      vecN<c_array<const PainterAttribute>, 1> aa(attrib_chunk);
      vecN<c_array<const PainterIndex>, 1> ii(index_chunk);
      vecN<int, 1> ia(index_adjust);
      clipOutCustom(shader, shader_data, aa, ii, ia);
    }

    /*!
     * Set the curve flatness requirement for TessellatedPath
     * and StrokedPath selection when stroking or filling paths
     * when passing to drawing methods a Path object. The value
     * represents the distance, in pixels, requested for between
     * the approximated curve (realized in TessellatedPath) and
     * the true curve (realized in Path). This value is combined
     * with a value derived from the current transformation
     * matrix to pass to Path::tessellation(float) to fetch a
     *  \ref TessellatedPath. Default value is 0.5.
     */
    void
    curve_flatness(float thresh);

    /*!
     * Returns the value set by curve_flatness(float).
     */
    float
    curve_flatness(void);

    /*!
     * Save the current state of this Painter onto the save state stack.
     * The state is restored (and the stack popped) by called restore().
     * The state saved is:
     * - transformation state (see concat(), transformation(), translate(),
     *   shear(), scale(), rotate()).
     * - clip state (see clip_in_rect(), clip_out_path(), clip_in_path())
     * - curve flatness requirement (see curve_flatness(float))
     * - composite shader (see composite_shader()).
     */
    void
    save(void);

    /*!
     * Restore the state of this Painter to the state
     * it had from the last call to save().
     */
    void
    restore(void);

    /*!
     * Return the default shaders for common drawing types.
     */
    const PainterShaderSet&
    default_shaders(void) const;

    /*!
     * Using the current transformation matrix and a,
     * compute what is an ideal way to render a sequence
     * glyphs layed out and scaled in local coordinates
     * to a given pixel size.
     * \param pixel_size size of text to render BEFORE
     *                   applying the transformation matrix.
     */
    GlyphRenderer
    compute_glyph_renderer(float pixel_size);

    /*!
     * Draw glyphs from a \ref GlyphSequence.
     * \param shader \ref PainterGlyphShader to draw the glyphs
     * \param draw data for how to draw
     * \param glyph_sequence \ref GlyphSequence providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will choose a \ref GlyphRenderer suitable
     *                 for the current transformation() value.
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
                const GlyphSequence &glyph_sequence,
		GlyphRenderer renderer = GlyphRenderer());

    /*!
     * Draw glyphs from a \ref GlyphSequence.
     * and the data of the passed \ref GlyphSequence.
     * \param draw data for how to draw
     * \param glyph_sequence \ref GlyphSequence providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will choose a \ref GlyphRenderer suitable
     *                 for the current transformation() value.
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterData &draw, const GlyphSequence &glyph_sequence,
		GlyphRenderer renderer = GlyphRenderer());

    /*!
     * Draw glyphs from a \ref GlyphRun.
     * \param shader \ref PainterGlyphShader to draw the glyphs
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will choose a \ref GlyphRenderer suitable
     *                 for the current transformation() value.
     * \param begin first character of GlyphRun to draw
     * \param count number of characters, startng at begin, of the GlyphRun to draw
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
                const GlyphRun &glyph_run,
                unsigned int begin, unsigned int count,
		GlyphRenderer renderer = GlyphRenderer());

    /*!
     * Draw glyphs from a \ref GlyphRun.
     * and the data of the passed \ref GlyphRun.
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will choose a \ref GlyphRenderer suitable
     *                 for the current transformation() value.
     * \param begin first character of GlyphRun to draw
     * \param count number of characters, startng at begin, of the GlyphRun to draw
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterData &draw, const GlyphRun &glyph_run,
                unsigned int begin, unsigned int count,
		GlyphRenderer renderer = GlyphRenderer());

    /*!
     * Draw all glyphs from a \ref GlyphRun.
     * \param shader \ref PainterGlyphShader to draw the glyphs
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will choose a \ref GlyphRenderer suitable
     *                 for the current transformation() value.
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
                const GlyphRun &glyph_run,
		GlyphRenderer renderer = GlyphRenderer());

    /*!
     * Draw all glyphs from a \ref GlyphRun.
     * and the data of the passed \ref GlyphRun.
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will choose a \ref GlyphRenderer suitable
     *                 for the current transformation() value.
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterData &draw, const GlyphRun &glyph_run,
		GlyphRenderer renderer = GlyphRenderer());

    /*!
     * Returns what value Painter currently uses for Path::tessellation(float) const
     * to fetch the \ref TessellatedPath from which it will fetch the \ref FilledPath
     * to perform a path fill.
     * \param path \ref Path to choose the thresh for
     */
    float
    compute_path_thresh(const Path &path);

    /*!
     * Returns what value Painter currently uses for Path::tessellation(float) const
     * to fetch the \ref TessellatedPath from which it will fetch the \ref StrokedPath
     * to perform path stroking.
     * \param path \ref Path to choose the thresh for
     * \param shader_data underlying data for stroking shader
     * \param selector object (see PainterStrokeShader::stroking_data_selector())
     *                 to use stroking parameters to help compute necessary thresh
     * \param[out] out_rounded_thresh location to which to write threshhold to be
     *                                used for rounded caps and joins of \ref
     *                                StrokedCapsJoins.
     */
    float
    compute_path_thresh(const Path &path,
                        const PainterShaderData::DataBase *shader_data,
                        const reference_counted_ptr<const StrokingDataSelectorBase> &selector,
                        float *out_rounded_thresh);

    /*!
     * Calls FilledPath::select_subsets() passing arguments derived from the
     * current state of the Painter.
     * \param path \ref FilledPath from which to compute subset selection
     * \param[out] dst location to which to write the FilledPath::Subset ID values
     * \returns the number of Subset object ID's written to dst, that
     *          number is guaranteed to be no more than FilledPath::number_subsets().
     */
    unsigned int
    select_subsets(const FilledPath &path, c_array<unsigned int> dst);

    /*!
     * Calls StrokedPath::select_subsets() passing arguments derived from the
     * current state of the Painter.
     * \param path \ref StrokedPath from which to compute subset selection
     * \param pixels_additional_room additional slack in -pixels- in selecting
     *                               subsets from the StrokedPath geometry data
     * \param item_space_additional_room amount additional slack in -local coordinate-
     *                                   in selecting subsets from the StrokedPath
     *                                   geometry data
     * \param[out] dst location to which to write the StrokedPath::Subset ID values
     * \returns the number of Subset object ID's written to dst, that
     *          number is guaranteed to be no more than StrokedPath::number_subsets().
     */
    unsigned int
    select_subsets(const StrokedPath &path,
                   float pixels_additional_room,
                   float item_space_additional_room,
                   c_array<unsigned int> dst);

    /*!
     * Calls StrokedCapsJoins::compute_chunks() passing arguments derived
     * from the current state of the Painter.
     * \param caps_joins \ref StrokedCapsJoins from which to compute chunk selection
     * \param pixels_additional_room additional slack in -pixels- in selecting
     *                               subsets from the StrokedPath geometry data
     * \param item_space_additional_room amount additional slack in -local coordinate-
     *                                   in selecting subsets from the StrokedPath
     *                                   geometry data
     * \param take_all_joins if true, filtering of joins is not performed, i.e. all
     *                       joins of the \ref StrokedCapsJoins are selected
     * \param[out] dst location to which to write what chunks
     */
    void
    select_chunks(const StrokedCapsJoins &caps_joins,
                  float pixels_additional_room,
                  float item_space_additional_room,
                  bool take_all_joins,
                  StrokedCapsJoins::ChunkSet *dst);

    /*!
     * Stroke a path.
     * \param shader shader with which to stroke the attribute data
     * \param draw data for how to draw
     * \param path StrokedPath to stroke
     * \param rounded_thresh value to feed to StrokedCapsJoins::rounded_joins()
     *                       and/or StrokedCapsJoins::rounded_caps() if rounded
     *                       joins and/or rounded caps are requested
     * \param stroke_style how to stroke the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill.
     */
    void
    stroke_path(const PainterStrokeShader &shader, const PainterData &draw,
                const StrokedPath &path, float rounded_thresh,
                const StrokingStyle &stroke_style = StrokingStyle(),
                enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Stroke a path.
     * \param shader shader with which to stroke the attribute data
     * \param draw data for how to draw
     * \param path Path to stroke
     * \param stroke_style how to stroke the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill.
     * \param stroking_method stroking method to select what \ref StrokedPath to use
     */
    void
    stroke_path(const PainterStrokeShader &shader, const PainterData &draw, const Path &path,
                const StrokingStyle &stroke_style = StrokingStyle(),
                enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto,
                enum stroking_method_t stroking_method = stroking_method_auto);

    /*!
     * Stroke a path using PainterShaderSet::stroke_shader() of default_shaders().
     * \param draw data for how to draw
     * \param path Path to stroke
     * \param stroke_style how to stroke the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill.
     * \param stroking_method stroking method to select what \ref StrokedPath to use
     */
    void
    stroke_path(const PainterData &draw, const Path &path,
                const StrokingStyle &stroke_style = StrokingStyle(),
                enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto,
                enum stroking_method_t stroking_method = stroking_method_auto);

    /*!
     * Stroke a path dashed.
     * \param shader shader with which to draw
     * \param draw data for how to draw
     * \param path StrokedPath to stroke
     * \param rounded_thresh value to feed to StrokedCapsJoins::rounded_joins()
     *                       and/or StrokedCapsJoins::rounded_caps() if rounded
     *                       joins and/or rounded caps are requested
     * \param stroke_style how to stroke the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill.
     */
    void
    stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw,
                       const StrokedPath &path, float rounded_thresh,
                       const StrokingStyle &stroke_style = StrokingStyle(),
                       enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Stroke a path dashed.
     * \param shader shader with which to draw
     * \param draw data for how to draw
     * \param path Path to stroke
     * \param stroke_style how to stroke the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill.
     * \param stroking_method stroking method to select what \ref StrokedPath to use
     */
    void
    stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw, const Path &path,
                       const StrokingStyle &stroke_style = StrokingStyle(),
                       enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto,
                       enum stroking_method_t stroking_method = stroking_method_auto);

    /*!
     * Stroke a path using PainterShaderSet::dashed_stroke_shader() of default_shaders().
     * \param draw data for how to draw
     * \param path Path to stroke
     * \param stroke_style how to stroke the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill.
     * \param stroking_method stroking method to select what \ref StrokedPath to use
     */
    void
    stroke_dashed_path(const PainterData &draw, const Path &path,
                       const StrokingStyle &stroke_style = StrokingStyle(),
                       enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto,
                       enum stroking_method_t stroking_method = stroking_method_auto);

    /*!
     * Fill a path.
     * \param shader shader with which to fill the attribute data
     * \param draw data for how to draw
     * \param data attribute and index data with which to fill a path
     * \param fill_rule fill rule with which to fill the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill.
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const FilledPath &data, enum fill_rule_t fill_rule,
              enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Fill a path.
     * \param shader shader with which to fill the attribute data
     * \param draw data for how to draw
     * \param path to fill
     * \param fill_rule fill rule with which to fill the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const Path &path, enum fill_rule_t fill_rule,
              enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Fill a path using the default shader to draw the fill.
     * \param draw data for how to draw
     * \param path path to fill
     * \param fill_rule fill rule with which to fill the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    fill_path(const PainterData &draw, const Path &path, enum fill_rule_t fill_rule,
              enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Fill a path.
     * \param shader shader with which to fill the attribute data
     * \param draw data for how to draw
     * \param data attribute and index data with which to fill a path
     * \param fill_rule custom fill rule with which to fill the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const FilledPath &data, const CustomFillRuleBase &fill_rule,
              enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Fill a path.
     * \param shader shader with which to fill the attribute data
     * \param draw data for how to draw
     * \param path to fill
     * \param fill_rule custom fill rule with which to fill the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const Path &path, const CustomFillRuleBase &fill_rule,
              enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Fill a path using the default shader to draw the fill.
     * \param draw data for how to draw
     * \param path path to fill
     * \param fill_rule custom fill rule with which to fill the path
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    fill_path(const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
              enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Draw a convex polygon using a custom shader.
     * \param shader shader with which to draw the convex polygon
     * \param draw data for how to draw
     * \param pts points of the polygon so that neighboring points (modulo pts.size())
     *            are the edges of the polygon.
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    draw_convex_polygon(const PainterFillShader &shader, const PainterData &draw,
                        c_array<const vec2> pts,
                        enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Draw a convex polygon using the default fill shader.
     * \param draw data for how to draw
     * \param pts points of the polygon so that neighboring points (modulo pts.size())
     *            are the edges of the polygon.
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    draw_convex_polygon(const PainterData &draw, c_array<const vec2> pts,
                        enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Draw a convex quad using a custom shader.
     * \param shader shader with which to draw the quad
     * \param draw data for how to draw
     * \param p0 first point of quad, shares an edge with p3
     * \param p1 point after p0, shares an edge with p0
     * \param p2 point after p1, shares an edge with p1
     * \param p3 point after p2, shares an edge with p2
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    draw_quad(const PainterFillShader &shader, const PainterData &draw,
              const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3,
              enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Draw a quad using the default fill shader.
     * \param draw data for how to draw
     * \param p0 first point of quad, shares an edge with p3
     * \param p1 point after p0, shares an edge with p0
     * \param p2 point after p1, shares an edge with p1
     * \param p3 point after p2, shares an edge with p2
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    draw_quad(const PainterData &draw,
              const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3,
              enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Draw a rect using a custom shader.
     * \param shader shader with which to draw the quad
     * \param draw data for how to draw
     * \param p min-corner of rect
     * \param wh width and height of rect
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    draw_rect(const PainterFillShader &shader, const PainterData &draw,
              const vec2 &p, const vec2 &wh,
              enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Draw a rect using the default fill shader.
     * \param draw data for how to draw
     * \param p min-corner of rect
     * \param wh width and height of rect
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    draw_rect(const PainterData &draw, const vec2 &p, const vec2 &wh,
              enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Draw a rounded rect using a fill shader
     * \param shader shader with which to draw the rounded rectangle
     * \param draw data for how to draw
     * \param R \ref RoundedRect to draw
     * \param anti_alias_quality specifies the shader based anti-alias
     *                           quality to apply to the path fill
     */
    void
    draw_rounded_rect(const PainterFillShader &shader, const PainterData &draw,
                      const RoundedRect &R,
                      enum shader_anti_alias_t anti_alias_quality = shader_anti_alias_auto);

    /*!
     * Draw generic attribute data.
     * \param shader shader with which to draw data
     * \param draw data for how to draw
     * \param attrib_chunk attribute data to draw
     * \param index_chunk index data into attrib_chunk
     * \param index_adjust amount by which to adjust the values in index_chunk
     * \param call_back if non-nullptr handle, call back called when attribute data
     *                  is added.
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 c_array<const PainterAttribute> attrib_chunk,
                 c_array<const PainterIndex> index_chunk,
                 int index_adjust = 0,
                 const reference_counted_ptr<PainterPacker::DataCallBack> &call_back
                 = reference_counted_ptr<PainterPacker::DataCallBack>())
    {
      vecN<c_array<const PainterAttribute>, 1> aa(attrib_chunk);
      vecN<c_array<const PainterIndex>, 1> ii(index_chunk);
      vecN<int, 1> ia(index_adjust);
      draw_generic(shader, draw, aa, ii, ia, call_back);
    }

    /*!
     * Draw generic attribute data.
     * \param shader shader with which to draw data
     * \param draw data for how to draw
     * \param attrib_chunks attribute data to draw
     * \param index_chunks the i'th element is index data into attrib_chunks[i]
     * \param index_adjusts if non-empty, the i'th element is the value by which
     *                      to adjust all of index_chunks[i]; if empty the index
     *                      values are not adjusted.
     * \param call_back if non-nullptr handle, call back called when attribute data
     *                  is added.
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 c_array<const c_array<const PainterAttribute> > attrib_chunks,
                 c_array<const c_array<const PainterIndex> > index_chunks,
                 c_array<const int> index_adjusts,
                 const reference_counted_ptr<PainterPacker::DataCallBack> &call_back
                 = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
     * Draw generic attribute data
     * \param shader shader with which to draw data
     * \param draw data for how to draw
     * \param attrib_chunks attribute data to draw
     * \param index_chunks the i'th element is index data into attrib_chunks[K]
     *                     where K = attrib_chunk_selector[i]
     * \param index_adjusts if non-empty, the i'th element is the value by which
     *                      to adjust all of index_chunks[i]; if empty the index
     *                      values are not adjusted.
     * \param attrib_chunk_selector selects which attribute chunk to use for
     *                              each index chunk
     * \param call_back if non-nullptr handle, call back called when attribute data
     *                  is added.
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 c_array<const c_array<const PainterAttribute> > attrib_chunks,
                 c_array<const c_array<const PainterIndex> > index_chunks,
                 c_array<const int> index_adjusts,
                 c_array<const unsigned int> attrib_chunk_selector,
                 const reference_counted_ptr<PainterPacker::DataCallBack> &call_back
                 = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
     * Draw generic attribute data
     * \param shader shader with which to draw data
     * \param draw data for how to draw
     * \param src DrawWriter to use to write attribute and index data
     * \param call_back if non-nullptr handle, call back called when attribute data
     *                  is added.
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 const PainterPacker::DataWriter &src,
                 const reference_counted_ptr<PainterPacker::DataCallBack> &call_back
                 = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
     * Queue an action that uses (or affects) the GPU. Through these actions,
     * one can mix FastUIDraw::Painter with native API calls on a surface.
     * However, adding an action induces a draw-break (and state restore)
     * after each such action. Also, the action is not called until end()
     * is called.
     * \param action action to execute within a draw-stream.
     */
    void
    queue_action(const reference_counted_ptr<const PainterDraw::Action> &action);

    /*!
     * Returns a stat on how much data the Packer has
     * handled since the last call to begin().
     * \param st stat to query
     */
    unsigned int
    query_stat(enum PainterPacker::stats_t st) const;

    /*!
     * Return the z-depth value that the next item will have.
     */
    int
    current_z(void) const;

    /*!
     *Increment the value of current_z(void) const.
     *\param amount amount by which to increment current_z(void) const
     */
    void
    increment_z(int amount = 1);

  private:

    void *m_d;
  };
/*! @} */
}
