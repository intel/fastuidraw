/*!
 * \file fill_rule.hpp
 * \brief file fill_rule.hpp
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


namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */
  /*!
    Base class to specify a custom fill rule.
   */
  class CustomFillRuleBase
  {
  public:
    /*!
      To be implemented by a derived class to return
      true if to draw those regions with the passed
      winding number.
      \param winding_number winding number value to test.
     */
    virtual
    bool
    operator()(int winding_number) const = 0;
  };

  /*!
    Class to specify a custom fill rule from
    a function.
  */
  class CustomFillRuleFunction:public CustomFillRuleBase
  {
  public:
    /*!
      Ctor.
      \param fill_rule function to use to implement
                       operator(int) const.
     */
    CustomFillRuleFunction(bool (*fill_rule)(int)):
      m_fill_rule(fill_rule)
    {}

    virtual
    bool
    operator()(int winding_number) const
    {
      return m_fill_rule && m_fill_rule(winding_number);
    }

  private:
    bool (*m_fill_rule)(int);
  };
/*! @} */
}
