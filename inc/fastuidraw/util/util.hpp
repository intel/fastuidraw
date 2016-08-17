/*!
 * \file util.hpp
 * \brief file util.hpp
 *
 * Adapted from: WRATHUtil.hpp and type_tag.hpp of WRATH:
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

#include <stdint.h>
#include <stddef.h>

namespace fastuidraw
{
/*!\addtogroup Utility
  @{
 */

  /*!\union generic_data
    Union to store 32-bit data.
   */
  union generic_data
  {
    /*!
      The data as an uint32_t
     */
    uint32_t u;

    /*!
      The data as an int32_t
     */
    int32_t  i;

    /*!
      The data as a float
     */
    float    f;
  };

  /*!
    Enumeration for simple return codes for functions
    for success or failure.
  */
  enum return_code
    {
      /*!
        Routine failed
      */
      routine_fail,

      /*!
        Routine suceeded
      */
      routine_success
    };
  /*!
    Returns the floor of the log2 of an unsinged integer,
    i.e. the value K so that 2^K <= x < 2^{K+1}
   */
  uint32_t
  uint32_log2(uint32_t v);

  /*!
    Returns the number of bits required to hold a 32-bit
    unsigned integer value.
   */
  uint32_t
  number_bits_required(uint32_t v);

  /*!
    Returns true if a uint32_t is
    an exact non-zero power of 2.
    \param v uint32_t to query
  */
  inline
  bool
  is_power_of_2(uint32_t v)
  {
    return v && !(v & (v - 1));
  }

  /*!
    Given if a bit should be up or down returns
    an input value with that bit made to be up
    or down.
    \param input_value value to return with the named bit(s) changed
    \param to_apply if true, return value has bits made up, otherwise has bits down
    \param bitfield_value bits to make up or down as according to to_apply
   */
  inline
  uint32_t
  apply_bit_flag(uint32_t input_value, bool to_apply,
		 uint32_t bitfield_value)
  {
    return to_apply?
      input_value|bitfield_value:
      input_value&(~bitfield_value);
  }

  /*!
    Returns a value rounded up to the nearest multiple of an
    alignment value
    \param v value to round up
    \param alignment value of which the return value is to be
           a multiple
   */
  unsigned int
  round_up_to_multiple(unsigned int v, unsigned int alignment);

  /*!
    Pack the lowest N bits of a value at a bit.
    \param bit0 bit location of return value at which to pack
    \param num_bits number of bits from value to pack
    \param value value to pack
   */
  inline
  uint32_t
  pack_bits(uint32_t bit0, uint32_t num_bits, uint32_t value)
  {
    uint32_t mask;
    mask = (1u << num_bits) - 1u;
    return (value & mask) << bit0;
  }

  /*!
    Unpack N bits from a bit location.
    \param bit0 starting bit from which to unpack
    \param num_bits number bits to unpack
    \param value value from which to unpack
   */
  inline
  uint32_t
  unpack_bits(uint32_t bit0, uint32_t num_bits, uint32_t value)
  {
    uint32_t mask;
    mask = (1u << num_bits) - 1u;
    return (value >> bit0) & mask;
  }

  /*!
    A class reprenting the STL range
    [m_begin, m_end).
  */
  template<typename T>
  class range_type
  {
  public:
    /*!
      Ctor.
      \param b value with which to initialize m_begin
      \param e value with which to initialize m_end
    */
    range_type(T b, T e):
      m_begin(b),
      m_end(e)
    {}

    /*!
      Empty ctor, m_begin and m_end are uninitialized.
    */
    range_type(void)
    {}

    /*!
      Iterator to first element
    */
    T m_begin;

    /*!
      iterator to one past the last element
    */
    T m_end;
  };

  /*!
    Class for which copy ctor and assignment operator
    are private functions.
   */
  class noncopyable
  {
  public:
    noncopyable(void)
    {}

  private:
    noncopyable(const noncopyable &obj);

    noncopyable&
    operator=(const noncopyable &rhs);
  };
/*! @} */
}

/*!\addtogroup Utility
  @{
 */

/*!\def FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS
  Macro that gives the maximum value that can be
  held with a given number of bits. Caveat:
  if X is 32 (or higher), bad things may happen
  from overflow.
  \param X number bits
 */
#define FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(X) ( (uint32_t(1) << uint32_t(X)) - uint32_t(1) )

/*!\def FASTUIDRAWunused
  Macro to stop the compiler from reporting
  an argument as unused. Typically used on
  those arguments used in assert invocation
  but otherwise unused.
  \param X expression of which to ignore the value
 */
#define FASTUIDRAWunused(X) do { (void)(X); } while(0)

/*! @} */
