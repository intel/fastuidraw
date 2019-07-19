/*!
 * \file varying_list.hpp
 * \brief file varying_list.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#ifndef FASTUIDRAW_VARYING_LIST_HPP
#define FASTUIDRAW_VARYING_LIST_HPP

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
     * should be declared in the \ref varying_list object passed to
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
       * Enumeration to define the interpolator type of a varying
       */
      enum interpolator_type_t
        {
          interpolator_smooth, /*!< corresponds to "smooth" of type float in GLSL */
          interpolator_noperspective, /*!< corresponds to "noperspective" of type float in GLSL */
          interpolator_flat, /*!< corresponds to "flat" of type float in GLSL */
          interpolator_uint, /*!< corresponds to "flat" of type uint in GLSL */
          interpolator_int, /*!< corresponds to "flat" of type int in GLSL */

          interpolator_number_types,
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
       * Returns the names of the varyings of the
       * specified interpolator type.
       * \param q interpolator type
       */
      c_array<const c_string>
      varyings(enum interpolator_type_t q) const;

      /*!
       * Returns the alias names of the aliases, i.e.
       * the values of the first argument of calls
       * to \ref add_varying_alias().
       */
      c_array<const c_string>
      alias_varying_names(void) const;

      /*!
       * Returns the source names of the aliases, i.e.
       * the values of the second argument of calls to
       * \ref add_varying_alias().
       */
      c_array<const c_string>
      alias_varying_source_names(void) const;

      /*!
       * Add a varying
       * \param pname name by which to reference the varying
       * \param q interpolator type of the varying
       */
      varying_list&
      add_varying(c_string pname, enum interpolator_type_t q);

      /*!
       * Add an uint varying, equivalent to
       * \code
       * add_varying(pname, interpolator_uint);
       * \endcode
       */
      varying_list&
      add_uint(c_string pname)
      {
        return add_varying(pname, interpolator_uint);
      }

      /*!
       * Add an uint varying, equivalent to
       * \code
       * add_varying(pname, interpolator_int);
       * \endcode
       */
      varying_list&
      add_int(c_string pname)
      {
        return add_varying(pname, interpolator_int);
      }

      /*!
       * Add an uint varying, equivalent to
       * \code
       * add_varying(pname, interpolator_smooth);
       * \endcode
       */
      varying_list&
      add_float(c_string pname)
      {
        return add_varying(pname, interpolator_smooth);
      }

      /*!
       * Add an uint varying, equivalent to
       * \code
       * add_varying(pname, interpolator_flat);
       * \endcode
       */
      varying_list&
      add_float_flat(c_string pname)
      {
        return add_varying(pname, interpolator_flat);
      }

      /*!
       * Add an uint varying, equivalent to
       * \code
       * add_varying(pname, interpolator_noperspective);
       * \endcode
       */
      varying_list&
      add_float_noperspective(c_string pname)
      {
        return add_varying(pname, interpolator_noperspective);
      }

      /*!
       * Add an alias to a varying. The use case being if a fixed
       * varying is used in two different roles and aliasing the
       * name makes the GLSL shader code more readable.
       * \param name the new identifier to reference an existing varying
       * \param src_name the varying referenced by name, which should
       *                 be a string value that has been passed as
       *                 the first argument to \ref add_varying() or
       *                 \ref add_varying_alias().
       */
      varying_list&
      add_varying_alias(c_string name, c_string src_name);

    private:
      void *m_d;
    };
/*! @} */

  }
}

#endif
