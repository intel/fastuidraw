/*!
 * \file painter_backend.hpp
 * \brief file painter_backend.hpp
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

#include <fastuidraw/util/blend_mode.hpp>
#include <fastuidraw/text/glyph_atlas.hpp>
#include <fastuidraw/image.hpp>
#include <fastuidraw/colorstop_atlas.hpp>
#include <fastuidraw/painter/packing/painter_draw.hpp>
#include <fastuidraw/painter/packing/painter_shader_registrar.hpp>


namespace fastuidraw
{
/*!\addtogroup PainterPacking
 * @{
 */

  /*!
   * \brief
   * A PainterBackend is an interface that defines the API-specific
   * elements to implement Painter:
   */
  class PainterBackend:public reference_counted<PainterBackend>::default_base
  {
  public:
    /*!
     * \brief
     * A ConfigurationBase holds how data should be set to a
     * PainterBackend
     */
    class ConfigurationBase
    {
    public:
      /*!
       * Ctor.
       */
      ConfigurationBase(void);

      /*!
       * Copy ctor.
       */
      ConfigurationBase(const ConfigurationBase &obj);

      ~ConfigurationBase();

      /*!
       * assignment operator
       */
      ConfigurationBase&
      operator=(const ConfigurationBase &obj);

      /*!
       * Swap operation
       * \param obj object with which to swap
       */
      void
      swap(ConfigurationBase &obj);

      /*!
       * Bits that are up in brush_shader_mask(void) that change
       * in PainterBrush::shader() trigger a call to
       * PainterDraw::draw_break().
       */
      uint32_t
      brush_shader_mask(void) const;

      /*!
       * Specify the value returned by brush_shader_mask(void) const,
       * default value is 0
       * \param v value
       */
      ConfigurationBase&
      brush_shader_mask(uint32_t v);

      /*!
       * Specifies the alignment in units of generic_data for
       * packing of seperately accessible entries of generic data
       * in PainterDraw::m_store.
       */
      int
      alignment(void) const;

      /*!
       * Specify the value returned by alignment(void) const,
       * default value is 4
       * \param v value
       */
      ConfigurationBase&
      alignment(int v);

      /*!
       * Returns the PainterCompositeShader::shader_type the \ref
       * PainterBackend accepts for \ref PainterCompositeShader
       * objects.
       */
      enum PainterCompositeShader::shader_type
      composite_type(void) const;

      /*!
       * Specify the return value to composite_type() const.
       * Default value is \ref PainterCompositeShader::dual_src.
       * \param tp composite shader type
       */
      ConfigurationBase&
      composite_type(enum PainterCompositeShader::shader_type tp);

      /*!
       * If true, indicates that the PainterBackend supports
       * bindless texturing. Default value is false.
       */
      bool
      supports_bindless_texturing(void) const;

      /*!
       * Specify the return value to supports_bindless_texturing() const.
       * Default value is false.
       */
      ConfigurationBase&
      supports_bindless_texturing(bool);

    private:
      void *m_d;
    };

    /*!
     * \brief
     * PerformanceHints provides miscellaneous data about
     * an implementation of a PainterBackend.
     */
    class PerformanceHints
    {
    public:
      /*!
       * Ctor.
       */
      PerformanceHints(void);

      /*!
       * Copy ctor.
       */
      PerformanceHints(const PerformanceHints &obj);

      ~PerformanceHints();

      /*!
       * assignment operator
       */
      PerformanceHints&
      operator=(const PerformanceHints &obj);

      /*!
       * Swap operation
       * \param obj object with which to swap
       */
      void
      swap(PerformanceHints &obj);

      /*!
       * Returns true if an implementation of PainterBackend
       * clips triangles (for example by a hardware clipper
       * or geometry shading) instead of discard to implement
       * clipping as embodied by \ref PainterClipEquations.
       */
      bool
      clipping_via_hw_clip_planes(void) const;

      /*!
       * Set the value returned by
       * clipping_via_hw_clip_planes(void) const,
       * default value is true.
       */
      PerformanceHints&
      clipping_via_hw_clip_planes(bool v);

    private:
      void *m_d;
    };

    /*!
     * Surface represents an interface to specify a buffer to
     * which a PainterBackend renders content.
     */
    class Surface:public reference_counted<Surface>::default_base
    {
    public:
      /*!
       * A Viewport specifies the sub-region within a Surface
       * to which one renders.
       */
      class Viewport
      {
      public:
        Viewport(void):
          m_origin(0, 0),
          m_dimensions(1, 1)
        {}

        /*!
         * Ctor.
         * \param x value with which to initialize x-coordinate of \ref m_origin
         * \param y value with which to initialize y-coordinate of \ref m_origin
         * \param w value with which to initialize x-coordinate of \ref m_dimensions
         * \param h value with which to initialize y-coordinate of \ref m_dimensions
         */
        Viewport(int x, int y, int w, int h):
          m_origin(x, y),
          m_dimensions(w, h)
        {}

        /*!
         * The origin of the viewport
         */
        ivec2 m_origin;

        /*!
         * The dimensions of the viewport
         */
        ivec2 m_dimensions;
      };

      virtual
      ~Surface()
      {}

      /*!
       * To be implemented by a derived class to return
       * the viewport into the Surface.
       */
      virtual
      Viewport
      viewport(void) const = 0;

      /*!
       * To be implemented by a derived class to return
       * the dimensions of the Surface backing store.
       */
      virtual
      ivec2
      dimensions(void) const = 0;
    };

    /*!
     * Ctor.
     * \param glyph_atlas GlyphAtlas for glyphs drawn by the PainterBackend
     * \param image_atlas ImageAtlas for images drawn by the PainterBackend
     * \param colorstop_atlas ColorStopAtlas for color stop sequences drawn by the PainterBackend
     * \param shader_registrar PainterShaderRegistrar to which shaders are registered
     * \param config ConfigurationBase for how to pack data to PainterBackend
     * \param pdefault_shaders default shaders for PainterBackend; shaders are
     *                         registered at constructor.
     */
    PainterBackend(reference_counted_ptr<GlyphAtlas> glyph_atlas,
                   reference_counted_ptr<ImageAtlas> image_atlas,
                   reference_counted_ptr<ColorStopAtlas> colorstop_atlas,
                   reference_counted_ptr<PainterShaderRegistrar> shader_registrar,
                   const ConfigurationBase &config,
                   const PainterShaderSet &pdefault_shaders);

    virtual
    ~PainterBackend();

    /*!
     * To be implemented by a derived class to return
     * the number of attributes a PainterDraw returned
     * by map_draw() is guaranteed to hold.
     */
    virtual
    unsigned int
    attribs_per_mapping(void) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the number of indices a PainterDraw returned
     * by map_draw() is guaranteed to hold.
     */
    virtual
    unsigned int
    indices_per_mapping(void) const = 0;

    /*!
     * Returns a handle to the GlyphAtlas of this
     * PainterBackend. All glyphs used by this
     * PainterBackend must live on glyph_atlas().
     */
    const reference_counted_ptr<GlyphAtlas>&
    glyph_atlas(void);

    /*!
     * Returns a handle to the ImageAtlas of this
     * PainterBackend. All images used by all brushes
     * of this PainterBackend must live on image_atlas().
     */
    const reference_counted_ptr<ImageAtlas>&
    image_atlas(void);

    /*!
     * Returns a handle to the ColorStopAtlas of this
     * PainterBackend. All color stops used by all brushes
     * of this PainterBackend must live on colorstop_atlas().
     */
    const reference_counted_ptr<ColorStopAtlas>&
    colorstop_atlas(void);

    /*!
     * Returns the PainterShaderRegistrar of this PainterBackend.
     * Use this return value to add custom shaders. NOTE: shaders
     * added within a thread are not useable within that thread
     * until the next call to begin().
     */
    const reference_counted_ptr<PainterShaderRegistrar>&
    painter_shader_registrar(void);

    /*!
     * Returns the ConfigurationBase passed in the ctor.
     */
    const ConfigurationBase&
    configuration_base(void) const;

    /*!
     * Called just before calling PainterDraw::draw() on a sequence
     * of PainterDraw objects who have had their PainterDraw::unmap()
     * routine called. An implementation will  will clear the depth
     * (aka occlusion) buffer and optionally the color buffer in the
     * viewport of the \ref PainterBackend::Surface.
     * \param surface the \ref PainterBackend::Surface to which to
     *                render content
     * \param clear_color_buffer if true, clear the color buffer
     *                           on the viewport of the surface.
     */
    virtual
    void
    on_pre_draw(const reference_counted_ptr<Surface> &surface,
                bool clear_color_buffer) = 0;

    /*!
     * Called just after calling PainterDraw::draw()
     * on a sequence of PainterDraw objects.
     */
    virtual
    void
    on_post_draw(void) = 0;

    /*!
     * Called to return an action to bind an Image whose backing
     * store requires API binding.
     * \param im Image backed by a gfx API surface that in order to be used,
     *           must be bound. In patricular im's Image::type() value
     *           is Image::context_texture2d
     */
    virtual
    reference_counted_ptr<PainterDraw::Action>
    bind_image(const reference_counted_ptr<const Image> &im) = 0;

    /*!
     * To be implemented by a derived class to return a PainterDraw
     * for filling of data.
     */
    virtual
    reference_counted_ptr<const PainterDraw>
    map_draw(void) = 0;

    /*!
     * Returns the PainterShaderSet for the backend.
     * Returned values will already be registerd by the
     * backend.
     */
    const PainterShaderSet&
    default_shaders(void);

    /*!
     * Returns the PerformanceHints for the PainterBackend,
     * may only be called after on_begin() has been called
     * atleast once. The value returned is expected to stay
     * constant once on_begin() has been called.
     */
    const PerformanceHints&
    hints(void) const;

  protected:
    /*!
     * To be accessed by a derived class in its ctor
     * to set the performance hint values for itself.
     */
    PerformanceHints&
    set_hints(void);

  private:
    void *m_d;
  };
/*! @} */

}
