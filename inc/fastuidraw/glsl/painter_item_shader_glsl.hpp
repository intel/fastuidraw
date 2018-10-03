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
     * A varying_list lists all the in's of a frag
     * shader (and their names) or all the out's of vertex
     * shader.
     *
     * A varying for a PainterShaderGL is a SCALAR. For a vertex
     * and fragment shader pair, the name of the varying does NOT
     * matter for the sending of a vertex shader out to a fragment
     * shader in. Instead, the slot matters. The virtual slots for
     * each varying type are seperate, i.e. slot 0 for uint is a
     * different slot than slot 0 for int. In addition the
     * interpolation type is part of the type for floats,
     * thus slot 0 for flat float is a different slot than
     * slot 0 for smooth float.
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
     * A shader_unpack_value represents a value to unpack
     * from the data store.
     */
    class shader_unpack_value
    {
    public:
      /*!
       * Enumeration specifing GLSL type for value
       * to unpack.
       */
      enum type_t
        {
          float_type, /*!< GLSL type is float */
          uint_type,  /*!< GLSL type is uint */
          int_type,   /*!< GLSL type is int */
        };

      /*!
       * Ctor.
       * \param pname value returned by name(). The string behind the passed pointer
       *              is copied
       * \param ptype the value returned by type().
       */
      shader_unpack_value(c_string pname = "", type_t ptype = float_type);

      /*!
       * Copy ctor
       */
      shader_unpack_value(const shader_unpack_value &obj);

      ~shader_unpack_value();

      /*!
       * Assignment operator
       */
      shader_unpack_value&
      operator=(const shader_unpack_value &rhs);

      /*!
       * Swap operation
       * \param obj object with which to swap
       */
      void
      swap(shader_unpack_value &obj);

      /*!
       * The name of the value to unpack as it appears in GLSL
       */
      c_string
      name(void) const;

      /*!
       * The GLSL type of the value to unpack
       */
      enum type_t
      type(void) const;

      /*!
       * Adds to a ShaderSource the GLSL code to unpack a
       * stream of values. Returns the number of blocks needed to unpack
       * the data in GLSL.
       * \param str location to which to add the GLSL code
       * \param labels GLSL names and types to which to unpack
       * \param offset_name GLSL name for offset from which to unpack
       *                    values
       * \param prefix string prefix by which to prefix the name values of labels
       */
      static
      unsigned int
      stream_unpack_code(ShaderSource &str,
                         c_array<const shader_unpack_value> labels,
                         c_string offset_name,
                         c_string prefix = "");

      /*!
       * Adds to a ShaderSource the GLSL function:
       * \code
       * uint
       * function_name(uint location, out out_type v)
       * \endcode
       * whose body is the unpacking of the values into an
       * out. Returns the number of blocks needed to unpack
       * the data in GLSL.
       * \param str location to which to add the GLSL code
       * \param labels GLSL names of the fields and their types
       * \param function_name name to give the function
       * \param out_type the out type of the function
       * \param returns_new_offset if true, function returns the offset after
       *                           the data it unpacks.
       */
      static
      unsigned int
      stream_unpack_function(ShaderSource &str,
                             c_array<const shader_unpack_value> labels,
                             c_string function_name,
                             c_string out_type,
                             bool returns_new_offset = true);
    private:
      void *m_d;
    };

    /*!
     * \brief
     * A shader_unpack_value_set is a convenience class wrapping
     * an array of shader_unpack_value objects.
     */
    template<size_t N>
    class shader_unpack_value_set:
      public vecN<shader_unpack_value, N>
    {
    public:
      /*!
       * Set the named element to a value.
       * \param i which element of this to set
       * \param name name value
       * \param type type value
       */
      shader_unpack_value_set&
      set(unsigned int i, c_string name,
          shader_unpack_value::type_t type = shader_unpack_value::float_type)
      {
        this->operator[](i) = shader_unpack_value(name, type);
        return *this;
      }

      /*!
       * Provided as an API convenience, equivalent to
       * \code
       * shader_unpack_value::stream_unpack_code(str, *this, offset_name);
       * \endcode
       * \param str location to which to add the GLSL code
       * \param offset_name GLSL name for offset from which to unpack
       *                    values
       * \param prefix string prefix by which to prefix the name values of labels
       */
      unsigned int
      stream_unpack_code(ShaderSource &str,
                         c_string offset_name,
                         c_string prefix = "")
      {
        return shader_unpack_value::stream_unpack_code(str, *this, offset_name, prefix);
      }

      /*!
       * Provided as an API convenience, equivalent to
       * \code
       * shader_unpack_value::stream_unpack_function(str, *this, function_name,
       *                                             out_type, returns_new_offset);
       * \endcode
       * \param str location to which to add the GLSL code
       * \param function_name name to give the function
       * \param out_type the out type of the function
       * \param returns_new_offset if true, function returns the offset after
       *                           the data it unpacks.
       */
      unsigned int
      stream_unpack_function(ShaderSource &str,
                             c_string function_name,
                             c_string out_type,
                             bool returns_new_offset = true)
      {
        return shader_unpack_value::stream_unpack_function(str, *this, function_name,
                                                           out_type, returns_new_offset);
      }
    };


    /*!
     * \brief
     * A PainterItemShaderGLSL is a collection of GLSL source code
     * fragments for a \ref PainterShaderRegistrarGLSL.
     *
     * The vertex shader code needs to implement the function:
     * \code
     *   vec4
     *   fastuidraw_gl_vert_main(in uint sub_shader,
     *                           in uvec4 attrib0,
     *                           in uvec4 attrib1,
     *                           in uvec4 attrib2,
     *                           in uint shader_data_offset,
     *                           out uint z_add)
     * \endcode
     * where
     *  - sub_shader corresponds to PainterItemShader::sub_shader()
     *  - attrib0 corresponds to PainterAttribute::m_attrib0,
     *  - attrib1 corresponds to PainterAttribute::m_attrib1,
     *  - attrib2 corresponds to PainterAttribute::m_attrib2 and
     *  - shader_data_offset is what block in the data store for
     *    the data packed by PainterItemShaderData::pack_data()
     *    of the PainterItemShaderData in the \ref Painter (or
     *    \ref PainterPacker) call; use the macro fastuidraw_fetch_data()
     *    to read the data.
     *
     * The function's return value's .xy gives the position of
     * the vertex in item coordinates and the .zw give the
     * position to feed the brush. The out z_add must be written
     * to as well and it is how much to add to the value in \ref
     * PainterHeader::m_z (the value is the value of
     * Painter::current_z()) for the purpose of intra-item
     * z-occluding.
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
     *  - the macro fastuidraw_fetch_glyph_data(B) to read the B'th value from the glyph geometry data
     *    (GlyphAtlasGeometryBackingStoreBase), return as uint. To get floating point data, use the GLSL
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
     * Lastly, one can use the classes \ref shader_unpack_value
     * and \ref shader_unpack_value_set to generate shader code
     * to unpack values from the data in the data store buffer.
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
