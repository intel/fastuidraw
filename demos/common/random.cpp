#include <stdlib.h>
#include "random.hpp"

static
void
init_srand(void)
{
  static bool inited(false);
  if(!inited)
    {
      inited = true;
      srand(0);
    }
}


float
random_value(float pmin, float pmax)
{
  int v;
  float r;

  init_srand();
  v = rand();
  r = static_cast<float>(v) / static_cast<float>(RAND_MAX);
  return pmin + r * (pmax - pmin);
}
