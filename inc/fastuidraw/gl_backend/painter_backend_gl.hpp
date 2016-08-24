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
        PainterBackend::ConfigurationBase m_config;

        /*!
          The ImageAtlasGL to be used by the painter
         */
        const reference_counted_ptr<ImageAtlasGL>&
        image_atlas(void) const;

        /*!
          Set the value returned by image_atlas(void) const.
         */
        params&
        image_atlas(const reference_counted_ptr<ImageAtlasGL> &v);

        /*!
          The ColorStopAtlasGL to be used by the painter
         */
        const reference_counted_ptr<ColorStopAtlasGL>&
        colorstop_atlas(void) const;

        /*!
          Set the value returned by colorstop_atlas(void) const.
         */
        params&
        colorstop_atlas(const reference_counted_ptr<ColorStopAtlasGL> &v);

        /*!
          The GlyphAtlasGL to be used by the painter
         */
        const reference_counted_ptr<GlyphAtlasGL>&
        glyph_atlas(void) const;

        /*!
          Set the value returned by glyph_atlas(void) const.
         */
        params&
        glyph_atlas(const reference_counted_ptr<GlyphAtlasGL> &v);

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
        params&
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
        params&
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
        params&
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
        params&
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
        params&
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
        params&
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
        params&
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
        params&
        unpack_header_and_brush_in_frag_shader(bool v);

      private:
        void *m_d;
      };

      /*!
        Ctor.
        \param P parameters specifying properties. A GL context must
                 be current when contructing a PainterBackendGL object.
                 Any GL context used with the constructed PainterBackendGL
                 must be in the same share group as the GL context that
                 was current when the PainterBackendGL was constructed.
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
      reference_counted_ptr<const PainterDrawCommand>
      map_draw_command(void);

      virtual
      void
      target_resolution(int w, int h);

      /*!
        Return the Program used to draw -all- content
        by this PainterBackendGL
       */
      reference_counted_ptr<Program>
      program(void);

    private:
      void *m_d;
    };
/*! @} */
  }
}
