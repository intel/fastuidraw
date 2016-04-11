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

#include <fastuidraw/painter/packing/painter_backend.hpp>
#include <fastuidraw/gl_backend/painter_shader_gl.hpp>
#include <fastuidraw/gl_backend/painter_blend_shader_gl.hpp>
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
    class PainterBackendGL:public PainterBackend
    {
    public:
      /*!
        Overload typedef for handle
       */
      typedef reference_counted_ptr<PainterBackendGL> handle;

      /*!
        Overload typedef for const_handle
       */
      typedef reference_counted_ptr<const PainterBackendGL> const_handle;

      /*!
        A params gives parameters how to contruct
        a PainterBackendGL.
       */
      class params
      {
      public:
        /*!
          Ctor.
         */
        params(void);

        /*!
          Copy ctor.
          \param obj value from which to copy
         */
        params(const params &obj);

        ~params();

        /*!
          Assignment operator
          \param rhs value from which to copy
         */
        params&
        operator=(const params &rhs);

        /*!
          Configuration parameters inherited from PainterBackend
         */
        PainterBackend::Configuration m_config;

        /*!
          The ImageAtlasGL to be used by the painter
         */
        const ImageAtlasGL::handle&
        image_atlas(void) const;

        /*!
          Set the value returned by image_atlas(void) const.
         */
        params&
        image_atlas(const ImageAtlasGL::handle &v);

        /*!
          The ColorStopAtlasGL to be used by the painter
         */
        const ColorStopAtlasGL::handle&
        colorstop_atlas(void) const;

        /*!
          Set the value returned by colorstop_atlas(void) const.
         */
        params&
        colorstop_atlas(const ColorStopAtlasGL::handle &v);

        /*!
          The GlyphAtlasGL to be used by the painter
         */
        const GlyphAtlasGL::handle&
        glyph_atlas(void) const;

        /*!
          Set the value returned by glyph_atlas(void) const.
         */
        params&
        glyph_atlas(const GlyphAtlasGL::handle &v);

        /*!
          Specifies the maximum number of attributes
          a PainterDrawCommand returned by
          map_draw_command() may store, i.e. the size
          of PainterDrawCommand::m_attributes.
          Initial value is 512 * 512.
         */
        unsigned int
        attributes_per_buffer(void) const;

        /*!
          Set the value for attributes_per_buffer(void) const
        */
        params&
        attributes_per_buffer(unsigned int v);

        /*!
          Specifies the maximum number of indices
          a PainterDrawCommand returned by
          map_draw_command() may store, i.e. the size
          of PainterDrawCommand::m_indices.
          Initial value is 1.5 times the initial value
          for attributes_per_buffer(void) const.
         */
        unsigned int
        indices_per_buffer(void) const;

        /*!
          Set the value for indices_per_buffer(void) const
        */
        params&
        indices_per_buffer(unsigned int v);

        /*!
          Specifies the maximum number of blocks of
          data a PainterDrawCommand returned by
          map_draw_command() may store. The size of
          PainterDrawCommand::m_store is given by
          data_blocks_per_store_buffer() *
          PainterBackend::Configuration::alignment(),
          Initial value is 1024 * 64.
         */
        unsigned int
        data_blocks_per_store_buffer(void) const;

        /*!
          Set the value for data_blocks_per_store_buffer(void) const
        */
        params&
        data_blocks_per_store_buffer(unsigned int v);

        /*!
          If true, has the PainterBackendGL use hardware clip-planes
          if available (i.e. gl_ClipDistance). Default value is true.
         */
        bool
        use_hw_clip_planes(void) const;

        /*!
          Set the value for use_hw_clip_planes(void) const
        */
        params&
        use_hw_clip_planes(bool v);

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
        params&
        number_pools(unsigned int v);

        /*!
          If true, place different vertex shadings in seperate
          entries of a glMultiDrawElements call. The use case
          is that if many vertices are processed in a single
          vertex shader invocation, a seperate HW draw call
          is issued but the API state remains the same.
          The motivation is that by placing in a seperate call,
          the shader invocation does not divergently branch.
          Default value is false.
         */
        bool
        break_on_vertex_shader_change(void) const;

        /*!
          Set the value for break_on_vertex_shader_change(void) const
        */
        params&
        break_on_vertex_shader_change(bool v);

        /*!
          If true, place different fragment shadings in seperate
          entries of a glMultiDrawElements call. The use case
          is that if a GPU has fragments from different triangles
          shaded in the same invocation of a fragment shader. By
          placing in a seperate HW draw call, the shader invocation
          will not divergenlty branch.
          Default value is false.
         */
        bool
        break_on_fragment_shader_change(void) const;

        /*!
          Set the value for break_on_fragment_shader_change(void) const
        */
        params&
        break_on_fragment_shader_change(bool v);

      private:
        void *m_d;
      };

      /*!
        Ctor.
        \param P parameters specifying properties
       */
      explicit
      PainterBackendGL(const params &P = params());

      ~PainterBackendGL();

      virtual
      void
      on_begin(void);

      virtual
      void
      on_end(void);

      virtual
      void
      on_pre_draw(void);

      virtual
      PainterDrawCommand::const_handle
      map_draw_command(void);

      virtual
      void
      target_resolution(int w, int h);

      /*!
        Return the Program used to draw -all- content
        by this PainterBackendGL
       */
      Program::handle
      program(void);

      /*!
        Add GLSL code that is to be visible to all vertex
        shaders. The code can define functions or macros.
        \param src shader source to add
       */
      void
      add_vertex_shader_util(const Shader::shader_source &src);

      /*!
        Add GLSL code that is to be visible to all vertex
        shaders. The code can define functions or macros.
        \param src shader source to add
       */
      void
      add_fragment_shader_util(const Shader::shader_source &src);

    protected:
      virtual
      PainterShader::Tag
      absorb_vert_shader(const PainterShader::handle &shader);

      virtual
      PainterShader::Tag
      absorb_frag_shader(const PainterShader::handle &shader);

      virtual
      PainterShader::Tag
      absorb_blend_shader(const PainterShader::handle &shader);

    private:
      void *m_d;
    };
  }
/*! @} */
}
