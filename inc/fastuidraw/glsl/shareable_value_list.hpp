/*!
 * \file shareable_value_list.hpp
 * \brief file shareable_value_list.hpp
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
     * A shareable_value_list is a list of values and their types
     * that a shader will have computed after running. These can
     * be used is shader-chaining to get some of the values made
     * from a dependee shader.
     */
    class shareable_value_list
    {
    public:
      /*!
       * \brief
       * Enumeration to define the types of the a shareable value
       * can be.
       */
      enum type_t
        {
          type_float, /*!< corresponds to float in GLSL */
          type_uint, /*!< corresponds to uint in GLSL */
          type_int, /*!< corresponds to int in GLSL */

          type_number_types,
        };

      /*!
       * Swap operation
       * \param obj object with which to swap
       */
      void
      swap(shareable_value_list &obj)
      {
        m_data.swap(obj.m_data);
      }

      /*!
       * Returns an array indexed by \ref type_t that
       * holds the number of shareable variables for
       * each type.
       */
      vecN<unsigned int, type_number_types>
      number_shareable_values(void) const
      {
        vecN<unsigned int, type_number_types> R;
        for (unsigned int i = 0; i < type_number_types; ++i)
          {
            R[i] = m_data[i].size();
          }
        return R;
      }

      /*!
       * Returns the names of the shareable values of the
       * specified type.
       * \param q type
       */
      c_array<const c_string>
      shareable_values(enum type_t q) const
      {
        return m_data[q].get();
      }

      /*!
       * Add a shareable value
       * \param pname name by which to reference the shareable value
       * \param q type of the shareable value
       */
      shareable_value_list&
      add_shareable_value(c_string pname, enum type_t q)
      {
        m_data[q].push_back(pname);
        return *this;
      }

      /*!
       * Add an uint shareable value, equivalent to
       * \code
       * add_shareable_value(pname, type_uint);
       * \endcode
       */
      shareable_value_list&
      add_uint(c_string pname)
      {
        return add_shareable_value(pname, type_uint);
      }

      /*!
       * Add an uint shareable value, equivalent to
       * \code
       * add_shareable_value(pname, type_int);
       * \endcode
       */
      shareable_value_list&
      add_int(c_string pname)
      {
        return add_shareable_value(pname, type_int);
      }

      /*!
       * Add an uint shareable value, equivalent to
       * \code
       * add_shareable_value(pname, type_smooth);
       * \endcode
       */
      shareable_value_list&
      add_float(c_string pname)
      {
        return add_shareable_value(pname, type_float);
      }
    private:
      vecN<string_array, type_number_types> m_data;
    };

/*! @} */
  }
}
