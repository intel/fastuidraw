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

#include <fastuidraw/painter/painter_enums.hpp>

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
      Typedef to fill rule function pointer type
     */
    typedef bool (*fill_rule_fcn)(int);

    /*!
      Ctor.
      \param fill_rule function to use to implement
                       operator(int) const.
     */
    explicit
    CustomFillRuleFunction(fill_rule_fcn fill_rule):
      m_fill_rule(fill_rule)
    {}

    /*!
      Ctor from a PainterEnums::fill_rule_t enumeration
      \param fill_rule enumeration for fill rule.
     */
    explicit
    CustomFillRuleFunction(enum PainterEnums::fill_rule_t fill_rule):
      m_fill_rule(function_from_enum(fill_rule))
    {}

    virtual
    bool
    operator()(int winding_number) const
    {
      return m_fill_rule && m_fill_rule(winding_number);
    }

    /*!
      Returns a \ref fill_rule_fcn implementing
      a fill rule from a \ref PainterEnums::fill_rule_t.
      \param fill_rule enumerated fill rule.
     */
    static
    fill_rule_fcn
    function_from_enum(enum PainterEnums::fill_rule_t fill_rule);

  private:
    fill_rule_fcn m_fill_rule;
  };
  /*! @} */
}
