#pragma once

#include <stdint.h>

namespace ieeehalfprecision
{
  void singles2halfp(uint16_t *target, const uint32_t *source, int numel);
  void halfp2singles(uint32_t *target, const uint16_t *source, int numel);
}
