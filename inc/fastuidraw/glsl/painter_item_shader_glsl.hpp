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

#include <fastuidraw/painter/painter_item_shader.hpp>
#include <fastuidraw/glsl/shader_source.hpp>

namespace fastuidraw
{
  namespace glsl
  {
/*!\addtogroup GLSL
 * @{
 */
    /*!
     * \brief
     * A varying_list lists all the in's of a frag shader (and
     * their names) or all the out's of vertex shader.
     *
     * A varying for a \ref PainterItemShaderGLSL is a SCALAR. For
     * a vertex and fragment shader pair, the name of the varying
     * does NOT matter for the sending of a vertex shader out to a
     * fragment shader in. Instead, the slot matters. The virtual
     * slots for each varying type are seperate, i.e. slot 0 for
     * uint is a different slot than slot 0 for int. In addition
     * the interpolation type is part of the type for floats, thus
     * slot 0 for flat float is a different slot than slot 0 for
     * smooth float.
     */
    class varying_list
    {
    public:
      /*!
       * \brief
       * Enumeration to define the interpolation of a varying
       */
      enum interpolation_qualifier_t
        {
          interpolation_smooth, /*!< corresponds to "smooth" in GLSL */
          interpolation_flat, /*!< corresponds to "flat" in GLSL */
          interpolation_noperspective, /*!< corresponds to "noperspective" in GLSL */

          interpolation_number_types,
        };

      /*!
       * Ctor.
       */
      varying_list(void);

      /*!
       * Copy ctor.
       * \param rhs value from which to copy
       */
      varying_list(const varying_list &rhs);

      ~varying_list();

      /*!
       * Assignment operator.
       * \param rhs value from which to copy
       */
      varying_list&
      operator=(const varying_list &rhs);

      /*!
       * Swap operation
       * \param obj object with which to swap
       */
      void
      swap(varying_list &obj);

      /*!
       * Returns the names for the slots of the float
       * varyings of the specified interpolation type.
       * \param q interpolation type
       */
      c_array<const c_string>
      floats(enum interpolation_qualifier_t q) const;

      /*!
       * Returns an array R, so that R[i] is
       * floats(i).size().
       */
      c_array<const size_t>
      float_counts(void) const;

      /*!
       * Returns the names for the slots of the uint varyings
       */
      c_array<const c_string>
      uints(void) const;

      /*!
       * Returns the names for the slots of the int varyings
       */
      c_array<const c_string>
      ints(void) const;

      /*!
       * Set a float of the named slot and qualifier to a name.
       * \param pname name to use
       * \param slot which float of the named qualifier
       * \param q interpolation qualifier
       */
      varying_list&
      set_float_varying(unsigned int slot, c_string pname,
                        enum interpolation_qualifier_t q = interpolation_smooth);

      /*!
       * Add a float varying, equivalent to
       * \code
       * set_float_varying(floats(q).size(), pname, q)
       * \endcode
       */
      varying_list&
      add_float_varying(c_string pname, enum interpolation_qualifier_t q = interpolation_smooth);

      /*!
       * Set a uint of the named slot to a name.
       * \param pname name to use
       * \param slot which uint
       */
      varying_list&
      set_uint_varying(unsigned int slot, c_string pname);

      /*!
       * Add an uint varying, equivalent to
       * \code
       * set_uint_varying(uints().size(), pname)
       * \endcode
       */
      varying_list&
      add_uint_varying(c_string pname);

      /*!
       * Set a int of the named slot to a name.
       * \param pname name to use
       * \param slot which uint
       */
      varying_list&
      set_int_varying(unsigned int slot, c_string pname);

      /*!
       * Add an int varying, equivalent to
       * \code
       * set_int_varying(ints().size(), pname)
       * \endcode
       */
      varying_list&
      add_int_varying(c_string pname);

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
     *                           in uint shader_data_offset,
     *                           out uint z_add,
     *                           out vec2 brush_p,
     *                           out vec3 clip_p)
     * \endcode
     * where
     *  - sub_shader corresponds to PainterItemShader::sub_shader()
     *  - attrib0 corresponds to PainterAttribute::m_attrib0,
     *  - attrib1 corresponds to PainterAttribute::m_attrib1,
     *  - attrib2 corresponds to PainterAttribute::m_attrib2 and
     *  - shader_data_offset is what block in the data store for
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
     *                           in uint shader_data_offset)
     * \endcode
     *
     * which returns the color of the fragment for the item -before-
     * the color modulation by the pen, brush or having compositing
     * applied. In addition, the color value returned is NOT
     * pre-multiplied by alpha either.
     *
     * Available to only the vertex shader are the following:
     *  - mat3 fastuidraw_item_matrix (the 3x3 matrix from item coordinate to clip coordinates)
     *
     * Available to both the vertex and fragment shader are the following:
     *  - sampler2DArray fastuidraw_imageAtlasLinear the color texels (AtlasColorBackingStoreBase) for images unfiltered
     *  - sampler2DArray fastuidraw_imageAtlasLinearFiltered the color texels (AtlasColorBackingStoreBase) for images bilinearly filtered
     *  - usampler2DArray fastuidraw_imageIndexAtlas the texels of the index atlas (AtlasIndexBackingStoreBase) for images
     *  - the macro fastuidraw_fetch_data(B) to fetch the B'th block from the data store buffer
     *    (PainterDraw::m_store), return as uvec4. To get floating point data use, the GLSL
     *    built-in function uintBitsToFloat().
     *  - the macro fastuidraw_fetch_glyph_data(B) to read the B'th value from the glyph data
     *    (GlyphAtlasBackingStoreBase), return as uint. To get floating point data, use the GLSL
     *    built-in function uintBitsToFloat().
     *  - the macro fastuidraw_colorStopFetch(x, L) to retrieve the color stop value at location x of layer L
     *  - vec2 fastuidraw_viewport_pixels the viewport dimensions in pixels
     *  - vec2 fastuidraw_viewport_recip_pixels reciprocal of fastuidraw_viewport_pixels
     *  - vec2 fastuidraw_viewport_recip_pixels_magnitude euclidean length of fastuidraw_viewport_recip_pixels
     *
     * For both stages, the value of the argument of shader_data_offset is which block into the data
     * store (PainterDraw::m_store) of the custom shader data. Do
     * \code
     * fastuidraw_fetch_data(shader_data_offset)
     * \endcode
     * to read the raw bits of the data. The type returned by the macro fastuidraw_fetch_data()
     *
     * Use the GLSL built-in uintBitsToFloat() to covert the uint bit-value to float
     * and just cast int() to get the value as an integer.
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
       * Ctor.
       * \param puses_discard set to true if and only if the shader code
       *                      will use discard. Discard should be used
       *                      in the GLSL code via the macro FASTUIDRAW_DISCARD.
       * \param vertex_src GLSL source holding vertex shader routine
       * \param fragment_src GLSL source holding fragment shader routine
       * \param varyings list of varyings of the shader
       * \param num_sub_shaders the number of sub-shaders it supports
       */
      PainterItemShaderGLSL(bool puses_discard,
                            const ShaderSource &vertex_src,
                            const ShaderSource &fragment_src,
                            const varying_list &varyings,
                            unsigned int num_sub_shaders = 1);

      ~PainterItemShaderGLSL();

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
       * Returns true if the fragment shader uses discard
       */
      bool
      uses_discard(void) const;

    private:
      void *m_d;
    };
/*! @} */

  }
}
