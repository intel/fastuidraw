/*!
 * \file painter_backend_gl.hpp
 * \brief file painter_backend_gl.hpp
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

#include <fastuidraw/painter/backend/painter_backend.hpp>
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/gl_backend/image_gl.hpp>
#include <fastuidraw/gl_backend/glyph_atlas_gl.hpp>
#include <fastuidraw/gl_backend/colorstop_atlas_gl.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/gl_backend/painter_backend_factory_gl.hpp>
#include <fastuidraw/gl_backend/painter_surface_gl.hpp>
#include <fastuidraw/gl_backend/painter_backend_factory_gl.hpp>

namespace fastuidraw
{
  namespace gl
  {
/*!\addtogroup GLBackend
 * @{
 */
    /*!
     * \brief
     * A PainterBackendGL implements PainterBackend
     * using the GL (or GLES) API.
     */
    class PainterBackendGL:
      public glsl::PainterShaderRegistrarGLSLTypes,
      public PainterBackend
    {
    public:
      ~PainterBackendGL();

      virtual
      unsigned int
      attribs_per_mapping(void) const override final;

      virtual
      unsigned int
      indices_per_mapping(void) const override final;

      virtual
      void
      on_pre_draw(const reference_counted_ptr<PainterSurface> &surface,
                  bool clear_color_buffer, bool begin_new_target) override final;

      virtual
      void
      on_post_draw(void) override final;

      virtual
      reference_counted_ptr<PainterDrawBreakAction>
      bind_image(unsigned int slot,
                 const reference_counted_ptr<const Image> &im) override final;

      virtual
      reference_counted_ptr<PainterDrawBreakAction>
      bind_coverage_surface(const reference_counted_ptr<PainterSurface> &surface) override final;

      virtual
      reference_counted_ptr<PainterDraw>
      map_draw(void) override final;

      virtual
      unsigned int
      on_painter_begin(void) override final;

    private:
      friend class PainterBackendFactoryGL;

      PainterBackendGL(const PainterBackendFactoryGL::ConfigurationGL &config_gl,
                       const UberShaderParams &uber_params,
                       const PainterShaderSet &shaders,
                       reference_counted_ptr<PainterShaderRegistrar> shader_registar);

      void *m_d;
    };
/*! @} */
  }
}
