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
#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/painter/stroked_path.hpp>
#include <fastuidraw/painter/filled_path.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>
#include <fastuidraw/painter/painter_data.hpp>
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
     - applying a brush (see PainterBrush)
     - single 3x3 transformation
     - save and restore state
     - clipIn against Path or rectangle
     - clipOut against Path

    The transformation of a Painter goes from local item coordinate
    to 3D API clip-coordinates (for example in GL, from item coordinates
    to gl_Position.xyw). FastUIDraw follows the convention that
    the top of the window is at normalized y-coordinate -1 and the
    bottom of the window is at normalized y-coordinate +1.

    One can specify the exact attribute and index data for a Painter
    to consume, see \ref draw_generic(). In addition, the class
    PainterAttributeData can be used to generate and save attribute and
    index data to be used repeatedly.
   */
  class Painter:public reference_counted<Painter>::default_base
  {
  public:
    /*!
      Base class to specify a custom fill rule.
     */
    class CustomFillRuleBase
    {
    public:
      /*!
        To be implemented by a derived class to return
        true if to draw those regions with the passed
        winding number.
        \param winding_number winding number value to test.
       */
      virtual
      bool
      operator()(int winding_number) const = 0;
    };

    /*!
      Class to specify a custom fill rule from
      a function.
     */
    class CustomFillRuleFunction:public CustomFillRuleBase
    {
    public:
      /*!
        Ctor.
        \param fill_rule function to use to implement
                         operator(int) const.
       */
      CustomFillRuleFunction(bool (*fill_rule)(int)):
        m_fill_rule(fill_rule)
      {}

      virtual
      bool
      operator()(int winding_number) const
      {
        return m_fill_rule && m_fill_rule(winding_number);
      }

    private:
      bool (*m_fill_rule)(int);
    };

    /*!
      Ctor.
     */
    explicit
    Painter(reference_counted_ptr<PainterBackend> backend);

    ~Painter(void);

    /*!
      Returns a handle to the GlyphAtlas of this
      Painter. All glyphs used by this
      Painter must live on glyph_atlas().
     */
    const reference_counted_ptr<GlyphAtlas>&
    glyph_atlas(void) const;

    /*!
      Returns a handle to the ImageAtlas of this
      Painter. All images used by all brushes of
      this Painter must live on image_atlas().
     */
    const reference_counted_ptr<ImageAtlas>&
    image_atlas(void) const;

    /*!
      Returns a handle to the ColorStopAtlas of this
      Painter. All color stops used by all brushes
      of this Painter must live on colorstop_atlas().
     */
    const reference_counted_ptr<ColorStopAtlas>&
    colorstop_atlas(void) const;

    /*!
      Returns the PainterPackedValuePool used to construct
      PainterPackedValue objects.
     */
    PainterPackedValuePool&
    packed_value_pool(void);

    /*!
      Returns the active blend shader
     */
    const reference_counted_ptr<PainterBlendShader>&
    blend_shader(void) const;

    /*!
      Returns the active 3D API blend mode
     */
    BlendMode::packed_value
    blend_mode(void) const;

    /*!
      Sets the blend shader. It is a crashing error for
      h to be NULL.
      \param h blend shader to use for blending.
      \param packed_blend_mode 3D API blend mode packed by BlendMode::packed()
     */
    void
    blend_shader(const reference_counted_ptr<PainterBlendShader> &h,
                 BlendMode::packed_value packed_blend_mode);

    /*!
      Equivalent to
      \code
      blend_shader(shader_set.shader(m),
                   shader_set.blend_mode(m))
      \endcode
      It is a crashing error if shader_set does not support
      the named blend mode.
      \param shader_set PainterBlendShaderSet from which to take blend shader
      \param m Blend mode to use
     */
    void
    blend_shader(const PainterBlendShaderSet &shader_set,
                 enum PainterEnums::blend_mode_t m)
    {
      blend_shader(shader_set.shader(m), shader_set.blend_mode(m));
    }

    /*!
      Equivalent to
      \code
      blend_shader(default_shaders().blend_shaders(), m)
      \endcode
      It is a crashing error if default_shaders() does not support
      the named blend mode.
      \param m Blend mode to use
     */
    void
    blend_shader(enum PainterEnums::blend_mode_t m)
    {
      blend_shader(default_shaders().blend_shaders(), m);
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
      sent to hardware until end() is called.
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
    transformation(const PainterItemMatrix &m)
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
      Returns the value of the current transformation.
     */
    const PainterItemMatrix&
    transformation(void);

    /*!
      Returns a handle to current state of the 3x3
      transformation that can be re-used by passing it to
      transformation_state(const PainterPackedValue<PainterItemMatrix>&)
     */
    const PainterPackedValue<PainterItemMatrix>&
    transformation_state(void);

    /*!
      Set the transformation state from a transformation state handle.
      \param h handle to transformation state.
     */
    void
    transformation_state(const PainterPackedValue<PainterItemMatrix> &h);

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
      Clip-out by a path, i.e. set the clipping to be
      the intersection of the current clipping against
      the -complement- of the fill of a path.
      \param path path by which to clip out
      \param fill_rule custom fill rule to apply to path
     */
    void
    clipOutPath(const Path &path, const CustomFillRuleBase &fill_rule);

    /*!
      Clip-in by a path, i.e. set the clipping to be
      the intersection of the current clipping against
      the the fill of a path.
      \param path path by which to clip out
      \param fill_rule custom fill rule to apply to path
     */
    void
    clipInPath(const Path &path, const CustomFillRuleBase &fill_rule);

    /*!
      Set the curve flatness requirement for TessellatedPath
      and StrokedPath selection when stroking or filling paths
      when passing to drawing methods a Path object. The value
      represents the distance, in pixels, requested for between
      the approximated curve (realized in TessellatedPath) and
      the true curve (realized in Path). This value is combined
      with a value derived from the current transformation matrix
      to pass to Path::tessellation_lod() to fetch a
      TessellatedPath.
     */
    void
    curveFlatness(float thresh);

    /*!
      Returns the value set by curveFlatness(float).
     */
    float
    curveFlatness(void);

    /*!
      Save the current state of this Painter onto the save state stack.
      The state is restored (and the stack popped) by called restore().
      The state saved is:
      - transformation state (see concat(), transformation(), translate(),
        shear(), scale(), rotate()).
      - clip state (see clipInRect(), clipOutPath(), clipInPath())
      - curve flatness requirement (see curveFlatness(float))
      - blend shader (see blend_shader()).
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
      Draw glyphs.
      \param draw data for how to draw
      \param data attribute and index data with which to draw the glyphs.
      \param shader with which to draw the glyphs
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_glyphs(const PainterGlyphShader &shader, const PainterData &draw,
                const PainterAttributeData &data,
                const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Draw glyphs.
      \param draw data for how to draw
      \param data attribute and index data with which to draw the glyphs
      \param use_anistopic_antialias if true, use default_shaders().glyph_shader_anisotropic()
                                     otherwise use default_shaders().glyph_shader()
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_glyphs(const PainterData &draw,
                const PainterAttributeData &data, bool use_anistopic_antialias = false,
                const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Stroke a path.
      \param shader shader with which to stroke the attribute data
      \param draw data for how to draw
      \param edge_data attribute and index data for drawing the edges,
                       NULL value indicates to not draw edges.
      \param edge_chunks which chunks to take from edge_data
      \param inc_edge amount by which to increment current_z() for the edge drawing
      \param cap_data attribute and index data for drawing the caps,
                      NULL value indicates to not draw caps.
      \param cap_chunk which chunk to take from cap_data
      \param join_data attribute and index data for drawing the joins,
                       NULL value indicates to not draw joins.
      \param join_chunks which chunks to take from join_data to draw the joins
      \param inc_join amount by which to increment current_z() for the join drawing
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    stroke_path(const PainterStrokeShader &shader, const PainterData &draw,
                const PainterAttributeData *edge_data, const_c_array<unsigned int> edge_chunks,
                unsigned int inc_edge,
                const PainterAttributeData *cap_data, unsigned int cap_chunk,
                const PainterAttributeData *join_data, const_c_array<unsigned int> join_chunks,
                unsigned int inc_join, bool with_anti_aliasing,
                const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Stroke a path.
      \param shader shader with which to stroke the attribute data
      \param draw data for how to draw
      \param path StrokedPath to stroke
      \param thresh threshold value to feed the StrokingDataSelectorBase of the shader
      \param close_contours if true, draw the closing edges (and joins) of each contour
                            of the path
      \param cp cap style
      \param js join style
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    stroke_path(const PainterStrokeShader &shader, const PainterData &draw,
                const StrokedPath &path, float thresh,
                bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                bool with_anti_aliasing,
                const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Stroke a path.
      \param shader shader with which to stroke the attribute data
      \param draw data for how to draw
      \param path Path to stroke
      \param close_contours if true, draw the closing edges (and joins) of each contour
                            of the path
      \param cp cap style
      \param js join style
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    stroke_path(const PainterStrokeShader &shader, const PainterData &draw, const Path &path,
                bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                bool with_anti_aliasing,
                const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Stroke a path using PainterShaderSet::stroke_shader() of default_shaders().
      \param draw data for how to draw
      \param path Path to stroke
      \param close_contours if true, draw the closing edges (and joins) of each contour
                            of the path
      \param cp cap style
      \param js join style
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    stroke_path(const PainterData &draw, const Path &path,
                bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                bool with_anti_aliasing,
                const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Stroke a path using PainterShaderSet::pixel_width_stroke_shader()
      of default_shaders().
      \param draw data for how to draw
      \param path Path to stroke
      \param close_contours if true, draw the closing edges (and joins) of each contour
                            of the path
      \param cp cap style
      \param js join style
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    stroke_path_pixel_width(const PainterData &draw, const Path &path,
                            bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                            bool with_anti_aliasing,
                            const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Stroke a path dashed.
      \param shader shader with which to draw
      \param draw data for how to draw
      \param edge_data attribute and index data for drawing the edges,
                       NULL value indicates to not draw edges.
      \param edge_chunks which chunk to take from edge_data
      \param inc_edge amount by which to increment current_z() for the edge drawing
      \param cap_data attribute and index data for drawing the caps,
                      NULL value indicates to not draw caps.
      \param cap_chunk which chunk to take from cap_data
      \param include_joins_from_closing_edge if false, disclude the joins formed
                                             from the closing edges of each contour
      \param dash_evaluator DashEvaluatorBase object to determine which joins
                            are to be drawn
      \param join_data attribute and index data for drawing the joins,
                       NULL value indicates to not draw joins.
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    stroke_dashed_path(const PainterStrokeShader &shader, const PainterData &draw,
                       const PainterAttributeData *edge_data, const_c_array<unsigned int> edge_chunks,
                       unsigned int inc_edge,
                       const PainterAttributeData *cap_data, unsigned int cap_chunk,
                       bool include_joins_from_closing_edge,
                       const DashEvaluatorBase *dash_evaluator, const PainterAttributeData *join_data,
                       bool with_anti_aliasing,
                       const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Stroke a path dashed.
      \param shader shader with which to draw
      \param draw data for how to draw
      \param path StrokedPath to stroke
      \param thresh threshold value to feed the StrokingDataSelectorBase of the shader
      \param close_contours if true, draw the closing edges (and joins) of each contour
                            of the path
      \param cp cap style
      \param js join style
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw,
                       const StrokedPath &path, float thresh,
                       bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                       bool with_anti_aliasing,
                       const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Stroke a path dashed.
      \param shader shader with which to draw
      \param draw data for how to draw
      \param path Path to stroke
      \param close_contours if true, draw the closing edges (and joins) of each contour
                            of the path
      \param cp cap style
      \param js join style
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    stroke_dashed_path(const PainterDashedStrokeShaderSet &shader, const PainterData &draw, const Path &path,
                       bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                       bool with_anti_aliasing,
                       const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Stroke a path using PainterShaderSet::dashed_stroke_shader() of default_shaders().
      \param draw data for how to draw
      \param path Path to stroke
      \param close_contours if true, draw the closing edges (and joins) of each contour
                            of the path
      \param cp cap style
      \param js join style
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    stroke_dashed_path(const PainterData &draw, const Path &path,
                       bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                       bool with_anti_aliasing,
                       const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Stroke a path using PainterShaderSet::pixel_width_dashed_stroke_shader() of default_shaders().
      \param draw data for how to draw
      \param path Path to stroke
      \param close_contours if true, draw the closing edges (and joins) of each contour
                            of the path
      \param cp cap style
      \param js join style
      \param with_anti_aliasing if true, draw a second pass to give sub-pixel anti-aliasing
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    stroke_dashed_path_pixel_width(const PainterData &draw, const Path &path,
                                   bool close_contours, enum PainterEnums::cap_style cp, enum PainterEnums::join_style js,
                                   bool with_anti_aliasing,
                                   const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Fill a path.
      \param shader shader with which to fill the attribute data
      \param draw data for how to draw
      \param data attribute and index data with which to fill a path
      \param fill_rule fill rule with which to fill the path
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const PainterAttributeData &data, enum PainterEnums::fill_rule_t fill_rule,
              const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Fill a path.
      \param shader shader with which to fill the attribute data
      \param draw data for how to draw
      \param path to fill
      \param fill_rule fill rule with which to fill the path
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const Path &path, enum PainterEnums::fill_rule_t fill_rule,
              const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Fill a path using the default shader to draw the fill.
      \param draw data for how to draw
      \param path path to fill
      \param fill_rule fill rule with which to fill the path
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    fill_path(const PainterData &draw, const Path &path, enum PainterEnums::fill_rule_t fill_rule,
              const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Fill a path.
      \param shader shader with which to fill the attribute data
      \param draw data for how to draw
      \param data attribute and index data with which to fill a path
      \param fill_rule custom fill rule with which to fill the path
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const PainterAttributeData &data, const CustomFillRuleBase &fill_rule,
              const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Fill a path.
      \param shader shader with which to fill the attribute data
      \param draw data for how to draw
      \param path to fill
      \param fill_rule custom fill rule with which to fill the path
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    fill_path(const PainterFillShader &shader, const PainterData &draw,
              const Path &path, const CustomFillRuleBase &fill_rule,
              const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Fill a path using the default shader to draw the fill.
      \param draw data for how to draw
      \param path path to fill
      \param fill_rule custom fill rule with which to fill the path
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    fill_path(const PainterData &draw, const Path &path, const CustomFillRuleBase &fill_rule,
              const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Draw a convex polygon using a custom shader.
      \param draw data for how to draw
      \param pts points of the polygon so that neighboring points (modulo pts.size())
                 are the edges of the polygon.
      \param shader shader with which to draw the convex polygon. The shader must
                    accept the exact same format as packed by
                    PainterAttributeDataFillerPathFill
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_convex_polygon(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
                        const_c_array<vec2> pts,
                        const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Draw a convex polygon using the default fill shader.
      \param draw data for how to draw
      \param pts points of the polygon so that neighboring points (modulo pts.size())
                 are the edges of the polygon.
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_convex_polygon(const PainterData &draw, const_c_array<vec2> pts,
                        const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Draw a quad using a custom shader.
      \param draw data for how to draw
      \param p0 first point of quad, shares an edge with p3
      \param p1 point after p0, shares an edge with p0
      \param p2 point after p1, shares an edge with p1
      \param p3 point after p2, shares an edge with p2
      \param shader shader with which to draw the convex polygon. The shader must
                    accept the exact same format as packed by
                    PainterAttributeDataFillerPathFill
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_quad(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
              const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3,
              const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Draw a quad using the default fill shader.
      \param draw data for how to draw
      \param p0 first point of quad, shares an edge with p3
      \param p1 point after p0, shares an edge with p0
      \param p2 point after p1, shares an edge with p1
      \param p3 point after p2, shares an edge with p2
      \param call_back handle to PainterPacker::DataCallBack for the draw
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_quad(const PainterData &draw,
              const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3,
              const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Draw a rect using a custom shader.
      \param draw data for how to draw
      \param p min-corner of rect
      \param wh width and height of rect
      \param shader shader with which to draw the convex polygon. The shader must
                    accept the exact same format as packed by
                    PainterAttributeDataFillerPathFill
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_rect(const reference_counted_ptr<PainterItemShader> &shader, const PainterData &draw,
              const vec2 &p, const vec2 &wh,
              const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Draw a rect using the default fill shader.
      \param draw data for how to draw
      \param p min-corner of rect
      \param wh width and height of rect
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_rect(const PainterData &draw, const vec2 &p, const vec2 &wh,
              const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Draw generic attribute data.
      \param draw data for how to draw
      \param attrib_chunk attribute data to draw
      \param index_chunk indx data into attrib_chunk
      \param shader shader with which to draw data
      \param call_back handle to PainterPacker::DataCallBack for the draw
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 const_c_array<PainterAttribute> attrib_chunk,
                 const_c_array<PainterIndex> index_chunk,
                 int index_adjust,
                 const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>())
    {
      vecN<const_c_array<PainterAttribute>, 1> aa(attrib_chunk);
      vecN<const_c_array<PainterIndex>, 1> ii(index_chunk);
      vecN<int, 1> ia(index_adjust);
      draw_generic(shader, draw, aa, ii, ia, call_back);
    }

    /*!
      Draw generic attribute data.
      \param draw data for how to draw
      \param attrib_chunks attribute data to draw
      \param index_chunks the i'th element is index data into attrib_chunks[i]
      \param shader shader with which to draw data
      \param call_back handle to PainterPacker::DataCallBack for the draw
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
                 const_c_array<const_c_array<PainterIndex> > index_chunks,
                 const_c_array<int> index_adjusts,
                 const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Draw generic attribute data
      \param draw data for how to draw
      \param attrib_chunks attribute data to draw
      \param index_chunks the i'th element is index data into attrib_chunks[K]
                          where K = attrib_chunk_selector[i]
      \param attrib_chunk_selector selects which attribute chunk to use for
             each index chunk
      \param shader shader with which to draw data
      \param call_back if non-NULL handle, call back called when attribute data
                       is added.
     */
    void
    draw_generic(const reference_counted_ptr<PainterItemShader> &shader,
                 const PainterData &draw,
                 const_c_array<const_c_array<PainterAttribute> > attrib_chunks,
                 const_c_array<const_c_array<PainterIndex> > index_chunks,
                 const_c_array<int> index_adjusts,
                 const_c_array<unsigned int> attrib_chunk_selector,
                 const reference_counted_ptr<PainterPacker::DataCallBack> &call_back = reference_counted_ptr<PainterPacker::DataCallBack>());

    /*!
      Returns a stat on how much data the Packer has
      handled since the last call to begin().
      \param st stat to query
     */
    unsigned int
    query_stat(enum PainterPacker::stats_t st) const;

    /*!
      Return the z-depth value that the next item will have.
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
      Registers a fill shader for use. Must not be called within
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

  private:
    void *m_d;
  };
/*! @} */
}
