/*!
 * \file painter_engine_gl.hpp
 * \brief file painter_engine_gl.hpp
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

#include <fastuidraw/painter/backend/painter_engine.hpp>
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/gl_backend/image_gl.hpp>
#include <fastuidraw/gl_backend/glyph_atlas_gl.hpp>
#include <fastuidraw/gl_backend/colorstop_atlas_gl.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>

namespace fastuidraw
{
  namespace gl
  {
/*!\addtogroup GLBackend
 * @{
 */
    /*!
     * \brief
     * A PainterEngineGL implements \ref PainterEngine
     * using the GL (or GLES) API.
     */
    class PainterEngineGL:
      public glsl::PainterShaderRegistrarGLSLTypes,
      public PainterEngine
    {
    public:
      /*!
       * \brief
       * Enumeration to specify which \ref Program
       * to fetch from the methods program().
       */
      enum program_type_t
        {
          /*!
           * Get the GLSL program that handles all item shaders
           */
          program_all,

          /*!
           * Get the GLSL program that handles those item shaders
           * which do not have discard
           */
          program_without_discard,

          /*!
           * Get the GLSL program that handles those item shaders
           * which do have discard
           */
          program_with_discard,

          /*!
           */
          number_program_types
        };

      /*!
       * \brief
       * A ConfigurationGL gives parameters how to contruct
       * a PainterEngineGL.
       */
      class ConfigurationGL
      {
      public:
        /*!
         * Ctor.
         */
        ConfigurationGL(void);

        /*!
         * Copy ctor.
         * \param obj value from which to copy
         */
        ConfigurationGL(const ConfigurationGL &obj);

        ~ConfigurationGL();

        /*!
         * Assignment operator
         * \param rhs value from which to copy
         */
        ConfigurationGL&
        operator=(const ConfigurationGL &rhs);

        /*!
         * Swap operation
         * \param obj object with which to swap
         */
        void
        swap(ConfigurationGL &obj);

        /*!
         * Return the parameters for creating the value returned
         * by image_atlas();
         */
        const ImageAtlasGL::params&
        image_atlas_params(void) const;

        /*!
         * Set the value for image_atlas_params(void) const
         */
        ConfigurationGL&
        image_atlas_params(const ImageAtlasGL::params&);

        /*!
         * Return the parameters for creating the value returned
         * by glyph_atlas();
         */
        const GlyphAtlasGL::params&
        glyph_atlas_params(void) const;

        /*!
         * Set the value for glyph_atlas_params(void) const
         */
        ConfigurationGL&
        glyph_atlas_params(const GlyphAtlasGL::params&);

        /*!
         * Return the parameters for creating the value returned
         * by _atlas();
         */
        const ColorStopAtlasGL::params&
        colorstop_atlas_params(void) const;

        /*!
         * Set the value for _atlas_params(void) const
         */
        ConfigurationGL&
        colorstop_atlas_params(const ColorStopAtlasGL::params&);

        /*!
         * The ImageAtlasGL to be used by the painter
         */
        const reference_counted_ptr<ImageAtlasGL>&
        image_atlas(void) const;

        /*!
         * The ColorStopAtlasGL to be used by the painter
         */
        const reference_counted_ptr<ColorStopAtlasGL>&
        colorstop_atlas(void) const;

        /*!
         * The GlyphAtlasGL to be used by the painter
         */
        const reference_counted_ptr<GlyphAtlasGL>&
        glyph_atlas(void) const;

        /*!
         * Specifies the maximum number of attributes
         * a PainterDraw returned by
         * map_draw() may store, i.e. the size
         * of PainterDraw::m_attributes.
         * Initial value is 512 * 512.
         */
        unsigned int
        attributes_per_buffer(void) const;

        /*!
         * Set the value for attributes_per_buffer(void) const
         */
        ConfigurationGL&
        attributes_per_buffer(unsigned int v);

        /*!
         * Specifies the maximum number of indices
         * a PainterDraw returned by
         * map_draw() may store, i.e. the size
         * of PainterDraw::m_indices.
         * Initial value is 1.5 times the initial value
         * for attributes_per_buffer(void) const.
         */
        unsigned int
        indices_per_buffer(void) const;

        /*!
         * Set the value for indices_per_buffer(void) const
         */
        ConfigurationGL&
        indices_per_buffer(unsigned int v);

        /*!
         * Specifies the maximum number of blocks of
         * data a PainterDraw returned by
         * map_draw() may store. The size of
         * PainterDraw::m_store is given by
         * data_blocks_per_store_buffer() * 4.
         * Initial value is 1024 * 64.
         */
        unsigned int
        data_blocks_per_store_buffer(void) const;

        /*!
         * Set the value for data_blocks_per_store_buffer(void) const
         */
        ConfigurationGL&
        data_blocks_per_store_buffer(unsigned int v);

        /*!
         * Returns how the data store is realized. The GL implementation
         * may impose size limits that will force that the size of the
         * data store might be smaller than that specified by
         * data_blocks_per_store_buffer(). The initial value
         * is \ref data_store_tbo.
         */
        enum data_store_backing_t
        data_store_backing(void) const;

        /*!
         * Set the value for data_store_backing(void) const
         */
        ConfigurationGL&
        data_store_backing(enum data_store_backing_t v);

        /*!
         * Specifies how the uber-shader will perform clipping.
         */
        enum clipping_type_t
        clipping_type(void) const;

        /*!
         * Set the value returned by clipping_type(void) const.
         * Default value is \ref clipping_via_gl_clip_distance.
         */
        ConfigurationGL&
        clipping_type(enum clipping_type_t);

        /*!
         * Returns the number of external textures (realized as
         * sampler2D uniforms) the uber-shader is to have.
         */
        unsigned int
        number_external_textures(void) const;

        /*!
         * Set the value returned by number_external_textures(void) const.
         * Default value is 8.
         */
        ConfigurationGL&
        number_external_textures(unsigned int);

        /*!
         * If true, use switch() statements in uber vertex shader,
         * if false use a chain of if-else. Default value is false.
         */
        bool
        vert_shader_use_switch(void) const;

        /*!
         * Set the value for vert_shader_use_switch(void) const
         */
        ConfigurationGL&
        vert_shader_use_switch(bool v);

        /*!
         * If true, use switch() statements in uber frag shader,
         * if false use a chain of if-else. Default value is false.
         */
        bool
        frag_shader_use_switch(void) const;

        /*!
         * Set the value for frag_shader_use_switch(void) const
         */
        ConfigurationGL&
        frag_shader_use_switch(bool v);

        /*!
         * If true, use switch() statements in uber blend shader,
         * if false use a chain of if-else. Default value is false.
         */
        bool
        blend_shader_use_switch(void) const;

        /*!
         * Set the value for blend_shader_use_switch(void) const
         */
        ConfigurationGL&
        blend_shader_use_switch(bool v);

        /*!
         * A PainterBackend for the GL/GLES backend has a set of pools
         * for the buffer objects to which to data to send to GL. Whenever
         * on_end() is called, the next pool is used (wrapping around to
         * the first pool when the last pool is finished). Ideally, one
         * should set this value to N * L where N is the number of times
         * Painter::begin() - Painter::end() pairs are within a frame and
         * L is the latency in frames from ending a frame to the GPU finishes
         * rendering the results of the frame. Initial value is 3.
         */
        unsigned int
        number_pools(void) const;

        /*!
         * Set the value for number_pools(void) const
         */
        ConfigurationGL&
        number_pools(unsigned int v);

        /*!
         * If true, place different item shaders in seperate
         * entries of a glMultiDrawElements call.
         * The motivation is that by placing in a seperate element
         * of a glMultiDrawElements call, each element is a seperate
         * HW draw call and by being seperate, the shader invocation
         * does not divergently branch. Default value is false.
         */
        bool
        break_on_shader_change(void) const;

        /*!
         * Set the value for break_on_shader_change(void) const
         */
        ConfigurationGL&
        break_on_shader_change(bool v);

        /*!
         * If false, each differen item shader (including sub-shaders) is
         * realized as a separate GLSL program. This means that a GLSL
         * shader change is invoked when item shader changes, potentially
         * massively harming performance. Default value is true.
         */
        bool
        use_uber_item_shader(void) const;

        /*!
         * Set the value for use_uber_item_shader(void) const
         */
        ConfigurationGL&
        use_uber_item_shader(bool);

        /*!
         * If true, the vertex shader inputs should be qualified
         * with a layout(location=) specifier. Default value is
         * true.
         */
        bool
        assign_layout_to_vertex_shader_inputs(void) const;

        /*!
         * Set the value for assign_layout_to_vertex_shader_inputs(void) const
         */
        ConfigurationGL&
        assign_layout_to_vertex_shader_inputs(bool v);

        /*!
         * If true, the varyings between vertex and fragment
         * shading should be qualified with a layout(location=)
         * specifier. Default value is false.
         */
        bool
        assign_layout_to_varyings(void) const;

        /*!
         * Set the value for assign_layout_to_varyings(void) const
         */
        ConfigurationGL&
        assign_layout_to_varyings(bool v);

        /*!
         * If true, the textures and buffers used in the
         * uber-shader should be qualified with a layout(binding=)
         * specifier. Default value is true.
         */
        bool
        assign_binding_points(void) const;

        /*!
         * Set the value for assign_binding_points(void) const
         */
        ConfigurationGL&
        assign_binding_points(bool v);

        /*!
         * If true, item and blend shaders are broken into
         * two classes: those that use discard and those that
         * do not. Each class is then realized as a seperate
         * GLSL program.
         */
        bool
        separate_program_for_discard(void) const;

        /*!
         * Set the value for separate_program_for_discard(void) const
         */
        ConfigurationGL&
        separate_program_for_discard(bool v);

        /*!
         * Returns the preferred way to implement blend shaders, i.e.
         * if a shader can be implemented with this blending type
         * it will be.
         */
        enum PainterBlendShader::shader_type
        preferred_blend_type(void) const;

        /*!
         * Specify the return value to preferred_blend_type() const.
         * Default value is \ref PainterBlendShader::dual_src
         * \param tp blend shader type
         */
        ConfigurationGL&
        preferred_blend_type(enum PainterBlendShader::shader_type tp);

        /*!
         * If true, will support blend shaders with \ref
         * PainterBlendShader::type() with value \ref
         * PainterBlendShader::dual_src.
         */
        bool
        support_dual_src_blend_shaders(void) const;

        /*!
         * Specify the return value to support_dual_src_blend_shaders() const.
         * Default value is true
         */
        ConfigurationGL&
        support_dual_src_blend_shaders(bool v);

        /*!
         * Returns how the painter will perform blending. If the
         * value is not \ref fbf_blending_not_supported, then will
         * support blend shaders with \ref PainterBlendShader::type()
         * with value \ref PainterBlendShader::framebuffer_fetch.
         */
        enum fbf_blending_type_t
        fbf_blending_type(void) const;

        /*!
         * Specify the return value to fbf_blending_type() const.
         * Default value is \ref fbf_blending_not_supported
         * \param tp blend shader type
         */
        ConfigurationGL&
        fbf_blending_type(enum fbf_blending_type_t tp);

        /*!
         * If true, \ref PainterSurfaceGL objects whose \ref
         * PainterSurface::image() routine will return a \ref
         * ImageAtlasGL::TextureImage whose \ref Image::type()
         * is \ref Image::bindless_texture2d if the GL/GLES
         * implementation support bindless texturing; otherwise
         * PainterSurfaceSurface::image() will always return
         * \ref ImageAtlasGL::TextureImage objects of \ref
         * Image::type() \ref Image::context_texture2d. A
         * number of buggy hardware drivers do not correctly
         * sample from bindless textures if the texture was
         * renderered to.
         */
        bool
        allow_bindless_texture_from_surface(void) const;

        /*!
         * Specify the return value to \ref
         * allow_bindless_texture_from_surface() const.
         * Default value is true.
         */
        ConfigurationGL&
        allow_bindless_texture_from_surface(bool);

        /*!
         * If a non-empty string, gives the GLSL version to be used
         * by the uber-shaders. This value is (string) maxed with
         * the computed GLSL version that is needed from the values
         * of ConfigurationGL. Return value is valid until the next
         * call to glsl_version_override(). Default value is an empty
         * string.
         */
        c_string
        glsl_version_override(void) const;

        /*!
         * Set the value returned by glsl_version_override(void) const.
         * NOTE: the string is -copied-, thus it is legal for the passed
         * string to be deallocated after the call.
         */
        ConfigurationGL&
        glsl_version_override(c_string);

        /*!
         * Set the values for optimal performance or rendering quality
         * by quering the GL context.
         * \param optimal_rendering_quality if true, select parameters
         *                                  that give optimal rendering
         *                                  quality (at potential sacrifice
         *                                  of performance). If false,
         *                                  choose for optimal performance
         *                                  even at the cost of rendering
         *                                  quality.
         * \param ctx Optional argument to pass to avoid re-querying
         *            the current GL context for extension and version
         */
        ConfigurationGL&
        configure_from_context(bool optimal_rendering_quality = false,
                               const ContextProperties &ctx = ContextProperties());

        /*!
         * Adjust values for current GL context.
         * \param ctx Optional argument to pass to avoid re-querying
         *            the current GL context for extension and version
         */
        ConfigurationGL&
        adjust_for_context(const ContextProperties &ctx = ContextProperties());

      private:
        void *m_d;
      };

      ~PainterEngineGL();

      /*!
       * Ctor. Create a PainterEngineGL configured via a ConfigurationGL
       * value. The configuration of the created PainterEngineGL will be
       * adjusted according to the functionaliy of the currentl current GL
       * context. In addition, the current GL context or a GL context of
       * its share group must be current when the PainterEngineGL is used.
       *
       * \param config_gl ConfigurationGL providing configuration parameters
       * \param ctx Optional argument to pass to avoid re-querying
       *            the current GL context for extension and version
       */
      static
      reference_counted_ptr<PainterEngineGL>
      create(ConfigurationGL config_gl,
             const ContextProperties &ctx = ContextProperties());

      /*!
       * Ctor. Create a PainterEngineGL and the atlases to be used by it.
       * The atlases and PainterEngineGL will be configured optimally as
       * according to the current GL context. In addition, the current GL
       * context or a GL context of its share group must be current when
       * the PainterEngineGL is used.
       * \param optimal_rendering_quality if true, select parameters
       *                                  that give optimal rendering
       *                                  quality (at potential sacrifice
       *                                  of performance). If false,
       *                                  choose for optimal performance
       *                                  even at the cost of rendering
       *                                  quality.
       * \param ctx Optional argument to pass to avoid re-querying
       *            the current GL context for extension and version
       */
      static
      reference_counted_ptr<PainterEngineGL>
      create(bool optimal_rendering_quality,
             const ContextProperties &ctx = ContextProperties());

      /*!
       * Return the specified \ref Program used to draw by
       * \ref PainterBackend objects generated by this \ref
       * PainterEngineGL.
       * \param discard_tp selects what item-shaders are included
       * \param blend_type selects what blend type
       */
      reference_counted_ptr<Program>
      program(enum program_type_t discard_tp,
              enum PainterBlendShader::shader_type blend_type);

      /*!
       * Returns the \ref Program used to draw to the deferred
       * coverage buffer.
       */
      reference_counted_ptr<Program>
      program_deferred_coverage_buffer(void);

      /*!
       * Returns the number of UBO binding units used; the
       * units used are 0, 1, ..., num_ubo_units() - 1.
       */
      unsigned int
      num_ubo_units(void) const;

      /*!
       * Returns the number of SSBO binding units used; the
       * units used are 0, 1, ..., num_ssbo_units() - 1.
       */
      unsigned int
      num_ssbo_units(void) const;

      /*!
       * Returns the number of texture binding units used; the
       * units used are 0, 1, ..., num_texture_units() - 1.
       */
      unsigned int
      num_texture_units(void) const;

      /*!
       * Returns the number of image binding units used; the
       * units used are 0, 1, ..., num_image_units() - 1.
       */
      unsigned int
      num_image_units(void) const;

      /*!
       * Returns the ConfigurationGL adapted from that passed
       * by ctor (for the properties of the GL context) of
       * the PainterBackendGL.
       */
      const ConfigurationGL&
      configuration_gl(void) const;

      virtual
      reference_counted_ptr<PainterBackend>
      create_backend(void) const override final;

      virtual
      reference_counted_ptr<PainterSurface>
      create_surface(ivec2 dims,
                     enum PainterSurface::render_type_t render_type) override final;

    private:
      PainterEngineGL(const ConfigurationGL &config_gl,
                              const UberShaderParams &uber_params,
                              const PainterShaderSet &shaders);

      void *m_d;
    };
/*! @} */
  }
}
