/*!
 * \file painter_shader_gl.hpp
 * \brief file painter_shader_gl.hpp
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

#include <fastuidraw/painter/painter_shader.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>

namespace fastuidraw
{
  namespace gl
  {
/*!\addtogroup GLBackend
  @{
 */

    /*!
      A varying_list lists all the in's of a frag
      shader (and their names) or all the out's of vertex
      shader. A varying for a PainterShaderGL is a
      SCALAR. For a vertex and fragment shader pair,
      the name of the varying does NOT matter for the
      sending of a vertex shader out to a fragment shader
      in. Instead, the slot matters. The virtual slots for
      each varying type are seperate, i.e. slot 0 for uint
      is a different slot than slot 0 for int. In addition
      the interpolation type is part of the type for floats,
      thus slot 0 for flat float is a different slot than
      slot 0 for smooth float.
    */
    class varying_list
    {
    public:
      /*!
        Enumeration to define the interpolation of a varying
      */
      enum interpolation_qualifier_t
        {
          interpolation_smooth, /*!< corresponds to "smooth" in GLSL */
          interpolation_flat, /*!< corresponds to "flat" in GLSL */
          interpolation_noperspective, /*!< corresponds to "noperspective" in GLSL */

          interpolation_number_types,
        };

      /*!
        Ctor.
       */
      varying_list(void);

      /*!
        Copy ctor.
        \param rhs value from which to copy
       */
      varying_list(const varying_list &rhs);

      ~varying_list();

      /*!
        Assignment operator.
        \param rhs value from which to copy
       */
      varying_list&
      operator=(const varying_list &rhs);

      /*!
        Returns the names for the slots of the float
        varyings of the specified interpolation type.
        \param q interpolation type
       */
      const_c_array<const char*>
      floats(enum interpolation_qualifier_t q) const;

      /*!
        Returns the names for the slots of the uint varyings
       */
      const_c_array<const char*>
      uints(void) const;

      /*!
        Returns the names for the slots of the int varyings
       */
      const_c_array<const char*>
      ints(void) const;

      /*!
        Set a float of the named slot and qualifier to a name.
        \param pname name to use
        \param slot which float of the named qualifier
        \param q interpolation qualifier
      */
      varying_list&
      set_float_varying(unsigned int slot, const char *pname,
                        enum interpolation_qualifier_t q = interpolation_smooth);

      /*!
        Add a float varying, equivalent to
        \code
        set_float_varying(floats(q).size(), pname, q)
        \endcode
      */
      varying_list&
      add_float_varying(const char *pname, enum interpolation_qualifier_t q = interpolation_smooth);

      /*!
        Set a uint of the named slot to a name.
        \param pname name to use
        \param slot which uint
      */
      varying_list&
      set_uint_varying(unsigned int slot, const char *pname);

      /*!
        Add an uint varying, equivalent to
        \code
        set_uint_varying(uints().size(), pname)
        \endcode
      */
      varying_list&
      add_uint_varying(const char *pname);

      /*!
        Set a int of the named slot to a name.
        \param pname name to use
        \param slot which uint
      */
      varying_list&
      set_int_varying(unsigned int slot, const char *pname);

      /*!
        Add an int varying, equivalent to
        \code
        set_int_varying(ints().size(), pname)
        \endcode
      */
      varying_list&
      add_int_varying(const char *pname);

    private:
      void *m_d;
    };

    /*!
      A glsl_shader_unpack_value represents a value to unpack
      from the data store.
     */
    class glsl_shader_unpack_value
    {
    public:
      /*!
        Enumeration specifing GLSL type for value
        to unpack.
       */
      enum type_t
        {
          float_type, /*!< GLSL type is float */
          uint_type,  /*!< GLSL type is uint */
          int_type,   /*!< GLSL type is int */
        };

      /*!
        Ctor.
        \param pname value returned by name(). The string behind the passed pointer
                     is copied
        \param ptype the value returned by type().
       */
      glsl_shader_unpack_value(const char *pname = "", type_t ptype = float_type);

      /*!
        Copy ctor
       */
      glsl_shader_unpack_value(const glsl_shader_unpack_value &obj);

      ~glsl_shader_unpack_value();

      /*!
        Assignment operator
       */
      glsl_shader_unpack_value&
      operator=(const glsl_shader_unpack_value &rhs);

      /*!
        The name of the value to unpack as it appears in GLSL
       */
      const char*
      name(void) const;

      /*!
        The GLSL type of the value to unpack
       */
      enum type_t
      type(void) const;

      /*!
        Adds to a Shader::shader_source the GLSL code to unpack a
        stream of values. Returns the number of blocks needed to unpack
        the data in GLSL.
        \param alignment the alignment of the data store used in a
                          PainterBackendGL (i.e. the value of
                          PainterBackend::Configuration::alignment())
        \param str location to which to add the GLSL code
        \param labels GLSL names and types to which to unpack
        \param offset_name GLSL name for offset from which to unpack
                           values
        \param prefix string prefix by which to prefix the name values of labels
       */
      static
      unsigned int
      stream_unpack_code(unsigned int alignment, Shader::shader_source &str,
                         const_c_array<glsl_shader_unpack_value> labels,
                         const char *offset_name,
                         const char *prefix = "");

      /*!
        Adds to a Shader::shader_source the GLSL function:
        \code
        uint
        function_name(uint location, out out_type v)
        \endcode
        whose body is the unpacking of the values into an
        out. Returns the number of blocks needed to unpack
        the data in GLSL.
        \param alignment the alignment of the data store used in a
                          PainterBackendGL (i.e. the value of
                          PainterBackend::Configuration::alignment())
        \param str location to which to add the GLSL code
        \param labels GLSL names of the fields and their types
        \param function_name name to give the function
        \param out_type the out type of the function
        \param returns_new_offset if true, function returns the offset after
                                  the data it unpacks.
       */
      static
      unsigned int
      stream_unpack_function(unsigned int alignment, Shader::shader_source &str,
                             const_c_array<glsl_shader_unpack_value> labels,
                             const char *function_name,
                             const char *out_type,
                             bool returns_new_offset = true);
    private:
      void *m_d;
    };

    /*!
      A glsl_shader_unpack_value_set is a convenience class wrapping
      an array of glsl_shader_unpack_value objects.
     */
    template<size_t N>
    class glsl_shader_unpack_value_set:
      public vecN<glsl_shader_unpack_value, N>
    {
    public:
      /*!
        Set the named element to a value.
        \param i which element of this to set
        \param name name value
        \param type type value
       */
      glsl_shader_unpack_value_set&
      set(unsigned int i, const char *name,
          glsl_shader_unpack_value::type_t type = glsl_shader_unpack_value::float_type)
      {
        this->operator[](i) = glsl_shader_unpack_value(name, type);
        return *this;
      }

      /*!
        Provided as an API convenience, equivalent to
        \code
        glsl_shader_unpack_value::stream_unpack_code(alignment, str, *this, offset_name);
        \endcode
        \param alignment the alignment of the data store used in a
                          PainterBackendGL (i.e. the value of
                          PainterBackend::Configuration::alignment())
        \param str location to which to add the GLSL code
        \param offset_name GLSL name for offset from which to unpack
                           values
        \param prefix string prefix by which to prefix the name values of labels
       */
      unsigned int
      stream_unpack_code(unsigned int alignment, Shader::shader_source &str,
                         const char *offset_name,
                         const char *prefix = "")
      {
        return glsl_shader_unpack_value::stream_unpack_code(alignment, str, *this, offset_name, prefix);
      }

      /*!
        Provided as an API convenience, equivalent to
        \code
        glsl_shader_unpack_value::stream_unpack_function(alignment, str, *this, function_name,
                                                         out_type, returns_new_offset);
        \endcode
        \param alignment the alignment of the data store used in a
                          PainterBackendGL (i.e. the value of
                          PainterBackend::Configuration::alignment())
        \param str location to which to add the GLSL code
        \param function_name name to give the function
        \param out_type the out type of the function
        \param returns_new_offset if true, function returns the offset after
                                  the data it unpacks.
       */
      unsigned int
      stream_unpack_function(unsigned int alignment, Shader::shader_source &str,
                             const char *function_name,
                             const char *out_type,
                             bool returns_new_offset = true)
      {
        return glsl_shader_unpack_value::stream_unpack_function(alignment, str, *this, function_name,
                                                                out_type, returns_new_offset);
      }
    };


    /*!
      A PainterShaderGL is a GLSL source code -fragment-
      for a PainterBackendGL.

      A vertex shader needs to implement the function:
      \code
        vec4
        fastuidraw_gl_vert_main(in vec4 primary_attrib,
                               in vec4 secondary_attrib,
                               in uvec4 uint_attrib,
                               in uint shader_data_offset,
                               out uint z_add)
      \endcode

      which given the attribute data and the offset to the shader
      location produces the position of the vertex in item coordinates
      and the position to feed the brush. The position of the item
      is in the return value' .xy and the position to feed the brush
      in .zw. The out z_add, must be written to and represent the
      value by which to add to the unnormalized z-value from the
      item header (the z-value from the item header is a uint).
      Available to the vertex shader are the following:
       - mat3 fastuidraw_item_matrix (the 3x3 matrix from item coordinate to clip coordinates)
       - sampler2DArray fastuidraw_imageAtlas the color texels (AtlasColorBackingStoreBase) for images unfiltered
       - sampler2DArray fastuidraw_imageAtlasFiltered the color texels (AtlasColorBackingStoreBase) for images bilinearly filtered
       - usampler2DArray fastuidraw_imageIndexAtlas the texels of the index atlas (AtlasIndexBackingStoreBase) for images
       - usampler2DArray fastuidraw_glyphTexelStoreUINT the glyph texels (GlyphAtlasTexelBackingStoreBase), only available if FASTUIDRAW_PAINTER_EMULATE_GLYPH_TEXEL_STORE_FLOAT is NOT defined
       - samplerBuffer fastuidraw_glyphGeometryDataStore the geometry data of glyphs (GlyphAtlasGeometryBackingStoreBase)
       - samplerBuffer fastuidraw_painterStoreFLOAT the data store (PainterDrawCommand::m_store) as floats
       - samplerBuffer fastuidraw_painterStoreUINT the data store (PainterDrawCommand::m_store) as uint
       - samplerBuffer fastuidraw_painterStoreINT the data store (PainterDrawCommand::m_store) as int
       - the macro fastuidraw_colorStopFetch(x, L) to retrieve the color stop value at location x of layer L
       - vec2 fastuidraw_viewport_pixels the viewport dimensions in pixels
       - vec2 fastuidraw_viewport_recip_pixels reciprocal of fastuidraw_viewport_pixels
       - vec2 fastuidraw_viewport_recip_pixels_magnitude euclidean length of fastuidraw_viewport_recip_pixels

      The value of shader_data_offset is the offset into data store
      (PainterDrawCommand::m_store) of the custom vertex shader data in
      units of the alignment of the data store. This way, reading from
      \code
      texelFetch(fastuidraw_painterStoreFLOAT, shader_data_offset)
      \endcode
      is the read to perform. The data store is so that the
      sampler buffer
      - is format R if PainterBackend::Configuration::alignment() is 1,
      - is format RG if PainterBackend::Configuration::alignment() is 2,
      - is format RGB if PainterBackend::Configuration::alignment() is 3 and
      - is format RGBA if PainterBackend::Configuration::alignment() is 4.

      A fragment shader needs to implement the function:
      \code
        vec4
        fastuidraw_gl_frag_main(in uint shader_data_offset)
      \endcode

      which returns the color of the fragment for the item -before-
      the color modulation by the pen, brush or having blending
      applied. In addition, the color value returned is NOT
      pre-multiplied by alpha either.

      Available to the fragment shader are the following:
       - sampler2DArray fastuidraw_imageAtlas the color texels (AtlasColorBackingStoreBase) for images unfiltered
       - sampler2DArray fastuidraw_imageAtlasFiltered the color texels (AtlasColorBackingStoreBase) for images bilinearly filtered
       - usampler2DArray fastuidraw_imageIndexAtlas the texels of the index atlas (AtlasIndexBackingStoreBase) for images
       - usampler2DArray fastuidraw_glyphTexelStoreUINT the glyph texels (GlyphAtlasTexelBackingStoreBase)
       - ssampler2DArray fastuidraw_glyphTexelStoreFLOAT the glyph texels (GlyphAtlasTexelBackingStoreBase), only available if FASTUIDRAW_PAINTER_EMULATE_GLYPH_TEXEL_STORE_FLOAT is NOT defined
       - samplerBuffer fastuidraw_glyphGeometryDataStore the geometry data of glyphs (GlyphAtlasGeometryBackingStoreBase)
       - samplerBuffer fastuidraw_painterStoreFLOAT the data store (PainterDrawCommand::m_store) as floats
       - samplerBuffer fastuidraw_painterStoreUINT the data store (PainterDrawCommand::m_store) as uint
       - samplerBuffer fastuidraw_painterStoreINT the data store (PainterDrawCommand::m_store) as int
       - the macro fastuidraw_colorStopFetch(x, L) to retrieve the color stop value at location x of layer L
       - vec2 fastuidraw_viewport_pixels the viewport dimensions in pixels
       - vec2 fastuidraw_viewport_recip_pixels reciprocal of fastuidraw_viewport_pixels
       - vec2 fastuidraw_viewport_recip_pixels_magnitude euclidean length of fastuidraw_viewport_recip_pixels
       - fastuidraw_compute_image_atlas_coord(in vec2 image_shader_coord, in int index_layer, in int num_lookups, in int slack, out vec2 image_atlas_coord, out int image_atlas_layer) to compute the texel coordinate in fastuidraw_imageAtlas/fastuidraw_imageAtlasFiltered from a coordinate in fastuidraw_imageIndexAtlas
       - float fastuidraw_anisotropic_coverage(float d, float dx, float dy) for computing an anisotropic coverage value for d > 0, given
         the derivatives of d in screen space

      The value of shader_data_offset is the offset into data store
      (PainterDrawCommand::m_store) of the custom fragment shader data in
      units of the alignment of the data store. This way, reading from
      \code
      texelFetch(fastuidraw_painterStoreFLOAT, shader_data_offset)
      \endcode
      is the read to perform. The data store is so that the
      sampler buffer
      - is format R if PainterBackend::Configuration::alignment() is 1,
      - is format RG if PainterBackend::Configuration::alignment() is 2,
      - is format RGB if PainterBackend::Configuration::alignment() is 3 and
      - is format RGBA if PainterBackend::Configuration::alignment() is 4.

      In addition, a PainterShaderGL can require out's for a
      vertex shader (or in's for a fragment shader). This
      requirement is specified with a varying_list object.
      The shader code is to refer directly to the names
      in the varying_list object.

      Lastly, one can use the classes glsl_shader_unpack_value
      and glsl_shader_unpack_value_set to generate shader code
      to unpack values from the data in fastuidraw_painterStoreFLOAT,
      fastuidraw_painterStoreUINT and fastuidraw_painterStoreINT.
     */
    class PainterShaderGL:public PainterShader
    {
    public:
      /*!
        Overload typedef for handle
      */
      typedef reference_counted_ptr<PainterShaderGL> handle;

      /*!
        Overload typedef for const_handle
      */
      typedef reference_counted_ptr<const PainterShaderGL> const_handle;

      /*!
        Ctor.
        \param src GLSL source holding shader routine
        \param varyings list of varyings of the shader
       */
      PainterShaderGL(const Shader::shader_source &src,
                      const varying_list &varyings = varying_list());

      ~PainterShaderGL();

      /*!
        Returns the varying of the shader
       */
      const varying_list&
      varyings(void) const;

      /*!
        Return the GLSL source of the shader
       */
      const Shader::shader_source&
      src(void) const;

    private:
      void *m_d;
    };

  }
/*! @} */
}
