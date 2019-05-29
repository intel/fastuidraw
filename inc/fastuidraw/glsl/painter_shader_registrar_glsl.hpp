/*!
 * \file painter_shader_registrar_glsl.hpp
 * \brief file painter_shader_registrar_glsl.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#pragma once

#include <fastuidraw/painter/backend/painter_shader_registrar.hpp>
#include <fastuidraw/painter/backend/painter_engine.hpp>
#include <fastuidraw/painter/backend/painter_surface.hpp>
#include <fastuidraw/glsl/shader_source.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_blend_shader_glsl.hpp>

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
           * that is an array of uvec4.
           */
          data_store_ubo,

          /*!
           * Data store is backed by a shader storage buffer object
           * that is an array of uvec4.
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
           * to each clip-plane and (virtually) skipping the
           * color write. This is active if the active \ref
           * PainterBlendShader has PainterBlendShader::type()
           * as \ref PainterBlendShader::framebuffer_fetch.
           * For other blend-types, this is the same as \ref
           * clipping_via_discard.
           */
          clipping_via_skip_color_write,
        };

      /*!
       * \brief
       * Enumeration to specify how to perform framebuffer-fetch blending
       */
      enum fbf_blending_type_t
        {
          /*!
           * Indicates that framebuffer-fetch blending is not supported.
           */
          fbf_blending_not_supported,

          /*!
           * Use framebuffer fetch (i.e. the out of the fragment shader
           * is an inout).
           */
          fbf_blending_framebuffer_fetch,

          /*!
           * Have the color buffer realized as an image2D and use
           * fragment shader interlock to get blending order correct.
           */
          fbf_blending_interlock,
        };

      /*!
       * \brief
       * Enumeration to specify how to access the backing store
       * of the glyph data stored in GlyphAtlas::store()
       */
      enum glyph_data_backing_t
        {
          /*!
           * Use a samplerBuffer to access the data
           */
          glyph_data_tbo,

          /*!
           * Use a sampler2DArray to access the data
           */
          glyph_data_texture_array,

          /*!
           * Use a buffer block to access the data
           */
          glyph_data_ssbo,
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
       * \brief
       * Enumeration to describe vertex shader input
       * slot layout.
       */
      enum vertex_shader_in_layout
        {
          /*!
           * Slot for the values of PainterAttribute::m_attrib0
           * of PainterDraw::m_attributes
           */
          attribute0_slot = 0,

          /*!
           * Slot for the values of PainterAttribute::m_attrib1
           * of PainterDraw::m_attributes
           */
          attribute1_slot,

          /*!
           * Slot for the values of PainterAttribute::m_attrib2
           * of PainterDraw::m_attributes
           */
          attribute2_slot,

          /*!
           * Slot for the values of PainterDraw::m_header_attributes
           */
          header_attrib_slot,
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
        UberShaderParams&
        preferred_blend_type(enum PainterBlendShader::shader_type tp);

        /*!
         * Returns how the painter will perform blending.
         */
        enum fbf_blending_type_t
        fbf_blending_type(void) const;

        /*!
         * Specify the return value to fbf_blending_type() const.
         * Default value is \ref fbf_blending_not_supported
         * \param tp blend shader type
         */
        UberShaderParams&
        fbf_blending_type(enum fbf_blending_type_t tp);

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
         * if true, assign the slot location of the vertex shader
         * inputs (via layout(location =) in GLSL). The layout
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
         * is PainterDraw::m_store.size().
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
         * Specifies how the glyph data (GlyphAtlas::store())
         * is accessed from the uber-shaders.
         */
        enum glyph_data_backing_t
        glyph_data_backing(void) const;

        /*!
         * Set the value returned by glyph_data_backing(void) const.
         * Default value is glyph_data_tbo.
         */
        UberShaderParams&
        glyph_data_backing(enum glyph_data_backing_t);

        /*!
         * Only used if glyph_data_backing(void) const has value
         * glyph_data_texture_array. Gives the log2 of the
         * width and height of the texture array backing the
         * glyph data (GlyphAtlas::store()).
         * Note: it must be that the width and height of the backing
         * 2D texture array are powers of 2.
         */
        ivec2
        glyph_data_backing_log2_dims(void) const;

        /*!
         * Set the value returned by glyph_data_backing_log2_dims(void) const.
         * Default value is (-1, -1).
         */
        UberShaderParams&
        glyph_data_backing_log2_dims(ivec2);

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
         * fastuidraw_uniforms and the name of the
         * uniform is fastuidraw_uniforms. In both cases,
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
         * Returns the number of external textures (realized as
         * sampler2D uniforms) the uber-shader is to have.
         */
        unsigned int
        number_context_textures(void) const;

        /*!
         * Set the value returned by number_context_textures(void) const.
         * Default value is 1.
         */
        UberShaderParams&
        number_context_textures(unsigned int);

        /*!
         * Returns the binding point for ColorStopAtlas::backing_store()
         * derived from the current value of this UberShaderParams.
         * The data type for the uniform is decided from the value
         * colorstop_atlas_backing():
         * - sampler1DArray if value is colorstop_texture_1d_array
         * - sampler2DArray if value is colorstop_texture_2d_array
         */
        int
        colorstop_atlas_binding(void) const;

        /*!
         * Returns the binding point for the sampler2DArray
         * derived from the current value of this UberShaderParams
         * with nearest filtering backed by ImageAtlas::color_store().
         */
        int
        image_atlas_color_tiles_nearest_binding(void) const;

        /*!
         * Returns the binding point for the sampler2DArray
         * derived from the current value of this UberShaderParams
         * with linear filtering backed by ImageAtlas::color_store().
         */
        int
        image_atlas_color_tiles_linear_binding(void) const;

        /*!
         * Returns the binding point for the usampler2DArray
         * derived from the current value of this UberShaderParams
         * backed by ImageAtlas::index_store().
         */
        int
        image_atlas_index_tiles_binding(void) const;

        /*!
         * Returns the binding point for the \ref GlyphAtlas
         * derived from the current value of this UberShaderParams.
         */
        int
        glyph_atlas_store_binding(void) const;

        /*!
         * Returns the binding point for the \ref GlyphAtlas
         * to access each value as a vec2 fp16 value. A value
         * of -1 indicates that there is no special binding
         * point for such access.
         */
        int
        glyph_atlas_store_binding_fp16x2(void) const;

        /*!
         * Returns the binding point of the data store buffer
         * derived from the current value of this UberShaderParams.
         */
        int
        data_store_buffer_binding(void) const;

        /*!
         * Returns the binding point of the first external texture
         * in their binding points; subsequence external textures
         * immediately follow the 1st.
         * derived from the current value of this UberShaderParams.
         */
        int
        context_texture_binding(void) const;

        /*!
         * Returns the binding point of the named external texture
         * derived from the current value of this UberShaderParams.
         */
        int
        context_texture_binding(unsigned int v) const
        {
          FASTUIDRAWassert(v < number_context_textures());
          return context_texture_binding() + v;
        }

        /*!
         * Returns the binding point of the deferred coverage
         * buffer (ala PainterSurface::deferred_coverage_buffer_type)
         * for reading.
         */
        int
        coverage_buffer_texture_binding(void) const;

        /*!
         * Specifies the binding point for the image2D (rgba8)
         * color buffer; derived from the current value only
         * active of this UberShaderParams; A return value of
         * -1 indicates that the color buffer does not use any
         * binding point.
         */
        int
        color_interlock_image_buffer_binding(void) const;

        /*!
         * Returns the binding point of the UBO for uniforms
         * derived from the current value of this UberShaderParams.
         * A return value of -1 indicates that the uniforms
         * does not use any binding points.
         */
        int
        uniforms_ubo_binding(void) const;

        /*!
         * Returns the number of UBO binding units used derived
         * from the current values of this UberShaderParams; the
         * units used are 0, 1, ..., num_ubo_units() - 1.
         */
        unsigned int
        num_ubo_units(void) const;

        /*!
         * Returns the number of SSBO binding units used derived
         * from the current values of this UberShaderParams; the
         * units used are 0, 1, ..., num_ssbo_units() - 1.
         */
        unsigned int
        num_ssbo_units(void) const;

        /*!
         * Returns the number of texture binding units used derived
         * from the current values of this UberShaderParams; the
         * units used are 0, 1, ..., num_texture_units() - 1.
         */
        unsigned int
        num_texture_units(void) const;

        /*!
         * Returns the number of image binding units used derived
         * from the current values of this UberShaderParams; the
         * units used are 0, 1, ..., num_image_units() - 1.
         */
        unsigned int
        num_image_units(void) const;

        /*!
         * Returns a PainterShaderSet derived from the current values
         * of this UberShaderParams.
         */
        PainterShaderSet
        default_shaders(void) const;

      private:
        void *m_d;
      };

      /*!
       * A BackendConstants stores constants coming from a backend
       * implementation that change the GLSL uber-shaders made by
       * PainterShaderRegisterGLSL::construct_shader().
       */
      class BackendConstants
      {
      public:
        /*!
         * Ctor.
         * \param p if non-null, set all values from the passed \ref
         *                       PainterEngine object,
         *                       otherwise set all values as 0.
         */
        BackendConstants(PainterEngine *p = nullptr);

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
         * Should be the same value as AtlasColorBackingStoreBase::dimensions() .x()
         * of ImageAtlas::color_store(). A value of zero indicates that sourcing
         * from an \ref Image with \ref Image::type() having value \ref Image::on_atlas
         * is not supported (i.e. there is no image-atlasing).
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
         * of ImageAtlas::color_store(). A value of zero indicates that sourcing
         * from an \ref Image with \ref Image::type() having value \ref Image::on_atlas
         * is not supported (i.e. there is no image-atlasing).
         */
        int
        image_atlas_color_store_height(void) const;

        /*!
         * Set the value returned by image_atlas_color_store_height(void) const.
         */
        BackendConstants&
        image_atlas_color_store_height(int);

        /*!
         * Should be the same as ImageAtlas::index_tile_size() and must be a power
         * of 2. A value of zero indicates that sourcing from an \ref Image with
         * \ref Image::type() having value \ref Image::on_atlas is not supported
         * (i.e. there is no image-atlasing).
         */
        int
        image_atlas_index_tile_size(void) const;

        /*!
         * Set the value returned by image_atlas_index_tile_size(void) const.
         */
        BackendConstants&
        image_atlas_index_tile_size(int);

        /*!
         * Should be the same as ImageAtlas::color_tile_size() and must be a power
         * of 2. A value of zero indicates that sourcing from an \ref Image with
         * \ref Image::type() having value \ref Image::on_atlas is not supported
         * (i.e. there is no image-atlasing).
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
         * from a \ref PainterEngine.
         */
        BackendConstants&
        set_from_backend(PainterEngine *p);

        /*!
         * Set all values of this BackendConstant by taking values
         * from a PainterBackend.
         */
        BackendConstants&
        set_from_atlas(ImageAtlas &p);

        /*!
         * Set all values of this BackendConstant by taking values
         * from a PainterBackend.
         */
        BackendConstants&
        set_from_atlas(ColorStopAtlas &p);

      private:
        void *m_d;
      };

      /*!
       * \brief
       * An ShaderFilter is used to specify whether or not
       * to include a named shader when creating an uber-shader.
       * \tparam ShaderType should be \ref PainterItemShaderGLSL
       *                    or \ref PainterItemCoverageShaderGLSL
       */
      template<typename ShaderType>
      class ShaderFilter
      {
      public:
        virtual
        ~ShaderFilter(void)
        {}

        /*!
         * To be implemented by a derived class to return true
         * if the named shader should be included in the uber-shader.
         */
        virtual
        bool
        use_shader(const reference_counted_ptr<ShaderType> &shader) const = 0;
      };
    };

    /*!
     * \brief
     * A PainterShaderRegistrarGLSL is an implementation of PainterRegistrar
     * that assembles the shader source code of PainterItemShaderGLSL
     * and PainterBlendShaderGLSL into an uber-shader.
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
       * same lock as used by the PainterShaderRegistrarGLSL. A backend will
       * need to define the the functions (or macros) in their
       * GLSL preamble:
       *  - fastuidraw_begin_interlock() which is called before access
       *  - fastuidraw_end_interlock() which is called after access
       * if UberShaderParams::ffb_blending_type() is \ref fbf_blending_interlock.
       * \param tp blend type of \ref PainterBlendShader objects to include in the uber-shader
       * \param backend_constants constant values that affect the created uber-shader.
       * \param out_vertex ShaderSource to which to add uber-vertex shader
       * \param out_fragment ShaderSource to which to add uber-fragment shader
       * \param construct_params specifies how to construct the uber-shaders.
       * \param item_shader_filter pointer to \ref PainterShaderRegistrarGLSLTypes::ShaderFilter
       *                           to use to filter which shaders to place into the uber-shader.
       *                           A value of nullptr indicates to add all item shaders to the
       *                           uber-shader.
       * \param discard_macro_value macro-value definintion for the macro
       *                            FASTUIDRAW_DISCARD. PainterItemShaderGLSL
       *                            fragment sources use FASTUIDRAW_DISCARD
       *                            instead of discard.
       */
      void
      construct_item_uber_shader(enum PainterBlendShader::shader_type tp,
                                 const BackendConstants &backend_constants,
                                 ShaderSource &out_vertex,
                                 ShaderSource &out_fragment,
                                 const UberShaderParams &construct_params,
                                 const ShaderFilter<PainterItemShaderGLSL> *item_shader_filter = nullptr,
                                 c_string discard_macro_value = "discard");

      /*!
       * Add the uber-vertex and fragment shaders to given ShaderSource values.
       * The \ref Mutex mutex() is NOT locked during this call, a caller should
       * lock the mutex before calling it. This way a derived class can use the
       * same lock as used by the PainterShaderRegistrarGLSL.
       * \param backend_constants constant values that affect the created uber-shader.
       * \param out_vertex ShaderSource to which to add uber-vertex shader
       * \param out_fragment ShaderSource to which to add uber-fragment shader
       * \param construct_params specifies how to construct the uber-shaders.
       * \param item_shader_filter pointer to \ref PainterShaderRegistrarGLSLTypes::ShaderFilter
       *                           to use to filter which shaders to place into the uber-shader.
       *                           A value of nullptr indicates to add all item coverage shaders
       *                           to the uber-shader.
       */
      void
      construct_item_uber_coverage_shader(const BackendConstants &backend_constants,
                                          ShaderSource &out_vertex,
                                          ShaderSource &out_fragment,
                                          const UberShaderParams &construct_params,
                                          const ShaderFilter<PainterItemCoverageShaderGLSL> *item_shader_filter = nullptr);

      /*!
       * Add the vertex and fragment shaders of a specific item shader to given
       * ShaderSource values. The \ref Mutex mutex() is NOT locked during this call,
       * a caller should lock the mutex before calling it. This way a derived class
       * can use the same lock as used by the PainterShaderRegistrarGLSL.
       * \param tp blend type of \ref PainterBlendShader objects to include in the uber-shader
       * \param backend_constants constant values that affect the created uber-shader.
       * \param out_vertex ShaderSource to which to add uber-vertex shader
       * \param out_fragment ShaderSource to which to add uber-fragment shader
       * \param construct_params specifies how to construct the uber-shaders.
       * \param shader_id item shader ID, i.e. PainterItemShader::ID().
       * \param discard_macro_value macro-value definintion for the macro
       *                            FASTUIDRAW_DISCARD. PainterItemShaderGLSL
       *                            fragment sources use FASTUIDRAW_DISCARD
       *                            instead of discard.
       */
      void
      construct_item_shader(enum PainterBlendShader::shader_type tp,
                            const BackendConstants &backend_constants,
                            ShaderSource &out_vertex,
                            ShaderSource &out_fragment,
                            const UberShaderParams &construct_params,
                            unsigned int shader_id,
                            c_string discard_macro_value = "discard");

      /*!
       * Add the vertex and fragment shaders of a specific item shader to given
       * ShaderSource values. The \ref Mutex mutex() is NOT locked during this call,
       * a caller should lock the mutex before calling it. This way a derived class
       * can use the same lock as used by the PainterShaderRegistrarGLSL.
       * \param backend_constants constant values that affect the created uber-shader.
       * \param out_vertex ShaderSource to which to add uber-vertex shader
       * \param out_fragment ShaderSource to which to add uber-fragment shader
       * \param construct_params specifies how to construct the uber-shaders.
       * \param shader_id item shader ID, i.e. PainterItemShader::ID().
       */
      void
      construct_item_coverage_shader(const BackendConstants &backend_constants,
                                     ShaderSource &out_vertex,
                                     ShaderSource &out_fragment,
                                     const UberShaderParams &construct_params,
                                     unsigned int shader_id);

      /*!
       * Returns the total number of shaders (item and blend)
       * registered to this PainterShaderRegistrarGLSL; a derived class
       * should track this count value and use it to determine
       * when it needs to reconstruct its uber-shader. The mutex()
       * is NOT locked for the duration of the function.
       */
      unsigned int
      registered_shader_count(void);

      /*!
       * Returns the number of blend shaders registered to this
       * PainterShaderRegistrarGLSL; a derived class
       * should track this count value and use it to determine
       * when it needs to reconstruct its shaders. The mutex()
       * is NOT locked for the duration of the function.
       */
      unsigned int
      registered_blend_shader_count(enum PainterBlendShader::shader_type tp);

      /*!
       * Fill a buffer to hold the values used by the uber-shader.
       * The buffer must be that p.size() is atleast ubo_size().
       * \param vwp current PainterSurface::Viewport to which is being rendered
       * \param p buffer to which to fill uniform data
       */
      void
      fill_uniform_buffer(const PainterSurface::Viewport &vwp,
                          c_array<uint32_t> p);

      /*!
       * Total size of UBO for uniforms in units of
       * uint32_t, see also fill_uniform_ubo().
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
       * compute the shader group of a PainterItemCoverageShader.
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
      compute_item_coverage_shader_group(PainterShader::Tag tag,
                                         const reference_counted_ptr<PainterItemCoverageShader> &shader);

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

      /*!
       * To be optionally implemented by a derived class to
       * compute the shader group of a PainterBrushShader.
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
      compute_custom_brush_shader_group(PainterShader::Tag tag,
                                        const reference_counted_ptr<PainterBrushShader> &shader);

      virtual
      PainterShader::Tag
      absorb_item_shader(const reference_counted_ptr<PainterItemShader> &shader) final override;

      virtual
      uint32_t
      compute_item_sub_shader_group(const reference_counted_ptr<PainterItemShader> &shader) final override;

      virtual
      PainterShader::Tag
      absorb_item_coverage_shader(const reference_counted_ptr<PainterItemCoverageShader> &shader) final override;

      virtual
      uint32_t
      compute_item_coverage_sub_shader_group(const reference_counted_ptr<PainterItemCoverageShader> &shader) final override;

      virtual
      PainterShader::Tag
      absorb_blend_shader(const reference_counted_ptr<PainterBlendShader> &shader) final override;

      virtual
      uint32_t
      compute_blend_sub_shader_group(const reference_counted_ptr<PainterBlendShader> &shader) final override;

      virtual
      PainterShader::Tag
      absorb_custom_brush_shader(const reference_counted_ptr<PainterBrushShader> &shader) final override;

      virtual
      uint32_t
      compute_custom_brush_sub_shader_group(const reference_counted_ptr<PainterBrushShader> &shader) final override;

    private:
      void *m_d;
    };

/*! @} */

  }
}
