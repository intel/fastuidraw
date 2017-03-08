#include <assert.h>
#include <stddef.h>
#include <fastuidraw/painter/fill_rule.hpp>

namespace
{
  bool
  odd_even(int w)
  {
    return w % 2 != 0;
  }

  bool
  complement_odd_even(int w)
  {
    return w % 2 == 0;
  }

  bool
  nonzero(int w)
  {
    return w != 0;
  }

  bool
  complement_nonzero(int w)
  {
    return w == 0;
  }
}

////////////////////////////////////
// fastuidraw::CustomFillRuleFunction methods
fastuidraw::CustomFillRuleFunction::fill_rule_fcn
fastuidraw::CustomFillRuleFunction::
function_from_enum(enum PainterEnums::fill_rule_t fill_rule)
{
  switch(fill_rule)
    {
    case PainterEnums::odd_even_fill_rule:
      return odd_even;

    case PainterEnums::complement_odd_even_fill_rule:
      return complement_odd_even;

    case PainterEnums::nonzero_fill_rule:
      return nonzero;

    case PainterEnums::complement_nonzero_fill_rule:
      return complement_nonzero;

    default:
      assert(!"Passed invald enumeration to fastuidraw::CustomFillRuleFunction::function_from_enum()\n");
      return NULL;
    }
}
