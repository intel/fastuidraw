/*!
 * \file unpack_source_generator.hpp
 * \brief file unpack_source_generator.hpp
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
     * An unpack source generator is used to generate shader
     * source code to unpack data from the data store buffer
     * into a GLSL struct.
     */
    class UnpackSourceGenerator
    {
    public:
      /*!
       * Enumeration specifing GLSL type for field
       * (or subfield) of a value to unpack.
       */
      enum type_t
        {
          float_type, /*!< GLSL type is float */
          uint_type,  /*!< GLSL type is uint */
          int_type,   /*!< GLSL type is int */

          /*!
           * indicates that the offset corresponds to
           * padding and not any field or data.
           */
          padding_type,
        };

      /*!
       * Ctor.
       * \param name of GLSL struct that to which to unpack data
       *             from the data store buffer
       */
      explicit
      UnpackSourceGenerator(c_string name_struct_type);

      /*!
       * Copy ctor.
       * \param obj value from which to copy values.
       */
      UnpackSourceGenerator(const UnpackSourceGenerator &obj);

      ~UnpackSourceGenerator();

      /*!
       * Assignment operator
       */
      UnpackSourceGenerator&
      operator=(const UnpackSourceGenerator &rhs);

      /*!
       * Swap operation
       * \param obj object with which to swap
       */
      void
      swap(UnpackSourceGenerator &obj);

      /*!
       * Set the field name that corresponds to an offset
       * \param offset offset from the start of the packed struct
       * \param field_name GLSL name of the field to which to unpack
       *                   the single scalar value. The value must
       *                   include the dot if it is a field member of
       *                   a struct.
       * \param type the GLSL type of the field.
       */
      UnpackSourceGenerator&
      set(unsigned int offset, c_string field_name, type_t type = float_type);

      /*!
       * Stream the unpack function into a \ref ShaderSource object.
       * The function generated is
       * \code
       * return_type
       * function_name(in uint location, out struct_name v)
       * \endcode
       * where location is the location of the struct to unpack,
       * struct_name is the name of the struct to unpack to given
       * in the ctor.
       * \param str \ref ShaderSource to which to stream the unpack function
       * \param function_name name to give the function
       * \param returns_new_offset if true, the return_type is an uint and
       *                           returns the offset of the last offset
       *                           supplied by set(). If value, return_type is
       *                           void.
       */
      void
      stream_unpack_function(ShaderSource &str,
                             c_string function_name,
                             bool returns_new_offset = true) const;

    private:
      void *m_d;
    };
/*! @} */

  }
}
