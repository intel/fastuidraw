#include "read_colorstops.hpp"

void
read_colorstops(fastuidraw::ColorStopSequence &seq, std::istream &input)
{
  while(input)
    {
      float t;
      unsigned int r, g, b, a;

      input >> t;
      input >> r;
      input >> g;
      input >> b;
      input >> a;

      if(!input.fail())
        {
          fastuidraw::ColorStop c(fastuidraw::u8vec4(r, g, b, a), t);
          seq.add(c);
        }
    }
}
