#include <stdlib.h>
#include "random.hpp"

float
random_value(float pmin, float pmax)
{
  int v;
  float r;
  static bool inited(false);

  if(!inited)
    {
      inited = true;
      srand(0);
    }

  v = rand();
  r = static_cast<float>(v) / static_cast<float>(RAND_MAX);
  return pmin + r * (pmax - pmin);
}
