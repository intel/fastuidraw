/*!
 * \file painter_brush_shader_glsl.hpp
 * \brief file painter_brush_shader_glsl.hpp
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

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/blend_mode.hpp>
#include <fastuidraw/painter/shader/painter_brush_shader.hpp>
#include <fastuidraw/glsl/shader_source.hpp>
#include <fastuidraw/glsl/varying_list.hpp>

namespace fastuidraw
{
  namespace glsl
  {
/*!\addtogroup GLSL
 * @{
 */
    /*!
     * \brief
     * A PainterBrushShaderGLSL is a PainterBrushShader whose
     * shader code fragment is via GLSL.The vertex shader code needs to implement the function:
     * \code
     *   void
     *   fastuidraw_gl_vert_brush_main(in uint sub_shader,
     *                                 in uint shader_data_offset,
     *                                 in vec2 brush_p)
     * \endcode
     * where
     *  - sub_shader corresponds to PainterBrushShader::sub_shader()
     *  - brush_p is the brush position emitted by the item shader and
     *  - shader_data_offset is what block in the data store for
     *    the data packed by PainterBrushShaderData::pack_data()
     *    of the PainterBrushShaderData in the \ref Painter call;
     *    use the macro fastuidraw_fetch_data() to read the data.
     *
     * The fragment shader code needs to implement the function:
     * \code
     *   vec4
     *   fastuidraw_gl_frag_brush_main(in uint sub_shader,
     *                                 in uint shader_data_offset)
     * \endcode
     * which returns the color value, pre-multiplied by alpha, by which
     * to modulate the outgoing fragment color.
     *
     * Available to only the vertex shader are the following:
     *  - the GLSL elements in the module \ref GLSLVertCode
     *
     * Available to only the fragment shader are the following:
     *  - the GLSL elements in the module \ref GLSLFragCode
     *
     * Available to both the vertex and fragment shader are the following:
     *  - the GLSL elements in the module \ref GLSLVertFragCode
     *
     * For both stages, the value of the argument of shader_data_offset is
     * which 128-bit block into the data store (PainterDraw::m_store) of the
     * shader data to be read with the GLSL macro \ref fastuidraw_fetch_data.
     *
     * Also, if one defines macros in any of the passed ShaderSource objects,
     * those macros MUST be undefined at the end. In addition, if one
     * has local helper functions, to avoid global name collision, those
     * function names should be wrapped in the macro FASTUIDRAW_LOCAL()
     * to make sure that the function is given a unique global name within
     * the uber-shader.
     *
     * Lastly, one can use the class \ref UnpackSourceGenerator to generate
     * shader code to unpack values from the data in the data store buffer.
     * That machine generated code uses the macro fastuidraw_fetch_data().
     */
    class PainterBrushShaderGLSL:public PainterBrushShader
    {
    public:
      /*!
       * \brief
       * If one wishes to make use of other \ref PainterItemCoverageShaderGLSL
       * fastuidraw_gl_vert_main()/fastuidraw_gl_frag_main() of other shaders
       * (for example to have a simple shader that adds on to a previous
       * shader), a DependencyList provides the means to do so.
       *
       * Each such used shader is given a name by which the caller will use it.
       * In addition, the caller has access to the varyings of the callee as well.
       * Thus, the varyings of the caller are the varyings listed in its ctor
       * together with the varyings of all the shaders listed in the DependencyList.
       */
      class DependencyList
      {
      public:
        /*!
         * Ctor.
         */
        DependencyList(void);

        /*!
         * Copy ctor.
         * \param obj value from which to copy
         */
        DependencyList(const DependencyList &obj);

        ~DependencyList();

        /*!
         * Assignment operator
         * \param rhs value from which to copy
         */
        DependencyList&
        operator=(const DependencyList &rhs);

        /*!
         * Swap operation
         * \param obj object with which to swap
         */
        void
        swap(DependencyList &obj);

        /*!
         * Add a shader to the DependencyList's list.
         * \param name name by which to call the shader
         * \param shader shader to add to this DependencyList
         */
        DependencyList&
        add_shader(c_string name,
                   const reference_counted_ptr<const PainterBrushShaderGLSL> &shader);

      private:
        friend class PainterBrushShaderGLSL;
        void *m_d;
      };

      /*!
       * Ctor.
       * \param number_context_textures number of external textures the shader uses
       * \param vertex_src GLSL source holding vertex shader routine
       * \param fragment_src GLSL source holding fragment shader routine
       * \param varyings list of varyings of the shader
       * \param num_sub_shaders the number of sub-shaders it supports
       * \param dependencies list of other \ref PainterBrushShaderGLSL
       *                     that are used directly.
       */
      PainterBrushShaderGLSL(unsigned int number_context_textures,
                             const ShaderSource &vertex_src,
                             const ShaderSource &fragment_src,
                             const varying_list &varyings,
                             unsigned int num_sub_shaders = 1,
                             const DependencyList &dependencies = DependencyList());

      ~PainterBrushShaderGLSL(void);

      /*!
       * Number of external textures the custum brush uses.
       */
      unsigned int
      number_context_textures(void) const;

      /*!
       * Returns the varying of the shader
       */
      const varying_list&
      varyings(void) const;

      /*!
       * Return the GLSL source of the vertex shader
       */
      const ShaderSource&
      vertex_src(void) const;

      /*!
       * Return the GLSL source of the fragment shader
       */
      const ShaderSource&
      fragment_src(void) const;

      /*!
       * Return the list of shaders on which this shader is dependent.
       */
      c_array<const reference_counted_ptr<const PainterBrushShaderGLSL> >
      dependency_list_shaders(void) const;

      /*!
       * Returns the names that each shader listed in \ref
       * dependency_list_shaders() is referenced by, i.e.
       * the i'th element of dependency_list_shaders() is
       * referenced as the i'th element of \ref
       * dependency_list_names().
       */
      c_array<const c_string>
      dependency_list_names(void) const;

    private:
      void *m_d;
    };
/*! @} */

  }
}
