/*!
 * \file painter_surface_gl.hpp
 * \brief file painter_surface_gl.hpp
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

#include <fastuidraw/painter/backend/painter_surface.hpp>
#include <fastuidraw/gl_backend/gl_header.hpp>

namespace fastuidraw
{
  namespace gl
  {
    ///@cond
    class PainterBackendFactoryGL;
    ///@endcond

/*!\addtogroup GLBackend
 * @{
 */

    /*!
     * A PainterSurfaceGL is the implementatin of \ref PainterSurface
     * for the GL backend. A PainterSurfaceGL must only be used with at most
     * one GL context (even GL contexts in the same share group cannot
     * shader PainterSurfaceGL objects).
     */
    class PainterSurfaceGL:public PainterSurface
    {
    public:
      /*!
       * Ctor. Creates and uses a backing color texture.
       * The viewport() is initialized to be exactly the
       * entire backing store.
       * \param dims the width and height of the PainterSurfaceGL
       * \param backend the PainterBackendFactoryGL that produces
       *                \ref PainterBackend objects can use the
       *                created \ref PainterSurfaceGL
       * \param render_type the render type of the surface (i.e.
       *                    is it a color buffer or deferred
       *                    coverage buffer)
       */
      explicit
      PainterSurfaceGL(ivec2 dims,
                       const PainterBackendFactoryGL &backend,
                       enum render_type_t render_type = color_buffer_type);

      /*!
       * Ctor. Use the passed GL texture to which to render
       * content; the gl_texture must have as its texture
       * target GL_TEXTURE_2D and must already have its
       * backing store allocated (i.e. glTexImage or
       * glTexStorage has been called on the texture). The
       * texture object's ownership is NOT passed to the
       * PainterSurfaceGL, the caller is still responible to delete
       * the texture (with GL) and the texture must not be
       * deleted (or have its backing store changed via
       * glTexImage) until the PainterSurfaceGL is deleted. The
       * viewport() is initialized to be exactly the entire
       * backing store.
       * \param dims width and height of the GL texture
       * \param gl_texture GL name of texture
       * \param backend the PainterBackendFactoryGL that produces
       *                \ref PainterBackend objects can use the
       *                created \ref PainterSurfaceGL
       * \param render_type the render type of the surface (i.e.
       *                    is it a color buffer or deferred
       *                    coverage buffer)
       */
      explicit
      PainterSurfaceGL(ivec2 dims, GLuint gl_texture,
                       const PainterBackendFactoryGL &backend,
                       enum render_type_t render_type = color_buffer_type);

      ~PainterSurfaceGL();

      /*!
       * Returns the GL name of the texture backing
       * the color buffer of the PainterSurfaceGL.
       */
      GLuint
      texture(void) const;

      /*!
       * Blit the PainterSurfaceGL color buffer to the FBO
       * currently bound to GL_DRAW_FRAMEBUFFER.
       * \param src source from this PainterSurfaceGL to which to bit
       * \param dst destination in FBO to which to blit
       * \param filter GL filter to apply to blit operation
       */
      void
      blit_surface(const Viewport &src,
                   const Viewport &dst,
                   GLenum filter = GL_NEAREST) const;

      /*!
       * Provided as a convenience, equivalent to
       * \code
       * PainterBackend::Viewport vw(0, 0, dimensions().x(), dimensions.y());
       * blit_surface(vw, vw, filter);
       * \endcode
       * \param filter GL filter to apply to blit operation
       */
      void
      blit_surface(GLenum filter = GL_NEAREST) const;

      virtual
      reference_counted_ptr<const Image>
      image(const reference_counted_ptr<ImageAtlas> &atlas) const override final;

      virtual
      const Viewport&
      viewport(void) const override final;

      virtual
      void
      viewport(const Viewport &vwp) override final;

      virtual
      const vec4&
      clear_color(void) const override final;

      virtual
      void
      clear_color(const vec4&) override final;

      virtual
      ivec2
      dimensions(void) const override final;

      virtual
      enum render_type_t
      render_type(void) const override final;

    private:
      friend class PainterBackendGL;
      void *m_d;
    };
/*! @} */
  }
}
