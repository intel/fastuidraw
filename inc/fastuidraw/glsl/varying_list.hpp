/*!
 * \file varying_list.hpp
 * \brief file varying_list.hpp
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

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/string_array.hpp>

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
     * their names) which is the same as the out's of the vertex
     * shader which which it is paired.
     *
     * A varying is ALWAYS a SCALAR. The varyings of shaders should
     * -never- be declared in the shader code. Instead, each varying
     * should be declared in the \ref* varying_list object passed to
     * the shader object's ctor. The \ref GLSL module will share the
     * varyings across different shaders within the uber-shader.
     * Indeed, the number of varying the uber-shader has is not the
     * sum of the varyings across all the shaders, but rather is the
     * maximum number of varyings across the shaders present in the
     * uber-shader.
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
       * Returns the names of the float varyings of the
       * specified interpolation type.
       * \param q interpolation type
       */
      c_array<const c_string>
      floats(enum interpolation_qualifier_t q) const;

      /*!
       * Returns the names of the uint varyings
       */
      c_array<const c_string>
      uints(void) const;

      /*!
       * Returns the names of the int varyings
       */
      c_array<const c_string>
      ints(void) const;

      /*!
       * Returns the source names of the aliases, i.e.
       * the values of the first argument of calls to
       * \ref add_alias().
       */
      c_array<const c_string>
      alias_list_names(void) const;

      /*!
       * Returns the alias names of the aliases, i.e.
       * the values of the second argument of calls
       * to \ref add_alias().
       */
      c_array<const c_string>
      alias_list_alias_names(void) const;

      /*!
       * Add a float varying, equivalent to
       * \code
       * set_float_varying(floats(q).size(), pname, q)
       * \endcode
       */
      varying_list&
      add_float(c_string pname, enum interpolation_qualifier_t q = interpolation_smooth);

      /*!
       * Add an uint varying, equivalent to
       * \code
       * set_uint_varying(uints().size(), pname)
       * \endcode
       */
      varying_list&
      add_uint(c_string pname);

      /*!
       * Add an int varying, equivalent to
       * \code
       * set_int_varying(ints().size(), pname)
       * \endcode
       */
      varying_list&
      add_int(c_string pname);

      /*!
       * Add an alias to a varying. The use case being if a fixed
       * varying is used in two different roles and aliasing the
       * name makes the GLSL shader code more readable.
       * \param name the name of the varying to alias
       * \param alias_name the alias for the varying
       */
      varying_list&
      add_alias(c_string name, c_string alias_name);

    private:
      void *m_d;
    };
/*! @} */

  }
}
