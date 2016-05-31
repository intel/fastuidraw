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
    Enumeration type to label to copy a range
    of values.
  */
  enum copy_range_tag_type
    {
      /*!
        Enumeration type to label to copy a range
        of values.
      */
      copy_range_tag,
    };

  /*!
    Enumeration to indicate which coordinate is fixed.
   */
  enum coordinate_type
    {
      /*!
        indicates the x-coordinate is fixed
        (and thus the y-coordinate is varying).
       */
      x_fixed=0,

      /*!
        indicates the y-coordinate is fixed
        (and thus the x-coordinate is varying).
       */
      y_fixed=1,

      /*!
        Equivalent to y_fixed
       */
      x_varying=y_fixed,

      /*!
        Equivalent to x_fixed
       */
      y_varying=x_fixed
    };

  /*!
    Returns the coordinate index that is fixed
    for a given coordinate_type, i.e.
    x_fixed returns 0.
    \param tp coordinate_type to query.
   */
  inline
  int
  fixed_coordinate(enum coordinate_type tp)
  {
    return tp;
  }

  /*!
    Returns the coordinateindex
    that is varying for a given
    coordinate_type, i.e.
    x_fixed returns 1.
    \param tp coordinate_type to query.
   */
  inline
  int
  varying_coordinate(enum coordinate_type tp)
  {
    return 1-fixed_coordinate(tp);
  }

  /*!
    Returns the smallest power of 2 which
    is atleast as large as the passed value,
    i.e. returns n = 2^k where
    2^{k-1} < v <= 2^k. If v=0, returns 1.
    \param v value to find the least power of 2 that is atleast
  */
  inline
  uint32_t
  ceiling_power_2(uint32_t v)
  {
    uint32_t n;

    n=(v>0)?
      (v-1):
      0;

    //algorithm:
    //  n = v -1
    // say n = abcdefgh (binary), then:
    //
    // n|= (n >> 1) makes n= a (a|b) (b|c) (c|d) (d|e) (e|f) (f|g)
    // n|= (n >> 2) makes n= a (a|b) (a|b|c) (a|b|c|d) (b|c|d|e) (c|d|e|f) (d|e|f|g)
    // n|= (n >> 4) makes n= a (a|b) (a|b|c) (a|b|c|d) (a|d|c|d|e) (a|b|c|d|e|f) (a|b|c|d|e|f|g)
    //
    // thus the bits of n have the property that once a bit is
    // up, all following bits are also up, thus n=-1 + 2^k
    // incrementing by 1 then makes n= 2^k, which is then
    // the precise power of 2 which is strictly greater than v-1.
    // ain't bits grand? Dug this algorithm up from somewhere
    // on the internet. Operation count= 5 bit-ors, 5 bit-shits,
    // one increment, one decrement and one conditional move.

    n|= (n >> 1);
    n|= (n >> 2);
    n|= (n >> 4);
    n|= (n >> 8);
    n|= (n >> 16);
    ++n;

    return n;
  }

  /*!
    Returns the floor of the log2 of an unsinged integer.
   */
  uint32_t
  uint32_log2(uint32_t v);

  /*!
    Returns the smallest power of 2
    for which a given uint32_t is
    greater than or equal to.
    \param v uint32_t to query
  */
  inline
  uint32_t
  floor_power_2(uint32_t v)
  {
    uint32_t n;

    n=ceiling_power_2(v);
    return (n==v)?
      v:
      n/2;
  }

  /*!
    Returns the ceiling log2, i.e. the value K so that
    2^K <= x < 2^{K+1}
   */
  uint32_t
  floor_log2(uint32_t x);

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
    Template meta-programming helper
    to specify a type via a tag.
  */
  template<typename T>
  struct type_tag
  {
    /*!
      Get the original type from this typedef.
    */
    typedef T type;
  };

  /*!
    Template meta-programming helper
    to get a type tag from a type.
  */
  template<typename T>
  struct type_tag<T>
  get_type_tag(const T&)
  {
    return type_tag<T>();
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
  held with a given number of bits
  \param X number bits
 */
#define FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(X) ( (1<<(X)) - 1 )

/*!\def FASTUIDRAWunused
  Macro to stop the compiler from reporting
  an argument as unused. Typically used on
  those arguments used in assert invocation
  but otherwise unused.
  \param X expression of which to ignore the value
 */
#define FASTUIDRAWunused(X) do { (void)(X); } while(0)

/*! @} */
