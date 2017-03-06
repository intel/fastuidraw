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

#include <fastuidraw/glsl/painter_backend_glsl.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_blend_shader_glsl.hpp>
#include <fastuidraw/gl_backend/image_gl.hpp>
#include <fastuidraw/gl_backend/glyph_atlas_gl.hpp>
#include <fastuidraw/gl_backend/colorstop_atlas_gl.hpp>

namespace fastuidraw
{
  namespace gl
  {
/*!\addtogroup GLBackend
  @{
 */
    /*!
      A PainterBackendGL implements PainterBackend
      using the GL (or GLES) API.
     */
    class PainterBackendGL:public glsl::PainterBackendGLSL
    {
    public:
      /*!
        Enumeration to specify which GLSL program
        to fetch from program(enum program_type_t).
      */
      enum program_type_t
        {
          /*!
            Get the GLSL program that handles all shaders
          */
          program_all,

          /*!
            Get the GLSL program that only handles those
            shader without discard
          */
          program_without_discard,

          /*!
            Get the GLSL program that only handles those
            shader with discard
          */
          program_with_discard,

          /*!
           */
          number_program_types
        };

      /*!
        A ConfigurationGL gives parameters how to contruct
        a PainterBackendGL.
       */
      class ConfigurationGL
      {
      public:
        /*!
          Ctor.
         */
        ConfigurationGL(void);

        /*!
          Copy ctor.
          \param obj value from which to copy
         */
        ConfigurationGL(const ConfigurationGL &obj);

        ~ConfigurationGL();

        /*!
          Assignment operator
          \param rhs value from which to copy
         */
        ConfigurationGL&
        operator=(const ConfigurationGL &rhs);

        /*!
          The ImageAtlasGL to be used by the painter
         */
        const reference_counted_ptr<ImageAtlasGL>&
        image_atlas(void) const;

        /*!
          Set the value returned by image_atlas(void) const.
         */
        ConfigurationGL&
        image_atlas(const reference_counted_ptr<ImageAtlasGL> &v);

        /*!
          The ColorStopAtlasGL to be used by the painter
         */
        const reference_counted_ptr<ColorStopAtlasGL>&
        colorstop_atlas(void) const;

        /*!
          Set the value returned by colorstop_atlas(void) const.
         */
        ConfigurationGL&
        colorstop_atlas(const reference_counted_ptr<ColorStopAtlasGL> &v);

        /*!
          The GlyphAtlasGL to be used by the painter
         */
        const reference_counted_ptr<GlyphAtlasGL>&
        glyph_atlas(void) const;

        /*!
          Set the value returned by glyph_atlas(void) const.
         */
        ConfigurationGL&
        glyph_atlas(const reference_counted_ptr<GlyphAtlasGL> &v);

        /*!
          Specifies the maximum number of attributes
          a PainterDraw returned by
          map_draw() may store, i.e. the size
          of PainterDraw::m_attributes.
          Initial value is 512 * 512.
         */
        unsigned int
        attributes_per_buffer(void) const;

        /*!
          Set the value for attributes_per_buffer(void) const
        */
        ConfigurationGL&
        attributes_per_buffer(unsigned int v);

        /*!
          Specifies the maximum number of indices
          a PainterDraw returned by
          map_draw() may store, i.e. the size
          of PainterDraw::m_indices.
          Initial value is 1.5 times the initial value
          for attributes_per_buffer(void) const.
         */
        unsigned int
        indices_per_buffer(void) const;

        /*!
          Set the value for indices_per_buffer(void) const
        */
        ConfigurationGL&
        indices_per_buffer(unsigned int v);

        /*!
          Specifies the maximum number of blocks of
          data a PainterDraw returned by
          map_draw() may store. The size of
          PainterDraw::m_store is given by
          data_blocks_per_store_buffer() *
          PainterBackend::ConfigurationBase::alignment(),
          Initial value is 1024 * 64.
         */
        unsigned int
        data_blocks_per_store_buffer(void) const;

        /*!
          Set the value for data_blocks_per_store_buffer(void) const
        */
        ConfigurationGL&
        data_blocks_per_store_buffer(unsigned int v);

        /*!
          Returns how the data store is realized. The GL implementation
          may impose size limits that will force that the size of the
          data store might be smaller than that specified by
          data_blocks_per_store_buffer(). The initial value
          is data_store_tbo.
         */
        enum data_store_backing_t
        data_store_backing(void) const;

        /*!
          Set the value for data_store_backing(void) const
        */
        ConfigurationGL&
        data_store_backing(enum data_store_backing_t v);

        /*!
          If true, has the PainterBackendGL use hardware clip-planes
          if available (i.e. gl_ClipDistance). Default value is true.
         */
        bool
        use_hw_clip_planes(void) const;

        /*!
          Set the value for use_hw_clip_planes(void) const
        */
        ConfigurationGL&
        use_hw_clip_planes(bool v);

        /*!
          If true, use switch() statements in uber vertex shader,
          if false use a chain of if-else. Default value is true.
         */
        bool
        vert_shader_use_switch(void) const;

        /*!
          Set the value for vert_shader_use_switch(void) const
        */
        ConfigurationGL&
        vert_shader_use_switch(bool v);

        /*!
          If true, use switch() statements in uber frag shader,
          if false use a chain of if-else. Default value is true.
         */
        bool
        frag_shader_use_switch(void) const;

        /*!
          Set the value for frag_shader_use_switch(void) const
        */
        ConfigurationGL&
        frag_shader_use_switch(bool v);

        /*!
          If true, use switch() statements in uber blend shader,
          if false use a chain of if-else. Default value is true.
         */
        bool
        blend_shader_use_switch(void) const;

        /*!
          Set the value for blend_shader_use_switch(void) const
        */
        ConfigurationGL&
        blend_shader_use_switch(bool v);

        /*!
          A PainterBackendGL has a set of pools for the buffer
          objects to which to data to send to GL. Whenever
          on_end() is called, the next pool is used (wrapping around
          to the first pool when the last pool is finished).
          Ideally, one should set this value to N * L where N
          is the number of times Painter::begin() - Painter::end()
          pairs are within a frame and L is the latency in
          frames from ending a frame to the GPU finishes
          rendering the results of the frame.
          Initial value is 3.
         */
        unsigned int
        number_pools(void) const;

        /*!
          Set the value for number_pools(void) const
        */
        ConfigurationGL&
        number_pools(unsigned int v);

        /*!
          If true, place different item shaders in seperate
          entries of a glMultiDrawElements call.
          The motivation is that by placing in a seperate element
          of a glMultiDrawElements call, each element is a seperate
          HW draw call and by being seperate, the shader invocation
          does not divergently branch. Default value is false.
         */
        bool
        break_on_shader_change(void) const;

        /*!
          Set the value for break_on_shader_change(void) const
        */
        ConfigurationGL&
        break_on_shader_change(bool v);

        /*!
          If true, unpacks the brush and fragment shader specific data
          from the data buffer at the fragment shader. If false, unpacks
          the data in the vertex shader and fowards the data as flats
          to the fragment shader.
         */
        bool
        unpack_header_and_brush_in_frag_shader(void) const;

        /*!
          Set the value for unpack_header_and_brush_in_frag_shader(void) const
        */
        ConfigurationGL&
        unpack_header_and_brush_in_frag_shader(bool v);

        /*!
          If true, the vertex shader inputs should be qualified
          with a layout(location=) specifier. Default value is
          true.
         */
        bool
        assign_layout_to_vertex_shader_inputs(void) const;

        /*!
          Set the value for assign_layout_to_vertex_shader_inputs(void) const
        */
        ConfigurationGL&
        assign_layout_to_vertex_shader_inputs(bool v);

        /*!
          If true, the varyings between vertex and fragment
          shading should be qualified with a layout(location=)
          specifier. Default value is false.
         */
        bool
        assign_layout_to_varyings(void) const;

        /*!
          Set the value for assign_layout_to_varyings(void) const
        */
        ConfigurationGL&
        assign_layout_to_varyings(bool v);

        /*!
          If true, the textures and buffers used in the
          uber-shader should be qualified with a layout(binding=)
          specifier. Default value is true.
         */
        bool
        assign_binding_points(void) const;

        /*!
          Set the value for assign_binding_points(void) const
        */
        ConfigurationGL&
        assign_binding_points(bool v);

        /*!
          If true, the uber-shader will use a UBO for uniform
          values common to all painter calls. Default value
          is false.
         */
        bool
        use_ubo_for_uniforms(void) const;

        /*!
          Set the value for use_ubo_for_uniforms(void) const
        */
        ConfigurationGL&
        use_ubo_for_uniforms(bool v);

        /*!
          If true, item and blend shaders are broken into
          two classes: those that use discard and those that
          do not. Each class is then realized as a seperate
          GLSL program.
         */
        bool
        separate_program_for_discard(void) const;

        /*!
          Set the value for separate_program_for_discard(void) const
        */
        ConfigurationGL&
        separate_program_for_discard(bool v);

        /*!
          If framebuffer fetch is available, this value is ignored.
          When framebuffer fetch is not availabe, for non-dashed
          anti-aliased stroking, the opaque pass draws the stroking
          at about 1-pixel less and the anti-alias pass draws after
          and underneath the entire stroke width with an alpha
          value. Under severe transformation distrotion, the opaque
          pass is unable to leave enough uncovered for the anti-alias
          pass (but only under severe skew/perspective does this
          occur). When discard is used, both passes cover the entire
          stroking width, but the opaque pass discards when coverage
          is less than 100%. Again, this option is ignored if
          framebuffer fetch is available because when that is available
          the coverage is handled by a secondary buffer.
         */
        bool
        non_dashed_stroke_shader_uses_discard(void) const;

        /*!
          Set the value returned by non_dashed_stroke_shader_uses_discard(void) const.
          Default value is false.
         */
        ConfigurationGL&
        non_dashed_stroke_shader_uses_discard(bool);

        /*!
          Returns the blend_type() to be used by the PainterBackendGL,
	  if the spcified blend type is not supported, falls back to
	  first to PainterBlendShader::dual_src and if that is
	  not supported falls back to PainterBlendShader::single_src.
         */
        enum PainterBlendShader::shader_type
        blend_type(void) const;

        /*!
          Set the value for (void) const
        */
        ConfigurationGL&
        blend_type(enum PainterBlendShader::shader_type v);

      private:
        void *m_d;
      };

      /*!
        Ctor. When contructing a GL context must
        be current when contructing a PainterBackendGL object.
        Any GL context used with the constructed PainterBackendGL
        must be in the same share group as the GL context that
        was current when the PainterBackendGL was constructed.
        The values for the parameters of the created object might
        be adjusted from the passed parameters in order for the
        object to function correctly in the GL context from which
        it was created.
        \param config_gl ConfigurationGL providing configuration parameters
        \param config_base ConfigurationBase parameters inherited from PainterBackend
       */
      PainterBackendGL(const ConfigurationGL &config_gl,
                       const ConfigurationBase &config_base);

      ~PainterBackendGL();

      virtual
      unsigned int
      attribs_per_mapping(void) const;

      virtual
      unsigned int
      indices_per_mapping(void) const;

      virtual
      void
      on_pre_draw(void);

      virtual
      void
      on_post_draw(void);

      virtual
      reference_counted_ptr<const PainterDraw>
      map_draw(void);

      /*!
        Return the specified Program use to draw
        with this PainterBackendGL.
       */
      reference_counted_ptr<Program>
      program(enum program_type_t tp);

      /*!
        Returns the ConfigurationGL adapted from that passed
        by ctor (for the properties of the GL context) of
        the PainterBackendGL.
       */
      const ConfigurationGL&
      configuration_gl(void) const;

    protected:

      virtual
      uint32_t
      compute_item_shader_group(PainterShader::Tag tag,
                                const reference_counted_ptr<PainterItemShader> &shader);

      virtual
      uint32_t
      compute_blend_shader_group(PainterShader::Tag tag,
                                const reference_counted_ptr<PainterBlendShader> &shader);

    private:
      void *m_d;
    };
/*! @} */
  }
}
