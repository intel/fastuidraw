/*!
 * \file painter_backend_glsl.hpp
 * \brief file painter_backend_glsl.hpp
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

#include <fastuidraw/painter/packing/painter_backend.hpp>
#include <fastuidraw/glsl/shader_source.hpp>

namespace fastuidraw
{
  namespace glsl
  {
    /*!
      A PainterBackendGLSL is a partial implementation of PainterBackend.
      It handles the building of the GLSL source code of an Uber-Shader.
     */
    class PainterBackendGLSL:public PainterBackend
    {
    public:
      /*!
        Enumeration to specify how the data store filled by
        \ref PainterDrawCommand::m_store is realized.
       */
      enum data_store_backing_t
        {
          /*!
            Data store is backed by a texture buffer object
           */
          data_store_tbo,

          /*!
            Data store is backed by a uniform buffer object
           */
          data_store_ubo
        };

      /*!
        Enumeration to specify how to access the backing store
        of the glyph geometry stored in GlyphAtlas::geometry_store()
       */
      enum glyph_geometry_backing_t
        {
          /*!
            Use a samplerBuffer to access the data
          */
          glyph_geometry_tbo,

          /*!
            Use a sampler2DArray to access the data
           */
          glyph_geometry_texture_array,
        };

      /*!
        A params gives parameters how to contruct
        a PainterBackendGLSL. These values influence
        the behavior of both the PainterBackendGLSL
        and the shaders it constructs via
        PainterBackendGLSL::construct_shader().
       */
      class ConfigurationGLSL
      {
      public:
        /*!
          Ctor.
         */
        ConfigurationGLSL(void);

        /*!
          Copy ctor.
          \param obj value from which to copy
         */
        ConfigurationGLSL(const ConfigurationGLSL &obj);

        ~ConfigurationGLSL();

        /*!
          Assignment operator
          \param rhs value from which to copy
         */
        ConfigurationGLSL&
        operator=(const ConfigurationGLSL &rhs);

        /*!
          Configuration parameters inherited from PainterBackend
         */
        PainterBackend::ConfigurationBase m_config;

        /*!
          If true, each item shader will be in a different
          shader group (see PainterShader::group()).
        */
        bool
        unique_group_per_item_shader(void) const;

        /*!
          Set the value returned by unique_group_per_item_shader(void) const.
          Default value is false.
         */
        ConfigurationGLSL&
        unique_group_per_item_shader(bool);

        /*!
          If true, each blend shader will be in a different
          shader group (see PainterShader::group()).
        */
        bool
        unique_group_per_blend_shader(void) const;

        /*!
          Set the value returned by unique_group_per_blend_shader(void) const.
          Default value is false.
         */
        ConfigurationGLSL&
        unique_group_per_blend_shader(bool);

        /*!
          If true, use HW clip planes (embodied by gl_ClipDistance).
         */
        bool
        use_hw_clip_planes(void) const;

        /*!
          Set the value returned by use_hw_clip_planes(void) const.
          Default value is true.
         */
        ConfigurationGLSL&
        use_hw_clip_planes(bool);

        /*!
          Set the blend shader type used by the blend
          shaders of the default shaders, as returned by
          PainterShaderSet::blend_shaders() of
          PainterBackend::default_shaders().
         */
        enum PainterBlendShader::shader_type
        default_blend_shader_type(void) const;

        /*!
          Set the value returned by default_blend_shader_type(void) const.
          Default value is PainterBlendShader::dual_src.
         */
        ConfigurationGLSL&
        default_blend_shader_type(enum PainterBlendShader::shader_type);

      private:
        void *m_d;
      };

      /*!
        An UberShaderParams specifies how to construct an uber-shader.
        Note that the usage of HW clip-planes is specified by by
        ConfigurationGLSL, NOT UberShaderParams.
       */
      class UberShaderParams
      {
      public:
        /*!
          Ctor.
         */
        UberShaderParams(void);

        /*!
          Copy ctor.
          \param obj value from which to copy
         */
        UberShaderParams(const UberShaderParams &obj);

        ~UberShaderParams();

        /*!
          Assignment operator
          \param rhs value from which to copy
         */
        UberShaderParams&
        operator=(const UberShaderParams &rhs);

        /*!
          If true, use a switch() in the uber-vertex shader to
          dispatch to the PainterItemShader.
         */
        bool
        vert_shader_use_switch(void) const;

        /*!
          Set the value returned by vert_shader_use_switch(void) const.
          Default value is false.
         */
        UberShaderParams&
        vert_shader_use_switch(bool);

        /*!
          If true, use a switch() in the uber-fragment shader to
          dispatch to the PainterItemShader.
         */
        bool
        frag_shader_use_switch(void) const;

        /*!
          Set the value returned by frag_shader_use_switch(void) const.
          Default value is false.
         */
        UberShaderParams&
        frag_shader_use_switch(bool);

        /*!
          If true, use a switch() in the uber-fragment shader to
          dispatch to the PainterBlendShader.
         */
        bool
        blend_shader_use_switch(void) const;

        /*!
          Set the value returned by blend_shader_use_switch(void) const.
          Default value is false.
         */
        UberShaderParams&
        blend_shader_use_switch(bool);

        /*!
          If true, unpack the PainterBrush data in the fragment shader.
          If false, unpack the data in the vertex shader and forward
          the data to the fragment shader via flat varyings.
         */
        bool
        unpack_header_and_brush_in_frag_shader(void) const;

        /*!
          Set the value returned by unpack_header_and_brush_in_frag_shader(void) const.
          Default value is false.
         */
        UberShaderParams&
        unpack_header_and_brush_in_frag_shader(bool);

        /*!
          Specify how to access the data in PainterDrawCommand::m_store
          from the GLSL shader.
         */
        enum data_store_backing_t
        data_store_backing(void) const;

        /*!
          Set the value returned by data_store_backing(void) const.
          Default value is data_store_tbo.
         */
        UberShaderParams&
        data_store_backing(enum data_store_backing_t);

        /*!
          Only needed if data_store_backing(void) const
          has value data_store_ubo. Gives the size in
          blocks of PainterDrawCommand::m_store which
          is PainterDrawCommand::m_store.size() divided
          by PainterBackend::configuration_base().alignment().
         */
        int
        data_blocks_per_store_buffer(void) const;

        /*!
          Set the value returned by data_blocks_per_store_buffer(void) const
          Default value is -1.
         */
        UberShaderParams&
        data_blocks_per_store_buffer(int);

        /*!
          Specifies how the glyph geometry data (GlyphAtlas::geometry_store())
          is accessed from the uber-shaders.
         */
        enum glyph_geometry_backing_t
        glyph_geometry_backing(void) const;

        /*!
          Set the value returned by glyph_geometry_backing(void) const.
          Default value is glyph_geometry_tbo.
         */
        UberShaderParams&
        glyph_geometry_backing(enum glyph_geometry_backing_t);

        /*!
          Only used if glyph_geometry_backing(void) const has value
          glyph_geometry_texture_array. Gives the log2 of the
          width and height of the texture array backing the
          glyph geometry data (GlyphAtlas::geometry_store()).
          Note: it must be that the width and height of the backing
          2D texture array are powers of 2.
         */
        ivec2
        glyph_geometry_backing_log2_dims(void) const;

        /*!
          Set the value returned by glyph_geometry_backing_log2_dims(void) const.
          Default value is (-1, -1).
         */
        UberShaderParams&
        glyph_geometry_backing_log2_dims(ivec2);

        /*!
          If true, can access the data of GlyphAtlas::texel_store() as a
          sampler2DArray as well.
         */
        bool
        have_float_glyph_texture_atlas(void) const;

        /*!
          Set the value returned by have_float_glyph_texture_atlas(void) const.
          Default value is true.
         */
        UberShaderParams&
        have_float_glyph_texture_atlas(bool);

        /*!
          Build the uber-shader with those blend shaders registered to
          the PainterBackendGLSL of this type only.
         */
        enum PainterBlendShader::shader_type
        blend_type(void) const;

        /*!
          Set the value returned by blend_type(void) const.
          Default value is PainterBlendShader::dual_src.
         */
        UberShaderParams&
        blend_type(enum PainterBlendShader::shader_type);

      private:
        void *m_d;
      };

      PainterBackendGLSL(reference_counted_ptr<GlyphAtlas> glyph_atlas,
                         reference_counted_ptr<ImageAtlas> image_atlas,
                         reference_counted_ptr<ColorStopAtlas> colorstop_atlas,
                         const ConfigurationGLSL &config);

      ~PainterBackendGLSL();

      /*!
        Returns the ConfigurationBase passed in the ctor.
      */
      const ConfigurationGLSL&
      configuration_glsl(void) const;

      /*!
        Add GLSL code that is to be visible to all vertex
        shaders. The code can define functions or macros.
        \param src shader source to add
       */
      void
      add_vertex_shader_util(const ShaderSource &src);

      /*!
        Add GLSL code that is to be visible to all vertex
        shaders. The code can define functions or macros.
        \param src shader source to add
       */
      void
      add_fragment_shader_util(const ShaderSource &src);

      /*!
        Construct the uber vertex and fragment shader.
       */
      void
      construct_shader(ShaderSource &out_vertex,
                       ShaderSource &out_fragment,
                       const UberShaderParams &contruct_params);

    protected:
      /*!
        Returns true if any shader code has been added since
        the last call to shader_code_added(). A derived class
        shall use this function to determine when it needs
        to recreate its uber-shader.
       */
      bool
      shader_code_added(void);

      virtual
      PainterShader::Tag
      absorb_item_shader(const reference_counted_ptr<PainterItemShader> &shader);

      virtual
      uint32_t
      compute_item_sub_shader_group(const reference_counted_ptr<PainterItemShader> &shader);

      virtual
      PainterShader::Tag
      absorb_blend_shader(const reference_counted_ptr<PainterBlendShader> &shader);

      virtual
      uint32_t
      compute_blend_sub_shader_group(const reference_counted_ptr<PainterBlendShader> &shader);

    private:
      void *m_d;
    };

  }
}
