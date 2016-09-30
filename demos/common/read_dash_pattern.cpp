#include "read_dash_pattern.hpp"

void
read_dash_pattern(std::vector<fastuidraw::PainterDashedStrokeParams::DashPatternElement> &pattern_out,
                  std::istream &input_stream)
{
  fastuidraw::PainterDashedStrokeParams::DashPatternElement dsh;

  pattern_out.clear();
  while(input_stream >> dsh.m_draw_length >> dsh.m_space_length)
    {
      pattern_out.push_back(dsh);
    }
}
