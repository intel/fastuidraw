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
        a PainterBackendGLSL.
       */
      class ConfigurationGLSL
      {
      public:
        PainterBackend::ConfigurationBase m_config;
        bool m_unique_group_per_item_shader;
        bool m_unique_group_per_blend_shader;
        bool m_use_hw_clip_planes;

        //blend shader type for default shaders.
        enum PainterBlendShader::shader_type m_blend_type;
      };

      class uber_shader_params
      {
      public:
        bool m_vert_shader_use_switch;
        bool m_frag_shader_use_switch;
        bool m_blend_shader_use_switch;
        bool m_unpack_header_and_brush_in_frag_shader;

        //info about how to access PainterDrawCommand::m_store
        enum data_store_backing_t m_data_store_backing;
        unsigned int m_data_blocks_per_store_buffer; //only needed if m_data_store_backing == data_store_ubo

        //info on how to access GlyphAtlas::geometry_store()
        enum glyph_geometry_backing_t m_glyph_geometry_backing;
        ivec2 m_glyph_geometry_backing_log2_dims; //only makes sense if m_glyph_geometry_backing == glyph_geometry_texture_array

        // if can access GlyphAtlas::texel_store() as sampler2DArray as well
        bool m_have_float_glyph_texture_atlas;

        // blend mode to build shader for
        enum PainterBlendShader::shader_type m_blend_type;
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
                       const uber_shader_params &contruct_params);

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
