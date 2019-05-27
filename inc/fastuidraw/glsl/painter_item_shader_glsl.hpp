/*!
 * \file painter_item_shader_glsl.hpp
 * \brief file painter_item_shader_glsl.hpp
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

#include <fastuidraw/painter/shader/painter_item_shader.hpp>
#include <fastuidraw/glsl/shader_source.hpp>
#include <fastuidraw/glsl/symbol_list.hpp>

namespace fastuidraw
{
  namespace glsl
  {
/*!\addtogroup GLSL
 * @{
 */
    /*!
     * \brief
     * A PainterItemCoverageShaderGLSL is a collection of GLSL source code
     * fragments for a \ref PainterShaderRegistrarGLSL.
     *
     * The vertex shader code needs to implement the function:
     * \code
     *   void
     *   fastuidraw_gl_vert_main(in uint sub_shader,
     *                           in uvec4 attrib0,
     *                           in uvec4 attrib1,
     *                           in uvec4 attrib2,
     *                           inout uint shader_data_block,
     *                           out vec3 clip_p)
     * \endcode
     * where
     *  - sub_shader corresponds to PainterItemCoverageShader::sub_shader()
     *  - attrib0 corresponds to PainterAttribute::m_attrib0,
     *  - attrib1 corresponds to PainterAttribute::m_attrib1,
     *  - attrib2 corresponds to PainterAttribute::m_attrib2 and
     *  - shader_data_block is what block in the data store for
     *    the data packed by PainterItemCoverageShaderData::pack_data()
     *    of the PainterItemCoverageShaderData in the \ref Painter call;
     *    use the macro fastuidraw_fetch_data() to read the data.
     *
     * The output clip_p is to hold the clip-coordinate of the vertex.
     *
     * The fragment shader code needs to implement the function:
     * \code
     *   float
     *   fastuidraw_gl_frag_main(in uint sub_shader,
     *                           inout uint shader_data_block)
     * \endcode
     *
     * which returns the value to write to the coverage buffer
     * from the fragment for the item.
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
     * For both stages, the value of the argument of shader_data_block is
     * which 128-bit block into the data store (PainterDraw::m_store) of the
     * shader data to be read with the GLSL macro \ref fastuidraw_fetch_data.
     * On exit, this value must be updated to the location just past the
     * shader data of the shader.
     *
     * For both stages, the value of the argument of sub_shader() is the
     * value of \ref PainterShader::sub_shader() of the active shader.
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
    class PainterItemCoverageShaderGLSL:public PainterItemCoverageShader
    {
    public:
      /*!
       * \brief
       * If one wishes to make use of other \ref PainterItemCoverageShaderGLSL
       * fastuidraw_gl_vert_main()/fastuidraw_gl_frag_main() of other shaders
       * (for example to have a simple shader that adds on to a previous shader),
       * a DependencyList provides the means to do so.
       *
       * Each such used shader is given a name by which the caller will use it.
       * In addition, the caller has access to the varyings and shared symbols
       * of the callee as well. A varying or shareable V of an element in the \ref
       * DependencyList is accessed from the parent shader with dep::V where dep
       * is the argument value of name to \ref add_shader(). Note that it is
       * accessed with the scope-resolution operator; the uber-shader assember
       * will convert the scope-resolution operator into acceptable GLSL code.
       *
       * By using the values of the shareables (embodied in fields \ref
       * symbol_list::m_vert_shareable_values and \ref symbol_list::m_frag_shareable_values),
       * reading and potentially modifying the values of the varyings, one can
       * create effects building off of the built-in shaders of the GLSL module.
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
                   const reference_counted_ptr<const PainterItemCoverageShaderGLSL> &shader);

      private:
        friend class PainterItemCoverageShaderGLSL;
        void *m_d;
      };

      /*!
       * Ctor.
       * \param vertex_src GLSL source holding vertex shader routine
       * \param fragment_src GLSL source holding fragment shader routine
       * \param symbols list of symbols of the shader
       * \param num_sub_shaders the number of sub-shaders it supports
       * \param dependencies list of other \ref PainterItemCoverageShaderGLSL
       *                     that are used directly.
       */
      PainterItemCoverageShaderGLSL(const ShaderSource &vertex_src,
                                    const ShaderSource &fragment_src,
                                    const symbol_list &symbols,
                                    unsigned int num_sub_shaders = 1,
                                    const DependencyList &dependencies = DependencyList());

      /*!
       * Ctor.
       * \param vertex_src GLSL source holding vertex shader routine
       * \param fragment_src GLSL source holding fragment shader routine
       * \param symbols list of symbols of the shader
       * \param num_sub_shaders the number of sub-shaders it supports
       * \param dependencies list of other \ref PainterItemCoverageShaderGLSL
       *                     that are used directly.
       */
      PainterItemCoverageShaderGLSL(const ShaderSource &vertex_src,
                                    const ShaderSource &fragment_src,
                                    const symbol_list &symbols,
                                    const DependencyList &dependencies,
                                    unsigned int num_sub_shaders = 1);

      ~PainterItemCoverageShaderGLSL();

      /*!
       * Returns the symbol of the shader
       */
      const symbol_list&
      symbols(void) const;

      /*!
       * Returns the varyings of the shader, equivalent to
       * \code
       * symbols().m_varying_list;
       * \endcode
       */
      const varying_list&
      varyings(void) const
      {
        return symbols().m_varying_list;
      }

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
      c_array<const reference_counted_ptr<const PainterItemCoverageShaderGLSL> >
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

    /*!
     * \brief
     * A PainterItemShaderGLSL is a collection of GLSL source code
     * fragments for a \ref PainterShaderRegistrarGLSL.
     *
     * The vertex shader code needs to implement the function:
     * \code
     *   void
     *   fastuidraw_gl_vert_main(in uint sub_shader,
     *                           in uvec4 attrib0,
     *                           in uvec4 attrib1,
     *                           in uvec4 attrib2,
     *                           inout uint shader_data_block,
     *                           out uint z_add,
     *                           out vec2 brush_p,
     *                           out vec3 clip_p)
     * \endcode
     * where
     *  - sub_shader corresponds to PainterItemShader::sub_shader()
     *  - attrib0 corresponds to PainterAttribute::m_attrib0,
     *  - attrib1 corresponds to PainterAttribute::m_attrib1,
     *  - attrib2 corresponds to PainterAttribute::m_attrib2 and
     *  - shader_data_block is what block in the data store for
     *    the data packed by PainterItemShaderData::pack_data()
     *    of the PainterItemShaderData in the \ref Painter call;
     *    use the macro fastuidraw_fetch_data() to read the data.
     *
     * The output clip_p is to hold the clip-coordinate of the vertex.
     * The output brush_p is to hold the coordinate for the brush of
     * the vertex. The out z_add must be written to as well and it
     * is how much to add to the value in \ref PainterHeader::m_z
     * for the purpose of intra-item z-occluding. Items that do
     * not self-occlude should write 0 to z_add.
     *
     * The fragment shader code needs to implement the function:
     * \code
     *   vec4
     *   fastuidraw_gl_frag_main(in uint sub_shader,
     *                           inout uint shader_data_block)
     * \endcode
     *
     * which returns the color of the fragment for the item -before-
     * the color modulation by the pen, brush or having blending
     * applied. In addition, the color value returned MUST be
     * pre-multiplied by alpha.
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
     * For both stages, the value of the argument of shader_data_block is
     * which 128-bit block into the data store (PainterDraw::m_store) of the
     * shader data to be read with the GLSL macro \ref fastuidraw_fetch_data.
     * On exit, this value must be updated to the location just past the
     * shader data of the shader.
     *
     * For both stages, the value of the argument of sub_shader() is the
     * value of \ref PainterShader::sub_shader() of the active shader.
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
    class PainterItemShaderGLSL:public PainterItemShader
    {
    public:
      /*!
       * \brief
       * If one wishes to make use of other \ref PainterItemShaderGLSL
       * fastuidraw_gl_vert_main()/fastuidraw_gl_frag_main() of other shaders
       * (for example to have a simple shader that adds on to a previous shader),
       * a DependencyList provides the means to do so.
       *
       * Each such used shader is given a name by which the caller will use it.
       * In addition, the caller has access to the varyings and shared symbols
       * of the callee as well. A varying or shareable V of an element in the \ref
       * DependencyList is accessed from the parent shader with dep::V where dep
       * is the argument value of name to \ref add_shader(). Note that it is
       * accessed with the scope-resolution operator; the uber-shader assember
       * will convert the scope-resolution operator into acceptable GLSL code.
       *
       * By using the values of the shareables (embodied in fields \ref
       * symbol_list::m_vert_shareable_values and \ref symbol_list::m_frag_shareable_values),
       * reading and potentially modifying the values of the varyings, one can
       * create effects building off of the built-in shaders of the GLSL module.
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
                   const reference_counted_ptr<const PainterItemShaderGLSL> &shader);

      private:
        friend class PainterItemShaderGLSL;
        void *m_d;
      };

      /*!
       * Ctor.
       * \param puses_discard set to true if and only if the shader code
       *                      will use discard. Discard should be used
       *                      in the GLSL code via the macro FASTUIDRAW_DISCARD.
       * \param vertex_src GLSL source holding vertex shader routine
       * \param fragment_src GLSL source holding fragment shader routine
       * \param symbols list of symbols of the shader
       * \param num_sub_shaders the number of sub-shaders it supports
       * \param cvg the coverage shader (if any) to be used by the item shader
       * \param dependencies list of other \ref PainterItemShaderGLSL that are
       *                     used directly.
       */
      PainterItemShaderGLSL(bool puses_discard,
                            const ShaderSource &vertex_src,
                            const ShaderSource &fragment_src,
                            const symbol_list &symbols,
                            unsigned int num_sub_shaders = 1,
                            const reference_counted_ptr<PainterItemCoverageShaderGLSL> &cvg =
                            reference_counted_ptr<PainterItemCoverageShaderGLSL>(),
                            const DependencyList &dependencies = DependencyList());

      /*!
       * Ctor.
       * \param puses_discard set to true if and only if the shader code
       *                      will use discard. Discard should be used
       *                      in the GLSL code via the macro FASTUIDRAW_DISCARD.
       * \param vertex_src GLSL source holding vertex shader routine
       * \param fragment_src GLSL source holding fragment shader routine
       * \param symbols list of symbols of the shader
       * \param num_sub_shaders the number of sub-shaders it supports
       * \param cvg the coverage shader (if any) to be used by the item shader
       * \param dependencies list of other \ref PainterItemShaderGLSL that are
       *                     used directly.
       */
      PainterItemShaderGLSL(bool puses_discard,
                            const ShaderSource &vertex_src,
                            const ShaderSource &fragment_src,
                            const symbol_list &symbols,
                            const reference_counted_ptr<PainterItemCoverageShaderGLSL> &cvg,
                            const DependencyList &dependencies = DependencyList(),
                            unsigned int num_sub_shaders = 1);

      /*!
       * Ctor.
       * \param puses_discard set to true if and only if the shader code
       *                      will use discard. Discard should be used
       *                      in the GLSL code via the macro FASTUIDRAW_DISCARD.
       * \param vertex_src GLSL source holding vertex shader routine
       * \param fragment_src GLSL source holding fragment shader routine
       * \param symbols list of symbols of the shader
       * \param num_sub_shaders the number of sub-shaders it supports
       * \param cvg the coverage shader (if any) to be used by the item shader
       * \param dependencies list of other \ref PainterItemShaderGLSL that are
       *                     used directly.
       */
      PainterItemShaderGLSL(bool puses_discard,
                            const ShaderSource &vertex_src,
                            const ShaderSource &fragment_src,
                            const symbol_list &symbols,
                            const DependencyList &dependencies,
                            const reference_counted_ptr<PainterItemCoverageShaderGLSL> &cvg =
                            reference_counted_ptr<PainterItemCoverageShaderGLSL>(),
                            unsigned int num_sub_shaders = 1);

      /*!
       * Ctor.
       * \param puses_discard set to true if and only if the shader code
       *                      will use discard. Discard should be used
       *                      in the GLSL code via the macro FASTUIDRAW_DISCARD.
       * \param vertex_src GLSL source holding vertex shader routine
       * \param fragment_src GLSL source holding fragment shader routine
       * \param symbols list of symbols of the shader
       * \param num_sub_shaders the number of sub-shaders it supports
       * \param cvg the coverage shader (if any) to be used by the item shader
       * \param dependencies list of other \ref PainterItemShaderGLSL that are
       *                     used directly.
       */
      PainterItemShaderGLSL(bool puses_discard,
                            const ShaderSource &vertex_src,
                            const ShaderSource &fragment_src,
                            const symbol_list &symbols,
                            const DependencyList &dependencies,
                            unsigned int num_sub_shaders,
                            const reference_counted_ptr<PainterItemCoverageShaderGLSL> &cvg =
                            reference_counted_ptr<PainterItemCoverageShaderGLSL>());

      ~PainterItemShaderGLSL();

      /*!
       * Returns the symbol of the shader
       */
      const symbol_list&
      symbols(void) const;

      /*!
       * Returns the varyings of the shader, equivalent to
       * \code
       * symbols().m_varying_list;
       * \endcode
       */
      const varying_list&
      varyings(void) const
      {
        return symbols().m_varying_list;
      }

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
       * Returns true if the fragment shader uses discard
       */
      bool
      uses_discard(void) const;

      /*!
       * Return the list of shaders on which this shader is dependent.
       */
      c_array<const reference_counted_ptr<const PainterItemShaderGLSL> >
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
