/*!
 * \file painter_engine.hpp
 * \brief file painter_engine.hpp
 *
 * Copyright 2019 by Intel.
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
#include <fastuidraw/util/rect.hpp>
#include <fastuidraw/text/glyph_atlas.hpp>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/image_atlas.hpp>
#include <fastuidraw/colorstop_atlas.hpp>
#include <fastuidraw/painter/backend/painter_backend.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */

  /*!
   * \brief
   * A \ref PainterEngine provides an interface to create
   * \ref PainterBackend derived objects.
   */
  class PainterEngine:public reference_counted<PainterEngine>::concurrent
  {
  public:
    /*!
     * \brief
     * A ConfigurationBase holds properties common to all \ref PainterBackend
     * objects returned by \ref PainterEngine::create_backend() from
     * a fixed PainterEngine
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
     * PerformanceHints provides miscellaneous data about \ref PainterBackend
     * objects returned by \ref PainterEngine::create_backend() from
     * a fixed PainterEngine
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

      /*!
       * Gives the maximum z-value an implementation of
       * PainterBackend support.
       */
      int
      max_z(void) const;

      /*!
       * Set the value returned by max_z(void) const,
       * default value is 2^20.
       */
      PerformanceHints&
      max_z(int);

    private:
      void *m_d;
    };

    /*!
     * Ctor.
     * \param glyph_atlas GlyphAtlas for glyphs drawn by each \ref PainterBackend
     *                    returned by \ref create_backend() of the created
     *                    \ref PainterEngine
     * \param image_atlas ImageAtlas for images drawn by each \ref PainterBackend
     *                    returned by \ref create_backend() of the created
     *                    \ref PainterEngine
     * \param colorstop_atlas ColorStopAtlas for color stop sequences drawn by
     *                        each \ref PainterBackend returned by \ref create_backend()
     *                        of the created PainterEngine
     * \param shader_registrar PainterShaderRegistrar used by each \ref PainterBackend
     *                         returned by \ref create_backend() of the created
     *                         \ref PainterEngine
     * \param config \ref ConfigurationBase for each \ref PainterBackend returned by
     *               \ref create_backend() of the created \ref PainterEngine
     * \param pdefault_shaders default shaders for each \ref PainterBackend returned
     *                         by \ref create_backend() of the created \ref
     *                         PainterEngine; shaders are registered at
     *                         construction of the created \ref PainterEngine
     */
    PainterEngine(reference_counted_ptr<GlyphAtlas> glyph_atlas,
                          reference_counted_ptr<ImageAtlas> image_atlas,
                          reference_counted_ptr<ColorStopAtlas> colorstop_atlas,
                          reference_counted_ptr<PainterShaderRegistrar> shader_registrar,
                          const ConfigurationBase &config,
                          const PainterShaderSet &pdefault_shaders);

    virtual
    ~PainterEngine();

    /*!
     * Returns the PainterShaderSet for the backend.
     * Returned values will already be registered to
     * the \ref PainterShaderRegistrar returned by
     * \ref painter_shader_registrar().
     */
    const PainterShaderSet&
    default_shaders(void) const;

    /*!
     * Returns the PerformanceHints for the PainterBackend,
     * may only be called after on_begin() has been called
     * atleast once. The value returned is expected to stay
     * constant once on_begin() has been called.
     */
    const PerformanceHints&
    hints(void) const;

    /*!
     * Returns a handle to the \ref GlyphAtlas of this
     * PainterEngine. All glyphs used by each \ref
     * PainterBackend made from this \ref PainterEngine
     * must live on glyph_atlas().
     */
    const reference_counted_ptr<GlyphAtlas>&
    glyph_atlas(void) const;

    /*!
     * Returns a handle to the \ref ImageAtlas of this
     * PainterEngine. All images used by each \ref
     * PainterBackend made from this \ref PainterEngine
     * must live on image_atlas().
     */
    const reference_counted_ptr<ImageAtlas>&
    image_atlas(void) const;

    /*!
     * Returns a handle to the \ref ColorStopAtlas of this
     * PainterEngine. All color stops used by all brushes
     * of each \ref PainterBackend made from this \ref
     * PainterEngine must live on colorstop_atlas().
     */
    const reference_counted_ptr<ColorStopAtlas>&
    colorstop_atlas(void) const;

    /*!
     * Returns a handle to the \ref GlyphCache made
     * from glyph_atlas().
     */
    const reference_counted_ptr<GlyphCache>&
    glyph_cache(void) const;

    /*!
     * Returns the PainterShaderRegistrar of this PainterEngine.
     * Use this return value to add custom shaders. NOTE: shaders
     * added within a thread are not useable by a \ref PainterBackend
     * made from this \ref PainterEngine within that thread
     * until the next call to its PainterBackend::begin().
     */
    const reference_counted_ptr<PainterShaderRegistrar>&
    painter_shader_registrar(void) const;

    /*!
     * Returns the ConfigurationBase passed in the ctor.
     */
    const ConfigurationBase&
    configuration_base(void) const;

    /*!
     * To be implemented by a derived class to create a
     * \ref PainterBackend object. All \ref PainterBackend
     * objects created by create_backend() from the same
     * \ref PainterEngine share the same
     *  - \ref PainterShaderRegistrar (see \ref painter_shader_registrar())
     *  - \ref GlyphAtlas (see \ref glyph_atlas())
     *  - \ref ImageAtlas (see \ref image_atlas())
     *  - \ref ColorStopAtlas (see \ref colorstop_atlas())
     * but are otherwise independent of each other.
     */
    virtual
    reference_counted_ptr<PainterBackend>
    create_backend(void) const = 0;

    /*!
     * To be implemented by a derived class to create a
     * Surface with its own backing that is useable by
     * any \ref PainterBackend object that this \ref
     * PainterEngine returns in create_backend()
     * \param dims the dimensions of the backing store of
     *             the returned Surface
     * \param render_type the render type of the surface (i.e.
     *                    is it a color buffer or deferred
     *                    coverage buffer).
     */
    virtual
    reference_counted_ptr<PainterSurface>
    create_surface(ivec2 dims,
                   enum PainterSurface::render_type_t render_type) = 0;

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
