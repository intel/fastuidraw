/*!
 * \file painter_surface.hpp
 * \brief file painter_surface.hpp
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

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/rect.hpp>
#include <fastuidraw/image.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */

  /*!
   * \brief
   * PainterSurface represents an interface to specify a buffer to
   * which a PainterBackend renders content.
   */
  class PainterSurface:public reference_counted<PainterSurface>::default_base
  {
  public:
    /*!
     * \brief
     * Enumeration to specify the render target of a Surface
     */
    enum render_type_t
      {
        /*!
         * Indicates that a \ref Surface represents a color
         * buffer; such \ref Surface objects are to also have
         * a depth buffer as well.
         */
        color_buffer_type,

        /*!
         * Indicates that a \ref Surface represents a
         * coverage buffer; such surfaces will have the
         * blending set to \ref BlendMode::MAX and do
         * not have a depth buffer.
         */
        deferred_coverage_buffer_type,

        number_buffer_types,
      };

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
       * Compute pixel coordinates from normalized device coords
       * using this Viewport values. The pixel coordinates are so
       * that (0, 0) is the bottom left.
       * \param ndc normalized device coordinates
       */
      vec2
      compute_pixel_coordinates(vec2 ndc) const
      {
        ndc += vec2(1.0f); // place in range [0, 2]
        ndc *= 0.5f;       // normalize to [0, 1]
        ndc *= vec2(m_dimensions); // normalize to dimension
        ndc += vec2(m_origin); // translate to origin
        return ndc;
      }

      /*!
       * Compute viewport coordiantes from normalized device coordinates
       * \param ndc normalized device coordinates
       * \param dims size of viewport (i.e. the value of \ref m_dimensions)
       */
      static
      vec2
      compute_viewport_coordinates(vec2 ndc, vec2 dims)
      {
        ndc += vec2(1.0f); // place in range [0, 2]
        ndc *= 0.5f;       // normalize to [0, 1]
        ndc *= dims;       // normalize to dimension
        return ndc;
      }

      /*!
       * Compute viewport coordiantes from normalized device coordinates
       * \param ndc normalized device coordinates
       * \param dims size of viewport (i.e. the value of \ref m_dimensions)
       */
      static
      vec2
      compute_viewport_coordinates(vec2 ndc, ivec2 dims)
      {
        return compute_viewport_coordinates(ndc, vec2(dims));
      }

      /*!
       * Compute viewport coordinates from normalized device coords
       * using this Viewport values. The viewport coordinates are so
       * that (0, 0) corresponds to pixel coordinates of value \ref
       * m_origin.
       * \param ndc normalized device coordinates
       */
      vec2
      compute_viewport_coordinates(vec2 ndc) const
      {
        return compute_viewport_coordinates(ndc, m_dimensions);
      }

      /*!
       * Compute normalized device coordinates from pixel
       * coordinates.
       * \param pixel pixel coordinates where (0, 0) corresponds to bottom
       *              left of the surface
       */
      vec2
      compute_normalized_device_coords(vec2 pixel) const
      {
        pixel -= vec2(m_origin); // translate from origin
        pixel /= vec2(m_dimensions); // normalize to [0, 1]
        pixel *= 2.0f; // normalize to [0, 2]
        pixel -= vec2(1.0f); // palce in range [-1, 1]
        return pixel;
      }

      /*!
       * Compute normalized device coordinates from viewport
       * coordinates.
       * \param viewport viewport coordinates
       */
      vec2
      compute_normalized_device_coords_from_viewport_coords(vec2 viewport_coords) const
      {
        viewport_coords /= vec2(m_dimensions); // normalize to [0, 1]
        viewport_coords *= 2.0f; // normalize to [0, 2]
        viewport_coords -= vec2(1.0f); // palce in range [-1, 1]
        return viewport_coords;
      }

      /*!
       * Compute normalized device coordinates from viewport
       * coordinates.
       * \param viewport viewport coordinates
       */
      vec2
      compute_normalized_device_coords_from_viewport_coords(ivec2 viewport_coords) const
      {
        return compute_normalized_device_coords_from_viewport_coords(vec2(viewport_coords));
      }

      /*!
       * Computes the clip-equations (in normalized device coordinates)
       * of this Viewport against a surface with the given dimensions.
       * \param surface_dims dimension of surface
       * \param[out] out_clip_equations location to which to write the
       *                                clip equations
       */
      void
      compute_clip_equations(ivec2 surface_dims,
                             vecN<vec3, 4> *out_clip_equations) const;

      /*!
       * Computes the rectangle in normalized device coordinates
       * of the intersection of a backing surface with the given
       * dimensions against this Viewport.
       * \param surface_dims dimension of surface
       * \param[out] out_rect location to which to write the
       *                      clipping rectangle in normalized
       *                      device coordinates
       */
      void
      compute_normalized_clip_rect(ivec2 surface_dims,
                                   Rect *out_rect) const;

      /*!
       * Returne the size needed by a surface to contain
       * the viewport, i.e. how many pixels the viewport
       * covers.
       */
      ivec2
      visible_dimensions(void) const
      {
        ivec2 return_value(m_dimensions);

        /* remove from the portion of the viewport that
         * is below/left of the surface
         */
        return_value.x() += t_min(0, m_origin.x());
        return_value.y() += t_min(0, m_origin.y());
        return return_value;
      }

      /*!
       * Computes the dimensions of the intersection
       * of this viewport against a surface with the
       * given resolution.
       * \param surface_dims dimension of surface
       */
      ivec2
      compute_visible_dimensions(ivec2 surface_dims) const
      {
        ivec2 return_value(visible_dimensions());

        return_value.x() = t_min(return_value.x(), surface_dims.x());
        return_value.y() = t_min(return_value.y(), surface_dims.y());
        return return_value;
      }

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
    ~PainterSurface()
    {}

    /*!
     * Return an \ref Image whose backing is the same as
     * this PainterSurface. It is expected that
     * backing \ref Image is the same for the lifetime of
     * the PainterSurface. The caller gaurantees
     * that the same ImageAtlas object will be passed on
     * each call to image().
     * \param atlas ImageAtlas to manage the returned Image
     */
    virtual
    reference_counted_ptr<const Image>
    image(const reference_counted_ptr<ImageAtlas> &atlas) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the viewport into the Surface.
     */
    virtual
    const Viewport&
    viewport(void) const = 0;

    /*!
     * To be implemented by a derived class to set
     * the viewport into the surface. The viewport
     * cannot be changed while the Surface is in
     * use by a \ref PainterBackend or \ref Painter.
     * \param vwp new viewport into the surface to use
     */
    virtual
    void
    viewport(const Viewport &vwp) = 0;

    /*!
     * To be implemented by a derived class to return
     * the clear color.
     */
    virtual
    const vec4&
    clear_color(void) const = 0;

    /*!
     * To be implemented by a derived class to set
     * the clear color.
     */
    virtual
    void
    clear_color(const vec4&) = 0;

    /*!
     * To be implemented by a derived class to return
     * the dimensions of the Surface backing store.
     */
    virtual
    ivec2
    dimensions(void) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the surface type of the Surface (i.e. is it
     * a color buffer or a deferred coverage buffer).
     */
    virtual
    enum render_type_t
    render_type(void) const = 0;

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * viewport().compute_visible_dimensions(dimensions())
     * \endcode
     */
    ivec2
    compute_visible_dimensions(void) const
    {
      return viewport().compute_visible_dimensions(dimensions());
    }
  };
/*! @} */

}
