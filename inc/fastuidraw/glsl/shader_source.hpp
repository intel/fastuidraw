/*!
 * \file shader_source.hpp
 * \brief file shader_source.hpp
 *
 * Adapted from: WRATHGLProgram.hpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */

#pragma once

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw {
namespace glsl {
/*!\addtogroup GLSLShaderBuilder
  @{
 */

/*!
  A ShaderSource represents the source code
  to a GLSL shader, specifying blocks of source
  code and macros to use.
 */
class ShaderSource
{
public:
  /*!
    Enumeration to indiciate
    the source for a shader.
   */
  enum source_t
    {
      /*!
        Shader source code is taken
        from the file whose name
        is the passed string.
       */
      from_file,

      /*!
        The passed string is the
        shader source code.
       */
      from_string,

      /*!
        The passed string is label
        for a string of text fetched
        with fastuidraw::fetch_static_resource()
       */
      from_resource,
    };

  /*!
    Enumeration to determine if source
    code or a macro
   */
  enum add_location_t
    {
      /*!
        add the source code or macro
        to the back.
       */
      push_back,

      /*!
        add the source code or macro
        to the front.
       */
      push_front
    };

  /*!
    Enumeration to indicate extension
    enable flags.
   */
  enum extension_enable_t
    {
      /*!
        Requires the named GLSL extension,
        i.e. will add <B>"#extension extension_name: require"</B>
        to GLSL source code.
       */
      require_extension,

      /*!
        Enables the named GLSL extension,
        i.e. will add <B>"#extension extension_name: enable"</B>
        to GLSL source code.
       */
      enable_extension,

      /*!
        Enables the named GLSL extension,
        but request that the GLSL compiler
        issues warning when the extension
        is used, i.e. will add
        <B>"#extension extension_name: warn"</B>
        to GLSL source code.
       */
      warn_extension,

      /*!
        Disables the named GLSL extension,
        i.e. will add <B>"#extension extension_name: disable"</B>
        to GLSL source code.
       */
      disable_extension
    };

  /*!
    Ctor.
   */
  ShaderSource(void);

  /*!
    Copy ctor.
    \param obj value from which to copy
   */
  ShaderSource(const ShaderSource &obj);

  ~ShaderSource();

  /*!
    Assignment operator.
    \param obj value from which to copy
   */
  ShaderSource&
  operator=(const ShaderSource &obj);

  /*!
    Specifies the version of GLSL to which to
    declare the shader. An empty string indicates
    to not have a "#version" directive in the shader.
   */
  ShaderSource&
  specify_version(const char *v);

  /*!
    Add shader source code to this ShaderSource.
    \param str string that is a filename, GLSL source or a resource name
    \param tp interpretation of str, i.e. determines if
              str is a filename, raw GLSL source or a resource
    \param loc location to add source
   */
  ShaderSource&
  add_source(const char *str, enum source_t tp = from_file,
             enum add_location_t loc = push_back);

  /*!
    Add the sources from another ShaderSource object.
    \param obj ShaderSource object from which to absorb
   */
  ShaderSource&
  add_source(const ShaderSource &obj);

  /*!
    Add a macro to this ShaderSource.
    Functionally, will insert \#define macro_name macro_value
    in the GLSL source code.
    \param macro_name name of macro
    \param macro_value value to which macro is given
    \param loc location to add macro within code
   */
  ShaderSource&
  add_macro(const char *macro_name, const char *macro_value = "",
            enum add_location_t loc = push_back);

  /*!
    Add a macro to this ShaderSource.
    Functionally, will insert \#define macro_name macro_value
    in the GLSL source code.
    \param macro_name name of macro
    \param macro_value value to which macro is given
    \param loc location to add macro within code
   */
  ShaderSource&
  add_macro(const char *macro_name, uint32_t macro_value,
            enum add_location_t loc = push_back);

  /*!
    Add a macro to this ShaderSource.
    Functionally, will insert \#define macro_name macro_value
    in the GLSL source code.
    \param macro_name name of macro
    \param macro_value value to which macro is given
    \param loc location to add macro within code
   */
  ShaderSource&
  add_macro(const char *macro_name, int32_t macro_value,
            enum add_location_t loc = push_back);

  /*!
    Adds the string
    \code
    #undef X
    \endcode
    where X is the passed macro name
    \param macro_name name of macro
   */
  ShaderSource&
  remove_macro(const char *macro_name);

  /*!
    Specifiy an extension and usage.
    \param ext_name name of GL extension
    \param tp usage of extension
   */
  ShaderSource&
  specify_extension(const char *ext_name,
                    enum extension_enable_t tp = enable_extension);

  /*!
    Add all the extension specifacation from another
    ShaderSource object to this ShaderSource objects.
    \param obj ShaderSource object from which to take
   */
  ShaderSource&
  specify_extensions(const ShaderSource &obj);

  /*!
    Returns the GLSL code assembled. The returned string is only
    gauranteed to be valid up until the ShaderSource object
    is modified.
   */
  const char*
  assembled_code(void) const;

  private:
    void *m_d;
};
/*! @} */

} //namespace glsl
} //namespace fastuidraw
