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
#include <fastuidraw/util/rect.hpp>
#include <fastuidraw/text/glyph_atlas.hpp>
#include <fastuidraw/image.hpp>
#include <fastuidraw/colorstop_atlas.hpp>
#include <fastuidraw/painter/backend/painter_draw.hpp>
#include <fastuidraw/painter/backend/painter_shader_registrar.hpp>
#include <fastuidraw/painter/backend/painter_surface.hpp>


namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */

  /*!
   * \brief
   * A PainterBackend is an interface that defines the API-specific
   * elements to implement \ref Painter. A fixed PainterBackend will
   * only be used by a single \ref Painter because a \ref Painter
   * does NOT use the backend it is passed, instead it creates a
   * backend for its own private use via \ref create_shared().
   *
   * A \ref Painter will use a \ref PainterBackend as follows within a
   * Painter::begin() and Painter::end() pair.
   * \code
   * fastuidraw::PainterBackend &backend;
   *
   * backend.on_painter_begin();
   * for (how many surfaces S needed to draw all)
   *   {
   *     std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw> > draws;
   *     for (how many PainterDraw objects needed to draw what is drawn in S)
   *       {
   *         fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw> p;
   *
   *         p = backend.map_draw();
   *         // fill the buffers on p, potentially calling
   *         // PainterDraw::draw_break() several times.
   *         p.get()->unmap(attributes_written, indices_written, data_store_written);
   *         draws.push_back(p);
   *       }
   *     backend.on_pre_draw(S, maybe_clear_color_buffer, maybe_begin_new_target);
   *     for (p in draws)
   *       {
   *         p.get()->draw();
   *       }
   *     draws.clear();
   *     backend.on_post_draw();
   *   }
   * \endcode
   */
  class PainterBackend:public reference_counted<PainterBackend>::concurrent
  {
  public:
    virtual
    ~PainterBackend()
    {}

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
     * Called just before calling PainterDraw::draw() on a sequence
     * of PainterDraw objects who have had their PainterDraw::unmap()
     * routine called. An implementation will  will clear the depth
     * (aka occlusion) buffer and optionally the color buffer in the
     * viewport of the \ref PainterSurface.
     * \param surface the \ref PainterSurface to which to
     *                render content
     * \param clear_color_buffer if true, clear the color buffer
     *                           on the viewport of the surface.
     * \param begin_new_target if true indicates that drawing is to
     *                         start on the surface (typically this
     *                         means that when this is true that the
     *                         backend will clear all auxiliary buffers
     *                         (such as the depth buffer).
     */
    virtual
    void
    on_pre_draw(const reference_counted_ptr<PainterSurface> &surface,
                bool clear_color_buffer,
                bool begin_new_target) = 0;

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
     * \param slot which of the external image slots to bind the image to
     * \param im Image backed by a gfx API surface that in order to be used,
     *           must be bound. In patricular im's Image::type() value
     *           is Image::context_texture2d
     */
    virtual
    reference_counted_ptr<PainterDrawBreakAction>
    bind_image(unsigned int slot,
               const reference_counted_ptr<const Image> &im) = 0;

    /*!
     * Called to return an action to bind a \ref PainterSurface
     * to be used as the read from the deferred coverage buffer.
     * \param cvg_surface coverage surface backing the deferred
     *                    coverage buffer from which to read
     */
    virtual
    reference_counted_ptr<PainterDrawBreakAction>
    bind_coverage_surface(const reference_counted_ptr<PainterSurface> &cvg_surface) = 0;

    /*!
     * To be implemented by a derived class to return a PainterDraw
     * for filling of data.
     */
    virtual
    reference_counted_ptr<PainterDraw>
    map_draw(void) = 0;

    /*!
     * To be implemented by a derived class to perform any caching
     * or other operations when \ref Painter has Painter::begin()
     * and to return the number of external texture slots the
     * PainterBackend supports.
     */
    virtual
    unsigned int
    on_painter_begin(void) = 0;
  };
/*! @} */

}
