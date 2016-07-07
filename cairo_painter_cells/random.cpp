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

vec2
random_value(const vec2 &pmin, const vec2 &pmax)
{
  vec2 R;
  R.x() = random_value(pmin.x(), pmax.x());
  R.x() = random_value(pmin.y(), pmax.y());
  return R;
}
