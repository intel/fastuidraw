#include "cycle_value.hpp"

void
cycle_value(unsigned int &value,
            bool decrement,
            unsigned int limit_value)
{
  if (decrement)
    {
      if (value == 0)
        {
          value = limit_value;
        }
      --value;
    }
  else
    {
      ++value;
      if (value == limit_value)
        {
          value = 0;
        }
    }
}
