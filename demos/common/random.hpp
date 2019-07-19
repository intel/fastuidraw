#ifndef FASTUIDRAW_DEMO_RANDOM_HPP
#define FASTUIDRAW_DEMO_RANDOM_HPP

#include <fastuidraw/util/vecN.hpp>

float
random_value(float pmin, float pmax);

template<size_t N>
fastuidraw::vecN<float, N>
random_value(fastuidraw::vecN<float, N> pmin, fastuidraw::vecN<float, N> pmax)
{
  fastuidraw::vecN<float, N> return_value;
  for(unsigned int i = 0; i < N; ++i)
    {
      return_value[i] = random_value(pmin[i], pmax[i]);
    }
  return return_value;
}

#endif
