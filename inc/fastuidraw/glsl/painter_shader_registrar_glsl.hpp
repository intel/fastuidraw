/*!
 * \file painter_shader_registrar_glsl.hpp
 * \brief file painter_shader_registrar_glsl.hpp
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

#include <fastuidraw/painter/packing/painter_shader_registrar.hpp>
#include <fastuidraw/painter/packing/painter_backend.hpp>
#include <fastuidraw/glsl/shader_source.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_composite_shader_glsl.hpp>

namespace fastuidraw
{
  namespace glsl
  {
/*!\addtogroup GLSL
 * @{
 */
    /*!
     * Conveniance class to hold types from which to inherit
     */
    class PainterShaderRegistrarGLSLTypes
    {
    public:
      /*!
       * \brief
       * Enumeration to specify how the data store filled by
       * \ref PainterDraw::m_store is realized.
       */
      enum data_store_backing_t
        {
          /*!
           * Data store is accessed by a usamplerBuffer
           * (i.e. a texture a buffer object).
           */
          data_store_tbo,

          /*!
           * Data store is backed by a uniform buffer object
           * that is an array of uvec4. The data store alignment
           * (see PainterBackend::ConfigurationBase::alignment())
           * must then be 4.
           */
          data_store_ubo,

          /*!
           * Data store is backed by a shader storage buffer object
           * that is an array of uvec4. The data store alignment
           * (see PainterBackend::ConfigurationBase::alignment())
           */
          data_store_ssbo
        };

      /*!
       * Enumeration specifying how the uber-shaders will
       * perform clipping against the clip-planes of \ref
       * PainterClipEquations
       */
      enum clipping_type_t
        {
          /*!
           * Clipping is performed in the vertex-shader
           * using gl_ClipDistance[i] for 0 <= i < 4.
           */
          clipping_via_gl_clip_distance,

          /*!
           * Clipping is performed by passing the distance
           * to each clip-plane and performing discard in
           * the fragment shader.
           */
          clipping_via_discard,

          /*!
           * Clipping is performed by passing the distance
           * to each clip-plane and (virtually) skipping
           * the color write. This requires that the
           * compositing mode is through framebuffer fetch,
           * i.e. UberShaderParams::compositing_type() is \ref
           * compositing_framebuffer_fetch or \ref compositing_interlock
           */
          clipping_via_skip_color_write,
        };

      /*!
       * \brief
       * Enumeration to specify how to perform painter compositing.
       */
      enum compositing_type_t
        {
          /*!
           * Use single source compositing; the non-seperable compositing
           * modes are not supported.
           */
          compositing_single_src,

          /*!
           * Use dual soruce compositing; the non-seperable compositing
           * modes are not supported.
           */
          compositing_dual_src,

          /*!
           * Use framebuffer fetch compositing; all compositing modes
           * are supported.
           */
          compositing_framebuffer_fetch,

          /*!
           * Have the color buffer realized as an image2D and use
           * fragment shader interlock to get compositing order correct;
           * all compositing modes are supported. A backend will
           * need to define the the functions (or macros) in their
           * GLSL preamble:
           *   - fastuidraw_begin_color_buffer_interlock()
           *   - fastuidraw_end_color_buffer_interlock()
           */
          compositing_interlock,
        };

      /*!
       * \brief
       * Enumeration to specify how to access the backing store
       * of the glyph geometry stored in GlyphAtlas::geometry_store()
       */
      enum glyph_geometry_backing_t
        {
          /*!
           * Use a samplerBuffer to access the data
           */
          glyph_geometry_tbo,

          /*!
           * Use a sampler2DArray to access the data
           */
          glyph_geometry_texture_array,

          /*!
           * Use a buffer block to access the data
           */
          glyph_geometry_ssbo,
        };

      /*!
       * \brief
       * Enumeration to specify how to access the backing store
       * of a color stop atlas store in ColorStopAtlas::backing_store().
       */
      enum colorstop_backing_t
        {
          /*!
           * Color stop backing store is realized as a
           * 1D texture array.
           */
          colorstop_texture_1d_array,

          /*!
           * Color stop backing store is realized as a
           * 2D texture array.
           */
          colorstop_texture_2d_array
        };

      /*!
       * \brief
       * Enumeration to specify the convention for a 3D API
       * for its normalized device coordinate in z.
       */
      enum z_coordinate_convention_t
        {
          /*!
           * Specifies that the normalized device coordinate
           * for z goes from -1 to 1.
           */
          z_minus_1_to_1,

          /*!
           * Specifies that the normalized device coordinate
           * for z goes from 0 to 1.
           */
          z_0_to_1,
        };

      /*!
       * Enumeration to describe auxiliary buffer support
       */
      enum auxiliary_buffer_t
        {
          /*!
           * No auxiliary buffer is present
           */
          no_auxiliary_buffer,

          /*!
           * Auxiliary buffer is realized by atomic operations;
           * To guarantee ordering between computing coverage
           * and using coverage a backend must intert a memory
           * barrier at the API level between such passed
           * (for example in GL, glMemoryBarrier()).The buffer
           * is realized as an "r32ui" image2D in the shader source.
           */
          auxiliary_buffer_atomic,

          /*!
           * Auxiliary buffer is present and ordering guarantees
           * are implemented by an interlock that can be called
           * in GLSL from any function and/or control flow. This
           * allows for cover then draw methods to be performed
           * WITHOUT any draw-breaks. The buffer is realized as
           * an "r8" image2D in the shader source. A backend will
           * need to define the the functions (or macros) in their
           * GLSL preamble:
           *  - fastuidraw_begin_aux_interlock() which is called before access
           *  - fastuidraw_end_aux_interlock() which is called after access
           */
          auxiliary_buffer_interlock,

          /*!
           * Auxiliary buffer is present and ordering guarantees
           * are implemented by an interlock that can only be
           * called in GLSL from main under NO conrol flow. This
           * allows for cover then draw methods to be performed
           * WITHOUT any draw-breaks. The buffer is realized as
           * an "r8" image2D in the shader source. A backend will
           * need to define the the functions (or macros) in their
           * GLSL preamble:
           *  - fastuidraw_begin_aux_interlock() which is called before access
           *  - fastuidraw_end_aux_interlock() which is called after access
           */
          auxiliary_buffer_interlock_main_only,

          /*!
           * Auxiliary buffer is present and ordering guarantees
           * are implemented via framebuffer fetch, i.e. the auxiliary
           * buffer is present as an inout global with type float
           * and layout(location = 1).
           */
          auxiliary_buffer_framebuffer_fetch,
        };

      /*!
       * \brief
       * Enumeration to describe vertex shader input
       * slot layout.
       */
      enum vertex_shader_in_layout
        {
          /*!
           * Slot for the values of PainterAttribute::m_primary_attrib
           * of PainterDraw::m_attributes
           */
          primary_attrib_slot = 0,

          /*!
           * Slot for the values of PainterAttribute::m_secondary_attributes
           * of PainterDraw::m_attributes
           */
          secondary_attrib_slot,

          /*!
           * Slot for the values of PainterAttribute::m_uint_attrib
           * of PainterDraw::m_attributes
           */
          uint_attrib_slot,

          /*!
           * Slot for the values of PainterDraw::m_header_attributes
           */
          header_attrib_slot,
        };

      /*!
       * \brief
       * Specifies the binding points (given in GLSL by layout(binding = ))
       * for the textures and buffers used by the uber-shader.
       */
      class BindingPoints
      {
      public:
        /*!
         * Ctor.
         */
        BindingPoints(void);

        /*!
         * Copy ctor.
         * \param obj value from which to copy
         */
        BindingPoints(const BindingPoints &obj);

        ~BindingPoints();

        /*!
         * Assignment operator
         * \param rhs value from which to copy
         */
        BindingPoints&
        operator=(const BindingPoints &rhs);

        /*!
         * Swap operation
         * \param obj object with which to swap
         */
        void
        swap(BindingPoints &obj);

        /*!
         * Specifies the binding point for ColorStopAtlas::backing_store().
         * The data type for the uniform is decided from the value
         * of UberShaderParams::colorstop_atlas_backing():
         * - sampler1DArray if value is colorstop_texture_1d_array
         * - sampler2DArray if value is colorstop_texture_2d_array
         */
        unsigned int
        colorstop_atlas(void) const;

        /*!
         * Set the value returned by colorstop_atlas(void) const.
         * Default value is 0.
         */
        BindingPoints&
        colorstop_atlas(unsigned int);

        /*!
         * Specifies the binding point for the sampler2DArray
         * with nearest filtering backed by ImageAtlas::color_store().
         */
        unsigned int
        image_atlas_color_tiles_nearest(void) const;

        /*!
         * Set the value returned by (void) const.
         * Default value is 1.
         */
        BindingPoints&
        image_atlas_color_tiles_nearest(unsigned int);

        /*!
         * Specifies the binding point for the sampler2DArray
         * with linear filtering backed by ImageAtlas::color_store().
         */
        unsigned int
        image_atlas_color_tiles_linear(void) const;

        /*!
         * Set the value returned by image_atlas_color_tiles_linear(void) const.
         * Default value is 2.
         */
        BindingPoints&
        image_atlas_color_tiles_linear(unsigned int);

        /*!
         * Specifies the binding point for the usampler2DArray
         * backed by ImageAtlas::index_store().
         */
        unsigned int
        image_atlas_index_tiles(void) const;

        /*!
         * Set the value returned by image_atlas_index_tiles(void) const.
         * Default value is 3.
         */
        BindingPoints&
        image_atlas_index_tiles(unsigned int);

        /*!
         * Specifies the binding point for the usampler2DArray
         * backed by GlyphAtlas::texel_store().
         */
        unsigned int
        glyph_atlas_texel_store_uint(void) const;

        /*!
         * Set the value returned by glyph_atlas_texel_store_uint(void) const.
         * Default value is 4.
         */
        BindingPoints&
        glyph_atlas_texel_store_uint(unsigned int);

        /*!
         * Specifies the binding point for the sampler2DArray
         * backed by GlyphAtlas::texel_store(). Only active
         * if UberShaderParams::have_float_glyph_texture_atlas()
         * is true.
         */
        unsigned int
        glyph_atlas_texel_store_float(void) const;

        /*!
         * Set the value returned by glyph_atlas_texel_store_float(void) const.
         * Default value is 5.
         */
        BindingPoints&
        glyph_atlas_texel_store_float(unsigned int);

        /*!
         * Specifies the binding point for the sampler2DArray
         * or samplerBuffer backed by GlyphAtlas::geometry_store().
         * The data type for the uniform is decided from the value
         * of UberShaderParams::glyph_geometry_backing():
         * - sampler2DArray if value is glyph_geometry_texture_array
         * - samplerBuffer if value is glyph_geometry_tbo
         */
        unsigned int
        glyph_atlas_geometry_store_texture(void) const;

        /*!
         * Set the value returned by
         * glyph_atlas_geometry_store_texture(void) const.
         * Default value is 6.
         */
        BindingPoints&
        glyph_atlas_geometry_store_texture(unsigned int);

        /*!
         * Specifies the binding point for an SSBO that backs
         * GlyphAtlas::geometry_store(); only active if the
         * geometry strore is backed by an SSBO.
         */
        unsigned int
        glyph_atlas_geometry_store_ssbo(void) const;

        /*!
         * Set the value returned by
         * glyph_atlas_geometry_store_ssbo(void) const.
         * Default value is 0.
         */
        BindingPoints&
        glyph_atlas_geometry_store_ssbo(unsigned int);

        /*!
         * Provided as a conveniance, returns one of \ref
         * glyph_atlas_geometry_store_texture() or
         * glyph_atlas_geometry_store_ssbo() based off of
         * the value of the passed \ref
         * glyph_geometry_backing_t.
         */
        unsigned int
        glyph_atlas_geometry_store(enum glyph_geometry_backing_t) const;

        /*!
         * Specifies the binding point of the UBO for uniforms.
         * Only active if UberShaderParams::use_ubo_for_uniforms()
         * is true.
         */
        unsigned int
        uniforms_ubo(void) const;

        /*!
         * Set the value returned by uniforms_ubo(void) const.
         * Default value is 1.
         */
        BindingPoints&
        uniforms_ubo(unsigned int);

        /*!
         * Specifies the buffer binding point of the data store
         * buffer (PainterDraw::m_store) as a samplerBuffer.
         * Only active if UberShaderParams::data_store_backing()
         * is \ref data_store_tbo.
         */
        unsigned int
        data_store_buffer_tbo(void) const;

        /*!
         * Set the value returned by data_store_buffer_tbo(void) const.
         * Default value is 7.
         */
        BindingPoints&
        data_store_buffer_tbo(unsigned int);

        /*!
         * Specifies the buffer binding point of the data store
         * buffer (PainterDraw::m_store) as a UBO.
         * Only active if UberShaderParams::data_store_backing()
         * is \ref data_store_ubo.
         */
        unsigned int
        data_store_buffer_ubo(void) const;

        /*!
         * Set the value returned by data_store_buffer_ubo(void) const.
         * Default value is 0.
         */
        BindingPoints&
        data_store_buffer_ubo(unsigned int);

        /*!
         * Specifies the buffer binding point of the data store
         * buffer (PainterDraw::m_store) as an SSBO.
         * Only active if UberShaderParams::data_store_backing()
         * is \ref data_store_ssbo.
         */
        unsigned int
        data_store_buffer_ssbo(void) const;

        /*!
         * Set the value returned by data_store_buffer_ssbo(void) const.
         * Default value is 1.
         */
        BindingPoints&
        data_store_buffer_ssbo(unsigned int);

        /*!
         * Provided as a conveniance, returns one of \ref
         * data_store_buffer_tbo(), data_store_buffer_ubo()
         * or data_store_buffer_ssbo() based off of the
         * value of the passed \ref data_store_backing_t.
         */
        unsigned int
        data_store_buffer(enum data_store_backing_t) const;

        /*!
         * Specifies the binding point for the image2D (r8)
         * auxiliary image buffer; only active if
         * UberShaderParams::provide_auxiliary_image_buffer()
         * is true. Default value is 0.
         */
        unsigned int
        auxiliary_image_buffer(void) const;

        /*!
         * Set the value returned by auxiliary_image_buffer(void) const.
         * Default value is 0.
         */
        BindingPoints&
        auxiliary_image_buffer(unsigned int);

        /*!
         * Specifies the binding point for the image2D (rgba8) color
         * buffer; only active if UberShaderParams::compositing_type()
         * is \ref compositing_interlock. Default value is 1.
         */
        unsigned int
        color_interlock_image_buffer(void) const;

        /*!
         * Set the value returned by color_interlock_image_buffer(void) const.
         * Default value is 0.
         */
        BindingPoints&
        color_interlock_image_buffer(unsigned int);

        /*!
         * Specifies the binding point of an external texture.
         */
        unsigned int
        external_texture(void) const;

        /*!
         * Set the value returned by external_texture(void) const.
         * Default value is 8.
         */
        BindingPoints&
        external_texture(unsigned int);

      private:
        void *m_d;
      };

      /*!
       * \brief
       * An UberShaderParams specifies how to construct an uber-shader.
       * Note that the usage of HW clip-planes is specified by
       * ConfigurationGLSL, NOT UberShaderParams.
       */
      class UberShaderParams
      {
      public:
        /*!
         * Ctor.
         */
        UberShaderParams(void);

        /*!
         * Copy ctor.
         * \param obj value from which to copy
         */
        UberShaderParams(const UberShaderParams &obj);

        ~UberShaderParams();

        /*!
         * Assignment operator
         * \param rhs value from which to copy
         */
        UberShaderParams&
        operator=(const UberShaderParams &rhs);

        /*!
         * Swap operation
         * \param obj object with which to swap
         */
        void
        swap(UberShaderParams &obj);

        /*!
         * Returns how the painter will perform compositing.
         */
        enum compositing_type_t
        compositing_type(void) const;

        /*!
         * Specify the return value to compositing_type() const.
         * Default value is \ref compositing_dual_src
         * \param tp composite shader type
         */
        UberShaderParams&
        compositing_type(enum compositing_type_t tp);

        /*!
         * Provided as a conveniance, returns the value of
         * compositing_type(void) const as a \ref
         * PainterCompositeShader::shader_type.
         */
        enum PainterCompositeShader::shader_type
        composite_type(void) const;

        /*!
         * If true, indicates that the PainterRegistrar supports
         * bindless texturing. Default value is false.
         */
        bool
        supports_bindless_texturing(void) const;

        /*!
         * Specify the return value to supports_bindless_texturing() const.
         * Default value is false.
         */
        UberShaderParams&
        supports_bindless_texturing(bool);

        /*!
         * Specifies how the uber-shader will perform clipping.
         */
        enum clipping_type_t
        clipping_type(void) const;

        /*!
         * Set the value returned by clipping_type(void) const.
         * Default value is \ref clipping_via_gl_clip_distance.
         */
        UberShaderParams&
        clipping_type(enum clipping_type_t);

        /*!
         * Specifies the normalized device z-coordinate convention
         * that the shader is to use.
         */
        enum z_coordinate_convention_t
        z_coordinate_convention(void) const;

        /*!
         * Set the value returned by normalized_device_z_coordinate_convention(void) const.
         * Default value is z_minus_1_to_1.
         */
        UberShaderParams&
        z_coordinate_convention(enum z_coordinate_convention_t);

        /*!
         * If set to true, negate te y-coordinate of
         * gl_Position before emitting it. The convention
         * in FastUIDraw is that normalized coordinates are
         * so that the top of the window is y = -1 and the
         * bottom is y = 1. For those API's that have this
         * reversed (for example Vulkan), then set this to
         * true.
         */
        bool
        negate_normalized_y_coordinate(void) const;

        /*!
         * Set the value returned by negate_normalized_y_coordinate(void) const.
         * Default value is false.
         */
        UberShaderParams&
        negate_normalized_y_coordinate(bool);

        /*!
         * if true, assign the slot location of the vertex shader
         * inputs (via layout(location =) in GLSL ). The layout
         * locations are defined by the enumeration
         * vertex_shader_in_layout.
         */
        bool
        assign_layout_to_vertex_shader_inputs(void) const;

        /*!
         * Set the value returned by assign_layout_to_vertex_shader_inputs(void) const.
         * Default value is true.
         */
        UberShaderParams&
        assign_layout_to_vertex_shader_inputs(bool);

        /*!
         * If true, assign the slot locations (via layout(location = ) in GLSL)
         * for the varyings of the uber-shaders.
         */
        bool
        assign_layout_to_varyings(void) const;

        /*!
         * Set the value returned by assign_layout_to_varyings(void) const.
         * Default value is true.
         */
        UberShaderParams&
        assign_layout_to_varyings(bool);

        /*!
         * If true, assign binding points (via layout(binding = ) in GLSL)
         * to the buffers and surfaces of the uber-shaders. The values
         * for the binding are set by binding_points(const BindingPoints&).
         */
        bool
        assign_binding_points(void) const;

        /*!
         * Set the value returned by assign_binding_points(void) const.
         * Default value is true.
         */
        UberShaderParams&
        assign_binding_points(bool);

        /*!
         * Specifies the binding points to use for surfaces and
         * buffers of the uber-shaders. Values only have effect
         * if assign_binding_points(void) const returns true.
         */
        const BindingPoints&
        binding_points(void) const;

        /*!
         * Set the value returned by binding_points(void) const.
         * Default value is a default constructed BindingPoints
         * object.
         */
        UberShaderParams&
        binding_points(const BindingPoints&);

        /*!
         * If true, use a switch() in the uber-vertex shader to
         * dispatch to the PainterItemShader.
         */
        bool
        vert_shader_use_switch(void) const;

        /*!
         * Set the value returned by vert_shader_use_switch(void) const.
         * Default value is false.
         */
        UberShaderParams&
        vert_shader_use_switch(bool);

        /*!
         * If true, use a switch() in the uber-fragment shader to
         * dispatch to the PainterItemShader.
         */
        bool
        frag_shader_use_switch(void) const;

        /*!
         * Set the value returned by frag_shader_use_switch(void) const.
         * Default value is false.
         */
        UberShaderParams&
        frag_shader_use_switch(bool);

        /*!
         * If true, use a switch() in the uber-fragment shader to
         * dispatch to the PainterCompositeShader.
         */
        bool
        composite_shader_use_switch(void) const;

        /*!
         * Set the value returned by composite_shader_use_switch(void) const.
         * Default value is false.
         */
        UberShaderParams&
        composite_shader_use_switch(bool);

        /*!
         * If true, use a switch() in the uber-fragment shader to
         * dispatch to the PainterBlendShader.
         */
        bool
        blend_shader_use_switch(void) const;

        /*!
         * Set the value returned by blend_shader_use_switch(void) const.
         * Default value is false.
         */
        UberShaderParams&
        blend_shader_use_switch(bool);

        /*!
         * If true, unpack the PainterBrush data in the fragment shader.
         * If false, unpack the data in the vertex shader and forward
         * the data to the fragment shader via flat varyings.
         */
        bool
        unpack_header_and_brush_in_frag_shader(void) const;

        /*!
         * Set the value returned by unpack_header_and_brush_in_frag_shader(void) const.
         * Default value is false.
         */
        UberShaderParams&
        unpack_header_and_brush_in_frag_shader(bool);

        /*!
         * Specify how to access the data in PainterDraw::m_store
         * from the GLSL shader.
         */
        enum data_store_backing_t
        data_store_backing(void) const;

        /*!
         * Set the value returned by data_store_backing(void) const.
         * Default value is data_store_tbo.
         */
        UberShaderParams&
        data_store_backing(enum data_store_backing_t);

        /*!
         * Only needed if data_store_backing(void) const
         * has value data_store_ubo. Gives the size in
         * blocks of PainterDraw::m_store which
         * is PainterDraw::m_store.size() divided
         * by PainterBackend::ConfigurationBase::alignment().
         */
        int
        data_blocks_per_store_buffer(void) const;

        /*!
         * Set the value returned by data_blocks_per_store_buffer(void) const
         * Default value is -1.
         */
        UberShaderParams&
        data_blocks_per_store_buffer(int);

        /*!
         * Specifies how the glyph geometry data (GlyphAtlas::geometry_store())
         * is accessed from the uber-shaders.
         */
        enum glyph_geometry_backing_t
        glyph_geometry_backing(void) const;

        /*!
         * Set the value returned by glyph_geometry_backing(void) const.
         * Default value is glyph_geometry_tbo.
         */
        UberShaderParams&
        glyph_geometry_backing(enum glyph_geometry_backing_t);

        /*!
         * Only used if glyph_geometry_backing(void) const has value
         * glyph_geometry_texture_array. Gives the log2 of the
         * width and height of the texture array backing the
         * glyph geometry data (GlyphAtlas::geometry_store()).
         * Note: it must be that the width and height of the backing
         * 2D texture array are powers of 2.
         */
        ivec2
        glyph_geometry_backing_log2_dims(void) const;

        /*!
         * Set the value returned by glyph_geometry_backing_log2_dims(void) const.
         * Default value is (-1, -1).
         */
        UberShaderParams&
        glyph_geometry_backing_log2_dims(ivec2);

        /*!
         * If true, can access the data of GlyphAtlas::texel_store() as a
         * sampler2DArray as well.
         */
        bool
        have_float_glyph_texture_atlas(void) const;

        /*!
         * Set the value returned by have_float_glyph_texture_atlas(void) const.
         * Default value is true.
         */
        UberShaderParams&
        have_float_glyph_texture_atlas(bool);

        /*!
         * Specifies how the bakcing store to the color stop atlas
         * (ColorStopAtlas::backing_store()) is accessed from the
         * uber-shaders.
         */
        enum colorstop_backing_t
        colorstop_atlas_backing(void) const;

        /*!
         * Set the value returned by colorstop_atlas_backing(void) const.
         * Default value is colorstop_texture_1d_array.
         */
        UberShaderParams&
        colorstop_atlas_backing(enum colorstop_backing_t);

        /*!
         * If true, use a UBO to back the uniforms of the
         * uber-shader. If false, use an array of uniforms
         * instead. The name of the UBO block is
         * fastuidraw_shader_uniforms and the name of the
         * uniform is fastuidraw_shader_uniforms. In both cases,
         * the buffer can be filled by the function
         * PainterShaderRegistrarGLSL::fill_uniform_buffer().
         * For the non-UBO case, the uniforms are realized
         * as an array of floats in GLSL.
         */
        bool
        use_ubo_for_uniforms(void) const;

        /*!
         * Set the value returned by use_ubo_for_uniforms(void) const.
         * Default value is true.
         */
        UberShaderParams&
        use_ubo_for_uniforms(bool);

        /*!
         * If true, provide an image2D (of type r8) uniform to
         * which to write coverage value for multi-pass shaders
         * (in particular shader based ant-aliased stroking).
         * Writing to the buffer should not be done, instead
         * one should use the functions:
         * - float fastuidraw_clear_auxiliary(void): clears the value to 0 and returns the old value
         * - void fastuidraw_max_auxiliary(in float v): maxes the value with the passed value
         */
        enum auxiliary_buffer_t
        provide_auxiliary_image_buffer(void) const;

        /*!
         * Set the value returned by
         * provide_auxiliary_image_buffer(void) const.
         * Default value is \ref no_auxiliary_buffer.
         */
        UberShaderParams&
        provide_auxiliary_image_buffer(enum auxiliary_buffer_t);

        /*!
         * If the PainterShaderRegistrarGLSL has bindless texturing enabled,
         * (see supports_bindless_texturing()) then have that the
         * handles to create sampler2D object is a uvec2. If false,
         * use uint64_t as the handle type in the GLSL source code.
         * Default value is true.
         */
        bool
        use_uvec2_for_bindless_handle(void) const;

        /*!
         * Set the value returned by use_uvec2_for_bindless_handle(void) const.
         * Default value is true.
         */
        UberShaderParams&
        use_uvec2_for_bindless_handle(bool);

        /*!
         * Returns a PainterShaderSet derived from the current values
         * of this UberShaderParams.
         */
        PainterShaderSet
        default_shaders(enum PainterStrokeShader::type_t stroke_tp,
                        const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass1,
                        const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass2) const;

      private:
        void *m_d;
      };

      /*!
       * A BackendConstants stores constants coming from a backend
       * implementation that change the GLSL uber-shaders made by
       *  PainterShaderRegisterGLSL::construct_shader().
       */
      class BackendConstants
      {
      public:
        /*!
         * Ctor.
         * \param p if non-null, set all values from the passed PainterBackend object,
         *                       otherwise set all values as 0.
         */
        BackendConstants(PainterBackend *p = nullptr);

        /*!
         * Copy ctor.
         * \param obj value from which to copy
         */
        BackendConstants(const BackendConstants &obj);

        ~BackendConstants();

        /*!
         * Assignment operator
         * \param rhs value from which to copy
         */
        BackendConstants&
        operator=(const BackendConstants &rhs);

        /*!
         * Swap operation
         * \param obj object with which to swap
         */
        void
        swap(BackendConstants &obj);

        /*!
         * Should be the same value as PainterBackend::ConfigurationBase::alignment()
         * of PainterBackend::configuration_base().
         */
        int
        data_store_alignment(void) const;

        /*!
         * Set the value returned by data_store_alignment(void) const.
         */
        BackendConstants&
        data_store_alignment(int);

        /*!
         * Should be the same as GlyphAtlasGeometryBackingStoreBase::alignment()
         * of GlyphAtlas::geometry_store().
         */
        int
        glyph_atlas_geometry_store_alignment(void) const;

        /*!
         * Set the value returned by glyph_atlas_geometry_store_alignment(void) const.
         */
        BackendConstants&
        glyph_atlas_geometry_store_alignment(int);

        /*!
         * Should be the same value as GlyphAtlasTexelBackingStoreBase::dimensions() .x()
         * of GlyphAtlas::texel_store()
         */
        int
        glyph_atlas_texel_store_width(void) const;

        /*!
         * Set the value returned by glyph_atlas_texel_store_width(void) const.
         */
        BackendConstants&
        glyph_atlas_texel_store_width(int);

        /*!
         * Should be the same value as GlyphAtlasTexelBackingStoreBase::dimensions() .y()
         * of GlyphAtlas::texel_store()
         */
        int
        glyph_atlas_texel_store_height(void) const;

        /*!
         * Set the value returned by glyph_atlas_texel_store_height(void) const.
         */
        BackendConstants&
        glyph_atlas_texel_store_height(int);

        /*!
         * Should be the same value as AtlasColorBackingStoreBase::dimensions() .x()
         * of ImageAtlas::color_store()
         */
        int
        image_atlas_color_store_width(void) const;

        /*!
         * Set the value returned by image_atlas_color_store_width(void) const.
         */
        BackendConstants&
        image_atlas_color_store_width(int);

        /*!
         * Should be the same value as AtlasColorBackingStoreBase::dimensions() .y()
         * of ImageAtlas::color_store()
         */
        int
        image_atlas_color_store_height(void) const;

        /*!
         * Set the value returned by image_atlas_color_store_height(void) const.
         */
        BackendConstants&
        image_atlas_color_store_height(int);

        /*!
         * Should be the same as ImageAtlas::index_tile_size()
         */
        int
        image_atlas_index_tile_size(void) const;

        /*!
         * Set the value returned by image_atlas_index_tile_size(void) const.
         */
        BackendConstants&
        image_atlas_index_tile_size(int);

        /*!
         * Should be the same as ImageAtlas::color_tile_size()
         * and must be a power of 2.
         */
        int
        image_atlas_color_tile_size(void) const;

        /*!
         * Set the value returned by image_atlas_color_tile_size(void) const.
         */
        BackendConstants&
        image_atlas_color_tile_size(int);

        /*!
         * Should be the same value as ColorStopBackingStore::dimensions().x()
         * of ColorStopAtlas::backing_store()
         */
        int
        colorstop_atlas_store_width(void) const;

        /*!
         * Set the value returned by color_atlas_store_width(void) const.
         */
        BackendConstants&
        colorstop_atlas_store_width(int);

        /*!
         * Set all values of this BackendConstant by taking values
         * from a PainterBackend.
         */
        BackendConstants&
        set_from_backend(PainterBackend *p);

        /*!
         * Set all values of this BackendConstant by taking values
         * from a PainterBackend.
         */
        BackendConstants&
        set_from_atlas(const reference_counted_ptr<ImageAtlas> &p);

        /*!
         * Set all values of this BackendConstant by taking values
         * from a PainterBackend.
         */
        BackendConstants&
        set_from_atlas(const reference_counted_ptr<GlyphAtlas> &p);

        /*!
         * Set all values of this BackendConstant by taking values
         * from a PainterBackend.
         */
        BackendConstants&
        set_from_atlas(const reference_counted_ptr<ColorStopAtlas> &p);

      private:
        void *m_d;
      };

      /*!
       * \brief
       * An ItemShaderFilter is used to specify whether or not
       * to include a named shader when creating an uber-shader.
       */
      class ItemShaderFilter
      {
      public:
        virtual
        ~ItemShaderFilter(void)
        {}

        /*!
         * To be implemented by a derived class to return true
         * if the named shader should be included in the uber-shader.
         */
        virtual
        bool
        use_shader(const reference_counted_ptr<PainterItemShaderGLSL> &shader) const = 0;
      };
    };

    /*!
     * \brief
     * A PainterShaderRegistrarGLSL is an implementation of PainterRegistrar
     * that assembles the shader source code of PainterItemShaderGLSL
     * and PainterCompositeShaderGLSL into an uber-shader.
     */
    class PainterShaderRegistrarGLSL:
      public PainterShaderRegistrar,
      public PainterShaderRegistrarGLSLTypes
    {
    public:

      /*!
       * Ctor.
       */
      explicit
      PainterShaderRegistrarGLSL(void);

      ~PainterShaderRegistrarGLSL();

      /*!
       * Add GLSL code that is to be visible to all vertex
       * shaders. The code can define functions or macros.
       * \param src shader source to add
       */
      void
      add_vertex_shader_util(const ShaderSource &src);

      /*!
       * Add GLSL code that is to be visible to all vertex
       * shaders. The code can define functions or macros.
       * \param src shader source to add
       */
      void
      add_fragment_shader_util(const ShaderSource &src);

      /*!
       * Add the uber-vertex and fragment shaders to given ShaderSource values.
       * The \ref Mutex mutex() is NOT locked during this call, a caller should
       * lock the mutex before calling it. This way a derived class can use the
       * same lock as used by the PainterShaderRegistrarGLSL.
       * \param backend_constants constant values that affect the created uber-shader.
       * \param out_vertex ShaderSource to which to add uber-vertex shader
       * \param out_fragment ShaderSource to which to add uber-fragment shader
       * \param contruct_params specifies how to construct the uber-shaders.
       * \param item_shader_filter pointer to ItemShaderFilter to use to filter
       *                           which shader to place into the uber-shader.
       *                           A value of nullptr indicates to add all item
       *                           shaders to the uber-shader.
       * \param discard_macro_value macro-value definintion for the macro
       *                            FASTUIDRAW_DISCARD. PainterItemShaderGLSL
       *                            fragment sources use FASTUIDRAW_DISCARD
       *                            instead of discard.
       */
      void
      construct_shader(const BackendConstants &backend_constants,
                       ShaderSource &out_vertex,
                       ShaderSource &out_fragment,
                       const UberShaderParams &contruct_params,
                       const ItemShaderFilter *item_shader_filter = nullptr,
                       c_string discard_macro_value = "discard");
      /*!
       * Returns the total number of shaders (item and composite)
       * registered to this PainterShaderRegistrarGLSL; a derived class
       * should track this count value and use it to determine
       * when it needs to reconstruct its uber-shader. The mutex()
       * is NOT locked for the duration of the function.
       */
      unsigned int
      registered_shader_count(void);

      /*!
       * Fill a buffer to hold the values used by the uber-shader.
       * The buffer must be that p.size() is atleast ubo_size().
       * \param vwp current PainterBackend::Surface::Viewport to which is being rendered
       * \param p buffer to which to fill uniform data
       */
      void
      fill_uniform_buffer(const PainterBackend::Surface::Viewport &vwp,
                          c_array<generic_data> p);

      /*!
       * Total size of UBO for uniforms in units of
       * generic_data, see also fill_uniform_ubo().
       */
      static
      uint32_t
      ubo_size(void);

    protected:

      /*!
       * To be optionally implemented by a derived class to
       * compute the shader group of a PainterItemShader.
       * The passed shader may or may not be a sub-shader.
       * The mutex() is locked for the duration of the function.
       * Default implementation is to return 0.
       * \param tag The value of PainterShader::tag() that PainterShaderRegistrarGLSL
       *            will assign to the shader. Do NOT access PainterShader::tag(),
       *            PainterShader::ID() or PainterShader::group() as they are
       *            not yet assgined.
       * \param shader shader whose group is to be computed
       */
      virtual
      uint32_t
      compute_item_shader_group(PainterShader::Tag tag,
                                const reference_counted_ptr<PainterItemShader> &shader);

      /*!
       * To be optionally implemented by a derived class to
       * compute the shader group of a PainterItemShader.
       * The passed shader may or may not be a sub-shader.
       * The mutex() is locked for the duration of the function.
       * Default implementation is to return 0.
       * \param tag The value of PainterShader::tag() that PainterShaderRegistrarGLSL
       *            will assign to the shader. Do NOT access PainterShader::tag(),
       *            PainterShader::ID() or PainterShader::group() as they are
       *            not yet assgined.
       * \param shader shader whose group is to be computed
       */
      virtual
      uint32_t
      compute_composite_shader_group(PainterShader::Tag tag,
                                     const reference_counted_ptr<PainterCompositeShader> &shader);

      /*!
       * To be optionally implemented by a derived class to
       * compute the shader group of a PainterBlendShader.
       * The passed shader may or may not be a sub-shader.
       * The mutex() is locked for the duration of the function.
       * Default implementation is to return 0.
       * \param tag The value of PainterShader::tag() that PainterShaderRegistrarGLSL
       *            will assign to the shader. Do NOT access PainterShader::tag(),
       *            PainterShader::ID() or PainterShader::group() as they are
       *            not yet assgined.
       * \param shader shader whose group is to be computed
       */
      virtual
      uint32_t
      compute_blend_shader_group(PainterShader::Tag tag,
                                 const reference_counted_ptr<PainterBlendShader> &shader);

      //////////////////////////////////////////////////////////////
      // virtual methods from PainterRegistrar, do NOT reimplement(!)
      virtual
      PainterShader::Tag
      absorb_item_shader(const reference_counted_ptr<PainterItemShader> &shader);

      virtual
      uint32_t
      compute_item_sub_shader_group(const reference_counted_ptr<PainterItemShader> &shader);

      virtual
      PainterShader::Tag
      absorb_composite_shader(const reference_counted_ptr<PainterCompositeShader> &shader);

      virtual
      uint32_t
      compute_composite_sub_shader_group(const reference_counted_ptr<PainterCompositeShader> &shader);

      virtual
      PainterShader::Tag
      absorb_blend_shader(const reference_counted_ptr<PainterBlendShader> &shader);

      virtual
      uint32_t
      compute_blend_sub_shader_group(const reference_counted_ptr<PainterBlendShader> &shader);

    private:
      void *m_d;
    };

/*! @} */

  }
}
