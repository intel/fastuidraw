/*!
 * \file string_array.hpp
 * \brief file string_array.hpp
 *
 * Copyright 2018 by Intel.
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

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/util.hpp>

/*!\addtogroup Utility
 * @{
 */
namespace fastuidraw
{
  /*!
   * Simple class to hold an array of strings; string values
   * are copied into the string_array.
   */
  class string_array
  {
  public:
    /*!
     * Ctor.
     */
    string_array(void);

    /*!
     * Copy ctor.
     * \param obj value from which to copy
     */
    string_array(const string_array &obj);

    ~string_array();
    
    /*!
     * Assignment operator
     * \param rhs value from which to copy
     */
    string_array&
    operator=(const string_array &rhs);

    /*!
     * Swap operator.
     */
    void
    swap(string_array &obj);

    /*!
     * Return the size() of the string_array.
     */
    unsigned int
    size(void) const;

    /*!
     * Resize of the string_array.
     * \param size new size of the array
     * \param value value with which to set all new elements
     */
    void
    resize(unsigned int size, c_string value = "");

    /*!
     * Get all elements of the string_array, pointer
     * and pointer contents are valid until the string_array
     * is modified.
     */
    c_array<const c_string>
    get(void) const;

    /*!
     * Return an element of the array, pointer is
     * valid until the string_array is modified.
     * \param I element to get
     */
    c_string
    get(unsigned int I) const;

    /*!
     * Set an element of the array
     * \param I element to set
     * \param value string from which to copy to the element
     */
    string_array&
    set(unsigned int I, c_string value);

    /*!
     * Add an element
     * \param value string from which to copy to the element
     */
    string_array&
    push_back(c_string value);

    /*!
     * Reduce the size by one.
     */
    string_array&
    pop_back(void);

  private:
    void *m_d;
  };
}
/*! @} */
