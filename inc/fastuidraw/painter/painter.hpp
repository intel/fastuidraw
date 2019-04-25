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

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/rounded_rect.hpp>

#include <fastuidraw/text/glyph_atlas.hpp>
#include <fastuidraw/colorstop_atlas.hpp>
#include <fastuidraw/image.hpp>
#include <fastuidraw/path.hpp>

#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/stroking_style.hpp>
#include <fastuidraw/painter/fill_rule.hpp>
#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>
#include <fastuidraw/painter/painter_packed_value_pool.hpp>

#include <fastuidraw/painter/attribute_data/painter_attribute_data.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute_writer.hpp>
#include <fastuidraw/painter/attribute_data/glyph_sequence.hpp>
#include <fastuidraw/painter/attribute_data/glyph_run.hpp>
#include <fastuidraw/painter/attribute_data/stroked_path.hpp>
#include <fastuidraw/painter/attribute_data/filled_path.hpp>

#include <fastuidraw/painter/shader/painter_shader_set.hpp>
#include <fastuidraw/painter/effects/painter_effect.hpp>

#include <fastuidraw/painter/backend/painter_engine.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * Painter implements a canvas rendering interface.
   *
   * Painter implements:
   *  - stroking
   *  - filling
   *  - drawing text
   *  - applying a brush (see \ref PainterBrush)
   *  - blending (see \ref PainterEnums::blend_mode_t)
   *  - single 3x3 transformation
   *  - save and restore state
   *  - transparency layers
   *  - clipIn against Path, rectangle or rounded rectangle
   *  - clipOut against Path, rectangle or rounded rectangle
   *
   * The transformation of a Painter goes from local item coordinate
   * to 3D API clip-coordinates (for example in GL, from item coordinates
   * to gl_Position.xyw). FastUIDraw follows the convention that the top
   * of the window is at normalized y-coordinate -1 and the bottom of the
   * window is at normalized y-coordinate +1. The transformation is to be
   * applied as matrix-vector multiplication, i.e.
   * \code
   * ClipCoordinates = transformation() * vec3(x, y, 1.0)
   * \endcode
   * for local coordiante (x, y). Normalized device coordinates are
   * defined as
   * \code
   * NormalizedDeviceCoordinates = ClipCoordinates.xy / ClipCoordinates.w
   * \endcode
   * where (-1, -1) corresponds to the bottom-left hand corner of the
   * viewport (see PainterSurface::viewport()) and (+1, +1)
   * is the top right hand corner of the viewport.
   *
   * The pixel pipeline of Painter is
   *   - Compute RGBA value from item shader (typically this is (0, 0, 0, alpha)
   *     where alpha is a coverage value
   *   - Modulate by the PainterBrush passing the item coordinates
   *     of the pixel to the \ref PainterBrush
   *   - Apply blending (see PainterEnums::blend_mode_t and
   *     Painter::blend_shader()) to the RGBA value against the
   *     current value in the framebuffer
   *
   * Painter uses clip-planes and the depth buffer to perform clipping.
   * The depth-buffer clips by occluding elements. For example, the
   * method clip_out_rect() simply draws a rectangle so that it does
   * not affect the color buffer but with a depth value that is infront
   * of the elements that it is to occlude. Painter uses the convention
   * that elements with greater than or equal depth values are visible
   * (for example in GL this corresponds to the depth test being set to
   * GL_GEQUAL). Unless an item occludes (or self occludes), the current
   * active depth value is unaffected. An item's vertex shader will emit
   * a relative z-value (i.e. relative to the item) which is then
   * incremented by the current z-value of the \ref Painter.
   *
   * A Painter is a HEAVY object (it creates pools of buffers during its
   * lifetime for reuse). As such, one should reuse Painter objects as
   * much as reasonably possible.
   */
  class Painter:
    public PainterEnums,
    public reference_counted<Painter>::concurrent
  {
  public:
    /*!
     * \brief
     * A \ref GlyphRendererChooser provides an interface for
     * choosing how to render glyphs depending on the current
     * transformation matrix, \ref Painter::transformation().
     */
    class GlyphRendererChooser
    {
    public:
      virtual
      ~GlyphRendererChooser()
      {}

      /*!
       * To be implemented by a derived class to choose
       * what GlyphRender to use when the transformation
       * matrix (see Painter::transformation()) does not
       * have perspective.
       * \param logical_pixel_size the pixel size at which
       *                           the glyphs of a GlyphRun
       *                           or GlyphSequence are
       *                           formatted
       * \param transformation the transformation matrix from
       *                       logical (i.e. item) coordinates
       *                       to normalized device coordinates,
       *                       i.e. the value of \ref
       *                       Painter::transformation().
       * \param max_singular_value the largest singluar value
       *                           the transformation matrix
       *                           concacted with the viewport
       *                           transformation
       * \param min_singular_value the smallest singluar value
       *                           the transformation matrix
       *                           concacted with the viewport
       *                           transformation
       */
      virtual
      GlyphRenderer
      choose_glyph_render(float logical_pixel_size,
                          const float3x3 &transformation,
                          float max_singular_value,
                          float min_singular_value) const = 0;

      /*!
       * To be implemented by a derived class to choose
       * what GlyphRender to use when the transformation
       * matrix (see Painter::transformation()) has
       * perspective.
       * \param logical_pixel_size the pixel size at which
       *                           the glyphs of a GlyphRun
       *                           or GlyphSequence are
       *                           formatted
       * \param transformation the transformation matrix from
       *                       logical (i.e. item) coordinates
       *                       to normalized device coordinates,
       *                       i.e. the value of \ref
       *                       Painter::transformation().
       */
      virtual
      GlyphRenderer
      choose_glyph_render(float logical_pixel_size,
                          const float3x3 &transformation) const = 0;
    };

    /*!
     * Ctor.
     * \param backend \ref PainterEngine object from which via
     *                \ref PainterEngine::create_backend(), the
     *                created \ref Painter object will use for its
     *                entire lifetime.
     */
    explicit
    Painter(const reference_counted_ptr<PainterEngine> &backend);

    ~Painter(void);

    /*!
     * Returns a reference to the \ref GlyphAtlas of
     * this \ref Painter. All glyphs used by this \ref
     * Painter must live on glyph_atlas().
     */
    GlyphAtlas&
    glyph_atlas(void) const;

    /*!
     * Returns a reference to the \ref ImageAtlas of of
     * this \ref Painter. All images used by this \ref
     * Painter must live on image_atlas().
     */
    ImageAtlas&
    image_atlas(void) const;

    /*!
     * Returns a reference to the ColorStopAtlas of this
     * \ref Painter. All color stops used by all brushes
     * of this \ref Painter must live on colorstop_atlas().
     */
    ColorStopAtlas&
    colorstop_atlas(void) const;

    /*!
     * Returns a handle to the \ref GlyphCache made
     * from glyph_atlas().
     */
    GlyphCache&
    glyph_cache(void) const;

    /*!
     * Returns the PainterShaderRegistrar of the PainterBackend
     * used by this Painter object. Use this return value to add
     * custom shaders. NOTE: shaders added within a thread are
     * not useable within that thread until the next call to
     * begin().
     */
    PainterShaderRegistrar&
    painter_shader_registrar(void) const;

    /*!
     * Returns the PainterPackedValuePool used to construct
     * PainterPackedValue objects.
     */
    PainterPackedValuePool&
    packed_value_pool(void);

    /*!
     * Returns the active blend shader
     */
    PainterBlendShader*
    blend_shader(void) const;

    /*!
     * Returns the active 3D API blend mode
     */
    BlendMode
    blend_mode(void) const;

    /*!
     * Sets the blend shader.
     * \param h blend shader to use for blending.
     * \param blend_mode 3D API blend mode
     */
    void
    blend_shader(const reference_counted_ptr<PainterBlendShader> &h,
                 BlendMode blend_mode);

    /*!
     * Equivalent to
     * \code
     * blend_shader(shader_set.shader(m), shader_set.blend_mode(m))
     * \endcode
     * It is a crashing error if shader_set does not support
     * the named blend mode.
     * \param shader_set PainterBlendShaderSet from which to take blend shader
     * \param m Blend mode to use
     */
    void
    blend_shader(const PainterBlendShaderSet &shader_set,
                 enum blend_mode_t m)
    {
      blend_shader(shader_set.shader(m), shader_set.blend_mode(m));
    }

    /*!
     * Equivalent to
     * \code
     * blend_shader(default_shaders().blend_shaders(), m)
     * \endcode
     * \param m Blend mode to use
     */
    void
    blend_shader(enum blend_mode_t m)
    {
      blend_shader(default_shaders().blend_shaders(), m);
    }

    /*!
     * Indicate to start drawing with methods of this Painter. The
     * transformation matrix will be intialized with given float3x3.
     * Drawing commands sent to 3D hardware are buffered and not sent
     * to the hardware until end() is called. All draw commands must
     * be between a begin()/end() pair.
     * \param surface the \ref PainterSurface to which to render content
     * \param initial_transformation value to initialize transformation() which
     *                               is the matrix from logical coordinates to
     *                               API 3D clip coordinates.
     * \param clear_color_buffer if true, clear the color buffer on the viewport
     *                           of the surface.
     */
    void
    begin(const reference_counted_ptr<PainterSurface> &surface,
          const float3x3 &initial_transformation,
          bool clear_color_buffer = true);

    /*!
     * Indicate to start drawing with methods of this Painter. The
     * ransformation matrix will be intialized with a projection
     * matrix derived from the passed screen_orientation
     * and the viewort of the passed PainterSurface. Drawing
     * commands sent to 3D hardware are buffered and not sent to the
     * hardware until end() is called. All draw commands must be between
     * a begin()/end() pair.
     * \param surface the \ref PainterSurface to which to render content
     * \param orientation orientation convention with which to initialize the
     *                    transformation
     * \param clear_color_buffer if true, clear the color buffer on the viewport
     *                           of the surface.
     */
    void
    begin(const reference_counted_ptr<PainterSurface> &surface,
          enum screen_orientation orientation,
          bool clear_color_buffer = true);

    /*!
     * Indicate to end drawing with methods of this Painter.
     * Drawing commands sent to 3D hardware are buffered and not
     * sent to hardware until end() is called. Returns the
     * list of surfaces used for offscreen rendering within
     * the begin()/end() pair; these surfaces are -owned- by
     * the Painter and their contents are potentially changed
     * (or even the object destroyed) on the next call to
     * begin(). All draw commands must be between a begin()/end()
     * pair.
     */
    c_array<const PainterSurface* const>
    end(void);

    /*!
     * Flushes the rendering and flushes the rendering commands
     * to the 3D API and maintain using the current PainterSurface.
     * It is not possible to flush if the Painter is within a
     * begin_layer()/end_layer() pair or if it is within a
     * begin_coverage_buffer()/end_coverage_buffer() pair.
     * Returns \ref routine_success if flush was executed and
     * \ref routine_fail if not.
     */
    enum return_code
    flush(void);

    /*!
     * Flushes the rendering and flushes the rendering commands
     * to the 3D API and shift to using a the passed surface;
     * the surface's viewport dimensions MUST match the
     * current surface's viewport dimensions, i.e. the value
     * of PainterSurface::viewport().m_dimensions surface() and
     * the passed PainterSurface must match. It is not possible
     * if the Painter is within a begin_layer()/end_layer() pair
     * or if it is within a begin_coverage_buffer()/end_coverage_buffer()
     * pair. Returns \ref routine_success if flush was executed
     * and \ref routine_fail if not.
     */
    enum return_code
    flush(const reference_counted_ptr<PainterSurface> &new_surface);

    /*!
     * Everytime Painter generates attribute/index data to
     * be sent to the 3D API, this counter is incremented.
     * The value is essentially equivalent to the number of
     * times draw_generic() is called (directly or indirectly
     * through other methods). The counter is reset when
     * begin() is called.
     */
    unsigned int
    draw_data_added_count(void) const;

    /*!
     * Returns the PainterSurface to which the Painter
     * is drawing. If there is no active surface, then returns
     * a null reference.
     */
    const reference_counted_ptr<PainterSurface>&
    surface(void) const;

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
    const float3x3&
    transformation(void);

    /*!
     * Returns the current clip-equations of the Painter.
     * The clip-equations are updated whenever clip_in_rect()
     * or clip_in_path() or restore() are called, as such
     * the returned c_array then becomes invalid. The equations
     * are in -CLIP- coordinates, thus do not change when
     * the transformation changes.
     */
    c_array<const vec3>
    clip_equations(void);

    /*!
     * Returns the convex polygon embodied by clip_equations().
     * The value changes whenever clip_in_rect(), clip_in_path()
     * or restore() are called, as such the returned c_array
     * then becomes invalid. The coordinates are in -CLIP-
     * coordinates, thus do not change when the transformation
     * changes.
     */
    c_array<const vec3>
    clip_polygon(void);

    /*!
     * If the clipping region is non-empty, returns true
     * and writes the min and max corner of the bounding box
     * in normalized device coordinates of the clipping region.
     * \param min_pt location to which to write the minimum corner point
     * \param max_pt location to which to write the maximum corner point
     */
    bool
    clip_region_bounds(vec2 *min_pt, vec2 *max_pt);

    /*!
     * If the clipping region is non-empty, returns true
     * and writes the min and max corner of the bounding box
     * in local coordinates of the clipping region.
     * \param min_pt location to which to write the minimum corner point
     * \param max_pt location to which to write the maximum corner point
     */
    bool
    clip_region_logical_bounds(vec2 *min_pt, vec2 *max_pt);

    /*!
     * Set clipping to the intersection of the current
     * clipping with a rectangle.
     * \param rect clip-in rectangle
     */
    void
    clip_in_rect(const Rect &rect);

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
     * Clipout by a rect
     * \param rect clip-out rectangle
     */
    void
    clip_out_rect(const Rect &rect);

    /*!
     * Set clipping to the intersection of the current
     * clipping with the complement of a rounded rectangle.
     * \param R rounded rectangle
     */
    void
    clip_out_rounded_rect(const RoundedRect &R);

    /*!
     * Clipout by a convex polygon
     * \param poly points of the convex polygon
     */
    void
    clip_out_convex_polygon(c_array<const vec2> poly);

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
    clip_out_custom(const reference_counted_ptr<PainterItemShader> &shader,
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
    clip_out_custom(const reference_counted_ptr<PainterItemShader> &shader,
                    const PainterData::value<PainterItemShaderData> &shader_data,
                    c_array<const c_array<const PainterAttribute> > attrib_chunks,
                    c_array<const c_array<const PainterIndex> > index_chunks,
                    c_array<const int> index_adjusts)
    {
      clip_out_custom(shader, shader_data,
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
    clip_out_custom(const reference_counted_ptr<PainterItemShader> &shader,
                    const PainterData::value<PainterItemShaderData> &shader_data,
                    c_array<const PainterAttribute> attrib_chunk,
                    c_array<const PainterIndex> index_chunk,
                    int index_adjust = 0)
    {
      vecN<c_array<const PainterAttribute>, 1> aa(attrib_chunk);
      vecN<c_array<const PainterIndex>, 1> ii(index_chunk);
      vecN<int, 1> ia(index_adjust);
      clip_out_custom(shader, shader_data, aa, ii, ia);
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
     *   shear(), scale(), rotate())
     * - clip state (see clip_in_rect(), clip_out_path(), clip_in_path())
     * - curve flatness requirement (see curve_flatness(float))
     * - blend shader (see blend_shader())
     * - blend mode (see blend_mode())
     * NOTE: it is an error (that is silently ignored) to end a
     * transparency layer (see begin_layer() and end_layer())
     * within a save()/restore() pair that was not started within
     * that same save()/restore() pair. In addition, it is also
     * an error to not end a layer within a save()/restore() pair
     * that was started in the that same save()/restore() pair.
     * The same rules apply to coverage buffers (see begin_coverage_buffer()
     * and end_coverage_buffer()).
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
     * Begin an FX layer. This marks first rendering into an
     * offscreen buffer and then blitting the buffer with the
     * passed FX applied. The buffer will be blitted with the
     * blend_shader(), and blend_mode() at the time of the call
     * to begin_layer(). All restore() commands called after a
     * begin_layer() must match a save() from after a begin_layer().
     * It is acceptable to layer any number of begin_layer() calls
     * as well.
     * \param effect FX to apply
     */
    void
    begin_layer(const reference_counted_ptr<PainterEffect> &effect);

    /*!
     * Begin an FX layer. This marks first rendering into an
     * offscreen buffer and then blitting the buffer with the
     * passed FX applied. The buffer will be blitted with the
     * blend_shader(), and blend_mode() at the time of the call
     * to begin_layer(). All restore() commands called after a
     * begin_layer() must match a save() from after a begin_layer().
     * It is acceptable to layer any number of begin_layer() calls
     * as well.
     * \param fx_passes FX passes to apply
     */
    void
    begin_layer(c_array<const reference_counted_ptr<PainterEffectPass> > fx_passes);

    /*!
     * Begin a transparency layer. This marks first
     * rendering into an offscreen buffer and then
     * blitting the buffer. The buffer will be blitted
     * with the blend_shader(), and blend_mode()
     * at the time of the call to begin_layer(). All
     * restore() commands called after a begin_layer()
     * must match a save() from after a begin_layer().
     * It is acceptable to layer any number of begin_layer()
     * calls as well.
     * \param color_modulate color value by which to modulate
     *                       the layer when it is to be blitted
     */
    void
    begin_layer(const vec4 &color_modulate);

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * begin_layer(vec4(1.0f, 1.0f, 1.0f, alpha));
     * \endcode
     * \param alpha alpha value for color modulation.
     */
    void
    begin_layer(float alpha)
    {
      begin_layer(vec4(1.0f, 1.0f, 1.0f, alpha));
    }

    /*!
     * End the current transparency layer and blit
     * the layer.
     */
    void
    end_layer(void);

    /*!
     * Begin a deferred coverage buffer layer. A deferred
     * coverage buffer is needed for those \ref PainterItemShader
     * values for which PainterItemShader::coverage_shader()
     * is non-null. When a coverage buffer is active, those
     * \ref PainterItemCoverageShader will render to the
     * current deferred coverage buffer and the \ref
     * PainterItemShader will read from it for its rendering.
     * This starts a coverage buffer layer region of the
     * size of the bounding box of the current clip-region.
     * It is strongly suggested to use begin_coverage_buffer(const Rect&)
     * to limit the size of the coverage buffer.
     */
    void
    begin_coverage_buffer(void);

    /*!
     * Acts the same as begin_coverage_buffer(void), but
     * limits the coverage buffer to the bounding box
     * of the intersection of the current clipping region
     * with the passed rectangle.
     * \param logical_rect rectangle in LOGICAL coordinates,
     *                     i.e. before transformation() is
     *                     applied.
     * \param additional_pixel_slack inflate the coverage buffer
     *                               region by this many pixels on
     *                               each side
     */
    void
    begin_coverage_buffer(const Rect &logical_rect,
                          float additional_pixel_slack = 0.0f);

    /*!
     * End the current coverage buffer.
     */
    void
    end_coverage_buffer(void);

    /*!
     * Return the default shaders for common drawing types.
     */
    const PainterShaderSet&
    default_shaders(void) const;

    /*!
     * Returns the default \ref GlyphRendererChooser
     * object that Painter uses.
     */
    const GlyphRendererChooser&
    default_glyph_renderer_chooser(void) const;

    /*!
     * Feed a logical pixel size and the current transformation
     * to default_glyph_renderer_chooser() to compute how to
     * render glyphs.
     * \param pixel_size size of text to render BEFORE
     *                   applying the transformation matrix.
     */
    GlyphRenderer
    compute_glyph_renderer(float pixel_size);

    /*!
     * Feed a logical pixel size and the current transformation
     * to a \ref GlyphRendererChooser to compute how to render
     * glyphs.
     * \param pixel_size size of text to render BEFORE
     *                   applying the transformation matrix.
     * \param chooser object that chooses how to render glyphs
     */
    GlyphRenderer
    compute_glyph_renderer(float pixel_size,
                           const GlyphRendererChooser &chooser);

    /*!
     * Draw glyphs from a \ref GlyphSequence.
     * \param shader \ref PainterGlyphShader to draw the glyphs
     * \param draw data for how to draw
     * \param glyph_sequence \ref GlyphSequence providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will use default_glyph_renderer_chooser()
     *                 to choose the renderer
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
                const GlyphSequence &glyph_sequence,
                GlyphRenderer renderer = GlyphRenderer(banded_rays_glyph));

    /*!
     * Draw glyphs from a \ref GlyphSequence.
     * \param draw data for how to draw
     * \param glyph_sequence \ref GlyphSequence providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will use default_glyph_renderer_chooser()
     *                 to choose the renderer
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterData &draw, const GlyphSequence &glyph_sequence,
                GlyphRenderer renderer = GlyphRenderer(banded_rays_glyph));

    /*!
     * Draw glyphs from a \ref GlyphSequence.
     * \code
     * draw_glyphs(PainterData(&brush), glyph_sequence, renderer);
     * \endcode
     * \param brush brush to apply to stroking
     * \param glyph_sequence \ref GlyphSequence providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will use default_glyph_renderer_chooser()
     *                 to choose the renderer
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterBrush &brush, const GlyphSequence &glyph_sequence,
                GlyphRenderer renderer = GlyphRenderer(banded_rays_glyph))
    {
      return draw_glyphs(PainterData(&brush), glyph_sequence, renderer);
    }

    /*!
     * Draw glyphs from a \ref GlyphSequence.
     * \param shader \ref PainterGlyphShader to draw the glyphs
     * \param draw data for how to draw
     * \param glyph_sequence \ref GlyphSequence providing glyphs
     * \param renderer_chooser \ref GlyphRendererChooser to use to choose how
     *                         to render the glyphs.
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
                const GlyphSequence &glyph_sequence,
                const GlyphRendererChooser &renderer_chooser);

    /*!
     * Draw glyphs from a \ref GlyphSequence.
     * and the data of the passed \ref GlyphSequence.
     * \param draw data for how to draw
     * \param glyph_sequence \ref GlyphSequence providing glyphs
     * \param renderer_chooser \ref GlyphRendererChooser to use to choose how
     *                         to render the glyphs.
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterData &draw, const GlyphSequence &glyph_sequence,
                const GlyphRendererChooser &renderer_chooser);

    /*!
     * Draw glyphs from a \ref GlyphSequence.
     * \code
     * draw_glyphs(PainterData(&brush), glyph_sequence, renderer_chooser);
     * \endcode
     * \param brush brush to apply to text
     * \param glyph_sequence \ref GlyphSequence providing glyphs
     * \param renderer_chooser \ref GlyphRendererChooser to use to choose how
     *                         to render the glyphs.
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterBrush &brush, const GlyphSequence &glyph_sequence,
                const GlyphRendererChooser &renderer_chooser)
    {
      return draw_glyphs(PainterData(&brush), glyph_sequence, renderer_chooser);
    }

    /*!
     * Draw glyphs from a \ref GlyphRun.
     * \param shader \ref PainterGlyphShader to draw the glyphs
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will use default_glyph_renderer_chooser()
     *                 to choose the renderer
     * \param begin first character of GlyphRun to draw
     * \param count number of characters, startng at begin, of the GlyphRun to draw
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
                const GlyphRun &glyph_run,
                unsigned int begin, unsigned int count,
                GlyphRenderer renderer = GlyphRenderer(banded_rays_glyph));

    /*!
     * Draw glyphs from a \ref GlyphRun.
     * and the data of the passed \ref GlyphRun.
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will use default_glyph_renderer_chooser()
     *                 to choose the renderer
     * \param begin first character of GlyphRun to draw
     * \param count number of characters, startng at begin, of the GlyphRun to draw
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterData &draw, const GlyphRun &glyph_run,
                unsigned int begin, unsigned int count,
                GlyphRenderer renderer = GlyphRenderer(banded_rays_glyph));

    /*!
     * Draw glyphs from a \ref GlyphRun.
     * and the data of the passed \ref GlyphRun.
     * \code
     * draw_glyphs(PainterData(&brush), glyph_run, begin, count, renderer);
     * \endcode
     * \param brush brush to apply to text
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will use default_glyph_renderer_chooser()
     *                 to choose the renderer
     * \param begin first character of GlyphRun to draw
     * \param count number of characters, startng at begin, of the GlyphRun to draw
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterBrush &brush, const GlyphRun &glyph_run,
                unsigned int begin, unsigned int count,
                GlyphRenderer renderer = GlyphRenderer(banded_rays_glyph))
    {
      return draw_glyphs(PainterData(&brush), glyph_run, begin, count, renderer);
    }

    /*!
     * Draw all glyphs from a \ref GlyphRun.
     * \param shader \ref PainterGlyphShader to draw the glyphs
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will use default_glyph_renderer_chooser()
     *                 to choose the renderer
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
                const GlyphRun &glyph_run,
                GlyphRenderer renderer = GlyphRenderer(banded_rays_glyph));

    /*!
     * Draw all glyphs from a \ref GlyphRun.
     * and the data of the passed \ref GlyphRun.
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will use default_glyph_renderer_chooser()
     *                 to choose the renderer
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterData &draw, const GlyphRun &glyph_run,
                GlyphRenderer renderer = GlyphRenderer(banded_rays_glyph));

    /*!
     * Draw glyphs from a \ref GlyphRun.
     * \code
     * draw_glyphs(PainterData(&brush), glyph_run, renderer);
     * \endcode
     * \param brush brush to apply to text
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer how to render the glyphs. If GlyphRenderer::valid() is false,
     *                 then the Painter will use default_glyph_renderer_chooser()
     *                 to choose the renderer
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterBrush &brush, const GlyphRun &glyph_run,
                GlyphRenderer renderer = GlyphRenderer(banded_rays_glyph))
    {
      return draw_glyphs(PainterData(&brush), glyph_run, renderer);
    }

    /*!
     * Draw glyphs from a \ref GlyphRun.
     * \param shader \ref PainterGlyphShader to draw the glyphs
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer_chooser \ref GlyphRendererChooser to use to choose how
     *                         to render the glyphs.
     * \param begin first character of GlyphRun to draw
     * \param count number of characters, startng at begin, of the GlyphRun to draw
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
                const GlyphRun &glyph_run,
                unsigned int begin, unsigned int count,
                const GlyphRendererChooser &renderer_chooser);

    /*!
     * Draw glyphs from a \ref GlyphRun.
     * and the data of the passed \ref GlyphRun.
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer_chooser \ref GlyphRendererChooser to use to choose how
     *                         to render the glyphs.
     * \param begin first character of GlyphRun to draw
     * \param count number of characters, startng at begin, of the GlyphRun to draw
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterData &draw, const GlyphRun &glyph_run,
                unsigned int begin, unsigned int count,
                const GlyphRendererChooser &renderer_chooser);
    /*!
     * Draw glyphs from a \ref GlyphRun.
     * \code
     * draw_glyphs(PainterData(&brush), glyph_run, begin, count, renderer_chooser);
     * \endcode
     * \param brush brush to apply to text
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param begin first character of GlyphRun to draw
     * \param count number of characters, startng at begin, of the GlyphRun to draw
     * \param renderer_chooser \ref GlyphRendererChooser to use to choose how
     *                         to render the glyphs.
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterBrush &brush, const GlyphRun &glyph_run,
                unsigned int begin, unsigned int count,
                const GlyphRendererChooser &renderer_chooser)
    {
      return draw_glyphs(PainterData(&brush), glyph_run, begin, count, renderer_chooser);
    }

    /*!
     * Draw all glyphs from a \ref GlyphRun.
     * \param shader \ref PainterGlyphShader to draw the glyphs
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer_chooser \ref GlyphRendererChooser to use to choose how
     *                         to render the glyphs.
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
                const GlyphRun &glyph_run,
                const GlyphRendererChooser &renderer_chooser);

    /*!
     * Draw all glyphs from a \ref GlyphRun.
     * and the data of the passed \ref GlyphRun.
     * \param draw data for how to draw
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer_chooser \ref GlyphRendererChooser to use to choose how
     *                         to render the glyphs.
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterData &draw, const GlyphRun &glyph_run,
                const GlyphRendererChooser &renderer_chooser);

    /*!
     * Draw glyphs from a \ref GlyphRun.
     * \code
     * draw_glyphs(PainterData(&brush), glyph_run, renderer);
     * \endcode
     * \param brush brush to apply to text
     * \param glyph_run \ref GlyphRun providing glyphs
     * \param renderer_chooser \ref GlyphRendererChooser to use to choose how
     *                         to render the glyphs.
     * \return Returns what \ref GlyphRenderer value used
     */
    GlyphRenderer
    draw_glyphs(const PainterBrush &brush, const GlyphRun &glyph_run,
                const GlyphRendererChooser &renderer_chooser)
    {
      return draw_glyphs(PainterData(&brush), glyph_run, renderer_chooser);
    }

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
     * \param shader_data data sent to stroking shader
     * \param selector object (see PainterStrokeShader::stroking_data_selector())
     *                 to use stroking parameters to help compute necessary thresh
     * \param[out] out_rounded_thresh location to which to write threshhold to be
     *                                used for rounded caps and joins of \ref
     *                                StrokedCapsJoins.
     */
    float
    compute_path_thresh(const Path &path,
                        const c_array<const generic_data> shader_data,
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
     * \param geometry_inflation amount path geometry is inflated, array
     *                           is indexed by the enumeration \ref
     *                           StrokingDataSelectorBase::path_geometry_inflation_index_t
     * \param[out] dst location to which to write the StrokedPath::Subset ID values
     * \returns the number of Subset object ID's written to dst, that
     *          number is guaranteed to be no more than StrokedPath::number_subsets().
     */
    unsigned int
    select_subsets(const StrokedPath &path,
                   c_array<const float> geometry_inflation,
                   c_array<unsigned int> dst);

    /*!
     * Calls StrokedCapsJoins::compute_chunks() passing arguments derived
     * from the current state of the Painter.
     * \param caps_joins \ref StrokedCapsJoins from which to compute chunk selection
     * \param geometry_inflation amount path geometry is inflated, array
     *                           is indexed by the enumeration \ref
     *                           StrokingDataSelectorBase::path_geometry_inflation_index_t
     * \param js join style
     * \param cp cap style
     * \param[out] dst location to which to write what chunks
     */
    void
    select_chunks(const StrokedCapsJoins &caps_joins,
                  c_array<const float> geometry_inflation,
                  enum join_style js,
                  enum fastuidraw::Painter::cap_style cp,
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
     * \param apply_shader_anti_aliasing if true, stroke with shader-based anti-aliasing
     */
    void
    stroke_path(const PainterStrokeShader &shader, const PainterData &draw,
                const StrokedPath &path, float rounded_thresh,
                const StrokingStyle &stroke_style = StrokingStyle(),
                bool apply_shader_anti_aliasing = true);

    /*!
     * Stroke a path.
     * \param shader shader with which to stroke the attribute data
     * \param draw data for how to draw
     * \param path Path to stroke
     * \param stroke_style how to stroke the path
     * \param apply_shader_anti_aliasing if true, stroke with shader-based anti-aliasing
     * \param stroking_method stroking method to select what \ref StrokedPath to use
     */
    void
    stroke_path(const PainterStrokeShader &shader, const PainterData &draw, const Path &path,
                const StrokingStyle &stroke_style = StrokingStyle(),
                bool apply_shader_anti_aliasing = true,
                enum stroking_method_t stroking_method = stroking_method_fastest);

    /*!
     * Stroke a path using PainterShaderSet::stroke_shader() of default_shaders().
     * \param draw data for how to draw
     * \param path Path to stroke
     * \param stroke_style how to stroke the path
     * \param apply_shader_anti_aliasing if true, stroke with shader-based anti-aliasing
     * \param stroking_method stroking method to select what \ref StrokedPath to use
     */
    void
    stroke_path(const PainterData &draw, const Path &path,
                const StrokingStyle &stroke_style = StrokingStyle(),
                bool apply_shader_anti_aliasing = true,
                enum stroking_method_t stroking_method = stroking_method_fastest);

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * stroke_path(PainterData(&brush, &stroking_params), path,
     *             stroke_style, apply_shader_anti_aliasing,
     *             stroking_method);
     * \endcode
     * \param brush brush to apply to stroking
     * \param stroking_params stroking parameters to apply to stroking
     * \param path Path to stroke
     * \param stroke_style how to stroke the path
     * \param apply_shader_anti_aliasing if true, stroke with shader-based anti-aliasing
     * \param stroking_method stroking method to select what \ref StrokedPath to use
     */
    void
    stroke_path(const PainterBrush &brush,
                const PainterStrokeParams &stroking_params,
                const Path &path, const StrokingStyle &stroke_style = StrokingStyle(),
                bool apply_shader_anti_aliasing = true,
                enum stroking_method_t stroking_method = stroking_method_fastest)
    {
      stroke_path(PainterData(&brush, &stroking_params), path,
                  stroke_style, apply_shader_anti_aliasing,
                  stroking_method);
    }

    /*!
     * Stroke a path dashed.
     * \param shader shader with which to draw
     * \param draw data for how to draw
     * \param path StrokedPath to stroke
     * \param rounded_thresh value to feed to StrokedCapsJoins::rounded_joins()
     *                       and/or StrokedCapsJoins::rounded_caps() if rounded
     *                       joins and/or rounded caps are requested
     * \param stroke_style how to stroke the path
     * \param apply_shader_anti_aliasing if true, stroke with shader-based anti-aliasing
     */
    void
    stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw,
                       const StrokedPath &path, float rounded_thresh,
                       const StrokingStyle &stroke_style = StrokingStyle(),
                       bool apply_shader_anti_aliasing = true);

    /*!
     * Stroke a path dashed.
     * \param shader shader with which to draw
     * \param draw data for how to draw
     * \param path Path to stroke
     * \param stroke_style how to stroke the path
     * \param apply_shader_anti_aliasing if true, stroke with shader-based anti-aliasing
     * \param stroking_method stroking method to select what \ref StrokedPath to use
     */
    void
    stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw, const Path &path,
                       const StrokingStyle &stroke_style = StrokingStyle(),
                       bool apply_shader_anti_aliasing = true,
                       enum stroking_method_t stroking_method = stroking_method_fastest);

    /*!
     * Stroke a path using PainterShaderSet::dashed_stroke_shader() of default_shaders().
     * \param draw data for how to draw
     * \param path Path to stroke
     * \param stroke_style how to stroke the path
     * \param apply_shader_anti_aliasing if true, stroke with shader-based anti-aliasing
     * \param stroking_method stroking method to select what \ref StrokedPath to use
     */
    void
    stroke_dashed_path(const PainterData &draw, const Path &path,
                       const StrokingStyle &stroke_style = StrokingStyle(),
                       bool apply_shader_anti_aliasing = true,
                       enum stroking_method_t stroking_method = stroking_method_fastest);

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * stroke_dashed_path(PainterData(&brush, &stroking_params), path,
     *                    stroke_style, apply_shader_anti_aliasing,
     *                    stroking_method);
     * \endcode
     * \param brush brush to apply to stroking
     * \param stroking_params stroking parameters to apply to stroking
     * \param path Path to stroke
     * \param stroke_style how to stroke the path
     * \param apply_shader_anti_aliasing if true, stroke with shader-based anti-aliasing
     * \param stroking_method stroking method to select what \ref StrokedPath to use
     */
    void
    stroke_dashed_path(const PainterBrush &brush,
                       const PainterDashedStrokeParams &stroking_params,
                       const Path &path, const StrokingStyle &stroke_style = StrokingStyle(),
                       bool apply_shader_anti_aliasing = true,
                       enum stroking_method_t stroking_method = stroking_method_fastest)
    {
      stroke_dashed_path(PainterData(&brush, &stroking_params), path,
                         stroke_style, apply_shader_anti_aliasing,
                         stroking_method);
    }

    /*!
     * Fill a path.
     * \param shader shader with which to fill the attribute data
     * \param draw data for how to draw
     * \param data attribute and index data with which to fill a path
     * \param fill_rule fill rule with which to fill the path
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const FilledPath &data, enum fill_rule_t fill_rule,
              bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a path.
     * \param shader shader with which to fill the attribute data
     * \param draw data for how to draw
     * \param path to fill
     * \param fill_rule fill rule with which to fill the path
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const Path &path, enum fill_rule_t fill_rule,
              bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a path.
     * \param shader shader with which to fill the attribute data
     * \param draw data for how to draw
     * \param data attribute and index data with which to fill a path
     * \param fill_rule custom fill rule with which to fill the path
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const FilledPath &data, const CustomFillRuleBase &fill_rule,
              bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a path.
     * \param shader shader with which to fill the attribute data
     * \param draw data for how to draw
     * \param path to fill
     * \param fill_rule custom fill rule with which to fill the path
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const Path &path, const CustomFillRuleBase &fill_rule,
              bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a path via \ref ShaderFilledPath
     * \param shader shader with which to draw the \ref ShaderFilledPath
     * \param draw data for how to draw
     * \param path \ref ShaderFilledPath to fill
     * \param fill_rule fill rule to apply to fill
     */
    void
    fill_path(const PainterGlyphShader &shader, const PainterData &draw,
              const ShaderFilledPath &path, enum fill_rule_t fill_rule);

    /*!
     * Fill a path using the default shader to draw the fill.
     * \param draw data for how to draw
     * \param path path to fill
     * \param fill_rule fill rule with which to fill the path
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_path(const PainterData &draw, const Path &path, enum fill_rule_t fill_rule,
              bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a path using the default shader to draw the fill.
     * \param draw data for how to draw
     * \param path path to fill
     * \param fill_rule custom fill rule with which to fill the path
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_path(const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
              bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a path via \ref ShaderFilledPath using the default shader
     * to draw the fill.
     * \param draw data for how to draw
     * \param path \ref ShaderFilledPath to fill
     * \param fill_rule fill rule to apply to fill
     */
    void
    fill_path(const PainterData &draw, const ShaderFilledPath &path,
              enum fill_rule_t fill_rule);

    /*!
     * Fill a path using the default shader to draw the fill.
     * Provided as a conveniance, equivalent to
     * \code
     * fill_path(PainterData(&brush), path, fill_rule, apply_shader_anti_aliasing);
     * \endcode
     * \param brush \ref PainterBrush to apply to fill
     * \param path path to fill
     * \param fill_rule fill rule with which to fill the path
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_path(const PainterBrush &brush, const Path &path, enum fill_rule_t fill_rule,
              bool apply_shader_anti_aliasing = true)
    {
      fill_path(PainterData(&brush), path, fill_rule, apply_shader_anti_aliasing);
    }

    /*!
     * Fill a path using the default shader to draw the fill.
     * \code
     * fill_path(PainterData(&brush), path, fill_rule, apply_shader_anti_aliasing);
     * \endcode
     * \param brush \ref PainterBrush to apply to fill
     * \param path path to fill
     * \param fill_rule custom fill rule with which to fill the path
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_path(const PainterBrush &brush, const Path &path, const CustomFillRuleBase &fill_rule,
              bool apply_shader_anti_aliasing = true)
    {
      fill_path(PainterData(&brush), path, fill_rule, apply_shader_anti_aliasing);
    }

    /*!
     * Fill a convex polygon using a custom shader.
     * \param shader shader with which to draw the convex polygon
     * \param draw data for how to draw
     * \param pts points of the polygon so that neighboring points (modulo pts.size())
     *            are the edges of the polygon.
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_convex_polygon(const PainterFillShader &shader, const PainterData &draw,
                        c_array<const vec2> pts,
                        bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a convex polygon using the default fill shader.
     * \param draw data for how to draw
     * \param pts points of the polygon so that neighboring points (modulo pts.size())
     *            are the edges of the polygon.
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_convex_polygon(const PainterData &draw, c_array<const vec2> pts,
                        bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a convex polygon using the default fill shader.
     * \code
     * fill_convex_polygon(PainterData(&brush), pts, apply_shader_anti_aliasing);
     * \endcode
     * \param brush \ref PainterBrush to apply to fill
     * \param pts points of the polygon so that neighboring points (modulo pts.size())
     *            are the edges of the polygon.
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_convex_polygon(const PainterBrush &brush, c_array<const vec2> pts,
                        bool apply_shader_anti_aliasing = true)
    {
      fill_convex_polygon(PainterData(&brush), pts, apply_shader_anti_aliasing);
    }

    /*!
     * Fill a rect using a custom shader.
     * \param shader shader with which to draw the rect
     * \param draw data for how to draw
     * \param rect rectangle to fill
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_rect(const PainterFillShader &shader, const PainterData &draw,
              const Rect &rect,
              bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a rect using the default fill shader.
     * \param draw data for how to draw
     * \param rect rectangle to fill
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_rect(const PainterData &draw, const Rect &rect,
              bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a rect using the default fill shader.
     * \code
     * fill_convex_polygon(PainterData(&brush), rect, apply_shader_anti_aliasing);
     * \endcode
     * \param brush \ref PainterBrush to apply to fill
     * \param rect rectangle to fill
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_rect(const PainterBrush &brush, const Rect &rect,
              bool apply_shader_anti_aliasing = true)
    {
      fill_rect(PainterData(&brush), rect, apply_shader_anti_aliasing);
    }

    /*!
     * Fill a rounded rect using a fill shader
     * \param shader shader with which to draw the rounded rectangle
     * \param draw data for how to draw
     * \param R \ref RoundedRect to draw
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_rounded_rect(const PainterFillShader &shader, const PainterData &draw,
                      const RoundedRect &R,
                      bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a rounded rect using the default fill shader
     * \param draw data for how to draw
     * \param R \ref RoundedRect to draw
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_rounded_rect(const PainterData &draw, const RoundedRect &R,
                      bool apply_shader_anti_aliasing = true);

    /*!
     * Fill a rounded rect using the default fill shader
     * \code
     * fill_rounded_rect(PainterData(&brush), R, apply_shader_anti_aliasing);
     * \endcode
     * \param brush brush to apply to filling
     * \param R \ref RoundedRect to draw
     * \param apply_shader_anti_aliasing if true, fill with shader based anti-aliasing
     */
    void
    fill_rounded_rect(const PainterBrush &brush, const RoundedRect &R,
                      bool apply_shader_anti_aliasing = true)
    {
      fill_rounded_rect(PainterData(&brush), R, apply_shader_anti_aliasing);
    }

    /*!
     * Draw generic attribute data.
     * \param shader shader with which to draw data
     * \param draw data for how to draw
     * \param attrib_chunk attribute data to draw
     * \param index_chunk index data into attrib_chunk
     * \param index_adjust amount by which to adjust the values in index_chunk
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 c_array<const PainterAttribute> attrib_chunk,
                 c_array<const PainterIndex> index_chunk,
                 int index_adjust = 0)
    {
      vecN<c_array<const PainterAttribute>, 1> aa(attrib_chunk);
      vecN<c_array<const PainterIndex>, 1> ii(index_chunk);
      vecN<int, 1> ia(index_adjust);
      draw_generic(shader, draw, aa, ii, ia);
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
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 c_array<const c_array<const PainterAttribute> > attrib_chunks,
                 c_array<const c_array<const PainterIndex> > index_chunks,
                 c_array<const int> index_adjusts);

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
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 c_array<const c_array<const PainterAttribute> > attrib_chunks,
                 c_array<const c_array<const PainterIndex> > index_chunks,
                 c_array<const int> index_adjusts,
                 c_array<const unsigned int> attrib_chunk_selector);

    /*!
     * Draw generic attribute data
     * \param shader shader with which to draw data
     * \param draw data for how to draw
     * \param src generator of attribute and index data
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 const PainterAttributeWriter &src);

    /*!
     * Draw generic attribute data where the item self occludes
     * \param shader shader with which to draw data
     * \param draw data for how to draw
     * \param attrib_chunk attribute data to draw
     * \param index_chunk index data into attrib_chunk
     * \param index_adjust amount by which to adjust the values in index_chunk
     * \param z_range z-range of item's attribute data
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 c_array<const PainterAttribute> attrib_chunk,
                 c_array<const PainterIndex> index_chunk,
                 range_type<int> z_range,
                 int index_adjust = 0);

    /*!
     * Draw generic attribute data where the item self occludes
     * \param shader shader with which to draw data
     * \param draw data for how to draw
     * \param attrib_chunks attribute data to draw
     * \param index_chunks index data into attrib_chunk
     * \param z_ranges z-ranges of item's attribute data
     * \param index_adjusts if non-empty, the i'th element is the value by which
     *                      to adjust all of index_chunks[i]; if empty the index
     *                      values are not adjusted.
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 c_array<const c_array<const PainterAttribute> > attrib_chunks,
                 c_array<const c_array<const PainterIndex> > index_chunks,
                 c_array<const range_type<int> > z_ranges,
                 c_array<const int> index_adjusts);

    /*!
     * Draw generic attribute data
     * \param shader shader with which to draw data
     * \param draw data for how to draw
     * \param src generator of attribute and index data
     * \param z_range z-range of item's attribute data
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 const PainterAttributeWriter &src,
                 range_type<int> z_range);

    /*!
     * Queue an action that uses (or affects) the GPU. Through these actions,
     * one can mix FastUIDraw::Painter with native API calls on a surface.
     * However, adding an action induces a draw-break (and state restore)
     * after each such action. Also, the action is not called until end()
     * is called.
     * \param action action to execute within a draw-stream.
     */
    void
    queue_action(const reference_counted_ptr<const PainterDrawBreakAction> &action);

    /*!
     * Returns a stat on how much data the Packer has
     * handled in the last begin()/end() pair. Calling
     * query_stat() within a begin()/end() pair gives
     * unreliable results.
     * \param st stat to query
     */
    unsigned int
    query_stat(enum query_stats_t st) const;

    /*!
     * Write into a c_array<> all the stats from the
     * last begin()/end() pair. The number of stats
     * can be fetched with number_stats(). The values
     * written into are indexed by \ref query_stats_t.
     * Calling query_stats() within a begin()/end()
     * pair gives unreliable results.
     */
    void
    query_stats(c_array<unsigned int> dst) const;

    /*!
     * Returns the number of stats the Painter type
     * supports.
     */
    static
    unsigned int
    number_stats(void);

  private:

    void *m_d;
  };
/*! @} */
}
