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
       * Enumeration specifying to bit-cast with
       * GLSL's built-in uintBitsToFloat() or not
       */
      enum cast_t
        {
          /*!
           * Reinterpret the bits as float bits, i.e.
           * use uintBitsToFloat() of GLSL
           */
          reinterpret_to_float_bits,

          /*!
           * only type-cast the bits.
           */
          type_cast,
        };

      /*!
       * Ctor.
       * \param type_name name of GLSL type to which to unpack
       *                  data from the data store buffer
       */
      explicit
      UnpackSourceGenerator(c_string type_name);

      /*!
       * Ctor.
       * \param type_names names of GLSL type to which to unpack
       *                   data from the data store buffer
       */
      explicit
      UnpackSourceGenerator(c_array<const c_string> type_names);

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
       *               in units of \ref generic_data
       * \param field_name GLSL name of the field to which to unpack
       *                   the single scalar value. The value must
       *                   include the dot if it is a field member of
       *                   a struct.
       * \param type the GLSL type of the field
       * \param cast how to interpret the bits of the value
       * \param struct_idx if the ctor was given an array of c_string
       *                   values, refers to the index into that
       *                   array of the values.
       */
      UnpackSourceGenerator&
      set(unsigned int offset, c_string field_name,
          type_t type, enum cast_t cast,
          unsigned int struct_idx = 0);

      /*!
       * Set the field name that corresponds to an offset and range of
       * bits within the value at the named offset
       * \param offset offset from the start of the packed struct
       *               in units of \ref generic_data
       * \param bit0 first bit of field value storted at offset
       * \param num_bits number of bits used to store value.
       * \param field_name GLSL name of the field to which to unpack
       *                   the single scalar value. The value must
       *                   include the dot if it is a field member of
       *                   a struct.
       * \param type the GLSL type of the field, must be \ref uint_type
       *             or \ref int_type
       * \param cast how to interpret the bits of the value
       * \param struct_idx if the ctor was given an array of c_string
       *                   values, refers to the index into that
       *                   array of the values.
       */
      UnpackSourceGenerator&
      set(unsigned int offset, unsigned int bit0, unsigned int num_bits,
          c_string field_name, type_t type, enum cast_t cast,
          unsigned int struct_idx = 0);

      /*!
       * Provided as a conveniance, equivalent to
       * \code
       * set(offset, field_name, float_type, reinterpret_to_float_bits, struct_idx);
       * \endcode
       * \param offset offset from the start of the packed struct
       *               in units of \ref generic_data
       * \param field_name GLSL name of the field to which to unpack
       *                   the single scalar value. The value must
       *                   include the dot if it is a field member of
       *                   a struct.
       * \param struct_idx if the ctor was given an array of c_string
       *                   values, refers to the index into that
       *                   array of the values.
       */
      UnpackSourceGenerator&
      set_float(unsigned int offset, c_string field_name,
                unsigned int struct_idx = 0)
      {
        return set(offset, field_name, float_type, reinterpret_to_float_bits, struct_idx);
      }

      /*!
       * Provided as a conveniance, equivalent to
       * \code
       * set(offset, field_name, uint_type, type_cast, struct_idx);
       * \endcode
       * \param offset offset from the start of the packed struct
       *               in units of \ref generic_data
       * \param field_name GLSL name of the field to which to unpack
       *                   the single scalar value. The value must
       *                   include the dot if it is a field member of
       *                   a struct.
       * \param struct_idx if the ctor was given an array of c_string
       *                   values, refers to the index into that
       *                   array of the values.
       */
      UnpackSourceGenerator&
      set_uint(unsigned int offset, c_string field_name,
                unsigned int struct_idx = 0)
      {
        return set(offset, field_name, uint_type, type_cast, struct_idx);
      }

      /*!
       * Provided as a conveniance, equivalent to
       * \code
       * set(offset, field_name, int_type, type_cast, struct_idx);
       * \endcode
       * \param offset offset from the start of the packed struct
       *               in units of \ref generic_data
       * \param field_name GLSL name of the field to which to unpack
       *                   the single scalar value. The value must
       *                   include the dot if it is a field member of
       *                   a struct.
       * \param struct_idx if the ctor was given an array of c_string
       *                   values, refers to the index into that
       *                   array of the values.
       */
      UnpackSourceGenerator&
      set_int(unsigned int offset, c_string field_name,
              unsigned int struct_idx = 0)
      {
        return set(offset, field_name, int_type, type_cast, struct_idx);
      }

      /*!
       * Stream the unpack function into a \ref ShaderSource object.
       * For values constructed passed a single c_string, the function
       * generated is
       * \code
       * void
       * function_name(in uint location, out struct_name v)
       * \endcode
       * and for those values constructed passed an array of c_string
       * values, the function generated is
       * \code
       * void
       * function_name(in uint location, out struct_name0 v0, out struct_name1 v1, ...)
       * \endcode
       * where for both, location is the location from which to unpack
       * data and the remaining arguments are the struct types to which
       * to unpacked as passed in the ctor.
       * \param str \ref ShaderSource to which to stream the unpack function
       * \param function_name name to give the function
       */
      const UnpackSourceGenerator&
      stream_unpack_function(ShaderSource &str,
                             c_string function_name) const;

      /*!
       * Stream a function with the given name that returns the
       * number of data blocks (i.e. the number of vecN<generic_data, 4>
       * elements) used to store the struct described by this
       * \ref UnpackSourceGenerator.
       * \param str \ref ShaderSource to which to stream the unpack function
       * \param function_name name to give the function
       */
      const UnpackSourceGenerator&
      stream_unpack_size_function(ShaderSource &str,
                                  c_string function_name) const;

      /*!
       * Provided as a conveniance, equivalent to
       * \code
       * ShaderSource return_value;
       * stream_unpack_function(return_value, function_name);
       * return return_value;
       * \endcode
       * \param name to give the function
       */
      ShaderSource
      stream_unpack_function(c_string function_name) const
      {
        ShaderSource return_value;
        stream_unpack_function(return_value, function_name);
        return return_value;
      }

      /*!
       * Provided as a conveniance, equivalent to
       * \code
       * ShaderSource return_value;
       * stream_unpack_size_function(return_value, function_name);
       * return return_value;
       * \endcode
       * \param name to give the function
       */
      ShaderSource
      stream_unpack_size_function(c_string function_name) const
      {
        ShaderSource return_value;
        stream_unpack_size_function(return_value, function_name);
        return return_value;
      }

    private:
      void *m_d;
    };
/*! @} */

  }
}
