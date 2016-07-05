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


SkScalar
random_value(SkScalar pmin, SkScalar pmax)
{
  int v;
  SkScalar r;

  init_srand();
  v = rand();
  r = SkScalar(v) / SkScalar(RAND_MAX);
  return pmin + r * (pmax - pmin);
}



SkPoint
random_value(SkPoint pmin, SkPoint pmax)
{
  SkPoint R;
  R.fX = random_value(pmin.x(), pmax.x());
  R.fY = random_value(pmin.y(), pmax.y());
  return R;
}
