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
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/gl_backend/painter_surface_gl.hpp>
#include <fastuidraw/gl_backend/painter_engine_gl.hpp>

#include <private/gl_backend/tex_buffer.hpp>
#include <private/gl_backend/texture_gl.hpp>
#include <private/gl_backend/painter_backend_gl_config.hpp>
#include <private/gl_backend/painter_vao_pool.hpp>
#include <private/gl_backend/painter_shader_registrar_gl.hpp>
#include <private/gl_backend/painter_surface_gl_private.hpp>
#include <private/gl_backend/binding_points.hpp>
#include <private/gl_backend/image_gl.hpp>
#include <private/gl_backend/colorstop_atlas_gl.hpp>
#include <private/gl_backend/glyph_atlas_gl.hpp>
#include <private/gl_backend/opengl_trait.hpp>

namespace fastuidraw
{
  namespace gl
  {
    namespace detail
    {
      class PainterBackendGL:
        public glsl::PainterShaderRegistrarGLSLTypes,
        public PainterBackend
      {
      public:
        PainterBackendGL(const fastuidraw::gl::PainterEngineGL *f);

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

        GLuint
        clear_buffers_of_current_surface(bool clear_depth, bool clear_color);

        bool
        use_uber_shader(void)
        {
          return !m_cached_item_programs;
        }

      private:
        class RenderTargetState
        {
        public:
          RenderTargetState(void):
            m_fbo(0),
            m_color_buffer_as_image(false)
          {}

          GLuint m_fbo;
          bool m_color_buffer_as_image;
        };

        class TextureImageBindAction;
        class CoverageTextureBindAction;
        class DrawState;
        class DrawCommand;
        class DrawEntry;

        RenderTargetState
        set_gl_state(RenderTargetState last_state,
                     enum PainterBlendShader::shader_type blend_type,
                     gpu_dirty_state v);

        reference_counted_ptr<PainterShaderRegistrarGL> m_reg_gl;
        reference_counted_ptr<GlyphAtlasGL> m_glyph_atlas;
        reference_counted_ptr<ImageAtlasGL> m_image_atlas;
        reference_counted_ptr<ColorStopAtlasGL> m_colorstop_atlas;

        GLuint m_nearest_filter_sampler;
        reference_counted_ptr<painter_vao_pool> m_pool;
        PainterSurfaceGLPrivate *m_surface_gl;
        bool m_uniform_ubo_ready;
        std::vector<GLuint> m_current_external_texture;
        GLuint m_current_coverage_buffer_texture;
        BindingPoints m_binding_points;
        DrawState *m_draw_state;
        PainterShaderRegistrarGL::program_set m_cached_programs;
        reference_counted_ptr<PainterShaderRegistrarGL::CachedItemPrograms> m_cached_item_programs;
        fastuidraw::vecN<enum PainterEngineGL::program_type_t, 2> m_choose_uber_program;
      };
    } //namespace detail
  } //namespace gl
} //namespace fastuidraw
